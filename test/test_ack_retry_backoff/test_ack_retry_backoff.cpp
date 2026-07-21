#include <gtest/gtest.h>

#include <helpers/BaseChatMesh.h>
#include <helpers/SimpleMeshTables.h>

#include <cstring>
#include <vector>

// Keep this integration harness independent of firmware entry points while
// exercising the real dispatcher, mesh and BaseChatMesh lifecycle.
#include "../../src/Dispatcher.cpp"
#include "../../src/Mesh.cpp"
#include "../../src/helpers/AdvertDataHelpers.cpp"
#include "../../src/helpers/BaseChatMesh.cpp"

void StrHelper::strncpy(char* dest, const char* src, size_t size) {
  if (size == 0) return;
  std::strncpy(dest, src, size - 1);
  dest[size - 1] = 0;
}
void StrHelper::strzcpy(char* dest, const char* src, size_t size) {
  if (size == 0) return;
  std::strncpy(dest, src, size);
  dest[size - 1] = 0;
}
const char* StrHelper::ftoa(float) { return "0"; }
const char* StrHelper::ftoa3(float) { return "0"; }
bool StrHelper::isBlank(const char* value) { return value == nullptr || *value == 0; }
uint32_t StrHelper::fromHex(const char*) { return 0; }

namespace {

class TestClock final : public mesh::MillisecondClock {
public:
  unsigned long now = 1000;
  unsigned long getMillis() override { return now; }
};

class TestRTC final : public mesh::RTCClock {
public:
  uint32_t now = 1;
  uint32_t getCurrentTime() override { return now; }
  void setCurrentTime(uint32_t value) override { now = value; }
};

class TestRNG final : public mesh::RNG {
public:
  uint32_t value = 0;
  void random(uint8_t* dest, size_t size) override {
    ASSERT_EQ(sizeof(value), size);
    memcpy(dest, &value, sizeof(value));
  }
};

class TestRadio final : public mesh::Radio {
public:
  uint32_t airtime = 20;
  bool complete = false;
  bool start_success = true;
  bool use_raw_length_as_airtime = false;

  int recvRaw(uint8_t*, int) override { return 0; }
  uint32_t getEstAirtimeFor(int raw_length) override {
    return use_raw_length_as_airtime ? raw_length : airtime;
  }
  float packetScore(float, int) override { return 1.0f; }
  bool startSendRaw(const uint8_t*, int) override { return start_success; }
  bool isSendComplete() override { return complete; }
  void onSendFinished() override {}
  bool isInRecvMode() const override { return true; }
};

class TestPacketManager final : public mesh::PacketManager {
  std::vector<mesh::Packet*> allocated;

public:
  struct Entry {
    mesh::Packet* packet;
    uint32_t scheduled_for;
  };
  std::vector<Entry> outbound;
  bool reject_next = false;

  ~TestPacketManager() {
    for (mesh::Packet* packet : allocated) delete packet;
  }

  mesh::Packet* allocNew() override {
    mesh::Packet* packet = new mesh::Packet();
    allocated.push_back(packet);
    return packet;
  }
  void free(mesh::Packet*) override {}
  void queueOutbound(mesh::Packet* packet, uint8_t, uint32_t scheduled_for) override {
    if (reject_next) {
      reject_next = false;
      return;
    }
    outbound.push_back({packet, scheduled_for});
  }
  mesh::Packet* getNextOutbound(uint32_t now) override {
    for (size_t i = 0; i < outbound.size(); i++) {
      if ((int32_t)(outbound[i].scheduled_for - now) <= 0) {
        mesh::Packet* packet = outbound[i].packet;
        outbound.erase(outbound.begin() + i);
        return packet;
      }
    }
    return nullptr;
  }
  mesh::Packet* peekNextOutbound(uint32_t now) override {
    for (const Entry& entry : outbound) {
      if ((int32_t)(entry.scheduled_for - now) <= 0) return entry.packet;
    }
    return nullptr;
  }
  int getOutboundCount(uint32_t now) const override {
    int count = 0;
    for (const Entry& entry : outbound) {
      if ((int32_t)(entry.scheduled_for - now) <= 0) count++;
    }
    return count;
  }
  int getOutboundTotal() const override { return outbound.size(); }
  int getFreeCount() const override { return 32; }
  mesh::Packet* getOutboundByIdx(int index) override {
    return index >= 0 && static_cast<size_t>(index) < outbound.size() ? outbound[index].packet : nullptr;
  }
  mesh::Packet* removeOutboundByIdx(int index) override {
    if (index < 0 || static_cast<size_t>(index) >= outbound.size()) return nullptr;
    mesh::Packet* packet = outbound[index].packet;
    outbound.erase(outbound.begin() + index);
    return packet;
  }
  void queueInbound(mesh::Packet*, uint32_t) override {}
  mesh::Packet* getNextInbound(uint32_t) override { return nullptr; }
};

class TestChatMesh final : public BaseChatMesh {
public:
  int timeout_count = 0;
  int ack_match_count = 0;
  uint32_t response_timeout = 1000;
  bool match_ack = false;
  uint32_t accepted_ack = 0;
  ContactInfo acknowledged_contact{};

  TestChatMesh(TestRadio& radio, TestClock& clock, TestRNG& rng, TestRTC& rtc,
               TestPacketManager& manager, mesh::MeshTables& tables)
      : BaseChatMesh(radio, clock, rng, rtc, manager, tables) {}

  void acceptAck(uint32_t ack) {
    accepted_ack = ack;
    match_ack = true;
  }

  void receiveStandaloneAck(uint32_t ack) {
    mesh::Packet packet;
    packet.header = ROUTE_TYPE_DIRECT | (PAYLOAD_TYPE_ACK << PH_TYPE_SHIFT);
    packet.path_len = 0;
    packet.payload_len = sizeof(ack);
    memcpy(packet.payload, &ack, sizeof(ack));
    onRecvPacket(&packet);
  }

  void receivePathAck(uint32_t ack) {
    uint8_t empty_path[1] = {0};
    onContactPathRecv(acknowledged_contact, empty_path, 0, empty_path, 0,
                      PAYLOAD_TYPE_ACK, reinterpret_cast<uint8_t*>(&ack), sizeof(ack));
  }

protected:
  void onDiscoveredContact(ContactInfo&, bool, uint8_t, const uint8_t*) override {}
  ContactInfo* processAck(const uint8_t* data) override {
    if (!match_ack || memcmp(data, &accepted_ack, sizeof(accepted_ack)) != 0) return nullptr;
    match_ack = false;
    ack_match_count++;
    return &acknowledged_contact;
  }
  void onContactPathUpdated(const ContactInfo&) override {}
  void onMessageRecv(const ContactInfo&, mesh::Packet*, uint32_t, const char*) override {}
  void onCommandDataRecv(const ContactInfo&, mesh::Packet*, uint32_t, const char*) override {}
  void onSignedMessageRecv(const ContactInfo&, mesh::Packet*, uint32_t, const uint8_t*, const char*) override {}
  uint32_t calcFloodTimeoutMillisFor(uint32_t) const override { return response_timeout; }
  uint32_t calcDirectTimeoutMillisFor(uint32_t, uint8_t) const override { return response_timeout; }
  void onSendTimeout() override { timeout_count++; }
  void onChannelMessageRecv(const mesh::GroupChannel&, mesh::Packet*, uint32_t, const char*) override {}
  uint8_t onContactRequest(const ContactInfo&, uint32_t, const uint8_t*, uint8_t, uint8_t*) override { return 0; }
  void onContactResponse(const ContactInfo&, const uint8_t*, uint8_t) override {}
};

struct AckRetryBackoffTest : public testing::Test {
  TestClock clock;
  TestRTC rtc;
  TestRNG rng;
  TestRadio radio;
  TestPacketManager manager;
  SimpleMeshTables tables;
  TestChatMesh mesh{radio, clock, rng, rtc, manager, tables};
  ContactInfo recipient{};

  void SetUp() override {
    recipient.id.pub_key[0] = 0x42;
    recipient.out_path_len = 1;
    recipient.out_path[0] = 0x42;
    recipient.shared_secret_valid = false;
    mesh.begin();
  }

  int send(uint8_t attempt, uint32_t& ack, uint32_t& timeout) {
    return mesh.sendMessage(recipient, 1234, attempt, "hello", ack, timeout);
  }

  void startAndCompleteTransmission() {
    clock.now++;
    mesh.loop();
    radio.complete = true;
    clock.now++;
    mesh.loop();
  }
};

TEST_F(AckRetryBackoffTest, DoesNotStartAckTimerUntilLocalTransmissionCompletes) {
  uint32_t ack;
  uint32_t timeout;
  ASSERT_EQ(MSG_SEND_SENT_DIRECT, send(0, ack, timeout));

  clock.now += timeout + 1;
  EXPECT_EQ(0, mesh.timeout_count);

  mesh.loop();
  radio.complete = true;
  clock.now++;
  mesh.loop();
  clock.now += 1001;
  mesh.loop();

  EXPECT_EQ(1, mesh.timeout_count);
}

TEST_F(AckRetryBackoffTest, CoalescesRetryWhileOriginalIsLocallyQueued) {
  uint32_t first_ack;
  uint32_t first_timeout;
  ASSERT_EQ(MSG_SEND_SENT_DIRECT, send(0, first_ack, first_timeout));

  uint32_t retry_ack;
  uint32_t retry_timeout;
  EXPECT_EQ(MSG_SEND_SENT_DIRECT, send(1, retry_ack, retry_timeout));

  EXPECT_EQ(first_ack, retry_ack);
  EXPECT_EQ(1u, manager.outbound.size());
  EXPECT_LE(retry_timeout, first_timeout);
}

TEST_F(AckRetryBackoffTest, CoalescesRetryDuringAckTransitWindow) {
  uint32_t first_ack;
  uint32_t first_timeout;
  ASSERT_EQ(MSG_SEND_SENT_DIRECT, send(0, first_ack, first_timeout));
  startAndCompleteTransmission();

  uint32_t retry_ack;
  uint32_t retry_timeout;
  EXPECT_EQ(MSG_SEND_SENT_DIRECT, send(1, retry_ack, retry_timeout));

  EXPECT_EQ(first_ack, retry_ack);
  EXPECT_TRUE(manager.outbound.empty());
  EXPECT_LE(retry_timeout, 1000u);
}

TEST_F(AckRetryBackoffTest, StandaloneAckClearsWaitAndAllowsSameOperationAgain) {
  uint32_t first_ack;
  uint32_t first_timeout;
  ASSERT_EQ(MSG_SEND_SENT_DIRECT, send(0, first_ack, first_timeout));
  startAndCompleteTransmission();

  mesh.acceptAck(first_ack);
  mesh.receiveStandaloneAck(first_ack);
  EXPECT_EQ(1, mesh.ack_match_count);

  clock.now += 1001;
  mesh.loop();
  EXPECT_EQ(0, mesh.timeout_count);

  uint32_t next_ack;
  uint32_t next_timeout;
  EXPECT_EQ(MSG_SEND_SENT_DIRECT, send(0, next_ack, next_timeout));
  EXPECT_EQ(1u, manager.outbound.size());
}

TEST_F(AckRetryBackoffTest, PathEmbeddedAckClearsWaitAndAllowsSameOperationAgain) {
  uint32_t first_ack;
  uint32_t first_timeout;
  ASSERT_EQ(MSG_SEND_SENT_DIRECT, send(0, first_ack, first_timeout));
  startAndCompleteTransmission();

  mesh.acceptAck(first_ack);
  mesh.receivePathAck(first_ack);
  EXPECT_EQ(1, mesh.ack_match_count);

  clock.now += 1001;
  mesh.loop();
  EXPECT_EQ(0, mesh.timeout_count);

  uint32_t next_ack;
  uint32_t next_timeout;
  EXPECT_EQ(MSG_SEND_SENT_DIRECT, send(0, next_ack, next_timeout));
  EXPECT_EQ(1u, manager.outbound.size());
}

TEST_F(AckRetryBackoffTest, DoesNotCoalesceDifferentTextWithReusedTimestamp) {
  uint32_t first_ack;
  uint32_t first_timeout;
  ASSERT_EQ(MSG_SEND_SENT_DIRECT, send(0, first_ack, first_timeout));

  uint32_t second_ack;
  uint32_t second_timeout;
  EXPECT_EQ(MSG_SEND_SENT_DIRECT,
            mesh.sendMessage(recipient, 1234, 0, "different", second_ack, second_timeout));

  EXPECT_EQ(2u, manager.outbound.size());
}

TEST_F(AckRetryBackoffTest, AppliesFreshBoundedBackoffAfterAckTimeout) {
  uint32_t ack;
  uint32_t timeout;
  ASSERT_EQ(MSG_SEND_SENT_DIRECT, send(0, ack, timeout));
  startAndCompleteTransmission();
  clock.now += 1001;
  mesh.loop();
  ASSERT_EQ(1, mesh.timeout_count);

  rng.value = 100;
  ASSERT_EQ(MSG_SEND_SENT_DIRECT, send(1, ack, timeout));
  ASSERT_EQ(1u, manager.outbound.size());
  EXPECT_EQ(clock.now + 350, manager.outbound[0].scheduled_for);
  EXPECT_EQ(1610u, timeout);
}

TEST_F(AckRetryBackoffTest, RejectsAttemptsBeyondLocalLimit) {
  uint32_t ack;
  uint32_t timeout;

  EXPECT_EQ(MSG_SEND_FAILED, send(ACK_RETRY_MAX_ATTEMPTS, ack, timeout));
  EXPECT_TRUE(manager.outbound.empty());
}

TEST_F(AckRetryBackoffTest, RejectsRetryAfterMaximumOperationAge) {
  uint32_t ack;
  uint32_t timeout;
  ASSERT_EQ(MSG_SEND_SENT_DIRECT, send(0, ack, timeout));
  startAndCompleteTransmission();
  clock.now += ACK_RETRY_MAX_AGE_MILLIS;

  EXPECT_EQ(MSG_SEND_FAILED, send(1, ack, timeout));
  EXPECT_TRUE(manager.outbound.empty());
}

TEST_F(AckRetryBackoffTest, ReportsQueuePressureInExternalTimeoutEstimate) {
  mesh::Packet* blocker = manager.allocNew();
  manager.queueOutbound(blocker, 0, clock.now + 60000);

  uint32_t ack;
  uint32_t timeout;
  ASSERT_EQ(MSG_SEND_SENT_DIRECT, send(0, ack, timeout));

  EXPECT_EQ(1520u, timeout);
}

TEST_F(AckRetryBackoffTest, IncludesEncodedDirectPathInAirtimeEstimate) {
  radio.use_raw_length_as_airtime = true;
  recipient.out_path_len = 10;
  memset(recipient.out_path, 0x42, 10);

  uint32_t ack;
  uint32_t timeout;
  ASSERT_EQ(MSG_SEND_SENT_DIRECT, send(0, ack, timeout));
  ASSERT_EQ(1u, manager.outbound.size());
  uint32_t encoded_airtime = manager.outbound[0].packet->getRawLength();

  EXPECT_EQ(1000u + encoded_airtime + ACK_RETRY_CAD_ESTIMATE_MS, timeout);
}

TEST_F(AckRetryBackoffTest, RejectsPacketWhenOutboundQueueRefusesIt) {
  manager.reject_next = true;
  uint32_t ack;
  uint32_t timeout;

  EXPECT_EQ(MSG_SEND_FAILED, send(0, ack, timeout));
  EXPECT_TRUE(manager.outbound.empty());
}

TEST_F(AckRetryBackoffTest, ReportsLocalRadioStartFailureWithoutWaitingForAckWindow) {
  radio.start_success = false;
  uint32_t ack;
  uint32_t timeout;
  ASSERT_EQ(MSG_SEND_SENT_DIRECT, send(0, ack, timeout));

  clock.now++;
  mesh.loop();
  EXPECT_EQ(0, mesh.timeout_count);
  clock.now++;
  mesh.loop();

  EXPECT_EQ(1, mesh.timeout_count);
}

TEST_F(AckRetryBackoffTest, SaturatedEstimateCannotCreateAnExpiredInternalDeadline) {
  mesh.response_timeout = UINT32_MAX;
  uint32_t ack;
  uint32_t timeout;
  ASSERT_EQ(MSG_SEND_SENT_DIRECT, send(0, ack, timeout));
  EXPECT_EQ(UINT32_MAX, timeout);

  startAndCompleteTransmission();
  clock.now++;
  mesh.loop();

  EXPECT_EQ(0, mesh.timeout_count);
}

}  // namespace

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

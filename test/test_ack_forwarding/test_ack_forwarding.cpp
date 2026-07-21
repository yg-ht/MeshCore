#include <gtest/gtest.h>

#include <Mesh.h>
#include <helpers/SimpleMeshTables.h>

#include <cstring>
#include <vector>

// The native environment builds a deliberately small source set. Include the
// two implementations under test here, following the existing packet-queue
// test pattern, so this suite does not broaden every native test executable.
#include "../../src/Dispatcher.cpp"
#include "../../src/Mesh.cpp"

namespace {

constexpr uint8_t ACK_BYTES[] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC};

class TestClock final : public mesh::MillisecondClock {
public:
  unsigned long now = 1000;

  unsigned long getMillis() override { return now; }
};

class TestRTC final : public mesh::RTCClock {
public:
  uint32_t now = 1;

  uint32_t getCurrentTime() override { return now; }
  void setCurrentTime(uint32_t time) override { now = time; }
};

class TestRNG final : public mesh::RNG {
public:
  void random(uint8_t* dest, size_t size) override { memset(dest, 0, size); }
};

class TestRadio final : public mesh::Radio {
public:
  int recvRaw(uint8_t*, int) override { return 0; }
  uint32_t getEstAirtimeFor(int) override { return 10; }
  float packetScore(float, int) override { return 1.0f; }
  bool startSendRaw(const uint8_t*, int) override { return true; }
  bool isSendComplete() override { return true; }
  void onSendFinished() override {}
  bool isInRecvMode() const override { return true; }
};

class TestPacketManager final : public mesh::PacketManager {
  std::vector<mesh::Packet*> allocated;
  std::vector<mesh::Packet*> outbound;
  int free_calls = 0;

public:
  ~TestPacketManager() {
    for (mesh::Packet* packet : allocated) delete packet;
  }

  mesh::Packet* allocNew() override {
    mesh::Packet* packet = new mesh::Packet();
    allocated.push_back(packet);
    return packet;
  }

  void free(mesh::Packet*) override { free_calls++; }

  void queueOutbound(mesh::Packet* packet, uint8_t, uint32_t) override {
    outbound.push_back(packet);
  }

  mesh::Packet* getNextOutbound(uint32_t) override {
    if (outbound.empty()) return nullptr;
    mesh::Packet* packet = outbound.front();
    outbound.erase(outbound.begin());
    return packet;
  }

  mesh::Packet* peekNextOutbound(uint32_t) override {
    return outbound.empty() ? nullptr : outbound.front();
  }

  int getOutboundCount(uint32_t) const override { return outbound.size(); }
  int getOutboundTotal() const override { return outbound.size(); }
  int getFreeCount() const override { return 32; }

  mesh::Packet* getOutboundByIdx(int index) override {
    return index >= 0 && static_cast<size_t>(index) < outbound.size() ? outbound[index] : nullptr;
  }

  mesh::Packet* removeOutboundByIdx(int index) override {
    if (index < 0 || static_cast<size_t>(index) >= outbound.size()) return nullptr;
    mesh::Packet* packet = outbound[index];
    outbound.erase(outbound.begin() + index);
    return packet;
  }

  void queueInbound(mesh::Packet*, uint32_t) override {}
  mesh::Packet* getNextInbound(uint32_t) override { return nullptr; }

  size_t getAllocationCount() const { return allocated.size(); }
  int getFreeCallCount() const { return free_calls; }
};

class TestMesh final : public mesh::Mesh {
  TestPacketManager& manager;
  uint8_t extra_ack_count;

protected:
  bool allowPacketForward(const mesh::Packet*) override { return true; }
  uint32_t getDirectRetransmitDelay(const mesh::Packet*) override { return 0; }
  uint8_t getExtraAckTransmitCount() const override { return extra_ack_count; }

  void onAckRecv(mesh::Packet*, uint32_t) override { ack_delivery_count++; }

public:
  int ack_delivery_count = 0;

  TestMesh(TestRadio& radio, TestClock& clock, TestRNG& rng, TestRTC& rtc,
           TestPacketManager& packet_manager, mesh::MeshTables& tables, uint8_t extra_acks)
      : Mesh(radio, clock, rng, rtc, packet_manager, tables),
        manager(packet_manager),
        extra_ack_count(extra_acks) {
    memset(self_id.pub_key, 0, sizeof(self_id.pub_key));
    self_id.pub_key[0] = 0xA1;
    self_id.pub_key[1] = 0xA2;
    self_id.pub_key[2] = 0xA3;
  }

  void receive(mesh::Packet* packet) {
    mesh::DispatcherAction action = onRecvPacket(packet);
    if (action == ACTION_RELEASE) {
      manager.free(packet);
    } else if (action != ACTION_MANUAL_HOLD) {
      uint8_t priority = (action >> 24) - 1;
      uint32_t delay = action & 0xFFFFFF;
      manager.queueOutbound(packet, priority, futureMillis(delay));
    }
  }

  void setExtraAckCount(uint8_t count) { extra_ack_count = count; }
};

struct AckForwardingTest : public testing::Test {
  TestClock clock;
  TestRTC rtc;
  TestRNG rng;
  TestRadio radio;
  TestPacketManager manager;
  SimpleMeshTables tables;
  TestMesh mesh;

  explicit AckForwardingTest(uint8_t extra_acks = 1)
      : mesh(radio, clock, rng, rtc, manager, tables, extra_acks) {}

  mesh::Packet* makeAck(bool multipart, uint8_t hash_size = 1, uint8_t hop_count = 1,
                        size_t ack_length = sizeof(ACK_BYTES)) {
    mesh::Packet* packet = manager.allocNew();
    packet->header = ROUTE_TYPE_DIRECT |
                     ((multipart ? PAYLOAD_TYPE_MULTIPART : PAYLOAD_TYPE_ACK) << PH_TYPE_SHIFT);
    packet->setPathHashSizeAndCount(hash_size, hop_count);

    // The first path entry identifies this relay. Remaining entries are filled
    // with stable data so tests can verify entry-wise removal.
    memcpy(packet->path, mesh.self_id.pub_key, hash_size);
    for (uint8_t i = hash_size; i < packet->getPathByteLen(); i++) {
      packet->path[i] = static_cast<uint8_t>(0xD0 + i);
    }

    if (multipart) {
      packet->payload[0] = (1 << 4) | PAYLOAD_TYPE_ACK;
      memcpy(&packet->payload[1], ACK_BYTES, ack_length);
      packet->payload_len = ack_length + 1;
    } else {
      memcpy(packet->payload, ACK_BYTES, ack_length);
      packet->payload_len = ack_length;
    }
    return packet;
  }
};

TEST_F(AckForwardingTest, MultipartThenNormalQueuesOneLogicalAck) {
  mesh.receive(makeAck(true));
  mesh.receive(makeAck(false));

  ASSERT_EQ(1, manager.getOutboundTotal());
  mesh::Packet* forwarded = manager.getOutboundByIdx(0);
  ASSERT_NE(nullptr, forwarded);
  EXPECT_EQ(PAYLOAD_TYPE_ACK, forwarded->getPayloadType());
  EXPECT_EQ(sizeof(ACK_BYTES), forwarded->payload_len);
  EXPECT_EQ(0, memcmp(ACK_BYTES, forwarded->payload, sizeof(ACK_BYTES)));
}

TEST_F(AckForwardingTest, NormalThenMultipartQueuesOneLogicalAck) {
  mesh.receive(makeAck(false));
  mesh.receive(makeAck(true));

  EXPECT_EQ(1, manager.getOutboundTotal());
}

TEST_F(AckForwardingTest, DuplicateNormalAcksQueueOneLogicalAck) {
  mesh.receive(makeAck(false));
  mesh.receive(makeAck(false));

  EXPECT_EQ(1, manager.getOutboundTotal());
  EXPECT_EQ(1, manager.getFreeCallCount());
}

TEST_F(AckForwardingTest, DuplicateMultipartAcksQueueOneLogicalAck) {
  mesh.receive(makeAck(true));
  mesh.receive(makeAck(true));

  EXPECT_EQ(1, manager.getOutboundTotal());
  EXPECT_EQ(1, manager.getFreeCallCount());
}

TEST_F(AckForwardingTest, RelaySettingCannotIncreaseForwardedCopies) {
  mesh.setExtraAckCount(0);
  mesh.receive(makeAck(true));
  mesh.receive(makeAck(false));

  EXPECT_EQ(1, manager.getOutboundTotal());
}

TEST_F(AckForwardingTest, RelayReusesReceivedPacketWithoutReplacementAllocation) {
  mesh::Packet* received = makeAck(true);
  ASSERT_EQ(1u, manager.getAllocationCount());

  mesh.receive(received);

  ASSERT_EQ(1, manager.getOutboundTotal());
  EXPECT_EQ(1u, manager.getAllocationCount());
  EXPECT_EQ(received, manager.getOutboundByIdx(0));
}

TEST_F(AckForwardingTest, RemovesOneCompleteMultiBytePathEntry) {
  mesh.receive(makeAck(true, 3, 2));

  ASSERT_EQ(1, manager.getOutboundTotal());
  mesh::Packet* forwarded = manager.getOutboundByIdx(0);
  EXPECT_EQ(3, forwarded->getPathHashSize());
  EXPECT_EQ(1, forwarded->getPathHashCount());
  const uint8_t expected_path[] = {0xD3, 0xD4, 0xD5};
  EXPECT_EQ(0, memcmp(expected_path, forwarded->path, sizeof(expected_path)));
}

TEST_F(AckForwardingTest, RemovesOneCompleteTwoBytePathEntry) {
  mesh.receive(makeAck(false, 2, 2));

  ASSERT_EQ(1, manager.getOutboundTotal());
  mesh::Packet* forwarded = manager.getOutboundByIdx(0);
  EXPECT_EQ(2, forwarded->getPathHashSize());
  EXPECT_EQ(1, forwarded->getPathHashCount());
  const uint8_t expected_path[] = {0xD2, 0xD3};
  EXPECT_EQ(0, memcmp(expected_path, forwarded->path, sizeof(expected_path)));
}

TEST_F(AckForwardingTest, RejectsMalformedMultipartAck) {
  mesh::Packet* malformed = makeAck(true);
  malformed->payload[0] = (1 << 4) | PAYLOAD_TYPE_RESPONSE;

  mesh.receive(malformed);

  EXPECT_EQ(0, manager.getOutboundTotal());
  EXPECT_EQ(0, mesh.ack_delivery_count);
}

TEST_F(AckForwardingTest, RejectsShortMultipartAck) {
  mesh::Packet* malformed = makeAck(true);
  malformed->payload_len = 4;

  mesh.receive(malformed);

  EXPECT_EQ(0, manager.getOutboundTotal());
  EXPECT_EQ(0, mesh.ack_delivery_count);
}

TEST_F(AckForwardingTest, DoesNotForwardWhenNextHopDoesNotMatch) {
  mesh::Packet* packet = makeAck(true);
  packet->path[0] ^= 0xFF;

  mesh.receive(packet);

  EXPECT_EQ(0, manager.getOutboundTotal());
}

TEST_F(AckForwardingTest, PreservesSupportedExtendedAckLengths) {
  for (size_t ack_length = 4; ack_length <= sizeof(ACK_BYTES); ack_length++) {
    SimpleMeshTables local_tables;
    TestPacketManager local_manager;
    TestMesh local_mesh(radio, clock, rng, rtc, local_manager, local_tables, 1);

    mesh::Packet* packet = local_manager.allocNew();
    packet->header = ROUTE_TYPE_DIRECT | (PAYLOAD_TYPE_MULTIPART << PH_TYPE_SHIFT);
    packet->setPathHashSizeAndCount(1, 1);
    packet->path[0] = local_mesh.self_id.pub_key[0];
    packet->payload[0] = (1 << 4) | PAYLOAD_TYPE_ACK;
    memcpy(&packet->payload[1], ACK_BYTES, ack_length);
    packet->payload_len = ack_length + 1;

    local_mesh.receive(packet);

    ASSERT_EQ(1, local_manager.getOutboundTotal());
    mesh::Packet* forwarded = local_manager.getOutboundByIdx(0);
    EXPECT_EQ(ack_length, forwarded->payload_len);
    EXPECT_EQ(0, memcmp(ACK_BYTES, forwarded->payload, ack_length));
  }
}

TEST_F(AckForwardingTest, DeliversMultipartAndNormalPairOnceAtEndpoint) {
  mesh::Packet* multipart = makeAck(true);
  multipart->path_len = 0;
  mesh::Packet* normal = makeAck(false);
  normal->path_len = 0;

  mesh.receive(multipart);
  mesh.receive(normal);

  EXPECT_EQ(0, manager.getOutboundTotal());
  EXPECT_EQ(1, mesh.ack_delivery_count);
}

TEST_F(AckForwardingTest, EndpointDeliveryIsIndependentOfCopyOrder) {
  mesh::Packet* normal = makeAck(false);
  normal->path_len = 0;
  mesh::Packet* multipart = makeAck(true);
  multipart->path_len = 0;

  mesh.receive(normal);
  mesh.receive(multipart);

  EXPECT_EQ(0, manager.getOutboundTotal());
  EXPECT_EQ(1, mesh.ack_delivery_count);
}

}  // namespace

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

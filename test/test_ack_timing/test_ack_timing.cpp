#include <gtest/gtest.h>

#include <Mesh.h>
#include <helpers/SimpleMeshTables.h>

#include <cstring>
#include <vector>

// Keep the host test isolated from the full firmware source graph.
#include "../../src/Dispatcher.cpp"
#include "../../src/Mesh.cpp"

namespace {

class TestClock final : public mesh::MillisecondClock {
public:
  unsigned long now = 1000;
  unsigned long getMillis() override { return now; }
};

class TestRTC final : public mesh::RTCClock {
public:
  uint32_t getCurrentTime() override { return 1; }
  void setCurrentTime(uint32_t) override {}
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
  uint32_t airtime = 10;
  bool use_raw_length_as_airtime = false;

  int recvRaw(uint8_t*, int) override { return 0; }
  uint32_t getEstAirtimeFor(int raw_length) override {
    return use_raw_length_as_airtime ? raw_length : airtime;
  }
  float packetScore(float, int) override { return 1.0f; }
  bool startSendRaw(const uint8_t*, int) override { return true; }
  bool isSendComplete() override { return true; }
  void onSendFinished() override {}
  bool isInRecvMode() const override { return true; }
};

class TestPacketManager final : public mesh::PacketManager {
  std::vector<mesh::Packet*> allocated;

public:
  struct QueuedPacket {
    mesh::Packet* packet;
    uint32_t scheduled_for;
  };
  std::vector<QueuedPacket> outbound;

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
    outbound.push_back({packet, scheduled_for});
  }
  mesh::Packet* getNextOutbound(uint32_t) override { return nullptr; }
  int getOutboundCount(uint32_t) const override { return outbound.size(); }
  int getOutboundTotal() const override { return outbound.size(); }
  int getFreeCount() const override { return 32; }
  mesh::Packet* getOutboundByIdx(int index) override {
    return index >= 0 && static_cast<size_t>(index) < outbound.size() ? outbound[index].packet : nullptr;
  }
  mesh::Packet* removeOutboundByIdx(int) override { return nullptr; }
  void queueInbound(mesh::Packet*, uint32_t) override {}
  mesh::Packet* getNextInbound(uint32_t) override { return nullptr; }

  void addQueuePressure(size_t count) {
    while (outbound.size() < count) outbound.push_back({nullptr, 0});
  }
};

class TestMesh final : public mesh::Mesh {
  TestPacketManager& manager;

protected:
  bool allowPacketForward(const mesh::Packet*) override { return true; }

public:
  TestMesh(TestRadio& radio, TestClock& clock, TestRNG& rng, TestRTC& rtc,
           TestPacketManager& packet_manager, mesh::MeshTables& tables)
      : Mesh(radio, clock, rng, rtc, packet_manager, tables), manager(packet_manager) {
    memset(self_id.pub_key, 0, sizeof(self_id.pub_key));
    self_id.pub_key[0] = 0xA1;
  }

  uint32_t firstDelay(const mesh::Packet* packet, uint32_t earliest) {
    return getAckTransmitDelay(packet, earliest);
  }

  uint32_t nextDelay(const mesh::Packet* packet, uint32_t previous) {
    return getNextAckTransmitDelay(packet, previous);
  }

  uint32_t directDelay(mesh::Packet* packet, const uint8_t* path, uint8_t path_len,
                       uint32_t earliest) {
    return getDirectAckTransmitDelay(packet, path, path_len, earliest);
  }

  void receive(mesh::Packet* packet) {
    mesh::DispatcherAction action = onRecvPacket(packet);
    if (action == ACTION_RELEASE) {
      manager.free(packet);
    } else if (action != ACTION_MANUAL_HOLD) {
      manager.queueOutbound(packet, (action >> 24) - 1, futureMillis(action & 0xFFFFFF));
    }
  }
};

struct AckTimingTest : public testing::Test {
  TestClock clock;
  TestRTC rtc;
  TestRNG rng;
  TestRadio radio;
  TestPacketManager manager;
  SimpleMeshTables tables;
  TestMesh mesh{radio, clock, rng, rtc, manager, tables};

  mesh::Packet packet;

  void SetUp() override {
    packet.header = PAYLOAD_TYPE_ACK << PH_TYPE_SHIFT;
    packet.payload_len = 4;
  }

  mesh::Packet* makeDirectAck(bool multipart = false) {
    mesh::Packet* ack = manager.allocNew();
    ack->header = ROUTE_TYPE_DIRECT |
                  ((multipart ? PAYLOAD_TYPE_MULTIPART : PAYLOAD_TYPE_ACK) << PH_TYPE_SHIFT);
    ack->setPathHashSizeAndCount(1, 1);
    ack->path[0] = mesh.self_id.pub_key[0];
    ack->payload_len = multipart ? 5 : 4;
    if (multipart) {
      ack->payload[0] = (1 << 4) | PAYLOAD_TYPE_ACK;
      memset(&ack->payload[1], 0xA5, 4);
    } else {
      memset(ack->payload, 0xA5, ack->payload_len);
    }
    return ack;
  }
};

TEST_F(AckTimingTest, PreservesEarliestDelayAndMinimumJitterWindow) {
  radio.airtime = 10;
  rng.value = 100;

  EXPECT_EQ(300u, mesh.firstDelay(&packet, 200));
}

TEST_F(AckTimingTest, ScalesJitterWithAirtime) {
  radio.airtime = 200;
  rng.value = 400;

  EXPECT_EQ(600u, mesh.firstDelay(&packet, 200));
}

TEST_F(AckTimingTest, IncludesDirectPathInAirtimeEstimate) {
  uint8_t path[MAX_PATH_SIZE];
  memset(path, 0x5A, sizeof(path));
  radio.use_raw_length_as_airtime = true;
  rng.value = 138;

  EXPECT_EQ(338u, mesh.directDelay(&packet, path, 63, 200));
  EXPECT_EQ(63, packet.getPathHashCount());
}

TEST_F(AckTimingTest, ExpandsJitterWithLocalQueuePressure) {
  radio.airtime = 200;
  manager.addQueuePressure(4);
  rng.value = 1200;

  EXPECT_EQ(1400u, mesh.firstDelay(&packet, 200));
}

TEST_F(AckTimingTest, CapsJitterForBoundedAckLatency) {
  radio.airtime = 1000;
  manager.addQueuePressure(4);
  rng.value = 2000;

  EXPECT_EQ(2200u, mesh.firstDelay(&packet, 200));
}

TEST_F(AckTimingTest, SeparatesRedundantCopyByAirtimeAndGuard) {
  radio.airtime = 200;
  rng.value = 0;

  EXPECT_EQ(550u, mesh.nextDelay(&packet, 300));
}

TEST_F(AckTimingTest, CommonDirectRelayPathAddsAckContentionJitter) {
  radio.airtime = 10;
  rng.value = 100;

  mesh.receive(makeDirectAck());

  ASSERT_EQ(1u, manager.outbound.size());
  EXPECT_EQ(clock.now + 100, manager.outbound[0].scheduled_for);
}

TEST_F(AckTimingTest, MultipartAndFinalFormsUseOneContendedRelayTransmission) {
  radio.airtime = 10;
  rng.value = 100;

  mesh.receive(makeDirectAck(true));
  mesh.receive(makeDirectAck());

  ASSERT_EQ(1u, manager.outbound.size());
  EXPECT_EQ(clock.now + 100, manager.outbound[0].scheduled_for);
}

}  // namespace

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

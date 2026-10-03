#include <array>
#include <cstdint>
#include <gtest/gtest.h>
#include <random>
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *, std::size_t);
namespace {
using Packet = std::array<std::uint8_t, 264>;
void replay(Packet &p, std::size_t n) {
  ASSERT_LE(n, 256u);
  EXPECT_EQ(LLVMFuzzerTestOneInput(p.data(), n + 8), 0);
}
TEST(PositionDistance8mPreparedFuzzReplay, EveryByteValue) {
  Packet p{};
  p[0] = 15;
  p[1] = 8;
  p[3] = 0;
  p[4] = 0;
  for (unsigned b = 0; b < 256; ++b) {
    p[8] = static_cast<std::uint8_t>(b);
    replay(p, 1);
  }
  RecordProperty("packets", 256);
}
TEST(PositionDistance8mPreparedFuzzReplay, SizesSchedulesAndLiteralOnly) {
  Packet p{};
  for (unsigned i = 8; i < p.size(); ++i)
    p[i] = static_cast<std::uint8_t>(i * 71);
  for (auto frame : {0u, 15u, 63u})
    for (auto n : {0u, 1u, 4u, 5u, 63u, 64u, 65u, 127u, 128u, 255u, 256u})
      for (auto flags : {0u, 4u, 8u, 16u, 28u}) {
        p[0] = frame;
        p[1] = flags;
        p[3] = n % 32;
        p[4] = n % 33;
        replay(p, n);
      }
  RecordProperty("packets", 165);
}
TEST(PositionDistance8mPreparedFuzzReplay, EveryAllocationStage) {
  Packet p{};
  p[0] = 15;
  p[1] = 12;
  p[3] = 128;
  p[4] = 2;
  for (unsigned i = 8; i < p.size(); ++i)
    p[i] = static_cast<std::uint8_t>(i % 7);
  for (unsigned stage = 1; stage <= 17; ++stage) {
    p[2] = stage - 1;
    replay(p, 256);
  }
  RecordProperty("packets", 17);
}
TEST(PositionDistance8mPreparedFuzzReplay, AlteredSizeFlagsProfileAndPayload) {
  Packet p{};
  p[0] = 15;
  p[3] = 1;
  p[4] = 3;
  p[5] = 1;
  p[6] = 3;
  p[7] = 255;
  for (unsigned i = 8; i < p.size(); ++i)
    p[i] = static_cast<std::uint8_t>(i);
  for (auto flags : {1u, 2u, 3u, 5u, 6u, 32u, 64u, 128u, 144u, 255u})
    for (auto n : {0u, 1u, 17u, 64u, 256u}) {
      p[1] = flags;
      replay(p, n);
    }
  RecordProperty("packets", 50);
}
TEST(PositionDistance8mPreparedFuzzReplay, DeterministicRandomPackets) {
  std::mt19937 rng(1445);
  Packet p{};
  for (unsigned i = 0; i < 128; ++i) {
    for (auto &b : p)
      b = static_cast<std::uint8_t>(rng());
    replay(p, rng() % 257);
  }
  RecordProperty("packets", 128);
}
TEST(PositionDistance8mPreparedFuzzReplay, MutatedWireAndMidDrainErrors) {
  Packet p{};
  p[0] = 15;
  p[1] = 8;
  for (unsigned i = 8; i < p.size(); ++i)
    p[i] = static_cast<std::uint8_t>(i % 7);
  for (unsigned mode = 0; mode < 4; ++mode)
    for (unsigned selector = 0; selector < 32; ++selector) {
      p[2] = selector % 8;
      p[5] = selector;
      p[6] = 128;
      p[7] = mode;
      replay(p, 256);
    }
  RecordProperty("packets", 128);
}
} // namespace

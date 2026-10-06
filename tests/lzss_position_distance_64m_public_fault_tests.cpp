#include "frame/lzss_position_distance_64m_owning_adapter.hpp"
#include "marc/marc.h"
#include <algorithm>
#include <array>
#include <gtest/gtest.h>
#include <vector>
// Standalone link seam: replace only the concrete delegate boundary. Every
// successful return still uses the real exact allocator and actual deletion.
namespace fault64m {
thread_local std::size_t calls{}, fail{}, live{};
}
namespace marc::frame::internal {
LzssPositionDistance64mOwnedBytes
owning64m_bytes(LzssPositionDistance64mExactStreamAllocator &a,
                std::size_t n) noexcept {
  if (++fault64m::calls == fault64m::fail)
    return {};
  auto b = a.bytes(n);
  fault64m::live += b.capacity;
  return b;
}
LzssPositionDistance64mOwnedIndex
owning64m_indices(LzssPositionDistance64mExactStreamAllocator &a,
                  std::size_t n) noexcept {
  if (++fault64m::calls == fault64m::fail)
    return {};
  auto b = a.indices(n);
  fault64m::live += 4 * b.capacity;
  return b;
}
void owning64m_release_bytes(LzssPositionDistance64mExactStreamAllocator &a,
                             LzssPositionDistance64mOwnedBytes &b) noexcept {
  auto n = b.capacity;
  a.release(b);
  fault64m::live -= n;
}
void owning64m_release_indices(LzssPositionDistance64mExactStreamAllocator &a,
                               LzssPositionDistance64mOwnedIndex &b) noexcept {
  auto n = 4 * b.capacity;
  a.release(b);
  fault64m::live -= n;
}
} // namespace marc::frame::internal
namespace {
auto config(std::size_t n) {
  marc_lzss_position_distance_dynamic_range_64m_config c{};
  EXPECT_EQ(marc_lzss_position_distance_dynamic_range_64m_config_init(&c),
            MARC_STATUS_OK);
  c.original_size = n;
  c.frame_size = 32;
  c.max_frame_size = c.max_block_size = 32;
  c.max_total_output_size = 1048576;
  c.max_compressed_payload_size = 65536;
  c.max_internal_buffered_bytes = 16 * 1048576;
  c.max_entropy_table_entries = 2632;
  c.max_expansion_ratio = 1048576;
  c.expansion_slack = 1048576;
  c.input_capacity_bytes = 33;
  c.output_capacity_bytes = 4096;
  c.external_retained_bytes = 65536;
  return c;
}
TEST(PublicFault32m, BothInitialOwnerAllocationsFailBeforeHandlePublication) {
  for (std::size_t fail : {1u, 2u}) {
    fault64m::calls = 0;
    fault64m::fail = fail;
    ASSERT_EQ(fault64m::live, 0u);
    auto c = config(33);
    marc_transform *h{};
    EXPECT_EQ(
        marc_lzss_position_distance_dynamic_range_64m_create_encoder(&c, &h),
        MARC_STATUS_OUT_OF_MEMORY);
    EXPECT_EQ(h, nullptr);
    EXPECT_EQ(fault64m::live, 0u);
    EXPECT_EQ(fault64m::calls, fail);
  }
  fault64m::fail = 0;
}
TEST(PublicFault32m,
     AllSecondCandidateFailuresPublishOnlyPreviouslyFinishedFrame) {
  fault64m::calls = fault64m::fail = 0;
  std::array<std::uint8_t, 33> raw{};
  raw.fill(65);
  std::array<std::uint8_t, 4096> good{};
  auto c = config(33);
  marc_transform *h{};
  ASSERT_EQ(
      marc_lzss_position_distance_dynamic_range_64m_create_encoder(&c, &h),
      MARC_STATUS_OK);
  auto reference = marc_transform_process(h, {raw.data(), raw.size()},
                                          {good.data(), good.size()},
                                          MARC_PROCESS_END_INPUT);
  ASSERT_EQ(reference.status, MARC_STATUS_END_OF_STREAM);
  marc_transform_destroy(h);
  ASSERT_EQ(fault64m::live, 0u);
  std::uint32_t payload{};
  for (unsigned i = 0; i < 4; ++i)
    payload |= std::uint32_t(good[144 + i]) << (8 * i);
  auto prior_size = 192 + payload;
  for (std::size_t fail : {7u, 8u, 9u, 10u}) {
    fault64m::calls = 0;
    fault64m::fail = fail;
    h = nullptr;
    std::array<std::uint8_t, 4096> output{};
    output.fill(0xa5);
    ASSERT_EQ(
        marc_lzss_position_distance_dynamic_range_64m_create_encoder(&c, &h),
        MARC_STATUS_OK);
    auto r = marc_transform_process(h, {raw.data(), raw.size()},
                                    {output.data(), output.size()},
                                    MARC_PROCESS_END_INPUT);
    EXPECT_EQ(r.status, MARC_STATUS_OUT_OF_MEMORY);
    EXPECT_EQ(r.input_consumed, raw.size());
    EXPECT_EQ(r.output_produced, prior_size);
    EXPECT_TRUE(
        std::equal(output.begin(), output.begin() + prior_size, good.begin()));
    EXPECT_TRUE(std::all_of(output.begin() + prior_size, output.end(),
                            [](auto b) { return b == 0xa5; }));
    auto sticky = marc_transform_process(h, {nullptr, 0},
                                         {output.data(), output.size()}, 0);
    EXPECT_EQ(sticky.status, r.status);
    EXPECT_EQ(sticky.output_produced, 0u);
    marc_transform_destroy(h);
    EXPECT_EQ(fault64m::live, 0u);
    EXPECT_EQ(fault64m::calls, fail);
  }
  fault64m::fail = 0;
}
} // namespace

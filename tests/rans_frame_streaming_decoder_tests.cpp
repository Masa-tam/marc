#include "frame/rans_frame_streaming_decoder.hpp"
#include "frame/rans_stream.hpp"
#include <algorithm>
#include <array>
#include <gtest/gtest.h>
#include <vector>
namespace {
constexpr std::array raw{std::byte{0x41}, std::byte{0x42}, std::byte{0x41},
                         std::byte{0x41}, std::byte{0x41}, std::byte{0x42},
                         std::byte{0x41}};
marc::frame::StreamHeader config() {
  marc::frame::StreamHeader s{};
  s.entropy_algorithm = marc::frame::EntropyAlgorithm::rans;
  s.entropy_variant = 1;
  s.frame_size = 4;
  s.entropy_block_size = 2;
  s.original_size = raw.size();
  return s;
}
std::vector<std::byte> encoded() {
  auto s = config();
  auto p = marc::frame::plan_rans_stream(s, {}, raw);
  std::vector<std::byte> v(p.serialized_size);
  EXPECT_EQ(marc::frame::encode_rans_stream(s, {}, raw, v).error,
            marc::frame::RansStreamCodecError::none);
  return v;
}
TEST(RansFrameStreamingDecoder, OneByteInputAndOutput) {
  auto in = encoded();
  std::array<std::byte, 1200> frame{};
  std::array<std::byte, 4> decoded{};
  std::array<marc::entropy::internal::RansBlockView, 2> views{};
  marc::frame::RansFrameStreamingDecoder d{{}, frame, decoded, views};
  std::vector<std::byte> out;
  std::size_t pos{};
  std::array<std::byte, 1> byte{};
  marc::core::StreamStatus status{};
  do {
    auto n = std::min<std::size_t>(1, in.size() - pos);
    auto chunk = std::span<const std::byte>{in}.subspan(pos, n);
    auto flags =
        pos + n == in.size()
            ? marc::core::flag_value(marc::core::ProcessFlags::end_input)
            : 0U;
    auto r = d.process(chunk, byte, flags);
    ASSERT_TRUE(marc::core::is_valid(r, chunk.size(), byte.size()));
    ASSERT_NE(r.status, marc::core::StreamStatus::error);
    pos += r.input_consumed;
    if (r.output_produced)
      out.push_back(byte[0]);
    status = r.status;
  } while (status != marc::core::StreamStatus::end_of_stream);
  EXPECT_EQ(pos, in.size());
  EXPECT_TRUE(std::ranges::equal(out, raw));
}
TEST(RansFrameStreamingDecoder,
     CommitsFirstFrameBeforeLaterCorruption) {
  auto in = encoded();
  auto stream = config();
  marc::frame::FrameHeader first{};
  const std::span<const std::byte, marc::frame::frame_header_size> first_header{
      in.data() + marc::frame::stream_header_size,
      marc::frame::frame_header_size};
  const marc::core::DecoderLimits limits{};
  ASSERT_EQ(marc::frame::parse_frame_header(
                first_header, {stream, limits, 0, 0}, first),
            marc::frame::FrameHeaderError::none);
  const auto second_start = marc::frame::stream_header_size
      + marc::frame::frame_header_size + first.block_descriptors_size
      + first.compressed_payload_size;
  const auto second_payload = second_start + marc::frame::frame_header_size
      + 2 * marc::entropy::internal::rans_descriptor_size;
  in[second_payload] = std::byte{0xff};
  std::array<std::byte, 1200> frame{};
  std::array<std::byte, 4> decoded{};
  std::array<marc::entropy::internal::RansBlockView, 2> views{};
  marc::frame::RansFrameStreamingDecoder d{{}, frame, decoded, views};
  std::array<std::byte, raw.size()> out{};
  out.fill(std::byte{0x5a});
  auto r = d.process(
      in, out, marc::core::flag_value(marc::core::ProcessFlags::end_input));
  EXPECT_EQ(r.status, marc::core::StreamStatus::error);
  EXPECT_EQ(r.output_produced, 4U);
  EXPECT_TRUE(std::ranges::equal(std::span<const std::byte>{out}.first(4),
                                 std::span<const std::byte>{raw}.first(4)));
  EXPECT_TRUE(
      std::ranges::all_of(std::span<const std::byte>{out}.subspan(4),
                          [](std::byte b) { return b == std::byte{0x5a}; }));
}
TEST(RansFrameStreamingDecoder, ReportsWorkspaceAndTruncation) {
  auto in = encoded();
  std::array<std::byte, 1> short_frame{};
  std::array<std::byte, 4> decoded{};
  std::array<marc::entropy::internal::RansBlockView, 2> views{};
  marc::frame::RansFrameStreamingDecoder d{{}, short_frame, decoded, views};
  auto r = d.process(
      in, {}, marc::core::flag_value(marc::core::ProcessFlags::end_input));
  EXPECT_EQ(r.error.code, marc::core::ErrorCode::out_of_memory);
  std::array<std::byte, 1200> frame{};
  marc::frame::RansFrameStreamingDecoder t{{}, frame, decoded, views};
  std::array<std::byte, raw.size()> output{};
  r = t.process(std::span<const std::byte>{in}.first(in.size() - 1), output,
                marc::core::flag_value(marc::core::ProcessFlags::end_input));
  EXPECT_EQ(r.status, marc::core::StreamStatus::error);
  EXPECT_EQ(r.error.code, marc::core::ErrorCode::malformed_stream);

  std::array<marc::entropy::internal::RansBlockView, 1> short_views{};
  marc::frame::RansFrameStreamingDecoder v{{}, frame, decoded, short_views};
  r = v.process(in, output,
                marc::core::flag_value(marc::core::ProcessFlags::end_input));
  EXPECT_EQ(r.status, marc::core::StreamStatus::error);
  EXPECT_EQ(r.error.code, marc::core::ErrorCode::out_of_memory);
}
} // namespace

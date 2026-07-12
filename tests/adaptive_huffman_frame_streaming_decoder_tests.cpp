#include "frame/adaptive_huffman_frame_streaming_decoder.hpp"
#include "frame/adaptive_huffman_stream.hpp"
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
  s.entropy_algorithm = marc::frame::EntropyAlgorithm::adaptive_huffman;
  s.entropy_variant = 1;
  s.frame_size = 4;
  s.original_size = raw.size();
  return s;
}
std::vector<std::byte> encoded() {
  auto s = config();
  auto p = marc::frame::plan_adaptive_huffman_stream(s, {}, raw);
  std::vector<std::byte> v(p.serialized_size);
  EXPECT_EQ(marc::frame::encode_adaptive_huffman_stream(s, {}, raw, v).error,
            marc::frame::AdaptiveHuffmanStreamCodecError::none);
  return v;
}
TEST(AdaptiveHuffmanFrameStreamingDecoder, OneByteInputAndOutput) {
  auto in = encoded();
  std::array<std::byte, 256> frame{};
  std::array<std::byte, 4> decoded{};
  marc::frame::AdaptiveHuffmanFrameStreamingDecoder d{{}, frame, decoded};
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
TEST(AdaptiveHuffmanFrameStreamingDecoder,
     CommitsFirstFrameBeforeLaterCorruption) {
  auto in = encoded();
  in.back() |= std::byte{0x80};
  std::array<std::byte, 256> frame{};
  std::array<std::byte, 4> decoded{};
  marc::frame::AdaptiveHuffmanFrameStreamingDecoder d{{}, frame, decoded};
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
TEST(AdaptiveHuffmanFrameStreamingDecoder, ReportsWorkspaceAndTruncation) {
  auto in = encoded();
  std::array<std::byte, 1> short_frame{};
  std::array<std::byte, 4> decoded{};
  marc::frame::AdaptiveHuffmanFrameStreamingDecoder d{{}, short_frame, decoded};
  auto r = d.process(
      in, {}, marc::core::flag_value(marc::core::ProcessFlags::end_input));
  EXPECT_EQ(r.error.code, marc::core::ErrorCode::out_of_memory);
  std::array<std::byte, 256> frame{};
  marc::frame::AdaptiveHuffmanFrameStreamingDecoder t{{}, frame, decoded};
  r = t.process(std::span<const std::byte>{in}.first(in.size() - 1), {},
                marc::core::flag_value(marc::core::ProcessFlags::end_input));
  EXPECT_TRUE(r.status == marc::core::StreamStatus::need_output ||
              r.status == marc::core::StreamStatus::error);
}
} // namespace

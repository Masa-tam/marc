#include "frame/lzss_position_distance_64m_compact_frame_decoder.hpp"
#include "frame/lzss_position_distance_64m_compact_frame_encoder.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace marc;
using namespace frame::internal;
constexpr std::size_t F = 67108864;
using Bytes = std::vector<std::byte>;
void check(bool value) {
  if (!value)
    throw std::runtime_error("conformance failure");
}
Bytes read(std::ifstream &f, std::size_t n) {
  check(n <= 1u << 20);
  Bytes b(n);
  f.read(reinterpret_cast<char *>(b.data()), n);
  check(bool(f));
  return b;
}
std::uint32_t number(std::ifstream &f) {
  const auto b = read(f, 4);
  std::uint32_t n = 0;
  for (unsigned i = 0; i < 4; ++i)
    n |= std::to_integer<std::uint32_t>(b[i]) << (8 * i);
  return n;
}
int main(int argc, char **argv) try {
  check(argc == 2);
  std::ifstream input(argv[1], std::ios::binary);
  const auto magic = read(input, 8);
  check(std::memcmp(magic.data(), "M64E0001", 8) == 0);
  check(number(input) == 3);
  for (unsigned k = 0; k < 3; ++k) {
    const auto length = number(input), extent = number(input);
    check(length == (k == 0 ? 0u : k == 1 ? 5u : 258u));
    const auto header = read(input, 112), expected = read(input, extent);
    core::DecoderLimits limits{};
    limits.max_frame_size = limits.max_block_size = limits.max_lz_distance = F;
    limits.max_internal_buffered_bytes = std::uint64_t{1024} << 20;
    TypedContextStreamHeader stream{};
    std::size_t consumed = 0;
    check(parse_lzss_position_distance_64m_stream_header(header, limits, stream,
                                                         consumed) ==
          LzssPositionDistance64mPreflightError::none);
    check(consumed == 112);
    Bytes raw(F, std::byte(length ? 0u : 65u));
    if (length)
      for (std::size_t i = 0; i < length; ++i)
        raw[i] = raw[F - length + i] = std::byte(1 + i % 255);
    Bytes output(2 * F + 80, std::byte{0xa5});
    TypedContextFrameLayout layout{};
    std::size_t written = 777;
    std::size_t encode_bytes = 0;
    {
      Bytes records(2 * F), staging(2 * F + 80), payload(2 * F);
      std::vector<std::uint32_t> index(1048576 + F);
      LzssPositionDistance64mCompactFrameWorkspace workspace{records, index,
                                                             staging, payload};
      const auto retained = header.capacity() + expected.capacity();
      TypedContextFrameValidationContext context{stream, limits, 0, 0};
      limits.max_internal_buffered_bytes = std::uint64_t{512} << 20;
      auto refused = encode_lzss_position_distance_64m_compact_frame(
          raw, context, workspace, output, layout, written, retained);
      check(refused.error ==
            LzssPositionDistance64mCompactFrameError::limit_exceeded);
      check(refused.bytes_committed == 0 && written == 777);
      check(std::all_of(output.begin(), output.end(),
                        [](auto b) { return b == std::byte{0xa5}; }));
      limits.max_internal_buffered_bytes = std::uint64_t{1024} << 20;
      auto plan = query_lzss_position_distance_64m_compact_frame_encode(
          raw, context, workspace, output.size(), retained);
      check(plan.error == LzssPositionDistance64mCompactFrameError::none);
      check(plan.bytes_required == expected.size());
      encode_bytes = plan.aggregate_bytes;
      limits.max_internal_buffered_bytes = encode_bytes;
      auto encoded = encode_lzss_position_distance_64m_compact_frame(
          raw, context, workspace, output, layout, written, retained);
      check(encoded.error == LzssPositionDistance64mCompactFrameError::none);
      check(written == expected.size() && encoded.bytes_committed == written);
      check(std::equal(expected.begin(), expected.end(), output.begin()));
      check(std::all_of(output.begin() + written, output.end(),
                        [](auto b) { return b == std::byte{0xa5}; }));
      const auto saved = output;
      std::array<std::byte, sizeof(layout)> saved_layout{};
      std::memcpy(saved_layout.data(), &layout, sizeof(layout));
      const auto previous_written = written;
      limits.max_internal_buffered_bytes = std::uint64_t{1024} << 20;
      plan = query_lzss_position_distance_64m_compact_frame_encode(
          raw, context, workspace, output.size(),
          retained + saved.capacity() + saved_layout.size());
      check(plan.error == LzssPositionDistance64mCompactFrameError::none);
      limits.max_internal_buffered_bytes = plan.aggregate_bytes - 1;
      encoded = encode_lzss_position_distance_64m_compact_frame(
          raw, context, workspace, output, layout, written,
          retained + saved.capacity() + saved_layout.size());
      check(encoded.error ==
            LzssPositionDistance64mCompactFrameError::limit_exceeded);
      check(encoded.bytes_committed == 0 && output == saved &&
            written == previous_written);
      check(std::memcmp(saved_layout.data(), &layout, sizeof(layout)) == 0);
    }
    limits.max_internal_buffered_bytes = std::uint64_t{1024} << 20;
    const TypedContextFrameValidationContext context{stream, limits, 0, 0};
    Bytes tokens(3 * F), scratch(3 * F), decoded(F), raw_scratch(F);
    const auto view = std::span(output).first(written);
    const auto retained = output.capacity() - written + raw.capacity() +
                          header.capacity() + expected.capacity();
    auto plan = query_lzss_position_distance_64m_compact_frame_decode(
        view, context, tokens.size(), scratch.size(), decoded.size(),
        raw_scratch.size(), retained);
    check(plan.error == LzssPositionDistance64mCompactFrameDecodeError::none);
    // The complete frame query is the admission authority; every retained owner
    // is declared.
    auto result = decode_lzss_position_distance_64m_compact_frame(
        view, context, tokens, scratch, decoded, raw_scratch, layout, retained);
    check(result.error == LzssPositionDistance64mCompactFrameDecodeError::none);
    check(result.raw_produced == F && decoded == raw);
    std::cout << "recipe " << length << " frame " << written << " encode_bytes "
              << encode_bytes << " PASS\n";
  }
  check(input.peek() == std::char_traits<char>::eof());
  return 0;
} catch (const std::exception &e) {
  std::cerr << e.what() << '\n';
  return 1;
}

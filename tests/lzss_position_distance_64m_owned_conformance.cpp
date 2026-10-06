#include "core/endian.hpp"
#include "frame/lzss_position_distance_64m_owned_stream_encoder.hpp"
#include "frame/lzss_position_distance_64m_stream_decoder.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>
using namespace marc;
using namespace frame::internal;
using Bytes = std::vector<std::byte>;
constexpr std::size_t F = 67108864;
constexpr auto End = core::flag_value(core::ProcessFlags::end_input);
void check(bool b, std::source_location at = std::source_location::current()) {
  if (!b)
    throw std::runtime_error("conformance at line " +
                             std::to_string(at.line()));
}
Bytes read(std::ifstream &file, std::size_t n) {
  check(n <= 1u << 20);
  Bytes b(n);
  file.read(reinterpret_cast<char *>(b.data()), n);
  check(bool(file));
  return b;
}
std::uint32_t number(std::ifstream &file) {
  auto b = read(file, 4);
  std::uint32_t n{};
  check(core::load_le(std::span<const std::byte>(b), 0, n));
  return n;
}
core::DecoderLimits limits() {
  core::DecoderLimits l{};
  l.max_frame_size = l.max_block_size = l.max_lz_distance = F;
  l.max_internal_buffered_bytes = std::uint64_t{1024} << 20;
  return l;
}
void decode(std::span<const std::byte> wire, const Bytes &raw, bool bad) {
  Bytes serial(2 * F + 80), tokens(3 * F), token_scratch(3 * F), private_raw(F),
      scratch(F), output(65536);
  auto l = limits();
  const auto retained = raw.capacity() + wire.size() + output.capacity();
  const auto q = query_lzss_position_distance_64m_stream_workspace(
      l, serial.size(), tokens.size(), token_scratch.size(), private_raw.size(),
      scratch.size(), retained);
  auto old_limits = l;
  old_limits.max_internal_buffered_bytes = 512u << 20;
  const auto refused = query_lzss_position_distance_64m_stream_workspace(
      old_limits, serial.size(), tokens.size(), token_scratch.size(),
      private_raw.size(), scratch.size(), retained);
  check(refused.error == core::ErrorCode::limit_exceeded);
  check(q.error == core::ErrorCode::none);
  l.max_internal_buffered_bytes = q.aggregate_bytes;
  LzssPositionDistance64mStreamDecoder decoder(l, serial, tokens, token_scratch,
                                               private_raw, scratch, retained);
  std::size_t pos = 0, produced = 0;
  bool terminal = false;
  for (unsigned call = 0; call < 1000000; ++call) {
    auto n = std::min(std::size_t{4093}, wire.size() - pos);
    auto r = decoder.process(wire.subspan(pos, n), output,
                             pos + n == wire.size() ? End : 0);
    check(core::is_valid(r, n, output.size()));
    pos += r.input_consumed;
    check(r.output_produced <= raw.size() - produced);
    check(std::equal(output.begin(), output.begin() + r.output_produced,
                     raw.begin() + produced));
    produced += r.output_produced;
    if (r.status == core::StreamStatus::error ||
        r.status == core::StreamStatus::end_of_stream) {
      check(bad ? r.status == core::StreamStatus::error && produced == F
                : r.status == core::StreamStatus::end_of_stream &&
                      produced == raw.size() && pos == wire.size());
      const auto again = decoder.process({}, output, 0);
      check(again.status == r.status && again.output_produced == 0);
      terminal = true;
      break;
    }
  }
  check(terminal);
}
int main(int argc, char **argv) try {
  check(argc == 2);
  std::ifstream file(argv[1], std::ios::binary);
  auto magic = read(file, 8);
  check(std::memcmp(magic.data(), "M64O0001", 8) == 0);
  check(number(file) == 7);
  for (unsigned k = 0; k < 7; ++k) {
    const auto n = number(file), length = number(file), extent = number(file);
    check(n >= F - 1 && n <= 2 * F + 1 &&
          (length == 0 || length == 5 || length == 258));
    auto expected = read(file, extent);
    Bytes raw(n, std::byte(length ? 0 : 65));
    if (length)
      for (std::size_t i = 0; i < length; ++i)
        raw[i] = raw[n - length + i] = std::byte(1 + i % 255);
    auto l = limits();
    TypedContextStreamHeader stream{};
    std::size_t used = 0;
    check(parse_lzss_position_distance_64m_stream_header(
              std::span(expected).first(112), l, stream, used) ==
          LzssPositionDistance64mPreflightError::none);
    Bytes encoded;
    encoded.reserve(extent);
    // Force mid-header and mid-frame starvation across a full-window boundary.
    Bytes output(n == F + 1 ? 1 : 65536);
    {
      LzssPositionDistance64mExactStreamAllocator allocator;
      const auto retained = raw.capacity() + expected.capacity() +
                            encoded.capacity() + output.capacity();
      LzssPositionDistance64mOwnedStreamEncoder encoder(stream, l, allocator,
                                                        retained);
      std::size_t pos = 0;
      bool terminal = false;
      for (unsigned call = 0; call < 1000000; ++call) {
        const auto chunk = std::min(std::size_t{65521}, raw.size() - pos);
        auto r = encoder.process(std::span(raw).subspan(pos, chunk), output,
                                 pos + chunk == raw.size() ? End : 0);
        check(core::is_valid(r, chunk, output.size()) &&
              r.status != core::StreamStatus::error);
        pos += r.input_consumed;
        check(encoded.size() + r.output_produced <= encoded.capacity());
        encoded.insert(encoded.end(), output.begin(),
                       output.begin() + r.output_produced);
        if (r.status == core::StreamStatus::end_of_stream) {
          check(pos == raw.size() && encoder.process({}, output, 0).status ==
                                         core::StreamStatus::end_of_stream);
          terminal = true;
          break;
        }
      }
      check(terminal && encoded == expected);
    }
    // Release the comparison owner before the full-capacity borrowed decode
    // phase.
    Bytes{}.swap(expected);
    Bytes{}.swap(output);
    decode(encoded, raw, false);
    if (n == 2 * F && !length) {
      encoded.back() ^= std::byte{1};
      decode(encoded, raw, true);
    }
    std::cout << "owned " << n << " far " << length << " PASS\n";
  }
  check(file.peek() == std::char_traits<char>::eof());
  return 0;
} catch (const std::exception &e) {
  std::cerr << e.what() << '\n';
  return 1;
}

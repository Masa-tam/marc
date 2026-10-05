#include "frame/lzss_position_distance_32m_stream_decoder.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <vector>
namespace {
using namespace marc;
using namespace frame::internal;
using Token = std::byte;
constexpr auto guard = std::byte{0xa5};
constexpr auto end = core::flag_value(core::ProcessFlags::end_input);
void require(bool b) {
  if (!b)
    std::abort();
}
core::DecoderLimits limits() {
  core::DecoderLimits l{};
  l.max_total_output_size = 65536;
  l.max_frame_size = 512;
  l.max_block_size = 512;
  l.max_compressed_payload_size = 4096;
  l.max_internal_buffered_bytes = 65536;
  l.max_lz_distance = 33554432;
  return l;
}
struct Workspace {
  std::array<std::byte, 4096> serial{};
  std::array<Token, 1536> tokens{}, scratch_tokens{};
  std::array<std::byte, 512> raw{}, scratch_raw{};
};
struct Observed {
  std::vector<std::byte> raw;
  std::size_t consumed{};
  core::StreamStatus status{};
  core::StreamError error{};
};
Observed scheduled(std::span<const std::byte> input, std::uint8_t selector,
                   bool split) {
  Workspace w;
  LzssPositionDistance32mStreamDecoder d(
      limits(), w.serial, w.tokens, w.scratch_tokens, w.raw, w.scratch_raw);
  Observed result;
  std::size_t steps = 0;
  const auto call = [&](std::span<const std::byte> in, std::size_t cap,
                        std::uint32_t flags) {
    std::array<std::byte, 35> out;
    out.fill(guard);
    const auto r = d.process(in, std::span(out).first(cap), flags);
    require(r.input_consumed <= in.size() && r.output_produced <= cap);
    require(r.status != core::StreamStatus::progress || r.input_consumed ||
            r.output_produced);
    require(std::ranges::all_of(std::span(out).subspan(r.output_produced),
                                [](auto b) { return b == guard; }));
    result.raw.insert(result.raw.end(), out.begin(),
                      out.begin() + r.output_produced);
    result.consumed += r.input_consumed;
    result.status = r.status;
    result.error = r.error;
    require(++steps < 100000 && result.raw.size() <= 65536);
    return r;
  };
  std::size_t offset = 0, stop = 0;
  while (true) {
    if (offset == stop && stop < input.size())
      stop = std::min(input.size(),
                      stop + (split ? 1 + selector % 31 : input.size()));
    const bool final = stop == input.size();
    // Deliberate output starvation is bounded; next call supplies capacity.
    auto cap = split && steps % 5 == 0 ? 0 : split ? 1 + selector % 32 : 32;
    auto r = call(input.subspan(offset, stop - offset), cap, final ? end : 0);
    offset += r.input_consumed;
    if (r.status == core::StreamStatus::error ||
        r.status == core::StreamStatus::end_of_stream)
      break;
  }
  const auto previous = result;
  auto terminal = call(input, 32, 999);
  require(!terminal.input_consumed && !terminal.output_produced &&
          terminal.status == previous.status);
  require(terminal.error.code == previous.error.code &&
          terminal.error.byte_position == previous.error.byte_position);
  return result;
}
// Separate finite-frame traversal is an oracle for the coordinator's committed
// frontier. It deliberately has no streaming status/drain state. The existing
// finite codec is shared: this qualifies scheduling, not independent Range
// math.
std::vector<std::byte> committed_oracle(std::span<const std::byte> input) {
  auto l = limits();
  TypedContextStreamHeader s{};
  std::size_t used{};
  if (parse_lzss_position_distance_32m_stream_header(input, l, s, used) !=
      LzssPositionDistance32mPreflightError::none)
    return {};
  std::vector<std::byte> out;
  std::uint64_t sequence = 0, raw = 0;
  Workspace w;
  while (raw < s.original_size) {
    TypedContextFrameLayout layout{};
    LzssPositionDistance32mFrameRequirements needed{};
    const TypedContextFrameValidationContext c{s, l, sequence, raw};
    auto rest = input.subspan(used);
    if (preflight_lzss_position_distance_32m_compact_frame_prefix(
            rest, c, layout, needed) !=
            LzssPositionDistance32mPreflightError::none ||
        needed.serialized_frame_bytes > rest.size() ||
        needed.serialized_frame_bytes > w.serial.size() ||
        needed.token_count > w.tokens.size() ||
        needed.raw_frame_bytes > w.raw.size())
      break;
    const auto r = decode_lzss_position_distance_32m_compact_frame(
        rest.first(needed.serialized_frame_bytes), c, w.tokens,
        w.scratch_tokens, w.raw, w.scratch_raw, layout);
    if (r.error != LzssPositionDistance32mCompactFrameDecodeError::none)
      break;
    out.insert(out.end(), w.raw.begin(), w.raw.begin() + r.raw_produced);
    raw += r.raw_produced;
    ++sequence;
    used += r.bytes_consumed;
  }
  return out;
}
} // namespace
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data,
                                      std::size_t size) {
  if (!size || size > 4097)
    return 0;
  const auto input = std::as_bytes(std::span(data + 1, size - 1));
  const auto a = scheduled(input, data[0], false),
             b = scheduled(input, data[0], true);
  require(a.raw == b.raw && a.consumed == b.consumed && a.status == b.status);
  require(a.error.code == b.error.code &&
          a.error.byte_position == b.error.byte_position);
  require(a.raw == committed_oracle(input));
  return 0;
}

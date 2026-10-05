#include "frame/lzss_position_distance_16m_frame_decoder.hpp"
#include "frame/lzss_position_distance_16m_owned_stream_encoder.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <vector>
namespace {
using namespace marc;
using namespace frame::internal;
void check(bool b) {
  if (!b)
    std::abort();
}
struct Allocator final : LzssPositionDistance16mStreamAllocator {
  LzssPositionDistance16mExactStreamAllocator exact;
  std::size_t calls{}, fail{}, live{};
  LzssPositionDistance16mAllocatorControls controls() const noexcept override {
    return {this, sizeof(*this), 128};
  }
  LzssPositionDistance16mOwnedBytes bytes(std::size_t n) noexcept override {
    if (++calls == fail)
      return {};
    auto b = exact.bytes(n);
    live += b.capacity;
    return b;
  }
  LzssPositionDistance16mOwnedIndex indices(std::size_t n) noexcept override {
    if (++calls == fail)
      return {};
    auto b = exact.indices(n);
    live += 4 * b.capacity;
    return b;
  }
  void release(LzssPositionDistance16mOwnedBytes &b) noexcept override {
    live -= b.capacity;
    exact.release(b);
  }
  void release(LzssPositionDistance16mOwnedIndex &b) noexcept override {
    live -= 4 * b.capacity;
    exact.release(b);
  }
};
} // namespace
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data,
                                      std::size_t size) {
  if (size < 4 || size > 132)
    return 0;
  const auto raw = std::as_bytes(std::span(data + 4, size - 4));
  Allocator a;
  if (data[0] & 1)
    a.fail = 1 + (data[1] % 16);
  core::DecoderLimits l{};
  l.max_block_size = 16777216;
  if (data[0] & 2)
    l.max_internal_buffered_bytes = std::size_t(data[2]) * 4096;
  TypedContextStreamHeader s{};
  s.original_size = raw.size();
  s.frame_size = 1 + (data[1] % 32);
  s.dictionary = {16777216, 3, 258, 0};
  s.dictionary_variant = 12;
  s.context_algorithm = 1;
  s.context_variant = 13;
  s.context_count = 48;
  s.range_model_total = 32768;
  std::array<std::byte, 20000> encoded{};
  std::array<std::byte, 128> buffer{};
  const auto extra = sizeof(a) + sizeof(encoded) + sizeof(buffer) + sizeof(l) +
                     sizeof(s) + 4 * sizeof(core::ProcessResult) +
                     32 * sizeof(std::size_t) + raw.size();
  std::size_t position = 0, written = 0;
  bool ended = false;
  {
    LzssPositionDistance16mOwnedStreamEncoder encoder(s, l, a, extra);
    for (unsigned call = 0; call < 20000; ++call) {
      buffer.fill(std::byte{0xa5});
      const auto n =
          std::min<std::size_t>(1 + (data[2] % 64), raw.size() - position);
      const auto capacity = 1 + (data[3] % 128);
      const auto flags =
          (position + n == raw.size()
               ? core::flag_value(core::ProcessFlags::end_input)
               : 0) |
          ((data[0] & 4) ? core::flag_value(core::ProcessFlags::flush) : 0) |
          ((data[0] & 8) ? core::flag_value(core::ProcessFlags::reset_block)
                         : 0);
      const auto r = encoder.process(raw.subspan(position, n),
                                     std::span(buffer).first(capacity), flags);
      check(core::is_valid(r, n, capacity));
      check(written + r.output_produced <= encoded.size());
      for (std::size_t i = r.output_produced; i < buffer.size(); ++i)
        check(buffer[i] == std::byte{0xa5});
      std::copy_n(buffer.begin(), r.output_produced, encoded.begin() + written);
      written += r.output_produced;
      position += r.input_consumed;
      if (r.status == core::StreamStatus::error) {
        const auto next = encoder.process({}, buffer, 0);
        check(next.status == core::StreamStatus::error &&
              next.error.code == r.error.code && !next.input_consumed &&
              !next.output_produced);
        break;
      }
      if (r.status == core::StreamStatus::end_of_stream) {
        ended = true;
        check(position == raw.size() && encoder.process({}, buffer, 0).status ==
                                            core::StreamStatus::end_of_stream);
        break;
      }
      check(call < 19999);
    }
  }
  check(!a.live);
  if (written >= 112) {
    core::DecoderLimits dl{};
    dl.max_block_size = 16777216;
    TypedContextStreamHeader parsed{};
    std::size_t consumed{};
    check(parse_lzss_position_distance_16m_stream_header(encoded, dl, parsed,
                                                         consumed) ==
              LzssPositionDistance16mPreflightError::none &&
          consumed == 112);
    std::size_t offset = 112, produced = 0;
    std::uint64_t sequence = 0;
    while (offset < written) {
      check(written - offset >= 80);
      std::uint32_t tokens{}, payload{};
      check(core::load_le(std::span<const std::byte>(encoded), offset + 20,
                          tokens) &&
            core::load_le(std::span<const std::byte>(encoded), offset + 32,
                          payload));
      check(tokens <= 128 && payload <= written - offset - 80);
      std::array<dictionary::internal::LzssTypedToken, 128> t{}, ts{};
      std::array<std::byte, 128> decoded{}, scratch{};
      TypedContextFrameLayout layout{};
      const TypedContextFrameValidationContext c{parsed, dl, sequence,
                                                 produced};
      const auto r = decode_lzss_position_distance_16m_frame(
          std::span(encoded).subspan(offset, 80 + payload), c, t, ts, decoded,
          scratch, layout,
          extra + sizeof(t) + sizeof(ts) + sizeof(decoded) + sizeof(scratch) +
              sizeof(c) + sizeof(layout) + 4096);
      check(r.error == LzssPositionDistance16mFrameDecodeError::none &&
            r.raw_produced <= raw.size() - produced &&
            std::equal(decoded.begin(), decoded.begin() + r.raw_produced,
                       raw.begin() + produced));
      produced += r.raw_produced;
      offset += 80 + payload;
      ++sequence;
    }
    check(offset == written);
    if (ended)
      check(produced == raw.size());
  } else
    check(!ended);
  return 0;
}

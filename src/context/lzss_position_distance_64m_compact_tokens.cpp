#include "context/lzss_position_distance_64m_compact_tokens.hpp"
#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"
#include "core/endian.hpp"
#include "entropy/lzss_position_distance_64m_range_decoder.hpp"
#include <algorithm>
#include <array>
namespace marc::context::internal {
namespace {
using namespace dictionary::internal;
using Error = LzssContextualRangeDecodeError;
using TokenError = LzssTypedTokenError;
using EntropyError = entropy::internal::ContextualDynamicRangeDecodeError;
struct WorkingState {
  entropy::internal::LzssPositionDistance64mRangeDecoder decoder{};
  LzssTypedToken token{};
  ModeledOperation operation{};
  LzssContextualRangeDecodeResult result{};
};
constexpr std::size_t working_bytes =
    sizeof(WorkingState) +
    sizeof(LzssPositionDistance64mCompactTokenRequirements);
struct Region {
  const void *data;
  std::size_t bytes;
};
Error overlap(const std::array<Region, 2> &writes,
              const std::array<Region, 6> &reads) noexcept {
  for (const auto &w : writes)
    for (const auto &r : reads) {
      const auto o =
          core::check_buffer_overlap(w.data, w.bytes, r.data, r.bytes);
      if (o != core::BufferOverlap::disjoint)
        return o == core::BufferOverlap::arithmetic_overflow
                   ? Error::arithmetic_overflow
                   : Error::overlapping_buffers;
    }
  const auto o = core::check_buffer_overlap(writes[0].data, writes[0].bytes,
                                            writes[1].data, writes[1].bytes);
  return o == core::BufferOverlap::disjoint ? Error::none
         : o == core::BufferOverlap::arithmetic_overflow
             ? Error::arithmetic_overflow
             : Error::overlapping_buffers;
}
bool read_token(WorkingState &w) noexcept {
  const auto next = [&] {
    w.result.entropy = w.decoder.decode_next(w.operation);
    if (w.result.entropy.error == EntropyError::none)
      return true;
    w.result.error = Error::entropy_error;
    return false;
  };
  if (!next())
    return false;
  if (w.operation.value == 0) {
    if (!next())
      return false;
    w.token = {LzssTypedTokenKind::literal,
               static_cast<std::uint8_t>(w.operation.value), 0, 0};
    return true;
  }
  if (!next())
    return false;
  const auto length_class = w.operation.value;
  const auto width = length_class == 8 ? 1 : length_class;
  std::uint32_t extra{};
  if (width) {
    if (!next())
      return false;
    extra = w.operation.value;
  }
  const auto length = length_class == 8
                          ? 3 + extra
                          : 4 + (std::uint32_t{1} << length_class) + extra;
  if (!next())
    return false;
  const auto distance_class = w.operation.value;
  extra = 0;
  if (distance_class) {
    if (!next())
      return false;
    extra = w.operation.value;
  }
  w.token = {LzssTypedTokenKind::match, 0,
             (std::uint32_t{1} << distance_class) + extra, length};
  return true;
}
} // namespace
std::size_t lzss_position_distance_64m_compact_token_working_bytes() noexcept {
  return working_bytes;
}
LzssPositionDistance64mCompactTokenRequirements
query_lzss_position_distance_64m_compact_tokens(
    const entropy::internal::ContextualDynamicRangeDescriptor &descriptor,
    std::span<const std::byte> payload,
    const dictionary::internal::LzssParameters &p,
    const LzssFieldContextValidationContext &c, const core::DecoderLimits &l,
    std::size_t output_capacity, std::size_t scratch_capacity,
    std::size_t retained) noexcept {
  LzssPositionDistance64mCompactTokenRequirements q{};
  q.token_error = validate_lzss_position_distance_64m_parameters(p, l);
  if (q.token_error != TokenError::none) {
    q.error = q.token_error == TokenError::limit_exceeded
                  ? Error::limit_exceeded
                  : Error::invalid_parameters;
    return q;
  }
  const std::uint64_t t = c.declared_token_count, e = c.declared_event_count,
                      d = c.declared_decision_count, f = c.declared_raw_size,
                      payload_bytes = descriptor.payload_size;
  if (!f || f > 67108864 || !t || t > f || e < 2 * t ||
      e > std::min(2 * f, 5 * t) || d < e || d > std::min(10 * f, 36 * t) ||
      descriptor.decision_count != d || descriptor.context_count != 50 ||
      payload_bytes < 5 || payload_bytes > std::min(2 * d + 5, 20 * f + 5) ||
      payload.size() != payload_bytes) {
    q.error = Error::invalid_counts;
    return q;
  }
  std::uint64_t total{};
  std::size_t out_bytes{}, scratch_bytes{}, aggregate{};
  if (!core::checked_add(c.output_already_committed, f, total) ||
      !core::checked_multiply(output_capacity, std::size_t{1}, out_bytes) ||
      !core::checked_multiply(scratch_capacity, std::size_t{1},
                              scratch_bytes) ||
      !core::checked_add(out_bytes, scratch_bytes, aggregate) ||
      !core::checked_add(aggregate, payload.size(), aggregate) ||
      !core::checked_add(aggregate, working_bytes, aggregate) ||
      !core::checked_add(aggregate, retained, aggregate)) {
    q.error = Error::arithmetic_overflow;
    return q;
  }
  if (f > l.max_frame_size || f > l.max_block_size ||
      total > l.max_total_output_size ||
      payload_bytes > l.max_compressed_payload_size ||
      aggregate > l.max_internal_buffered_bytes ||
      2632 > l.max_entropy_table_entries || 32768 > l.max_range_model_total) {
    q.error = Error::limit_exceeded;
    return q;
  }
  const core::FrameBounds bounds{f,
                                 0,
                                 payload_bytes,
                                 f,
                                 0,
                                 p.window_size,
                                 p.max_match_length,
                                 0,
                                 2632,
                                 32768,
                                 working_bytes,
                                 aggregate - working_bytes,
                                 1};
  const auto bound =
      core::validate_frame_bounds(l, bounds, c.output_already_committed);
  if (bound != core::LimitError::none) {
    q.error = bound == core::LimitError::arithmetic_overflow
                  ? Error::arithmetic_overflow
                  : Error::limit_exceeded;
    return q;
  }
  const auto capacity = static_cast<std::size_t>(std::min(3 * f, 9 * t));
  if (output_capacity < capacity || scratch_capacity < capacity) {
    q.error = Error::output_too_small;
    return q;
  }
  q.aggregate_bytes = aggregate;
  q.working_state_bytes = working_bytes;
  return q;
}
LzssContextualRangeDecodeResult
decode_lzss_position_distance_64m_compact_tokens(
    const entropy::internal::ContextualDynamicRangeDescriptor &descriptor,
    std::span<const std::byte> payload,
    const dictionary::internal::LzssParameters &p,
    const LzssFieldContextValidationContext &c, const core::DecoderLimits &l,
    std::span<std::byte> output, std::span<std::byte> scratch,
    std::size_t &bytes_committed, std::size_t retained) noexcept {
  const auto q = query_lzss_position_distance_64m_compact_tokens(
      descriptor, payload, p, c, l, output.size(), scratch.size(), retained);
  if (q.error != Error::none) {
    LzssContextualRangeDecodeResult r{};
    r.error = q.error;
    r.token_error = q.token_error;
    return r;
  }
  const auto alias = overlap(
      std::array{Region{output.data(), output.size_bytes()},
                 Region{scratch.data(), scratch.size_bytes()}},
      std::array{Region{payload.data(), payload.size()},
                 Region{&descriptor, sizeof(descriptor)}, Region{&p, sizeof(p)},
                 Region{&c, sizeof(c)}, Region{&l, sizeof(l)},
                 Region{&bytes_committed, sizeof(bytes_committed)}});
  if (alias != Error::none) {
    LzssContextualRangeDecodeResult r{};
    r.error = alias;
    return r;
  }
  WorkingState w{};
  auto &r = w.result;
  r.entropy = w.decoder.begin(descriptor, payload, l);
  if (r.entropy.error != EntropyError::none) {
    r.error = Error::entropy_error;
    return r;
  }
  std::size_t cursor{};
  while (r.token_count < c.declared_token_count) {
    r.token_index = r.token_count;
    if (!read_token(w))
      return r;
    const auto checked = validate_lzss_position_distance_64m_token(
        w.token, p, {r.raw_size, c.declared_raw_size}, l);
    if (checked.error != TokenError::none) {
      r.token_error = checked.error;
      r.error = checked.error == TokenError::limit_exceeded
                    ? Error::limit_exceeded
                : checked.error == TokenError::arithmetic_overflow
                    ? Error::arithmetic_overflow
                    : Error::invalid_token;
      return r;
    }
    const bool literal = w.token.kind == LzssTypedTokenKind::literal;
    const std::size_t record_bytes = literal ? 2 : 9;
    if (record_bytes > scratch.size() - cursor) {
      r.error = Error::output_too_small;
      return r;
    }
    scratch[cursor] = literal ? std::byte{0} : std::byte{1};
    if (literal) {
      scratch[cursor + 1] = static_cast<std::byte>(w.token.literal);
    } else if (!core::store_le(scratch, cursor + 1, w.token.distance) ||
               !core::store_le(scratch, cursor + 5, w.token.length)) {
      r.error = Error::internal_error;
      return r;
    }
    cursor += record_bytes;
    r.raw_size = checked.next_raw_size;
    ++r.token_count;
  }
  r.token_index = r.token_count;
  r.entropy =
      w.decoder.finish(c.declared_event_count, c.declared_decision_count);
  if (r.entropy.error != EntropyError::none) {
    r.error = Error::entropy_error;
    return r;
  }
  if (r.raw_size != c.declared_raw_size) {
    r.error = Error::raw_size_mismatch;
    return r;
  }
  std::copy_n(scratch.begin(), cursor, output.begin());
  bytes_committed = cursor;
  return r;
}
} // namespace marc::context::internal

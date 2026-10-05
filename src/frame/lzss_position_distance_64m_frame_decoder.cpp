#include "frame/lzss_position_distance_64m_frame_decoder.hpp"
#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"
#include <algorithm>
#include <array>
namespace marc::frame::internal {
namespace {
using Error = LzssPositionDistance64mFrameDecodeError;
using TokenError = context::internal::LzssContextualRangeDecodeError;
struct Region {
  const void *data;
  std::size_t size;
};
Error overlap(const std::array<Region, 5> &writes,
              const std::array<Region, 4> &reads) noexcept {
  for (const auto &w : writes)
    for (const auto &r : reads) {
      const auto o = core::check_buffer_overlap(w.data, w.size, r.data, r.size);
      if (o != core::BufferOverlap::disjoint)
        return o == core::BufferOverlap::arithmetic_overflow
                   ? Error::arithmetic_overflow
                   : Error::overlapping_buffers;
    }
  for (std::size_t i = 0; i < writes.size(); ++i)
    for (std::size_t j = i + 1; j < writes.size(); ++j) {
      const auto o = core::check_buffer_overlap(writes[i].data, writes[i].size,
                                                writes[j].data, writes[j].size);
      if (o != core::BufferOverlap::disjoint)
        return o == core::BufferOverlap::arithmetic_overflow
                   ? Error::arithmetic_overflow
                   : Error::overlapping_buffers;
    }
  return Error::none;
}
} // namespace
LzssPositionDistance64mFrameDecodePlan
query_lzss_position_distance_64m_frame_decode(
    std::span<const std::byte> input,
    const TypedContextFrameValidationContext &c, std::size_t token_capacity,
    std::size_t token_scratch_capacity, std::size_t raw_capacity,
    std::size_t raw_scratch_capacity, std::size_t retained) noexcept {
  LzssPositionDistance64mFrameDecodePlan q{};
  q.preflight_error = preflight_lzss_position_distance_64m_frame_prefix(
      input, c, q.layout, q.prefix_requirements);
  if (q.preflight_error != LzssPositionDistance64mPreflightError::none) {
    q.error = Error::preflight_error;
    return q;
  }
  if (input.size() < q.layout.serialized_size) {
    q.error = Error::truncated_frame;
    return q;
  }
  if (input.size() > q.layout.serialized_size) {
    q.error = Error::trailing_frame;
    return q;
  }
  const auto &h = q.layout.header;
  q.token_context = {h.token_count, h.event_count, h.decision_count,
                     h.uncompressed_size, c.output_already_committed};
  q.frame_state_bytes = sizeof(LzssPositionDistance64mFrameDecodePlan) +
                        sizeof(LzssPositionDistance64mFrameDecodeResult);
  // Payload is a view of the same serialized owner: charge only header here.
  std::size_t additional{};
  if (!core::checked_add(std::size_t{80}, raw_capacity, additional) ||
      !core::checked_add(additional, raw_scratch_capacity, additional) ||
      !core::checked_add(additional, q.frame_state_bytes, additional) ||
      !core::checked_add(additional, retained, additional)) {
    q.error = Error::arithmetic_overflow;
    return q;
  }
  q.retained_token_bytes = additional;
  q.token_requirements =
      context::internal::query_lzss_position_distance_64m_tokens(
          q.layout.descriptor, input.subspan(80), c.stream.dictionary,
          q.token_context, c.limits, token_capacity, token_scratch_capacity,
          additional);
  switch (q.token_requirements.error) {
  case TokenError::none:
    break;
  case TokenError::arithmetic_overflow:
    q.error = Error::arithmetic_overflow;
    return q;
  case TokenError::limit_exceeded:
    q.error = Error::limit_exceeded;
    return q;
  case TokenError::output_too_small:
    q.error = Error::storage_too_small;
    return q;
  default:
    q.error = Error::token_error;
    return q;
  }
  if (raw_capacity < h.uncompressed_size ||
      raw_scratch_capacity < h.uncompressed_size)
    q.error = Error::storage_too_small;
  return q;
}
LzssPositionDistance64mFrameDecodeResult
decode_lzss_position_distance_64m_frame(
    std::span<const std::byte> input,
    const TypedContextFrameValidationContext &c,
    std::span<dictionary::internal::LzssTypedToken> tokens,
    std::span<dictionary::internal::LzssTypedToken> token_scratch,
    std::span<std::byte> raw_output, std::span<std::byte> raw_scratch,
    TypedContextFrameLayout &layout_output, std::size_t retained) noexcept {
  const auto q = query_lzss_position_distance_64m_frame_decode(
      input, c, tokens.size(), token_scratch.size(), raw_output.size(),
      raw_scratch.size(), retained);
  LzssPositionDistance64mFrameDecodeResult r{};
  r.error = q.error;
  r.preflight_error = q.preflight_error;
  if (r.error != Error::none)
    return r;
  r.error = overlap(
      std::array{Region{tokens.data(), tokens.size_bytes()},
                 Region{token_scratch.data(), token_scratch.size_bytes()},
                 Region{raw_output.data(), raw_output.size()},
                 Region{raw_scratch.data(), raw_scratch.size()},
                 Region{&layout_output, sizeof(layout_output)}},
      std::array{Region{input.data(), input.size()}, Region{&c, sizeof(c)},
                 Region{&c.stream, sizeof(c.stream)},
                 Region{&c.limits, sizeof(c.limits)}});
  if (r.error != Error::none)
    return r;
  r.token_result = context::internal::decode_lzss_position_distance_64m_tokens(
      q.layout.descriptor, input.subspan(80), c.stream.dictionary,
      q.token_context, c.limits, tokens, token_scratch, q.retained_token_bytes);
  if (r.token_result.error != TokenError::none) {
    r.error = Error::token_error;
    return r;
  }
  std::size_t produced{};
  for (std::size_t i = 0; i < q.layout.header.token_count; ++i) {
    const auto &t = tokens[i];
    const auto valid =
        context::internal::validate_lzss_position_distance_64m_token(
            t, c.stream.dictionary,
            {produced, q.layout.header.uncompressed_size}, c.limits);
    if (valid.error != dictionary::internal::LzssTypedTokenError::none) {
      r.error = Error::raw_error;
      return r;
    }
    if (t.kind == dictionary::internal::LzssTypedTokenKind::literal)
      raw_scratch[produced++] = static_cast<std::byte>(t.literal);
    else
      for (std::uint32_t j = 0; j < t.length; ++j) {
        raw_scratch[produced] = raw_scratch[produced - t.distance];
        ++produced;
      }
    if (produced != valid.next_raw_size) {
      r.error = Error::raw_error;
      return r;
    }
  }
  if (produced != q.layout.header.uncompressed_size) {
    r.error = Error::raw_error;
    return r;
  }
  std::copy_n(raw_scratch.begin(), produced, raw_output.begin());
  layout_output = q.layout;
  r.raw_produced = produced;
  r.bytes_consumed = q.layout.serialized_size;
  return r;
}
} // namespace marc::frame::internal

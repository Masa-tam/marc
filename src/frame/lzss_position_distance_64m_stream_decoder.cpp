#include "frame/lzss_position_distance_64m_stream_decoder.hpp"
#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"
#include <algorithm>
#include <cstring>
namespace marc::frame::internal {
namespace {
using Code = core::ErrorCode;
struct Region {
  const void *data;
  std::size_t bytes;
};
// Conservative sum, rather than a physical stack estimate. Includes query
// result, nested prefix/header parser metadata, borrowed context and overlap
// table even though some lifetimes do not overlap. Frame result/plan and token
// helper are charged separately; persistent copies are in sizeof(owner).
constexpr auto control_bytes =
    sizeof(LzssPositionDistance64mStreamRequirements) +
    sizeof(TypedContextStreamHeader) + sizeof(TypedContextFrameLayout) +
    sizeof(LzssPositionDistance64mFrameRequirements) +
    sizeof(TypedContextFrameValidationContext) + sizeof(std::size_t) +
    sizeof(std::array<Region, 8>);
Code category(LzssPositionDistance64mPreflightError e) noexcept {
  using E = LzssPositionDistance64mPreflightError;
  switch (e) {
  case E::none:
    return Code::none;
  case E::limit_exceeded:
  case E::arithmetic_overflow:
    return Code::limit_exceeded;
  case E::unsupported_feature:
  case E::unsupported_version:
  case E::unsupported_format:
    return Code::unsupported;
  default:
    return Code::malformed_stream;
  }
}
} // namespace
LzssPositionDistance64mStreamRequirements
query_lzss_position_distance_64m_stream_workspace(
    const core::DecoderLimits &limits, std::size_t serial, std::size_t tokens,
    std::size_t scratch, std::size_t raw, std::size_t raw_scratch,
    std::size_t extra) noexcept {
  LzssPositionDistance64mStreamRequirements q{};
  q.owner_bytes = sizeof(LzssPositionDistance64mStreamDecoder);
  q.control_bytes = control_bytes;
  q.helper_bytes = lzss_position_distance_64m_compact_frame_working_bytes() +
                   context::internal::
                       lzss_position_distance_64m_compact_token_working_bytes();
  std::size_t tb{}, sb{}, total = serial;
  if (core::validate_limits(limits) != core::LimitError::none) {
    q.error = Code::invalid_argument;
    return q;
  }
  if (!core::checked_multiply(tokens, std::size_t{1}, tb) ||
      !core::checked_multiply(scratch, std::size_t{1}, sb)) {
    q.error = Code::limit_exceeded;
    return q;
  }
  for (auto n : {tb, sb, raw, raw_scratch, q.owner_bytes, q.control_bytes,
                 q.helper_bytes, extra})
    if (!core::checked_add(total, n, total)) {
      q.error = Code::limit_exceeded;
      return q;
    }
  q.aggregate_bytes = total;
  if (total > limits.max_internal_buffered_bytes)
    q.error = Code::limit_exceeded;
  return q;
}
bool LzssPositionDistance64mStreamDecoder::disjoint(
    std::span<const std::byte> input,
    std::span<std::byte> output) const noexcept {
  const std::array regions{Region{input.data(), input.size()},
                           Region{output.data(), output.size()},
                           Region{serialized_.data(), serialized_.size()},
                           Region{tokens_.data(), token_bytes_},
                           Region{token_scratch_.data(), scratch_token_bytes_},
                           Region{raw_.data(), raw_.size()},
                           Region{raw_scratch_.data(), raw_scratch_.size()},
                           Region{this, sizeof(*this)}};
  for (std::size_t i = 0; i < regions.size(); ++i)
    for (std::size_t j = i + 1; j < regions.size(); ++j)
      if (core::check_buffer_overlap(regions[i].data, regions[i].bytes,
                                     regions[j].data, regions[j].bytes) !=
          core::BufferOverlap::disjoint)
        return false;
  return true;
}
LzssPositionDistance64mStreamDecoder::LzssPositionDistance64mStreamDecoder(
    core::DecoderLimits limits, std::span<std::byte> serial,
    std::span<std::byte> tokens, std::span<std::byte> scratch,
    std::span<std::byte> raw, std::span<std::byte> raw_scratch,
    std::size_t extra) noexcept
    : limits_(limits), serialized_(serial), raw_(raw),
      raw_scratch_(raw_scratch), tokens_(tokens), token_scratch_(scratch) {
  if (!core::checked_multiply(tokens.size(), std::size_t{1}, token_bytes_) ||
      !core::checked_multiply(scratch.size(), std::size_t{1},
                              scratch_token_bytes_) ||
      !disjoint({}, {})) {
    static_cast<void>(fail(Code::invalid_argument, 0, 0, 0));
    return;
  }
  const auto q = query_lzss_position_distance_64m_stream_workspace(
      limits, serial.size(), tokens.size(), scratch.size(), raw.size(),
      raw_scratch.size(), extra);
  if (q.error != Code::none) {
    static_cast<void>(fail(q.error, 0, 0, 0));
    return;
  }
  // Admission succeeded, hence every subset sum here is representable.
  retained_ = q.owner_bytes + q.control_bytes + extra;
}
core::ProcessResult
LzssPositionDistance64mStreamDecoder::fail(Code code, std::uint64_t position,
                                           std::size_t consumed,
                                           std::size_t produced) noexcept {
  state_ = State::error;
  error_ = {code, position, 0};
  return {consumed, produced, core::StreamStatus::error, error_};
}
core::ProcessResult
LzssPositionDistance64mStreamDecoder::process(std::span<const std::byte> input,
                                              std::span<std::byte> output,
                                              std::uint32_t flags) noexcept {
  using S = core::StreamStatus;
  if (state_ == State::error)
    return {0, 0, S::error, error_};
  if (state_ == State::ended)
    return {0, 0, S::end_of_stream, {}};
  constexpr auto end = core::flag_value(core::ProcessFlags::end_input);
  constexpr auto allowed = end | core::flag_value(core::ProcessFlags::flush);
  if (flags & ~allowed)
    return fail(Code::unsupported, input_position_, 0, 0);
  if (!disjoint(input, output))
    return fail(Code::invalid_argument, input_position_, 0, 0);
  if (end_seen_ && !input.empty())
    return fail(Code::malformed_stream, input_position_, 0, 0);
  std::size_t consumed{}, produced{};
  const bool final = (flags & end) != 0;
  while (true) {
    if (final && consumed == input.size())
      end_seen_ = true;
    if (state_ == State::draining) {
      const auto n = std::min(needed_.raw_frame_bytes - drained_,
                              output.size() - produced);
      if (n)
        std::memcpy(output.data() + produced, raw_.data() + drained_, n);
      produced += n;
      drained_ += n;
      if (drained_ != needed_.raw_frame_bytes)
        return {consumed, produced, S::need_output, {}};
      collected_ = 0;
      frame_start_ = input_position_;
      state_ = validated_ == stream_.original_size ? State::awaiting_end
                                                   : State::prefix;
      continue;
    }
    if (state_ == State::awaiting_end) {
      if (consumed != input.size())
        return fail(Code::malformed_stream, input_position_, consumed,
                    produced);
      if (end_seen_) {
        state_ = State::ended;
        return {consumed, produced, S::end_of_stream, {}};
      }
      return {consumed,
              produced,
              consumed || produced ? S::progress : S::need_input,
              {}};
    }
    auto dest = state_ == State::header ? std::span<std::byte>{header_}
                : state_ == State::prefix
                    ? std::span<std::byte>{prefix_}
                    : serialized_.first(needed_.serialized_frame_bytes);
    const auto n = std::min(dest.size() - collected_, input.size() - consumed);
    std::uint64_t next{};
    if (!core::checked_add(input_position_, static_cast<std::uint64_t>(n),
                           next))
      return fail(Code::limit_exceeded, input_position_, consumed, produced);
    if (n)
      std::memcpy(dest.data() + collected_, input.data() + consumed, n);
    consumed += n;
    collected_ += n;
    input_position_ = next;
    if (final && consumed == input.size())
      end_seen_ = true;
    if (collected_ != dest.size()) {
      if (end_seen_)
        return fail(Code::malformed_stream, input_position_, consumed,
                    produced);
      return {consumed,
              produced,
              consumed || produced ? S::progress : S::need_input,
              {}};
    }
    if (state_ == State::header) {
      std::size_t parsed{};
      const auto e = parse_lzss_position_distance_64m_stream_header(
          header_, limits_, stream_, parsed);
      if (e != LzssPositionDistance64mPreflightError::none)
        return fail(category(e), 0, consumed, produced);
      collected_ = 0;
      frame_start_ = input_position_;
      state_ = stream_.original_size ? State::prefix : State::awaiting_end;
    } else if (state_ == State::prefix) {
      const TypedContextFrameValidationContext c{stream_, limits_, sequence_,
                                                 validated_};
      const auto e = preflight_lzss_position_distance_64m_compact_frame_prefix(
          prefix_, c, layout_, needed_);
      if (e != LzssPositionDistance64mPreflightError::none)
        return fail(category(e), frame_start_, consumed, produced);
      std::size_t raw_bound{}, token_bound{};
      if (!core::checked_multiply(needed_.raw_frame_bytes, std::size_t{3},
                                  raw_bound) ||
          !core::checked_multiply(needed_.token_count, std::size_t{9},
                                  token_bound))
        return fail(Code::limit_exceeded, frame_start_, consumed, produced);
      const auto record_capacity = std::min(raw_bound, token_bound);
      if (needed_.serialized_frame_bytes > serialized_.size() ||
          record_capacity > tokens_.size() ||
          record_capacity > token_scratch_.size() ||
          needed_.raw_frame_bytes > raw_.size() ||
          needed_.raw_frame_bytes > raw_scratch_.size())
        return fail(Code::limit_exceeded, frame_start_, consumed, produced);
      std::memcpy(serialized_.data(), prefix_.data(), prefix_.size());
      // Prefix already occupies the first 80 bytes of this destination.
      state_ = State::payload;
    } else {
      std::uint64_t next_raw{}, next_seq{};
      if (!core::checked_add(
              validated_, static_cast<std::uint64_t>(needed_.raw_frame_bytes),
              next_raw) ||
          !core::checked_add(sequence_, std::uint64_t{1}, next_seq))
        return fail(Code::limit_exceeded, frame_start_, consumed, produced);
      const TypedContextFrameValidationContext c{stream_, limits_, sequence_,
                                                 validated_};
      frame_detail_ = decode_lzss_position_distance_64m_compact_frame(
          serialized_.first(needed_.serialized_frame_bytes), c, tokens_,
          token_scratch_, raw_, raw_scratch_, layout_,
          retained_ + serialized_.size() - needed_.serialized_frame_bytes);
      if (frame_detail_.error !=
          LzssPositionDistance64mCompactFrameDecodeError::none) {
        auto code = Code::malformed_stream;
        if (frame_detail_.error ==
            LzssPositionDistance64mCompactFrameDecodeError::preflight_error)
          code = category(frame_detail_.preflight_error);
        if (frame_detail_.error ==
                LzssPositionDistance64mCompactFrameDecodeError::
                    limit_exceeded ||
            frame_detail_.error ==
                LzssPositionDistance64mCompactFrameDecodeError::
                    arithmetic_overflow)
          code = Code::limit_exceeded;
        return fail(code, frame_start_ + 80, consumed, produced);
      }
      validated_ = next_raw;
      sequence_ = next_seq;
      drained_ = 0;
      state_ = State::draining;
    }
  }
}
} // namespace marc::frame::internal

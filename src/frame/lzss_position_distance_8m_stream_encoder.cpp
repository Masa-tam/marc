#include "frame/lzss_position_distance_8m_stream_encoder.hpp"
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
// Named conservative call/query controls are separate from persistent copies.
constexpr auto controls =
    sizeof(LzssPositionDistance8mStreamEncodeRequirements) +
    sizeof(LzssPositionDistance8mStreamEncodeCapacities) +
    sizeof(TypedContextFrameValidationContext) + sizeof(core::ProcessResult) +
    sizeof(LzssPositionDistance8mSerializeResult) +
    sizeof(std::array<Region, 12>) + sizeof(std::array<std::size_t, 5>) +
    sizeof(std::array<std::size_t, 6>) + 4 * sizeof(std::size_t) +
    sizeof(TypedContextStreamHeader) + sizeof(core::DecoderLimits) +
    sizeof(LzssPositionDistance8mFrameEncodeWorkspace) +
    2 * sizeof(std::span<std::byte>) + sizeof(std::size_t);
} // namespace
LzssPositionDistance8mStreamEncodeRequirements
query_lzss_position_distance_8m_stream_encode_workspace(
    const core::DecoderLimits &l,
    const LzssPositionDistance8mStreamEncodeCapacities &c,
    std::size_t extra) noexcept {
  LzssPositionDistance8mStreamEncodeRequirements q{};
  q.owner_bytes = sizeof(LzssPositionDistance8mStreamEncoder);
  q.control_bytes = controls;
  q.helper_bytes =
      std::max(lzss_position_distance_8m_frame_encode_working_bytes(),
               lzss_position_distance_8m_stream_serialize_working_bytes());
  if (core::validate_limits(l) != core::LimitError::none) {
    q.error = Code::invalid_argument;
    return q;
  }
  std::array<std::size_t, 5> bytes{};
  if (!core::checked_multiply(
          c.tokens, sizeof(dictionary::internal::LzssTypedToken), bytes[0]) ||
      !core::checked_multiply(c.token_scratch,
                              sizeof(dictionary::internal::LzssTypedToken),
                              bytes[1]) ||
      !core::checked_multiply(c.index_entries, sizeof(std::uint32_t),
                              bytes[2]) ||
      !core::checked_multiply(c.operations,
                              sizeof(context::internal::ModeledOperation),
                              bytes[3]) ||
      !core::checked_multiply(c.operation_scratch,
                              sizeof(context::internal::ModeledOperation),
                              bytes[4])) {
    q.error = Code::limit_exceeded;
    return q;
  }
  for (auto n : bytes)
    if (!core::checked_add(q.buffer_bytes, n, q.buffer_bytes)) {
      q.error = Code::limit_exceeded;
      return q;
    }
  for (auto n :
       {c.raw_bytes, c.publication_bytes, c.frame_bytes, c.payload_bytes})
    if (!core::checked_add(q.buffer_bytes, n, q.buffer_bytes)) {
      q.error = Code::limit_exceeded;
      return q;
    }
  q.aggregate_bytes = q.buffer_bytes;
  for (auto n : {q.owner_bytes, q.control_bytes, q.helper_bytes, extra,
                 c.input_bytes, c.output_bytes})
    if (!core::checked_add(q.aggregate_bytes, n, q.aggregate_bytes)) {
      q.error = Code::limit_exceeded;
      return q;
    }
  if (q.aggregate_bytes > l.max_internal_buffered_bytes)
    q.error = Code::limit_exceeded;
  return q;
}
bool LzssPositionDistance8mStreamEncoder::disjoint(
    std::span<const std::byte> in, std::span<std::byte> out) const noexcept {
  const std::array regions{
      Region{in.data(), in.size()},
      Region{out.data(), out.size()},
      Region{raw_.data(), raw_.size()},
      Region{publication_.data(), publication_.size()},
      Region{workspace_.tokens.data(), typed_bytes_[0]},
      Region{workspace_.token_scratch.data(), typed_bytes_[1]},
      Region{workspace_.index.data(), typed_bytes_[2]},
      Region{workspace_.operations.data(), typed_bytes_[3]},
      Region{workspace_.operation_scratch.data(), typed_bytes_[4]},
      Region{workspace_.frame.data(), workspace_.frame.size()},
      Region{workspace_.payload_scratch.data(),
             workspace_.payload_scratch.size()},
      Region{this, sizeof(*this)}};
  for (std::size_t i = 0; i < regions.size(); ++i)
    for (std::size_t j = i + 1; j < regions.size(); ++j)
      if (core::check_buffer_overlap(regions[i].data, regions[i].bytes,
                                     regions[j].data, regions[j].bytes) !=
          core::BufferOverlap::disjoint)
        return false;
  return true;
}
LzssPositionDistance8mStreamEncoder::LzssPositionDistance8mStreamEncoder(
    TypedContextStreamHeader stream, core::DecoderLimits limits,
    std::span<std::byte> raw, std::span<std::byte> publication,
    LzssPositionDistance8mFrameEncodeWorkspace workspace,
    std::size_t extra) noexcept
    : stream_(stream), limits_(limits), raw_(raw), publication_(publication),
      workspace_(workspace), external_(extra) {
  const LzssPositionDistance8mStreamEncodeCapacities caps{
      raw.size(),
      publication.size(),
      workspace.tokens.size(),
      workspace.token_scratch.size(),
      workspace.index.size(),
      workspace.operations.size(),
      workspace.operation_scratch.size(),
      workspace.frame.size(),
      workspace.payload_scratch.size()};
  base_ = query_lzss_position_distance_8m_stream_encode_workspace(limits_, caps,
                                                                  extra);
  if (base_.error != Code::none) {
    static_cast<void>(fail(base_.error, 0));
    return;
  }
  // All subset products are representable after numeric admission.
  typed_bytes_ = {
      workspace.tokens.size() * sizeof(dictionary::internal::LzssTypedToken),
      workspace.token_scratch.size() *
          sizeof(dictionary::internal::LzssTypedToken),
      workspace.index.size() * sizeof(std::uint32_t),
      workspace.operations.size() * sizeof(context::internal::ModeledOperation),
      workspace.operation_scratch.size() *
          sizeof(context::internal::ModeledOperation)};
  if (!disjoint({}, {})) {
    static_cast<void>(fail(Code::invalid_argument, 0));
    return;
  }
  if (raw.size() <
      std::min<std::uint64_t>(stream.frame_size, stream.original_size)) {
    static_cast<void>(fail(Code::limit_exceeded, 0));
    return;
  }
  const auto local =
      112 + lzss_position_distance_8m_stream_serialize_working_bytes();
  if (local > base_.aggregate_bytes) {
    static_cast<void>(fail(Code::internal_error, 0));
    return;
  }
  const auto h = serialize_lzss_position_distance_8m_stream_header(
      stream_, limits_, header_, header_written_,
      base_.aggregate_bytes - local);
  if (h.error != LzssPositionDistance8mSerializeError::none) {
    static_cast<void>(
        fail(h.error == LzssPositionDistance8mSerializeError::limit_exceeded
                 ? Code::limit_exceeded
                 : Code::invalid_argument,
             0));
    return;
  }
}
core::ProcessResult LzssPositionDistance8mStreamEncoder::fail(
    Code c, std::uint64_t pos, std::size_t used, std::size_t made) noexcept {
  state_ = State::error;
  error_ = {c, pos, 0};
  return {used, made, core::StreamStatus::error, error_};
}
core::ProcessResult
LzssPositionDistance8mStreamEncoder::process(std::span<const std::byte> input,
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
    return fail(Code::unsupported, accepted_);
  if (!disjoint(input, output))
    return fail(Code::invalid_argument, accepted_);
  std::size_t top = base_.aggregate_bytes;
  if (!core::checked_add(top, input.size(), top) ||
      !core::checked_add(top, output.size(), top) ||
      top > limits_.max_internal_buffered_bytes)
    return fail(Code::limit_exceeded, accepted_);
  if (end_seen_ && !input.empty())
    return fail(Code::malformed_stream, accepted_);
  std::size_t used{}, made{};
  const bool final = (flags & end) != 0;
  while (true) {
    if (final && used == input.size())
      end_seen_ = true;
    if (state_ == State::header_drain || state_ == State::frame_drain) {
      const auto pending =
          state_ == State::header_drain
              ? std::span<const std::byte>(header_).first(header_written_)
              : std::span<const std::byte>(publication_).first(frame_written_);
      const auto n = std::min(pending.size() - drained_, output.size() - made);
      if (n)
        std::memcpy(output.data() + made, pending.data() + drained_, n);
      // Cursor sums are bounded by their admitted live span sizes.
      made += n;
      drained_ += n;
      if (drained_ != pending.size())
        return {used, made, S::need_output, {}};
      drained_ = 0;
      collected_ = 0;
      state_ = validated_ == stream_.original_size ? State::awaiting_end
                                                   : State::collecting;
      continue;
    }
    if (state_ == State::awaiting_end) {
      if (used != input.size())
        return fail(Code::malformed_stream, accepted_, used, made);
      if (end_seen_) {
        state_ = State::ended;
        return {used, made, S::end_of_stream, {}};
      }
      return {used, made, used || made ? S::progress : S::need_input, {}};
    }
    const auto needed = static_cast<std::size_t>(std::min<std::uint64_t>(
        stream_.frame_size, stream_.original_size - validated_));
    const auto n = std::min(needed - collected_, input.size() - used);
    std::uint64_t next{};
    if (!core::checked_add(accepted_, static_cast<std::uint64_t>(n), next))
      return fail(Code::limit_exceeded, accepted_, used, made);
    if (n)
      std::memcpy(raw_.data() + collected_, input.data() + used, n);
    used += n;
    collected_ += n;
    accepted_ = next;
    if (final && used == input.size())
      end_seen_ = true;
    if (collected_ != needed) {
      if (end_seen_)
        return fail(Code::malformed_stream, accepted_, used, made);
      return {used, made, used || made ? S::progress : S::need_input, {}};
    }
    std::uint64_t next_raw{}, next_seq{};
    if (!core::checked_add(validated_, static_cast<std::uint64_t>(needed),
                           next_raw) ||
        !core::checked_add(sequence_, std::uint64_t{1}, next_seq))
      return fail(Code::limit_exceeded, validated_, used, made);
    std::size_t local = base_.buffer_bytes - raw_.size();
    if (!core::checked_add(local, needed, local) ||
        !core::checked_add(local, base_.helper_bytes, local) || local > top)
      return fail(Code::internal_error, validated_, used, made);
    const TypedContextFrameValidationContext context{stream_, limits_,
                                                     sequence_, validated_};
    frame_result_ = encode_lzss_position_distance_8m_frame(
        raw_.first(needed), context, workspace_, publication_, layout_,
        frame_written_, top - local);
    if (frame_result_.error != LzssPositionDistance8mFrameEncodeError::none) {
      const auto code =
          frame_result_.error ==
                      LzssPositionDistance8mFrameEncodeError::invalid_stream ||
                  frame_result_.error ==
                      LzssPositionDistance8mFrameEncodeError::
                          invalid_position ||
                  frame_result_.error ==
                      LzssPositionDistance8mFrameEncodeError::
                          inconsistent_counts
              ? Code::internal_error
              : Code::limit_exceeded;
      return fail(code, validated_, used, made);
    }
    if (frame_result_.aggregate_bytes != top)
      return fail(Code::internal_error, validated_, used, made);
    validated_ = next_raw;
    sequence_ = next_seq;
    state_ = State::frame_drain;
    drained_ = 0;
  }
}
} // namespace marc::frame::internal

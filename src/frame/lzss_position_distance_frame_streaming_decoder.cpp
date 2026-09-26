#include "frame/lzss_position_distance_frame_streaming_decoder.hpp"

#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"
#include "entropy/lzss_position_distance_range_state.hpp"
#include "frame/lzss_position_distance_frame_decoder.hpp"

#include <algorithm>
#include <cstring>

namespace marc::frame::internal {
namespace {
core::ErrorCode category(const LzssShortMatchPreflightError error) noexcept {
    using E = LzssShortMatchPreflightError;
    switch (error) {
    case E::none: return core::ErrorCode::none;
    case E::limit_exceeded:
    case E::arithmetic_overflow: return core::ErrorCode::limit_exceeded;
    case E::unsupported_feature:
    case E::unsupported_version:
    case E::unsupported_format: return core::ErrorCode::unsupported;
    default: return core::ErrorCode::malformed_stream;
    }
}
}

bool LzssPositionDistanceFrameStreamingDecoder::disjoint(
    const std::span<const std::byte> input, const std::span<std::byte> output) const noexcept {
    struct Region { const void* data; std::size_t size; };
    const std::array regions{Region{input.data(), input.size()},
        Region{output.data(), output.size()}, Region{serialized_.data(), serialized_.size()},
        Region{tokens_.data(), token_bytes_}, Region{raw_.data(), raw_.size()},
        Region{this, sizeof(*this)}};
    for (std::size_t i = 0; i < regions.size(); ++i) {
        for (std::size_t j = i + 1; j < regions.size(); ++j) {
            if (core::check_buffer_overlap(regions[i].data, regions[i].size,
                    regions[j].data, regions[j].size) != core::BufferOverlap::disjoint)
                return false;
        }
    }
    return true;
}

LzssPositionDistanceFrameStreamingDecoder::LzssPositionDistanceFrameStreamingDecoder(
    const core::DecoderLimits limits, const std::span<std::byte> serialized,
    const std::span<dictionary::internal::LzssTypedToken> tokens,
    const std::span<std::byte> raw) noexcept
    : limits_(limits), serialized_(serialized), tokens_(tokens), raw_(raw) {
    if (core::validate_limits(limits_) != core::LimitError::none
        || !core::checked_multiply(tokens_.size(), sizeof(dictionary::internal::LzssTypedToken), token_bytes_)
        || !disjoint({}, {})) {
        state_ = State::error;
        error_ = {core::ErrorCode::invalid_argument, 0, 0};
        return;
    }
    // Charge supplied storage, retained headers and bounded model/replay state.
    std::size_t aggregate = sizeof(*this);
    for (const auto bytes : {serialized_.size(), token_bytes_, raw_.size(),
            sizeof(entropy::internal::LzssPositionDistanceRangeState)}) {
        if (!core::checked_add(aggregate, bytes, aggregate)
            || aggregate > limits_.max_internal_buffered_bytes) {
            state_ = State::error;
            error_ = {core::ErrorCode::limit_exceeded, 0, 0};
            return;
        }
    }
}

core::ProcessResult LzssPositionDistanceFrameStreamingDecoder::fail(
    const core::ErrorCode code, const std::uint64_t position,
    const std::size_t consumed, const std::size_t produced) noexcept {
    state_ = State::error;
    error_ = {code, position, 0};
    return {consumed, produced, core::StreamStatus::error, error_};
}

core::ProcessResult LzssPositionDistanceFrameStreamingDecoder::process(
    const std::span<const std::byte> input, const std::span<std::byte> output,
    const std::uint32_t flags) noexcept {
    using Status = core::StreamStatus;
    using Code = core::ErrorCode;
    if (state_ == State::error) return {0, 0, Status::error, error_};
    if (state_ == State::ended) return {0, 0, Status::end_of_stream, {}};
    constexpr auto end_flag = core::flag_value(core::ProcessFlags::end_input);
    constexpr auto allowed = end_flag | core::flag_value(core::ProcessFlags::flush);
    if ((flags & ~allowed) != 0) return fail(Code::unsupported, input_position_, 0, 0);
    if (!disjoint(input, output)) return fail(Code::invalid_argument, input_position_, 0, 0);
    if (end_seen_ && !input.empty()) return fail(Code::malformed_stream, input_position_, 0, 0);
    const bool final = (flags & end_flag) != 0;
    std::size_t consumed{}, produced{};
    while (true) {
        if (final && consumed == input.size()) end_seen_ = true;
        if (state_ == State::draining) {
            const auto count = std::min(needed_.raw_frame_bytes - drained_, output.size() - produced);
            if (count != 0) std::memcpy(output.data() + produced, raw_.data() + drained_, count);
            produced += count;
            drained_ += count;
            if (drained_ != needed_.raw_frame_bytes) return {consumed, produced, Status::need_output, {}};
            collected_ = 0;
            frame_start_ = input_position_;
            state_ = raw_validated_ == stream_.original_size ? State::awaiting_end : State::frame_prefix;
            continue;
        }
        if (state_ == State::awaiting_end) {
            if (consumed != input.size()) return fail(Code::malformed_stream, input_position_, consumed, produced);
            if (end_seen_) {
                state_ = State::ended;
                return {consumed, produced, Status::end_of_stream, {}};
            }
            return {consumed, produced, consumed || produced ? Status::progress : Status::need_input, {}};
        }
        auto destination = state_ == State::stream_header ? std::span<std::byte>{header_}
            : state_ == State::frame_prefix ? std::span<std::byte>{prefix_}
            : serialized_.first(needed_.serialized_frame_bytes);
        const auto count = std::min(destination.size() - collected_, input.size() - consumed);
        std::uint64_t next_position{};
        if (!core::checked_add(input_position_, static_cast<std::uint64_t>(count), next_position))
            return fail(Code::limit_exceeded, input_position_, consumed, produced);
        if (count != 0) std::memcpy(destination.data() + collected_, input.data() + consumed, count);
        consumed += count;
        collected_ += count;
        input_position_ = next_position;
        if (final && consumed == input.size()) end_seen_ = true;
        if (collected_ != destination.size()) {
            if (end_seen_) return fail(Code::malformed_stream, input_position_, consumed, produced);
            return {consumed, produced, consumed || produced ? Status::progress : Status::need_input, {}};
        }
        if (state_ == State::stream_header) {
            std::size_t parsed{};
            const auto error = parse_lzss_position_distance_stream_header(header_, limits_, stream_, parsed);
            if (error != LzssShortMatchPreflightError::none) return fail(category(error), 0, consumed, produced);
            collected_ = 0;
            frame_start_ = input_position_;
            state_ = stream_.original_size == 0 ? State::awaiting_end : State::frame_prefix;
        } else if (state_ == State::frame_prefix) {
            TypedContextFrameLayout layout{};
            const auto error = preflight_lzss_position_distance_frame_prefix(prefix_,
                {stream_, limits_, sequence_, raw_validated_}, layout, needed_);
            if (error != LzssShortMatchPreflightError::none)
                return fail(category(error), frame_start_, consumed, produced);
            if (needed_.serialized_frame_bytes > serialized_.size()
                || needed_.token_count > tokens_.size() || needed_.raw_frame_bytes > raw_.size())
                return fail(Code::limit_exceeded, frame_start_, consumed, produced);
            std::memcpy(serialized_.data(), prefix_.data(), prefix_size);
            state_ = State::payload;
        } else {
            const auto decoded = decode_lzss_position_distance_frame(
                serialized_.first(needed_.serialized_frame_bytes),
                {stream_, limits_, sequence_, raw_validated_},
                tokens_.first(needed_.token_count), raw_.first(needed_.raw_frame_bytes));
            if (decoded.error != LzssShortMatchFrameDecodeError::none)
                return fail(Code::malformed_stream, frame_start_ + prefix_size, consumed, produced);
            // Preflight bounded each increment by remaining declared output.
            raw_validated_ += needed_.raw_frame_bytes;
            ++sequence_;
            drained_ = 0;
            state_ = State::draining;
        }
    }
}

} // namespace marc::frame::internal

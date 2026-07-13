#include "dictionary/lzss_streaming_encoder.hpp"

#include "core/checked_math.hpp"

#include <algorithm>
#include <cstring>
#include <limits>

namespace marc::dictionary::internal {
namespace {

constexpr std::uint32_t known_flags =
    core::flag_value(core::ProcessFlags::flush)
    | core::flag_value(core::ProcessFlags::end_input)
    | core::flag_value(core::ProcessFlags::reset_block);

} // namespace

LzssStreamingEncoder::LzssStreamingEncoder(
    const LzssParameters parameters,
    const std::uint64_t declared_frame_size,
    const core::DecoderLimits limits,
    const std::span<std::byte> raw_storage,
    const std::span<std::byte> encoded_storage) noexcept
    : parameters_(parameters), limits_(limits), raw_storage_(raw_storage),
      encoded_storage_(encoded_storage),
      declared_frame_size_(declared_frame_size) {
    if (core::validate_limits(limits_) != core::LimitError::none
        || validate_lzss_parameters(parameters_, limits_)
               != LzssFormatError::none) {
        state_ = State::error;
        terminal_error_ = {core::ErrorCode::invalid_argument, 0, 0};
    } else if (declared_frame_size_ > limits_.max_frame_size) {
        state_ = State::error;
        terminal_error_ = {core::ErrorCode::limit_exceeded, 0, 0};
    } else if (declared_frame_size_
                   > static_cast<std::uint64_t>(
                       std::numeric_limits<std::size_t>::max())
               || raw_storage_.size()
                      < static_cast<std::size_t>(declared_frame_size_)) {
        state_ = State::error;
        terminal_error_ = {core::ErrorCode::out_of_memory, 0, 0};
    } else if (declared_frame_size_ == 0) {
        state_ = State::awaiting_end;
    }
}

core::ProcessResult LzssStreamingEncoder::fail(
    const core::ErrorCode code, const std::size_t consumed,
    const std::size_t produced) noexcept {
    state_ = State::error;
    terminal_error_ = {code, raw_collected_, 0};
    return {consumed, produced, core::StreamStatus::error, terminal_error_};
}

bool LzssStreamingEncoder::prepare_encoded_frame() noexcept {
    preparation_error_ = core::ErrorCode::internal_error;
    const auto raw = std::span<const std::byte>{raw_storage_}.first(
        raw_collected_);
    const auto planned = plan_lzss_token_stream(raw, parameters_, limits_);
    if (planned.error != LzssEncodeError::none) {
        preparation_error_ =
            planned.error == LzssEncodeError::serialized_limit_exceeded
                || planned.error == LzssEncodeError::input_limit_exceeded
            ? core::ErrorCode::limit_exceeded
            : core::ErrorCode::internal_error;
        return false;
    }
    std::uint64_t buffered_bytes{};
    if (!core::checked_add(declared_frame_size_,
                           static_cast<std::uint64_t>(planned.output_size),
                           buffered_bytes)
        || buffered_bytes > limits_.max_internal_buffered_bytes) {
        preparation_error_ = core::ErrorCode::limit_exceeded;
        return false;
    }
    if (planned.output_size > encoded_storage_.size()) {
        preparation_error_ = core::ErrorCode::out_of_memory;
        return false;
    }
    const auto encoded = encode_lzss_token_stream(
        raw, parameters_, limits_, encoded_storage_.first(planned.output_size));
    if (encoded.error != LzssEncodeError::none
        || encoded.output_size != planned.output_size)
        return false;
    encoded_size_ = encoded.output_size;
    output_offset_ = 0;
    state_ = encoded_size_ == 0 ? State::awaiting_end : State::draining;
    return true;
}

core::ProcessResult LzssStreamingEncoder::process(
    const std::span<const std::byte> input,
    const std::span<std::byte> output,
    const std::uint32_t flags) noexcept {
    if (state_ == State::error)
        return {0, 0, core::StreamStatus::error, terminal_error_};
    if (state_ == State::ended)
        return {0, 0, core::StreamStatus::end_of_stream, {}};
    if ((flags & ~known_flags) != 0
        || (flags & core::flag_value(core::ProcessFlags::reset_block)) != 0)
        return fail(core::ErrorCode::unsupported, 0, 0);
    const bool end_requested =
        (flags & core::flag_value(core::ProcessFlags::end_input)) != 0;
    std::size_t consumed{};
    std::size_t produced{};

    while (true) {
        if (state_ == State::draining) {
            const auto remaining = encoded_size_ - output_offset_;
            const auto count = std::min(remaining, output.size() - produced);
            if (count != 0) {
                std::memmove(output.data() + produced,
                             encoded_storage_.data() + output_offset_, count);
                output_offset_ += count;
                produced += count;
            }
            if (output_offset_ != encoded_size_) {
                if (end_requested && consumed == input.size()) end_seen_ = true;
                return {consumed, produced, core::StreamStatus::need_output, {}};
            }
            state_ = State::awaiting_end;
            continue;
        }

        if (state_ == State::awaiting_end) {
            if (consumed != input.size())
                return fail(core::ErrorCode::invalid_argument, consumed,
                            produced);
            if (end_seen_ || end_requested) {
                state_ = State::ended;
                return {consumed, produced,
                        core::StreamStatus::end_of_stream, {}};
            }
            return {consumed, produced,
                    (consumed != 0 || produced != 0)
                        ? core::StreamStatus::progress
                        : core::StreamStatus::need_input,
                    {}};
        }

        const auto frame_size = static_cast<std::size_t>(declared_frame_size_);
        const auto needed = frame_size - raw_collected_;
        const auto count = std::min(needed, input.size() - consumed);
        if (count != 0) {
            std::memmove(raw_storage_.data() + raw_collected_,
                         input.data() + consumed, count);
            raw_collected_ += count;
            consumed += count;
        }
        if (raw_collected_ != frame_size) {
            if (end_requested && consumed == input.size())
                return fail(core::ErrorCode::invalid_argument, consumed,
                            produced);
            return {consumed, produced,
                    (consumed != 0 || produced != 0)
                        ? core::StreamStatus::progress
                        : core::StreamStatus::need_input,
                    {}};
        }
        if (consumed != input.size())
            return fail(core::ErrorCode::invalid_argument, consumed, produced);
        if (end_requested) end_seen_ = true;
        if (!prepare_encoded_frame())
            return fail(preparation_error_, consumed, produced);
    }
}

} // namespace marc::dictionary::internal

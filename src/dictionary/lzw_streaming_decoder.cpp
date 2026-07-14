#include "dictionary/lzw_streaming_decoder.hpp"

#include "core/checked_math.hpp"

#include <algorithm>

namespace marc::dictionary::internal {
namespace {

constexpr std::uint32_t known_flags =
    core::flag_value(core::ProcessFlags::flush)
    | core::flag_value(core::ProcessFlags::end_input)
    | core::flag_value(core::ProcessFlags::reset_block);

} // namespace

LzwStreamingDecoder::LzwStreamingDecoder(
    const LzwParameters parameters,
    const std::uint64_t declared_frame_size,
    const core::DecoderLimits limits,
    const std::span<LzwPhraseEntry> dictionary_workspace) noexcept
    : parameters_(parameters), limits_(limits),
      dictionary_workspace_(dictionary_workspace),
      declared_frame_size_(declared_frame_size) {
    if (core::validate_limits(limits_) != core::LimitError::none
        || validate_lzw_parameters(parameters_, limits_)
               != LzwFormatError::none) {
        state_ = State::error;
        terminal_error_ = {core::ErrorCode::invalid_argument, 0, 0};
        return;
    }
    if (declared_frame_size_ > limits_.max_frame_size) {
        state_ = State::error;
        terminal_error_ = {core::ErrorCode::limit_exceeded, 0, 0};
        return;
    }
    const auto capacity = static_cast<std::uint64_t>(
        lzw_code_limit(parameters_) - lzw_first_free_code);
    const auto required_entries = declared_frame_size_ == 0
        ? UINT64_C(0)
        : std::min(declared_frame_size_ - 1, capacity);
    std::uint64_t workspace_bytes{};
    if (!core::checked_multiply(
            required_entries,
            static_cast<std::uint64_t>(sizeof(LzwPhraseEntry)),
            workspace_bytes)
        || workspace_bytes > limits_.max_internal_buffered_bytes) {
        state_ = State::error;
        terminal_error_ = {core::ErrorCode::limit_exceeded, 0, 0};
        return;
    }
    if (dictionary_workspace_.size() < required_entries) {
        state_ = State::error;
        terminal_error_ = {core::ErrorCode::out_of_memory, 0, 0};
        return;
    }
    if (declared_frame_size_ == 0) state_ = State::awaiting_end;
}

bool LzwStreamingDecoder::phrase_byte(
    std::uint32_t code, const std::uint64_t forward_offset,
    std::byte& value) const noexcept {
    while (code >= lzw_first_free_code) {
        if (code >= next_code_) return false;
        const auto index = static_cast<std::size_t>(
            code - lzw_first_free_code);
        if (index >= dictionary_workspace_.size()) return false;
        const auto& entry = dictionary_workspace_[index];
        if (entry.length != 0 && entry.length - 1 == forward_offset) {
            value = static_cast<std::byte>(entry.trailing_byte);
            return true;
        }
        if (entry.length <= forward_offset || entry.prefix_code >= code)
            return false;
        code = entry.prefix_code;
    }
    if (forward_offset != 0) return false;
    value = static_cast<std::byte>(code);
    return true;
}

core::ProcessResult LzwStreamingDecoder::fail(
    const core::ErrorCode code, const std::size_t consumed,
    const std::size_t produced) noexcept {
    state_ = State::error;
    terminal_error_ = {code, input_position_, 0};
    return {consumed, produced, core::StreamStatus::error, terminal_error_};
}

core::ProcessResult LzwStreamingDecoder::process(
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
        if (state_ == State::draining_phrase) {
            while (phrase_emitted_ < phrase_length_
                   && produced < output.size()) {
                std::byte value{};
                if (!phrase_byte(phrase_code_, phrase_emitted_, value))
                    return fail(core::ErrorCode::internal_error, consumed,
                                produced);
                output[produced++] = value;
                ++phrase_emitted_;
                ++output_committed_;
            }
            if (phrase_emitted_ != phrase_length_) {
                if (end_requested && consumed == input.size()) end_seen_ = true;
                return {consumed, produced, core::StreamStatus::need_output, {}};
            }
            phrase_length_ = 0;
            phrase_emitted_ = 0;
            if (output_committed_ == declared_frame_size_) {
                if (reader_.align_to_byte(true)
                    != core::BitIoStatus::complete)
                    return fail(core::ErrorCode::malformed_stream, consumed,
                                produced);
                state_ = State::awaiting_end;
            } else {
                state_ = State::collecting_code;
            }
            continue;
        }

        if (state_ == State::awaiting_end) {
            if (consumed != input.size())
                return fail(core::ErrorCode::malformed_stream, consumed,
                            produced);
            if (end_seen_ || end_requested) {
                state_ = State::ended;
                return {consumed, produced, core::StreamStatus::end_of_stream,
                        {}};
            }
            return {consumed, produced,
                    (consumed != 0 || produced != 0)
                        ? core::StreamStatus::progress
                        : core::StreamStatus::need_input,
                    {}};
        }

        const auto serialized_limit = std::min(
            {limits_.max_dictionary_serialized_size,
             limits_.max_compressed_payload_size,
             limits_.max_internal_buffered_bytes});
        if (input_position_ > serialized_limit
            || input.size() - consumed > serialized_limit - input_position_)
            return fail(core::ErrorCode::limit_exceeded, consumed, produced);

        if (bits_collected_ == 0 && !first_code_
            && code_width_ < parameters_.maximum_code_width
            && next_code_ == (UINT32_C(1) << code_width_) - 1)
            ++code_width_;
        const auto remaining_bits = code_width_ - bits_collected_;
        const auto read = reader_.read_bits(
            input.subspan(consumed),
            static_cast<std::uint8_t>(remaining_bits));
        pending_code_ |= static_cast<std::uint32_t>(
            read.value << bits_collected_);
        bits_collected_ += read.bits_produced;
        consumed += read.bytes_consumed;
        input_position_ += read.bytes_consumed;
        if (bits_collected_ != code_width_) {
            if (end_requested && consumed == input.size())
                return fail(core::ErrorCode::malformed_stream, consumed,
                            produced);
            return {consumed, produced,
                    (consumed != 0 || produced != 0)
                        ? core::StreamStatus::progress
                        : core::StreamStatus::need_input,
                    {}};
        }

        const auto code = pending_code_;
        pending_code_ = 0;
        bits_collected_ = 0;
        std::uint8_t current_first{};
        std::uint64_t current_length{};
        if (first_code_) {
            if (code >= lzw_first_free_code)
                return fail(core::ErrorCode::malformed_stream, consumed,
                            produced);
            current_first = static_cast<std::uint8_t>(code);
            current_length = 1;
            first_code_ = false;
        } else if (code < next_code_) {
            if (code < lzw_first_free_code) {
                current_first = static_cast<std::uint8_t>(code);
                current_length = 1;
            } else {
                const auto& entry = dictionary_workspace_[
                    code - lzw_first_free_code];
                current_first = entry.first_byte;
                current_length = entry.length;
            }
        } else if (code == next_code_
                   && next_code_ < lzw_code_limit(parameters_)) {
            current_first = previous_first_byte_;
            if (!core::checked_add(previous_length_, UINT64_C(1),
                                   current_length))
                return fail(core::ErrorCode::malformed_stream, consumed,
                            produced);
        } else {
            return fail(core::ErrorCode::malformed_stream, consumed, produced);
        }

        std::uint64_t next_output{};
        if (!core::checked_add(output_committed_, current_length, next_output)
            || next_output > declared_frame_size_)
            return fail(core::ErrorCode::malformed_stream, consumed, produced);

        if (!first_code_ && previous_length_ != 0
            && next_code_ < lzw_code_limit(parameters_)) {
            std::uint64_t inserted_length{};
            if (!core::checked_add(previous_length_, UINT64_C(1),
                                   inserted_length))
                return fail(core::ErrorCode::malformed_stream, consumed,
                            produced);
            dictionary_workspace_[next_code_ - lzw_first_free_code] = {
                previous_code_, current_first, previous_first_byte_,
                inserted_length};
            ++next_code_;
        }
        previous_code_ = code;
        previous_first_byte_ = current_first;
        previous_length_ = current_length;
        phrase_code_ = code;
        phrase_length_ = current_length;
        phrase_emitted_ = 0;
        state_ = State::draining_phrase;
    }
}

} // namespace marc::dictionary::internal

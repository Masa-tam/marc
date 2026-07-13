#include "dictionary/lz78_streaming_decoder.hpp"

#include "core/checked_math.hpp"

#include <algorithm>
#include <cstring>

namespace marc::dictionary::internal {
namespace {

constexpr std::uint32_t known_flags =
    core::flag_value(core::ProcessFlags::flush)
    | core::flag_value(core::ProcessFlags::end_input)
    | core::flag_value(core::ProcessFlags::reset_block);

} // namespace

Lz78StreamingDecoder::Lz78StreamingDecoder(
    const Lz78Parameters parameters,
    const std::uint64_t declared_frame_size,
    const core::DecoderLimits limits,
    const std::span<Lz78PhraseEntry> dictionary_workspace) noexcept
    : parameters_(parameters), limits_(limits),
      dictionary_workspace_(dictionary_workspace),
      declared_frame_size_(declared_frame_size) {
    if (core::validate_limits(limits_) != core::LimitError::none
        || validate_lz78_parameters(parameters_, limits_)
               != Lz78FormatError::none) {
        state_ = State::error;
        terminal_error_ = {core::ErrorCode::invalid_argument, 0, 0};
        return;
    }
    if (declared_frame_size_ > limits_.max_frame_size) {
        state_ = State::error;
        terminal_error_ = {core::ErrorCode::limit_exceeded, 0, 0};
        return;
    }
    const auto required_entries = std::min<std::uint64_t>(
        declared_frame_size_, parameters_.maximum_entries);
    std::uint64_t workspace_bytes{};
    if (!core::checked_multiply(
            required_entries,
            static_cast<std::uint64_t>(sizeof(Lz78PhraseEntry)),
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

bool Lz78StreamingDecoder::phrase_byte(
    std::uint32_t phrase_index, const std::uint64_t forward_offset,
    std::byte& value) const noexcept {
    while (phrase_index != 0) {
        if (phrase_index > dictionary_entries_
            || phrase_index > dictionary_workspace_.size())
            return false;
        const auto& entry = dictionary_workspace_[phrase_index - 1];
        if (entry.length == forward_offset + 1) {
            value = static_cast<std::byte>(entry.symbol);
            return true;
        }
        if (entry.length <= forward_offset
            || entry.prefix_index >= phrase_index)
            return false;
        phrase_index = entry.prefix_index;
    }
    return false;
}

core::ProcessResult Lz78StreamingDecoder::fail(
    const core::ErrorCode code, const std::size_t consumed,
    const std::size_t produced) noexcept {
    state_ = State::error;
    terminal_error_ = {code, input_position_, 0};
    return {consumed, produced, core::StreamStatus::error, terminal_error_};
}

core::ProcessResult Lz78StreamingDecoder::process(
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
        if (state_ == State::draining_token) {
            while (phrase_emitted_ < phrase_length_
                   && produced < output.size()) {
                std::byte value{};
                if (!phrase_byte(token_.phrase_index, phrase_emitted_, value))
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
            if (symbol_pending_ && produced < output.size()) {
                output[produced++] = static_cast<std::byte>(token_.symbol);
                ++output_committed_;
                symbol_pending_ = false;
            }
            if (symbol_pending_) {
                if (end_requested && consumed == input.size()) end_seen_ = true;
                return {consumed, produced, core::StreamStatus::need_output, {}};
            }

            token_collected_ = 0;
            phrase_length_ = 0;
            phrase_emitted_ = 0;
            state_ = output_committed_ == declared_frame_size_
                ? State::awaiting_end : State::collecting_token;
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
            limits_.max_dictionary_serialized_size,
            limits_.max_internal_buffered_bytes);
        if (input_position_ > serialized_limit
            || input.size() - consumed > serialized_limit - input_position_)
            return fail(core::ErrorCode::limit_exceeded, consumed, produced);

        const auto needed = lz78_token_size - token_collected_;
        const auto count = std::min(needed, input.size() - consumed);
        if (count != 0) {
            std::memmove(token_bytes_.data() + token_collected_,
                         input.data() + consumed, count);
            token_collected_ += count;
            consumed += count;
            input_position_ += count;
        }
        if (token_collected_ != lz78_token_size) {
            if (end_requested && consumed == input.size())
                return fail(core::ErrorCode::malformed_stream, consumed,
                            produced);
            return {consumed, produced,
                    (consumed != 0 || produced != 0)
                        ? core::StreamStatus::progress
                        : core::StreamStatus::need_input,
                    {}};
        }
        if (parse_lz78_token(token_bytes_, token_) != Lz78FormatError::none)
            return fail(core::ErrorCode::malformed_stream, consumed, produced);
        if (token_.phrase_index > dictionary_entries_)
            return fail(core::ErrorCode::malformed_stream, consumed, produced);

        phrase_length_ = token_.phrase_index == 0
            ? 0 : dictionary_workspace_[token_.phrase_index - 1].length;
        std::uint64_t token_output_length = phrase_length_;
        if (token_.tag == Lz78TokenTag::pair) {
            if (!core::checked_add(
                    token_output_length, UINT64_C(1), token_output_length))
                return fail(core::ErrorCode::malformed_stream, consumed,
                            produced);
        } else if (token_.phrase_index == 0) {
            return fail(core::ErrorCode::malformed_stream, consumed, produced);
        }
        std::uint64_t next_output{};
        if (!core::checked_add(
                output_committed_, token_output_length, next_output)
            || next_output > declared_frame_size_
            || (token_.tag == Lz78TokenTag::final_index
                && next_output != declared_frame_size_))
            return fail(core::ErrorCode::malformed_stream, consumed, produced);

        if (token_.tag == Lz78TokenTag::pair
            && dictionary_entries_ < parameters_.maximum_entries) {
            dictionary_workspace_[dictionary_entries_] = {
                token_.phrase_index, token_.symbol, token_output_length};
            ++dictionary_entries_;
        }
        phrase_emitted_ = 0;
        symbol_pending_ = token_.tag == Lz78TokenTag::pair;
        state_ = State::draining_token;
    }
}

} // namespace marc::dictionary::internal

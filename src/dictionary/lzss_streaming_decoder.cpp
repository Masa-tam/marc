#include "dictionary/lzss_streaming_decoder.hpp"

#include <algorithm>
#include <cstring>

namespace marc::dictionary::internal {
namespace {

constexpr std::uint32_t known_flags =
    core::flag_value(core::ProcessFlags::flush)
    | core::flag_value(core::ProcessFlags::end_input)
    | core::flag_value(core::ProcessFlags::reset_block);

} // namespace

LzssStreamingDecoder::LzssStreamingDecoder(
    const LzssParameters parameters,
    const std::uint64_t declared_frame_size,
    const core::DecoderLimits limits,
    const std::span<std::byte> history_storage) noexcept
    : parameters_(parameters), limits_(limits),
      history_storage_(history_storage),
      declared_frame_size_(declared_frame_size) {
    if (core::validate_limits(limits_) != core::LimitError::none
        || validate_lzss_parameters(parameters_, limits_)
               != LzssFormatError::none) {
        state_ = State::error;
        terminal_error_ = {core::ErrorCode::invalid_argument, 0, 0};
    } else if (declared_frame_size_ > limits_.max_frame_size) {
        state_ = State::error;
        terminal_error_ = {core::ErrorCode::limit_exceeded, 0, 0};
    } else if (history_storage_.size()
               < std::min<std::uint64_t>(parameters_.window_size,
                                         declared_frame_size_)) {
        state_ = State::error;
        terminal_error_ = {core::ErrorCode::out_of_memory, 0, 0};
    } else if (declared_frame_size_ == 0) {
        state_ = State::awaiting_end;
    }
}

core::ProcessResult LzssStreamingDecoder::fail(
    const core::ErrorCode code, const std::size_t consumed,
    const std::size_t produced) noexcept {
    state_ = State::error;
    terminal_error_ = {code, input_position_, 0};
    return {consumed, produced, core::StreamStatus::error, terminal_error_};
}

core::ProcessResult LzssStreamingDecoder::process(
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
            while (match_copied_ < token_.length && produced < output.size()) {
                const auto source = output_committed_ - token_.distance;
                const auto value = history_storage_[static_cast<std::size_t>(
                    source % history_storage_.size())];
                output[produced] = value;
                history_storage_[static_cast<std::size_t>(
                    output_committed_ % history_storage_.size())] = value;
                ++match_copied_;
                ++output_committed_;
                ++produced;
            }
            if (match_copied_ != token_.length) {
                if (end_requested && consumed == input.size()) end_seen_ = true;
                return {consumed, produced, core::StreamStatus::need_output, {}};
            }
            if (literal_pending_ && produced < output.size()) {
                const auto value = static_cast<std::byte>(token_.literal);
                output[produced++] = value;
                history_storage_[static_cast<std::size_t>(
                    output_committed_ % history_storage_.size())] = value;
                ++output_committed_;
                literal_pending_ = false;
            }
            if (literal_pending_) {
                if (end_requested && consumed == input.size()) end_seen_ = true;
                return {consumed, produced, core::StreamStatus::need_output, {}};
            }
            token_collected_ = 0;
            token_required_ = 0;
            match_copied_ = 0;
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
                return {consumed, produced,
                        core::StreamStatus::end_of_stream, {}};
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

        if (token_collected_ == 0) {
            if (consumed == input.size()) {
                if (end_requested)
                    return fail(core::ErrorCode::malformed_stream, consumed,
                                produced);
                return {consumed, produced,
                        (consumed != 0 || produced != 0)
                            ? core::StreamStatus::progress
                            : core::StreamStatus::need_input,
                        {}};
            }
            token_bytes_[0] = input[consumed++];
            ++input_position_;
            token_collected_ = 1;
            const auto tag = std::to_integer<std::uint8_t>(token_bytes_[0]);
            if (tag > static_cast<std::uint8_t>(LzssTokenTag::match))
                return fail(core::ErrorCode::malformed_stream, consumed,
                            produced);
            token_required_ = tag == static_cast<std::uint8_t>(
                                       LzssTokenTag::literal)
                ? lzss_literal_size : lzss_match_size;
        }

        const auto needed = token_required_ - token_collected_;
        const auto count = std::min(needed, input.size() - consumed);
        if (count != 0) {
            std::memmove(token_bytes_.data() + token_collected_,
                         input.data() + consumed, count);
            token_collected_ += count;
            consumed += count;
            input_position_ += count;
        }
        if (token_collected_ != token_required_) {
            if (end_requested && consumed == input.size())
                return fail(core::ErrorCode::malformed_stream, consumed,
                            produced);
            return {consumed, produced,
                    (consumed != 0 || produced != 0)
                        ? core::StreamStatus::progress
                        : core::StreamStatus::need_input,
                    {}};
        }

        std::size_t parsed_size{};
        if (parse_lzss_token(
                std::span<const std::byte>{token_bytes_}.first(token_required_),
                token_, parsed_size) != LzssFormatError::none
            || parsed_size != token_required_)
            return fail(core::ErrorCode::malformed_stream, consumed, produced);
        std::uint64_t next_size{};
        if (validate_lzss_token(
                token_, parameters_,
                {output_committed_, declared_frame_size_}, limits_, next_size)
            != LzssFormatError::none)
            return fail(core::ErrorCode::malformed_stream, consumed, produced);
        match_copied_ = 0;
        literal_pending_ = token_.tag == LzssTokenTag::literal;
        state_ = State::draining_token;
    }
}

} // namespace marc::dictionary::internal

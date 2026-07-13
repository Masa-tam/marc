#include "dictionary/lz77_streaming_decoder.hpp"

#include <algorithm>
#include <cstring>

namespace marc::dictionary::internal {
namespace {

constexpr std::uint32_t known_flags =
    core::flag_value(core::ProcessFlags::flush)
    | core::flag_value(core::ProcessFlags::end_input)
    | core::flag_value(core::ProcessFlags::reset_block);

} // namespace

Lz77StreamingDecoder::Lz77StreamingDecoder(
    const Lz77Parameters parameters,
    const std::uint64_t declared_frame_size,
    const core::DecoderLimits limits,
    const std::span<std::byte> history_storage) noexcept
    : parameters_(parameters), limits_(limits),
      history_storage_(history_storage),
      declared_frame_size_(declared_frame_size) {
    if (core::validate_limits(limits_) != core::LimitError::none
        || validate_lz77_parameters(parameters_, limits_)
               != Lz77FormatError::none) {
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

core::ProcessResult Lz77StreamingDecoder::fail(
    const core::ErrorCode code, const std::size_t consumed,
    const std::size_t produced) noexcept {
    state_ = State::error;
    terminal_error_ = {code, input_position_, 0};
    return {consumed, produced, core::StreamStatus::error, terminal_error_};
}

core::ProcessResult Lz77StreamingDecoder::process(
    const std::span<const std::byte> input,
    const std::span<std::byte> output,
    const std::uint32_t flags) noexcept {
    if (state_ == State::error)
        return {0, 0, core::StreamStatus::error, terminal_error_};
    if (state_ == State::ended)
        return {0, 0, core::StreamStatus::end_of_stream, {}};
    if ((flags & ~known_flags) != 0
        || (flags & core::flag_value(core::ProcessFlags::reset_block)) != 0) {
        return fail(core::ErrorCode::unsupported, 0, 0);
    }
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
            || input.size() - consumed > serialized_limit - input_position_) {
            return fail(core::ErrorCode::limit_exceeded, consumed, produced);
        }
        const auto needed = lz77_token_size - token_collected_;
        const auto count = std::min(needed, input.size() - consumed);
        if (count != 0) {
            std::memmove(token_bytes_.data() + token_collected_,
                         input.data() + consumed, count);
            token_collected_ += count;
            consumed += count;
            input_position_ += count;
        }
        if (token_collected_ != lz77_token_size) {
            if (end_requested && consumed == input.size())
                return fail(core::ErrorCode::malformed_stream, consumed,
                            produced);
            return {consumed, produced,
                    (consumed != 0 || produced != 0)
                        ? core::StreamStatus::progress
                        : core::StreamStatus::need_input,
                    {}};
        }
        if (parse_lz77_token(token_bytes_, token_) != Lz77FormatError::none) {
            return fail(core::ErrorCode::malformed_stream, consumed, produced);
        }
        std::uint64_t next_size{};
        if (validate_lz77_token(
                token_, parameters_,
                {output_committed_, declared_frame_size_}, limits_, next_size)
            != Lz77FormatError::none) {
            return fail(core::ErrorCode::malformed_stream, consumed, produced);
        }
        match_copied_ = 0;
        literal_pending_ = token_.tag != Lz77TokenTag::terminal_match;
        state_ = State::draining_token;
    }
}

} // namespace marc::dictionary::internal

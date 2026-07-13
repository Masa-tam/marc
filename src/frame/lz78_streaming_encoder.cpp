#include "frame/lz78_streaming_encoder.hpp"

#include "core/checked_math.hpp"

#include <algorithm>
#include <cstring>

namespace marc::frame {
namespace {

constexpr std::uint32_t known_flags =
    core::flag_value(core::ProcessFlags::flush)
    | core::flag_value(core::ProcessFlags::end_input)
    | core::flag_value(core::ProcessFlags::reset_block);

[[nodiscard]] bool supported_pipeline(const StreamHeader& stream) noexcept {
    return stream.dictionary_algorithm == DictionaryAlgorithm::lz78
        && stream.dictionary_variant == 1
        && stream.entropy_algorithm == EntropyAlgorithm::none
        && stream.entropy_variant == 0
        && stream.entropy_block_size == 0
        && stream.dictionary_parameters_size
               == dictionary::internal::lz78_parameter_size
        && stream.entropy_parameters_size == 0;
}

} // namespace

Lz78FrameStreamingEncoder::Lz78FrameStreamingEncoder(
    const StreamHeader stream,
    const dictionary::internal::Lz78Parameters parameters,
    const core::DecoderLimits limits,
    const std::span<std::byte> frame_input_storage,
    const std::span<std::byte> frame_encoded_storage,
    const std::span<dictionary::internal::Lz78EncoderEntry>
        dictionary_workspace) noexcept
    : stream_(stream), parameters_(parameters), limits_(limits),
      frame_input_storage_(frame_input_storage),
      frame_encoded_storage_(frame_encoded_storage),
      dictionary_workspace_(dictionary_workspace) {
    const auto frame_workspace = static_cast<std::size_t>(
        std::min<std::uint64_t>(stream_.original_size, stream_.frame_size));
    const auto dictionary_entries =
        dictionary::internal::lz78_encoder_workspace_entries(
            frame_workspace, parameters_);
    const std::span<std::byte, stream_header_size> header_output{
        prefix_.data(), stream_header_size};
    const std::span<std::byte, dictionary::internal::lz78_parameter_size>
        parameter_output{prefix_.data() + stream_header_size,
                         dictionary::internal::lz78_parameter_size};
    if (validate_stream_header(stream_, limits_) != StreamHeaderError::none
        || !supported_pipeline(stream_)
        || dictionary::internal::validate_lz78_parameters(parameters_, limits_)
               != dictionary::internal::Lz78FormatError::none
        || serialize_stream_header(stream_, limits_, header_output)
               != StreamHeaderError::none
        || dictionary::internal::serialize_lz78_parameters(
               parameters_, limits_, parameter_output)
               != dictionary::internal::Lz78FormatError::none) {
        state_ = State::error;
        terminal_error_ = {core::ErrorCode::invalid_argument, 0, 0};
    } else if (frame_input_storage_.size() < frame_workspace
               || dictionary_workspace_.size() < dictionary_entries) {
        state_ = State::error;
        terminal_error_ = {core::ErrorCode::out_of_memory, 0, 0};
    }
}

core::ProcessResult Lz78FrameStreamingEncoder::fail(
    const core::ErrorCode code, const std::size_t consumed,
    const std::size_t produced) noexcept {
    state_ = State::error;
    terminal_error_ = {code, input_received_, 0};
    return {consumed, produced, core::StreamStatus::error, terminal_error_};
}

bool Lz78FrameStreamingEncoder::prepare_frame() noexcept {
    preparation_error_ = core::ErrorCode::internal_error;
    const auto plan = plan_lz78_frame(
        stream_, parameters_, limits_, frame_sequence_, input_committed_,
        frame_input_storage_.first(frame_input_size_), dictionary_workspace_);
    if (plan.error != Lz78FrameCodecError::none) {
        if (plan.error == Lz78FrameCodecError::input_size_mismatch)
            preparation_error_ = core::ErrorCode::invalid_argument;
        else if (plan.encode_error
                     == dictionary::internal::Lz78EncodeError::input_limit_exceeded
                 || plan.encode_error
                     == dictionary::internal::Lz78EncodeError::serialized_limit_exceeded
                 || plan.encode_error
                     == dictionary::internal::Lz78EncodeError::workspace_limit_exceeded)
            preparation_error_ = core::ErrorCode::limit_exceeded;
        else if (plan.encode_error
                 == dictionary::internal::Lz78EncodeError::workspace_too_small)
            preparation_error_ = core::ErrorCode::out_of_memory;
        return false;
    }
    const auto dictionary_entries =
        dictionary::internal::lz78_encoder_workspace_entries(
            frame_input_size_, parameters_);
    std::uint64_t dictionary_bytes{};
    if (!core::checked_multiply(
            static_cast<std::uint64_t>(dictionary_entries),
            static_cast<std::uint64_t>(
                sizeof(dictionary::internal::Lz78EncoderEntry)),
            dictionary_bytes)) {
        preparation_error_ = core::ErrorCode::limit_exceeded;
        return false;
    }
    std::uint64_t buffered_bytes{};
    if (!core::checked_add(static_cast<std::uint64_t>(frame_input_size_),
                           static_cast<std::uint64_t>(plan.serialized_size),
                           buffered_bytes)
        || !core::checked_add(buffered_bytes, dictionary_bytes, buffered_bytes)
        || buffered_bytes > limits_.max_internal_buffered_bytes) {
        preparation_error_ = core::ErrorCode::limit_exceeded;
        return false;
    }
    if (frame_encoded_storage_.size() < plan.serialized_size) {
        preparation_error_ = core::ErrorCode::out_of_memory;
        return false;
    }
    const auto encoded = encode_lz78_frame(
        stream_, parameters_, limits_, frame_sequence_, input_committed_,
        frame_input_storage_.first(frame_input_size_), dictionary_workspace_,
        frame_encoded_storage_.first(plan.serialized_size));
    if (encoded.error != Lz78FrameCodecError::none) return false;
    pending_size_ = plan.serialized_size;
    pending_offset_ = 0;
    input_committed_ += frame_input_size_;
    ++frame_sequence_;
    frame_input_size_ = 0;
    state_ = State::draining_frame;
    return true;
}

core::ProcessResult Lz78FrameStreamingEncoder::process(
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
    if (input.size() > stream_.original_size - input_received_)
        return fail(core::ErrorCode::invalid_argument, 0, 0);
    const bool end_requested =
        (flags & core::flag_value(core::ProcessFlags::end_input)) != 0;
    if (end_requested
        && input.size() != stream_.original_size - input_received_)
        return fail(core::ErrorCode::invalid_argument, 0, 0);

    std::size_t consumed{};
    std::size_t produced{};
    while (true) {
        if (state_ == State::draining_prefix
            || state_ == State::draining_frame) {
            const auto remaining = pending_size_ - pending_offset_;
            const auto count = std::min(remaining, output.size() - produced);
            if (count != 0) {
                const auto* source = state_ == State::draining_prefix
                    ? prefix_.data() : frame_encoded_storage_.data();
                std::memmove(output.data() + produced,
                             source + pending_offset_, count);
                pending_offset_ += count;
                produced += count;
            }
            if (pending_offset_ != pending_size_)
                return {consumed, produced, core::StreamStatus::need_output, {}};
            pending_offset_ = 0;
            pending_size_ = 0;
            if (state_ == State::draining_prefix) {
                state_ = stream_.original_size == 0
                    ? State::awaiting_end : State::collecting;
            } else if (input_committed_ == stream_.original_size) {
                state_ = end_seen_ ? State::ended : State::awaiting_end;
            } else {
                state_ = State::collecting;
            }
            if (state_ == State::ended)
                return {consumed, produced,
                        core::StreamStatus::end_of_stream, {}};
            continue;
        }
        if (state_ == State::awaiting_end) {
            if (consumed != input.size())
                return fail(core::ErrorCode::invalid_argument, consumed,
                            produced);
            if (end_requested) {
                end_seen_ = true;
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
        const auto remaining_stream = stream_.original_size - input_committed_;
        const auto expected_frame = static_cast<std::size_t>(
            std::min<std::uint64_t>(stream_.frame_size, remaining_stream));
        const auto count = std::min(expected_frame - frame_input_size_,
                                    input.size() - consumed);
        if (count != 0) {
            std::memmove(frame_input_storage_.data() + frame_input_size_,
                         input.data() + consumed, count);
            frame_input_size_ += count;
            input_received_ += count;
            consumed += count;
        }
        if (end_requested && consumed == input.size()) end_seen_ = true;
        if (frame_input_size_ == expected_frame) {
            if (!prepare_frame())
                return fail(preparation_error_, consumed, produced);
            continue;
        }
        return {consumed, produced,
                (consumed != 0 || produced != 0)
                    ? core::StreamStatus::progress
                    : core::StreamStatus::need_input,
                {}};
    }
}

} // namespace marc::frame

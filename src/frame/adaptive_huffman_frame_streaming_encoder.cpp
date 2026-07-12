#include "frame/adaptive_huffman_frame_streaming_encoder.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace marc::frame {
namespace {

constexpr std::uint32_t known_flags =
    core::flag_value(core::ProcessFlags::flush)
    | core::flag_value(core::ProcessFlags::end_input)
    | core::flag_value(core::ProcessFlags::reset_block);

} // namespace

AdaptiveHuffmanFrameStreamingEncoder::AdaptiveHuffmanFrameStreamingEncoder(
    const StreamHeader stream, const core::DecoderLimits limits,
    const std::span<std::byte> frame_input_storage,
    const std::span<std::byte> frame_encoded_storage) noexcept
    : stream_(stream), limits_(limits),
      frame_input_storage_(frame_input_storage),
      frame_encoded_storage_(frame_encoded_storage) {
    if (validate_stream_header(stream_, limits_) != StreamHeaderError::none
        || stream_.dictionary_algorithm != DictionaryAlgorithm::none
        || stream_.dictionary_variant != 0
        || stream_.entropy_algorithm != EntropyAlgorithm::adaptive_huffman
        || stream_.entropy_variant != 1
        || stream_.entropy_block_size != 0
        || stream_.dictionary_parameters_size != 0
        || stream_.entropy_parameters_size != 0
        || frame_input_storage_.size() < std::min<std::uint64_t>(
            stream_.original_size, stream_.frame_size)
        || serialize_stream_header(stream_, limits_, stream_header_)
            != StreamHeaderError::none) {
        state_ = State::error;
        terminal_error_ = {core::ErrorCode::invalid_argument, 0, 0};
    }
}

core::ProcessResult AdaptiveHuffmanFrameStreamingEncoder::fail(
    const core::ErrorCode code, const std::size_t input_consumed,
    const std::size_t output_produced) noexcept {
    state_ = State::error;
    terminal_error_ = {code, input_received_, 0};
    return {input_consumed, output_produced,
            core::StreamStatus::error, terminal_error_};
}

bool AdaptiveHuffmanFrameStreamingEncoder::prepare_frame() noexcept {
    preparation_error_ = core::ErrorCode::internal_error;
    const auto plan = plan_adaptive_huffman_frame(
        stream_, limits_, frame_sequence_, input_committed_,
        frame_input_storage_.first(frame_input_size_));
    if (plan.error != AdaptiveHuffmanFrameCodecError::none) {
        if (plan.encode_error
            == entropy::internal::AdaptiveHuffmanEncodeError::limit_exceeded) {
            preparation_error_ = core::ErrorCode::limit_exceeded;
        } else if (plan.error
                   == AdaptiveHuffmanFrameCodecError::input_size_mismatch) {
            preparation_error_ = core::ErrorCode::invalid_argument;
        }
        return false;
    }
    if (frame_encoded_storage_.size() < plan.serialized_size) {
        preparation_error_ = core::ErrorCode::out_of_memory;
        return false;
    }
    const auto encoded = encode_adaptive_huffman_frame(
        stream_, limits_, frame_sequence_, input_committed_,
        frame_input_storage_.first(frame_input_size_),
        frame_encoded_storage_.first(plan.serialized_size));
    if (encoded.error != AdaptiveHuffmanFrameCodecError::none) return false;
    pending_size_ = plan.serialized_size;
    pending_offset_ = 0;
    input_committed_ += frame_input_size_;
    ++frame_sequence_;
    frame_input_size_ = 0;
    state_ = State::draining_frame;
    return true;
}

core::ProcessResult AdaptiveHuffmanFrameStreamingEncoder::process(
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
        if (state_ == State::draining_header
            || state_ == State::draining_frame) {
            const auto remaining = pending_size_ - pending_offset_;
            const auto count = std::min(remaining, output.size() - produced);
            if (count != 0) {
                const auto* source = state_ == State::draining_header
                    ? stream_header_.data() : frame_encoded_storage_.data();
                std::memmove(output.data() + produced,
                             source + pending_offset_, count);
                pending_offset_ += count;
                produced += count;
            }
            if (pending_offset_ != pending_size_)
                return {consumed, produced, core::StreamStatus::need_output, {}};
            pending_offset_ = 0;
            pending_size_ = 0;
            if (state_ == State::draining_header) {
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
                return fail(core::ErrorCode::invalid_argument,
                            consumed, produced);
            if (end_requested) {
                end_seen_ = true;
                state_ = State::ended;
                return {consumed, produced,
                        core::StreamStatus::end_of_stream, {}};
            }
            return {consumed, produced,
                    (consumed != 0 || produced != 0)
                        ? core::StreamStatus::progress
                        : core::StreamStatus::need_input, {}};
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
                    : core::StreamStatus::need_input, {}};
    }
}

} // namespace marc::frame

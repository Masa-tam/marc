#include "frame/checksum_raw_streaming_encoder.hpp"

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
    return stream.dictionary_algorithm == DictionaryAlgorithm::none
        && stream.dictionary_variant == 0
        && stream.entropy_algorithm == EntropyAlgorithm::none
        && stream.entropy_variant == 0
        && stream.entropy_block_size == 0
        && stream.dictionary_parameters_size == 0
        && stream.entropy_parameters_size == 0
        && stream.hash_descriptors_size == hash_descriptor_size
        && stream.header_extension_size == 0;
}

[[nodiscard]] FrameHeader make_frame_header(
    const std::uint64_t sequence, const std::size_t size) noexcept {
    FrameHeader header{};
    header.sequence = sequence;
    header.uncompressed_size = static_cast<std::uint32_t>(size);
    header.dictionary_serialized_size = static_cast<std::uint32_t>(size);
    header.compressed_payload_size = static_cast<std::uint32_t>(size);
    header.checksum_trailer_size = frame_checksum_trailer_size;
    return header;
}

} // namespace

ChecksumRawStreamingEncoder::ChecksumRawStreamingEncoder(
    const StreamHeader stream, const HashDescriptor descriptor,
    const core::DecoderLimits limits,
    const std::span<std::byte> serialized_frame_workspace) noexcept
    : stream_(stream), descriptors_{descriptor}, limits_(limits),
      workspace_(serialized_frame_workspace) {
    const std::span<std::byte, stream_header_size> header_output{
        prefix_.data(), stream_header_size};
    const auto descriptor_output = std::span<std::byte>{prefix_}.subspan(
        stream_header_size, hash_descriptor_size);
    if (validate_stream_header_v1_1(stream_, limits_)
            != StreamHeaderError::none
        || !supported_pipeline(stream_)
        || validate_hash_descriptor_region(descriptors_)
               != HashDescriptorRegionError::none
        || validate_frame_checksum_profile_v1_1(
               descriptors_, frame_checksum_trailer_size)
               != FrameChecksumError::none
        || serialize_stream_header_v1_1(stream_, limits_, header_output)
               != StreamHeaderError::none
        || serialize_hash_descriptor_region(descriptors_, descriptor_output)
               != HashDescriptorRegionError::none) {
        state_ = State::error;
        terminal_error_ = {core::ErrorCode::invalid_argument, 0, 0};
    }
}

core::ProcessResult ChecksumRawStreamingEncoder::fail(
    const core::ErrorCode code, const std::size_t consumed,
    const std::size_t produced) noexcept {
    state_ = State::error;
    terminal_error_ = {code, input_received_, 0};
    return {consumed, produced, core::StreamStatus::error, terminal_error_};
}

bool ChecksumRawStreamingEncoder::prepare_frame() noexcept {
    preparation_error_ = core::ErrorCode::internal_error;
    std::size_t serialized_size{};
    if (!core::checked_add(frame_header_size, frame_input_size_,
                           serialized_size)
        || !core::checked_add(serialized_size, frame_checksum_trailer_size,
                              serialized_size)) {
        preparation_error_ = core::ErrorCode::limit_exceeded;
        return false;
    }
    if (serialized_size > workspace_.size()) {
        preparation_error_ = core::ErrorCode::out_of_memory;
        return false;
    }
    if (serialized_size > limits_.max_internal_buffered_bytes) {
        preparation_error_ = core::ErrorCode::limit_exceeded;
        return false;
    }

    const auto header = make_frame_header(frame_sequence_, frame_input_size_);
    const FrameValidationContext context{
        stream_, limits_, frame_sequence_, input_committed_, descriptors_};
    const std::span<std::byte, frame_header_size> header_output{
        workspace_.data(), frame_header_size};
    if (serialize_frame_header_v1_1(header, context, header_output)
            != FrameHeaderError::none
        || generate_frame_checksum_v1_1(
               workspace_.subspan(frame_header_size, frame_input_size_),
               descriptors_, frame_checksum_trailer_size,
               workspace_.subspan(frame_header_size + frame_input_size_,
                                  frame_checksum_trailer_size))
               != FrameChecksumError::none) {
        return false;
    }

    pending_size_ = serialized_size;
    pending_offset_ = 0;
    input_committed_ += frame_input_size_;
    ++frame_sequence_;
    frame_input_size_ = 0;
    state_ = State::draining_frame;
    return true;
}

core::ProcessResult ChecksumRawStreamingEncoder::process(
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
    if (input_received_ > stream_.original_size
        || input.size() > stream_.original_size - input_received_) {
        return fail(core::ErrorCode::invalid_argument, 0, 0);
    }
    const bool end_requested =
        (flags & core::flag_value(core::ProcessFlags::end_input)) != 0;
    if (end_requested
        && input.size() != stream_.original_size - input_received_) {
        return fail(core::ErrorCode::invalid_argument, 0, 0);
    }

    std::size_t consumed{};
    std::size_t produced{};
    while (true) {
        if (state_ == State::draining_prefix
            || state_ == State::draining_frame) {
            const auto remaining = pending_size_ - pending_offset_;
            const auto count = std::min(remaining, output.size() - produced);
            if (count != 0) {
                const auto* source = state_ == State::draining_prefix
                    ? prefix_.data() : workspace_.data();
                std::memmove(output.data() + produced,
                             source + pending_offset_, count);
                pending_offset_ += count;
                produced += count;
            }
            if (pending_offset_ != pending_size_) {
                return {consumed, produced, core::StreamStatus::need_output,
                        {}};
            }
            pending_offset_ = 0;
            pending_size_ = 0;
            if (state_ == State::draining_prefix) {
                state_ = stream_.original_size == 0
                    ? State::awaiting_end : State::collecting_frame;
            } else if (input_committed_ == stream_.original_size) {
                state_ = end_seen_ ? State::ended : State::awaiting_end;
            } else {
                state_ = State::collecting_frame;
            }
            if (state_ == State::ended) {
                return {consumed, produced,
                        core::StreamStatus::end_of_stream, {}};
            }
            continue;
        }
        if (state_ == State::awaiting_end) {
            if (consumed != input.size()) {
                return fail(core::ErrorCode::invalid_argument, consumed,
                            produced);
            }
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
        if (workspace_.size() < frame_header_size
                + frame_checksum_trailer_size
            || expected_frame > workspace_.size()
                - frame_header_size - frame_checksum_trailer_size) {
            return fail(core::ErrorCode::out_of_memory, consumed, produced);
        }
        const auto count = std::min(expected_frame - frame_input_size_,
                                    input.size() - consumed);
        if (count != 0) {
            std::memmove(workspace_.data() + frame_header_size
                             + frame_input_size_,
                         input.data() + consumed, count);
            frame_input_size_ += count;
            input_received_ += count;
            consumed += count;
        }
        if (end_requested && consumed == input.size()) end_seen_ = true;
        if (frame_input_size_ == expected_frame) {
            if (!prepare_frame()) {
                return fail(preparation_error_, consumed, produced);
            }
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

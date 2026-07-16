#include "frame/checksum_raw_streaming_decoder.hpp"

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

} // namespace

ChecksumRawStreamingDecoder::ChecksumRawStreamingDecoder(
    const core::DecoderLimits limits,
    const std::span<std::byte> serialized_frame_workspace) noexcept
    : limits_(limits), workspace_(serialized_frame_workspace) {
    if (core::validate_limits(limits_) != core::LimitError::none) {
        state_ = State::error;
        terminal_error_ = {core::ErrorCode::invalid_argument, 0, 0};
    }
}

core::ProcessResult ChecksumRawStreamingDecoder::fail(
    const core::ErrorCode code, const std::size_t consumed,
    const std::size_t produced) noexcept {
    state_ = State::error;
    terminal_error_ = {code, input_position_, 0};
    return {consumed, produced, core::StreamStatus::error, terminal_error_};
}

bool ChecksumRawStreamingDecoder::parse_collected_prefix() noexcept {
    const std::span<const std::byte, stream_header_size> header{
        prefix_.data(), stream_header_size};
    StreamHeader parsed{};
    if (parse_stream_header_v1_1(header, limits_, parsed)
            != StreamHeaderError::none
        || !supported_pipeline(parsed)) {
        return false;
    }
    std::array<HashDescriptor, 1> parsed_descriptors{};
    std::size_t descriptor_count{};
    if (parse_hash_descriptor_region(
            std::span<const std::byte>{prefix_}.subspan(
                stream_header_size, hash_descriptor_size),
            parsed_descriptors, descriptor_count)
            != HashDescriptorRegionError::none
        || descriptor_count != 1
        || validate_frame_checksum_profile_v1_1(
               parsed_descriptors, frame_checksum_trailer_size)
               != FrameChecksumError::none) {
        return false;
    }
    stream_ = parsed;
    descriptors_ = parsed_descriptors;
    return true;
}

bool ChecksumRawStreamingDecoder::parse_collected_frame_header() noexcept {
    preparation_error_ = core::ErrorCode::malformed_stream;
    const std::span<const std::byte, frame_header_size> header_bytes{
        workspace_.data(), frame_header_size};
    const FrameValidationContext context{
        stream_, limits_, frame_sequence_, output_committed_, descriptors_};
    if (parse_frame_header_v1_1(header_bytes, context, frame_)
        != FrameHeaderError::none) {
        return false;
    }

    std::size_t size{};
    if (!core::checked_add(frame_header_size,
                           static_cast<std::size_t>(
                               frame_.compressed_payload_size),
                           size)
        || !core::checked_add(size,
                              static_cast<std::size_t>(
                                  frame_.checksum_trailer_size),
                              size)) {
        preparation_error_ = core::ErrorCode::limit_exceeded;
        return false;
    }
    if (size > workspace_.size()) {
        preparation_error_ = core::ErrorCode::out_of_memory;
        return false;
    }
    if (size > limits_.max_internal_buffered_bytes) {
        preparation_error_ = core::ErrorCode::limit_exceeded;
        return false;
    }
    frame_serialized_size_ = size;
    return true;
}

bool ChecksumRawStreamingDecoder::verify_collected_frame() noexcept {
    const auto payload = std::span<const std::byte>{workspace_}.subspan(
        frame_header_size, frame_.compressed_payload_size);
    const auto trailer = std::span<const std::byte>{workspace_}.subspan(
        frame_header_size + frame_.compressed_payload_size,
        frame_.checksum_trailer_size);
    if (verify_frame_checksum_v1_1(
            payload, descriptors_, frame_.checksum_trailer_size, trailer)
        != FrameChecksumError::none) {
        return false;
    }
    output_offset_ = 0;
    output_committed_ += frame_.uncompressed_size;
    ++frame_sequence_;
    state_ = State::draining_frame;
    return true;
}

core::ProcessResult ChecksumRawStreamingDecoder::process(
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
        if (state_ == State::draining_frame) {
            const auto remaining = static_cast<std::size_t>(
                frame_.uncompressed_size) - output_offset_;
            const auto count = std::min(remaining, output.size() - produced);
            if (count != 0) {
                std::memmove(output.data() + produced,
                             workspace_.data() + frame_header_size
                                 + output_offset_,
                             count);
                output_offset_ += count;
                produced += count;
            }
            if (output_offset_ != frame_.uncompressed_size) {
                return {consumed, produced, core::StreamStatus::need_output,
                        {}};
            }
            output_offset_ = 0;
            collected_ = 0;
            frame_serialized_size_ = 0;
            state_ = output_committed_ == stream_.original_size
                ? State::awaiting_end : State::collecting_frame_header;
            if (state_ == State::awaiting_end && end_seen_) {
                state_ = State::ended;
                return {consumed, produced,
                        core::StreamStatus::end_of_stream, {}};
            }
            continue;
        }
        if (state_ == State::awaiting_end) {
            if (consumed != input.size()) {
                return fail(core::ErrorCode::malformed_stream, consumed,
                            produced);
            }
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
        if (state_ == State::collecting_prefix) {
            const auto needed = checksum_raw_stream_prefix_size - collected_;
            const auto count = std::min(needed, input.size() - consumed);
            if (count != 0) {
                std::memmove(prefix_.data() + collected_,
                             input.data() + consumed, count);
                collected_ += count;
                consumed += count;
                input_position_ += count;
            }
            if (collected_ != checksum_raw_stream_prefix_size) {
                if (end_requested && consumed == input.size()) {
                    return fail(core::ErrorCode::malformed_stream, consumed,
                                produced);
                }
                return {consumed, produced,
                        consumed != 0 ? core::StreamStatus::progress
                                      : core::StreamStatus::need_input,
                        {}};
            }
            if (!parse_collected_prefix()) {
                return fail(core::ErrorCode::malformed_stream, consumed,
                            produced);
            }
            collected_ = 0;
            state_ = stream_.original_size == 0
                ? State::awaiting_end : State::collecting_frame_header;
            if (state_ == State::awaiting_end && consumed != input.size()) {
                return fail(core::ErrorCode::malformed_stream, consumed,
                            produced);
            }
            if (state_ == State::awaiting_end && end_requested) end_seen_ = true;
            continue;
        }
        if (state_ == State::collecting_frame_header) {
            if (workspace_.size() < frame_header_size) {
                return fail(core::ErrorCode::out_of_memory, consumed,
                            produced);
            }
            const auto needed = frame_header_size - collected_;
            const auto count = std::min(needed, input.size() - consumed);
            if (count != 0) {
                std::memmove(workspace_.data() + collected_,
                             input.data() + consumed, count);
                collected_ += count;
                consumed += count;
                input_position_ += count;
            }
            if (collected_ != frame_header_size) {
                if (end_requested && consumed == input.size()) {
                    return fail(core::ErrorCode::malformed_stream, consumed,
                                produced);
                }
                return {consumed, produced,
                        (consumed != 0 || produced != 0)
                            ? core::StreamStatus::progress
                            : core::StreamStatus::need_input,
                        {}};
            }
            if (!parse_collected_frame_header()) {
                return fail(preparation_error_, consumed, produced);
            }
            state_ = State::collecting_frame_body;
            continue;
        }

        const auto needed = frame_serialized_size_ - collected_;
        const auto count = std::min(needed, input.size() - consumed);
        if (count != 0) {
            std::memmove(workspace_.data() + collected_,
                         input.data() + consumed, count);
            collected_ += count;
            consumed += count;
            input_position_ += count;
        }
        if (collected_ != frame_serialized_size_) {
            if (end_requested && consumed == input.size()) {
                return fail(core::ErrorCode::malformed_stream, consumed,
                            produced);
            }
            return {consumed, produced,
                    (consumed != 0 || produced != 0)
                        ? core::StreamStatus::progress
                        : core::StreamStatus::need_input,
                    {}};
        }
        const bool final_frame = output_committed_ + frame_.uncompressed_size
            == stream_.original_size;
        if (final_frame && consumed != input.size()) {
            return fail(core::ErrorCode::malformed_stream, consumed, produced);
        }
        if (!verify_collected_frame()) {
            return fail(core::ErrorCode::malformed_stream, consumed, produced);
        }
        if (final_frame && end_requested && consumed == input.size()) {
            end_seen_ = true;
        }
    }
}

} // namespace marc::frame

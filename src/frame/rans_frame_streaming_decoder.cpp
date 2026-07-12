#include "frame/rans_frame_streaming_decoder.hpp"

#include "core/checked_math.hpp"

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

RansFrameStreamingDecoder::RansFrameStreamingDecoder(
    const core::DecoderLimits limits,
    const std::span<std::byte> frame_encoded_storage,
    const std::span<std::byte> frame_decoded_storage,
    const std::span<entropy::internal::RansBlockView> frame_views)
    noexcept
    : limits_(limits),
      frame_encoded_storage_(frame_encoded_storage),
      frame_decoded_storage_(frame_decoded_storage),
      frame_views_(frame_views) {
    if (core::validate_limits(limits_) != core::LimitError::none) {
        state_ = State::error;
        terminal_error_ = {core::ErrorCode::invalid_argument, 0, 0};
    }
}

core::ProcessResult RansFrameStreamingDecoder::fail(
    const core::ErrorCode code,
    const std::size_t input_consumed,
    const std::size_t output_produced) noexcept {
    state_ = State::error;
    terminal_error_ = {code, input_position_, 0};
    return {input_consumed, output_produced,
            core::StreamStatus::error, terminal_error_};
}

bool RansFrameStreamingDecoder::parse_collected_stream_header()
    noexcept {
    if (parse_stream_header(stream_header_bytes_, limits_, stream_)
        != StreamHeaderError::none) {
        return false;
    }
    return stream_.dictionary_algorithm == DictionaryAlgorithm::none
        && stream_.dictionary_variant == 0
        && stream_.entropy_algorithm == EntropyAlgorithm::rans
        && stream_.entropy_variant == 1
        && stream_.dictionary_parameters_size == 0
        && stream_.entropy_parameters_size == 0;
}

bool RansFrameStreamingDecoder::parse_collected_frame_header()
    noexcept {
    preparation_error_ = core::ErrorCode::malformed_stream;
    const FrameValidationContext context{
        stream_, limits_, frame_sequence_, output_committed_};
    if (parse_frame_header(frame_header_bytes_, context, frame_)
        != FrameHeaderError::none) {
        return false;
    }
    if (frame_.entropy_block_count > frame_views_.size()
        || frame_.dictionary_serialized_size > frame_decoded_storage_.size()) {
        preparation_error_ = core::ErrorCode::out_of_memory;
        return false;
    }
    std::size_t size{};
    if (!core::checked_add(
            frame_header_size,
            static_cast<std::size_t>(frame_.block_descriptors_size), size)
        || !core::checked_add(
            size, static_cast<std::size_t>(frame_.compressed_payload_size),
            size)
        || size > frame_encoded_storage_.size()) {
        preparation_error_ = core::ErrorCode::out_of_memory;
        return false;
    }
    frame_serialized_size_ = size;
    std::memmove(frame_encoded_storage_.data(), frame_header_bytes_.data(),
                 frame_header_size);
    frame_collected_ = frame_header_size;
    return true;
}

bool RansFrameStreamingDecoder::decode_collected_frame() noexcept {
    const auto decoded = decode_rans_frame(
        stream_, limits_, frame_sequence_, output_committed_,
        frame_encoded_storage_.first(frame_serialized_size_),
        frame_decoded_storage_.first(frame_.dictionary_serialized_size),
        frame_views_);
    if (decoded.error != RansFrameCodecError::none) {
        return false;
    }
    decoded_size_ = frame_.dictionary_serialized_size;
    output_offset_ = 0;
    output_committed_ += decoded_size_;
    ++frame_sequence_;
    header_collected_ = 0;
    frame_collected_ = 0;
    state_ = State::draining_frame;
    return true;
}

core::ProcessResult RansFrameStreamingDecoder::process(
    const std::span<const std::byte> input,
    const std::span<std::byte> output,
    const std::uint32_t flags) noexcept {
    if (state_ == State::error) {
        return {0, 0, core::StreamStatus::error, terminal_error_};
    }
    if (state_ == State::ended) {
        return {0, 0, core::StreamStatus::end_of_stream, {}};
    }
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
            const auto remaining = decoded_size_ - output_offset_;
            const auto count = std::min(remaining, output.size() - produced);
            if (count != 0) {
                std::memmove(output.data() + produced,
                             frame_decoded_storage_.data() + output_offset_,
                             count);
                output_offset_ += count;
                produced += count;
            }
            if (output_offset_ != decoded_size_) {
                return {consumed, produced,
                        core::StreamStatus::need_output, {}};
            }
            decoded_size_ = 0;
            output_offset_ = 0;
            state_ = output_committed_ == stream_.original_size
                ? State::awaiting_end
                : State::collecting_frame_header;
            continue;
        }

        if (state_ == State::awaiting_end) {
            if (consumed != input.size()) {
                return fail(core::ErrorCode::malformed_stream,
                            consumed, produced);
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

        if (state_ == State::collecting_stream_header) {
            const auto needed = stream_header_size - header_collected_;
            const auto count = std::min(needed, input.size() - consumed);
            if (count != 0) {
                std::memmove(stream_header_bytes_.data() + header_collected_,
                             input.data() + consumed, count);
                header_collected_ += count;
                consumed += count;
                input_position_ += count;
            }
            if (header_collected_ != stream_header_size) {
                if (end_requested && consumed == input.size()) {
                    return fail(core::ErrorCode::malformed_stream,
                                consumed, produced);
                }
                return {consumed, produced,
                        consumed != 0 ? core::StreamStatus::progress
                                      : core::StreamStatus::need_input,
                        {}};
            }
            if (!parse_collected_stream_header()) {
                return fail(core::ErrorCode::malformed_stream,
                            consumed, produced);
            }
            header_collected_ = 0;
            state_ = stream_.original_size == 0
                ? State::awaiting_end
                : State::collecting_frame_header;
            if (state_ == State::awaiting_end
                && consumed != input.size()) {
                return fail(core::ErrorCode::malformed_stream,
                            consumed, produced);
            }
            if (state_ == State::awaiting_end && end_requested) {
                end_seen_ = true;
            }
            continue;
        }

        if (state_ == State::collecting_frame_header) {
            const auto needed = frame_header_size - header_collected_;
            const auto count = std::min(needed, input.size() - consumed);
            if (count != 0) {
                std::memmove(frame_header_bytes_.data() + header_collected_,
                             input.data() + consumed, count);
                header_collected_ += count;
                consumed += count;
                input_position_ += count;
            }
            if (header_collected_ != frame_header_size) {
                if (end_requested && consumed == input.size()) {
                    return fail(core::ErrorCode::malformed_stream,
                                consumed, produced);
                }
                return {consumed, produced,
                        (consumed != 0 || produced != 0)
                            ? core::StreamStatus::progress
                            : core::StreamStatus::need_input,
                        {}};
            }
            if (!parse_collected_frame_header()) {
                return fail(preparation_error_,
                            consumed, produced);
            }
            state_ = State::collecting_frame_body;
            continue;
        }

        const auto needed = frame_serialized_size_ - frame_collected_;
        const auto count = std::min(needed, input.size() - consumed);
        if (count != 0) {
            std::memmove(frame_encoded_storage_.data() + frame_collected_,
                         input.data() + consumed, count);
            frame_collected_ += count;
            consumed += count;
            input_position_ += count;
        }
        if (frame_collected_ != frame_serialized_size_) {
            if (end_requested && consumed == input.size()) {
                return fail(core::ErrorCode::malformed_stream,
                            consumed, produced);
            }
            return {consumed, produced,
                    (consumed != 0 || produced != 0)
                        ? core::StreamStatus::progress
                        : core::StreamStatus::need_input,
                    {}};
        }
        const bool final_frame = output_committed_
            + frame_.dictionary_serialized_size == stream_.original_size;
        if (final_frame && consumed != input.size()) {
            return fail(core::ErrorCode::malformed_stream,
                        consumed, produced);
        }
        if (!decode_collected_frame()) {
            return fail(core::ErrorCode::malformed_stream,
                        consumed, produced);
        }
        if (final_frame && end_requested && consumed == input.size()) {
            end_seen_ = true;
        }
    }
}

} // namespace marc::frame

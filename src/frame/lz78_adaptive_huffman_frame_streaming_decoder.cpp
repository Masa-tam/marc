#include "frame/lz78_adaptive_huffman_frame_streaming_decoder.hpp"

#include "core/checked_math.hpp"

#include <algorithm>
#include <cstring>

namespace marc::frame {
namespace {

constexpr std::uint32_t known_flags =
    core::flag_value(core::ProcessFlags::flush)
    | core::flag_value(core::ProcessFlags::end_input)
    | core::flag_value(core::ProcessFlags::reset_block);

} // namespace

Lz78AdaptiveHuffmanFrameStreamingDecoder::
Lz78AdaptiveHuffmanFrameStreamingDecoder(
    const core::DecoderLimits limits,
    const std::span<std::byte> frame_encoded_storage,
    const std::span<std::byte> dictionary_staging,
    const std::span<std::byte> frame_decoded_storage,
    const std::span<dictionary::internal::Lz78PhraseEntry>
        phrase_workspace) noexcept
    : limits_(limits), frame_encoded_storage_(frame_encoded_storage),
      dictionary_staging_(dictionary_staging),
      frame_decoded_storage_(frame_decoded_storage),
      phrase_workspace_(phrase_workspace) {
    if (core::validate_limits(limits_) != core::LimitError::none) {
        state_ = State::error;
        terminal_error_ = {core::ErrorCode::invalid_argument, 0, 0};
    }
}

core::ProcessResult Lz78AdaptiveHuffmanFrameStreamingDecoder::fail(
    const core::ErrorCode code,
    const std::size_t consumed,
    const std::size_t produced) noexcept {
    state_ = State::error;
    terminal_error_ = {code, input_position_, 0};
    return {consumed, produced, core::StreamStatus::error, terminal_error_};
}

bool Lz78AdaptiveHuffmanFrameStreamingDecoder::parse_collected_prefix()
    noexcept {
    const std::span<const std::byte, stream_header_size> header{
        prefix_bytes_.data(), stream_header_size};
    if (parse_stream_header(header, limits_, stream_)
        != StreamHeaderError::none) {
        return false;
    }
    if (stream_.dictionary_algorithm != DictionaryAlgorithm::lz78
        || stream_.dictionary_variant != 1
        || stream_.entropy_algorithm != EntropyAlgorithm::adaptive_huffman
        || stream_.entropy_variant != 1
        || stream_.frame_size > (UINT32_C(1) << 20)
        || stream_.entropy_block_size != 0
        || stream_.dictionary_parameters_size
               != dictionary::internal::lz78_parameter_size
        || stream_.entropy_parameters_size != 0) {
        return false;
    }
    const std::span<const std::byte,
                    dictionary::internal::lz78_parameter_size>
        parameter_bytes{prefix_bytes_.data() + stream_header_size,
                        dictionary::internal::lz78_parameter_size};
    return dictionary::internal::parse_lz78_parameters(
               parameter_bytes, limits_, parameters_)
        == dictionary::internal::Lz78FormatError::none;
}

bool Lz78AdaptiveHuffmanFrameStreamingDecoder::
parse_collected_frame_header() noexcept {
    preparation_error_ = core::ErrorCode::malformed_stream;
    const FrameValidationContext context{
        stream_, limits_, frame_sequence_, output_committed_};
    if (parse_frame_header(frame_header_bytes_, context, frame_)
        != FrameHeaderError::none) {
        return false;
    }

    std::uint64_t maximum_dictionary_size{};
    std::uint64_t maximum_payload_size{};
    if (!core::checked_multiply(
            static_cast<std::uint64_t>(frame_.uncompressed_size),
            static_cast<std::uint64_t>(dictionary::internal::lz78_token_size),
            maximum_dictionary_size)
        || !core::checked_multiply(
            static_cast<std::uint64_t>(frame_.dictionary_serialized_size),
            UINT64_C(33), maximum_payload_size)
        || frame_.dictionary_serialized_size == 0
        || frame_.dictionary_serialized_size
               % dictionary::internal::lz78_token_size != 0
        || frame_.dictionary_serialized_size > maximum_dictionary_size
        || frame_.dictionary_serialized_size
               > entropy::internal::adaptive_huffman_max_frame_size
        || frame_.entropy_block_count != 1
        || frame_.block_descriptors_size
               != entropy::internal::adaptive_huffman_descriptor_size
        || frame_.compressed_payload_size == 0
        || frame_.compressed_payload_size > maximum_payload_size) {
        return false;
    }

    const auto required_phrases =
        dictionary::internal::lz78_validation_workspace_entries(
            frame_.dictionary_serialized_size, parameters_);
    if (frame_.dictionary_serialized_size > dictionary_staging_.size()
        || frame_.uncompressed_size > frame_decoded_storage_.size()
        || required_phrases > phrase_workspace_.size()) {
        preparation_error_ = core::ErrorCode::out_of_memory;
        return false;
    }
    std::size_t serialized_size{};
    if (!core::checked_add(
            frame_header_size,
            static_cast<std::size_t>(frame_.block_descriptors_size),
            serialized_size)
        || !core::checked_add(
            serialized_size,
            static_cast<std::size_t>(frame_.compressed_payload_size),
            serialized_size)
        || serialized_size > frame_encoded_storage_.size()) {
        preparation_error_ = core::ErrorCode::out_of_memory;
        return false;
    }

    std::uint64_t phrase_bytes{};
    std::uint64_t buffered_bytes{};
    if (!core::checked_multiply(
            static_cast<std::uint64_t>(required_phrases),
            static_cast<std::uint64_t>(
                sizeof(dictionary::internal::Lz78PhraseEntry)),
            phrase_bytes)
        || !core::checked_add(
            static_cast<std::uint64_t>(serialized_size),
            static_cast<std::uint64_t>(frame_.dictionary_serialized_size),
            buffered_bytes)
        || !core::checked_add(
            buffered_bytes,
            static_cast<std::uint64_t>(frame_.uncompressed_size),
            buffered_bytes)
        || !core::checked_add(buffered_bytes, phrase_bytes, buffered_bytes)
        || buffered_bytes > limits_.max_internal_buffered_bytes) {
        preparation_error_ = core::ErrorCode::limit_exceeded;
        return false;
    }
    frame_serialized_size_ = serialized_size;
    std::memmove(frame_encoded_storage_.data(), frame_header_bytes_.data(),
                 frame_header_size);
    frame_collected_ = frame_header_size;
    return true;
}

bool Lz78AdaptiveHuffmanFrameStreamingDecoder::decode_collected_frame()
    noexcept {
    const auto decoded = decode_lz78_adaptive_huffman_frame_to_staging(
        stream_, parameters_, limits_, frame_sequence_, output_committed_,
        frame_encoded_storage_.first(frame_serialized_size_),
        dictionary_staging_.first(frame_.dictionary_serialized_size),
        phrase_workspace_,
        frame_decoded_storage_.first(frame_.uncompressed_size));
    if (decoded.error != Lz78AdaptiveHuffmanFrameValidationError::none) {
        return false;
    }
    decoded_size_ = frame_.uncompressed_size;
    output_offset_ = 0;
    output_committed_ += decoded_size_;
    ++frame_sequence_;
    header_collected_ = 0;
    frame_collected_ = 0;
    state_ = State::draining_frame;
    return true;
}

core::ProcessResult Lz78AdaptiveHuffmanFrameStreamingDecoder::process(
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
            const auto count =
                std::min(remaining, output.size() - produced);
            if (count != 0) {
                std::memmove(output.data() + produced,
                             frame_decoded_storage_.data() + output_offset_,
                             count);
                output_offset_ += count;
                produced += count;
            }
            if (output_offset_ != decoded_size_) {
                if (end_requested && consumed == input.size()) {
                    end_seen_ = true;
                }
                return {consumed, produced, core::StreamStatus::need_output,
                        {}};
            }
            decoded_size_ = 0;
            output_offset_ = 0;
            state_ = output_committed_ == stream_.original_size
                ? State::awaiting_end : State::collecting_frame_header;
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
        if (state_ == State::collecting_prefix) {
            const auto needed = lz78_adaptive_huffman_stream_prefix_size
                - header_collected_;
            const auto count = std::min(needed, input.size() - consumed);
            if (count != 0) {
                std::memmove(prefix_bytes_.data() + header_collected_,
                             input.data() + consumed, count);
                header_collected_ += count;
                consumed += count;
                input_position_ += count;
            }
            if (header_collected_
                != lz78_adaptive_huffman_stream_prefix_size) {
                if ((end_seen_ || end_requested)
                    && consumed == input.size()) {
                    return fail(core::ErrorCode::malformed_stream,
                                consumed, produced);
                }
                return {consumed, produced,
                        consumed != 0 ? core::StreamStatus::progress
                                      : core::StreamStatus::need_input,
                        {}};
            }
            if (!parse_collected_prefix()) {
                return fail(core::ErrorCode::malformed_stream,
                            consumed, produced);
            }
            header_collected_ = 0;
            state_ = stream_.original_size == 0
                ? State::awaiting_end : State::collecting_frame_header;
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
                if ((end_seen_ || end_requested)
                    && consumed == input.size()) {
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
                return fail(preparation_error_, consumed, produced);
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
            if ((end_seen_ || end_requested)
                && consumed == input.size()) {
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
            + frame_.uncompressed_size == stream_.original_size;
        if (final_frame && consumed != input.size()) {
            return fail(core::ErrorCode::malformed_stream,
                        consumed, produced);
        }
        if (end_requested && consumed == input.size()) {
            end_seen_ = true;
        }
        if (!decode_collected_frame()) {
            return fail(core::ErrorCode::malformed_stream,
                        consumed, produced);
        }
    }
}

} // namespace marc::frame

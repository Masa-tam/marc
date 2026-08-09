#include "frame/lzss_contextual_rans_frame_streaming_decoder.hpp"

#include "core/checked_math.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace marc::frame::internal {
namespace {

constexpr std::uint32_t known_flags =
    core::flag_value(core::ProcessFlags::flush)
    | core::flag_value(core::ProcessFlags::end_input)
    | core::flag_value(core::ProcessFlags::reset_block);

enum class OverlapCheck : std::uint8_t {
    disjoint,
    overlap,
    arithmetic_overflow,
};

[[nodiscard]] OverlapCheck regions_overlap(
    const void* first_data, const std::size_t first_size,
    const void* second_data, const std::size_t second_size) noexcept {
    if (first_size == 0 || second_size == 0) return OverlapCheck::disjoint;
    const auto first_begin = reinterpret_cast<std::uintptr_t>(first_data);
    const auto second_begin = reinterpret_cast<std::uintptr_t>(second_data);
    std::uintptr_t first_end{};
    std::uintptr_t second_end{};
    if (!core::checked_add(
            first_begin, static_cast<std::uintptr_t>(first_size), first_end)
        || !core::checked_add(
            second_begin, static_cast<std::uintptr_t>(second_size),
            second_end)) {
        return OverlapCheck::arithmetic_overflow;
    }
    return first_begin < second_end && second_begin < first_end
        ? OverlapCheck::overlap
        : OverlapCheck::disjoint;
}

} // namespace

LzssContextualRansFrameStreamingDecoder::
LzssContextualRansFrameStreamingDecoder(
    const core::DecoderLimits limits,
    const std::span<std::byte> serialized_frame_workspace,
    const std::span<entropy::internal::RansDecodeEntry> table_workspace,
    const std::span<dictionary::internal::LzssTypedToken> token_workspace,
    const std::span<std::byte> raw_frame_workspace) noexcept
    : limits_(limits),
      serialized_frame_workspace_(serialized_frame_workspace),
      table_workspace_(table_workspace), token_workspace_(token_workspace),
      raw_frame_workspace_(raw_frame_workspace) {
    std::size_t table_bytes{};
    std::size_t token_bytes{};
    const bool valid_extents = core::checked_multiply(
        table_workspace_.size(),
        sizeof(entropy::internal::RansDecodeEntry), table_bytes)
        && core::checked_multiply(
            token_workspace_.size(),
            sizeof(dictionary::internal::LzssTypedToken), token_bytes);
    const std::array overlaps{
        valid_extents
            ? regions_overlap(
                serialized_frame_workspace_.data(),
                serialized_frame_workspace_.size(), table_workspace_.data(),
                table_bytes)
            : OverlapCheck::arithmetic_overflow,
        valid_extents
            ? regions_overlap(
                serialized_frame_workspace_.data(),
                serialized_frame_workspace_.size(), token_workspace_.data(),
                token_bytes)
            : OverlapCheck::arithmetic_overflow,
        regions_overlap(
            serialized_frame_workspace_.data(),
            serialized_frame_workspace_.size(), raw_frame_workspace_.data(),
            raw_frame_workspace_.size()),
        valid_extents
            ? regions_overlap(
                table_workspace_.data(), table_bytes, token_workspace_.data(),
                token_bytes)
            : OverlapCheck::arithmetic_overflow,
        valid_extents
            ? regions_overlap(
                table_workspace_.data(), table_bytes,
                raw_frame_workspace_.data(), raw_frame_workspace_.size())
            : OverlapCheck::arithmetic_overflow,
        valid_extents
            ? regions_overlap(
                token_workspace_.data(), token_bytes,
                raw_frame_workspace_.data(), raw_frame_workspace_.size())
            : OverlapCheck::arithmetic_overflow,
    };
    if (!valid_extents
        || core::validate_limits(limits_) != core::LimitError::none
        || std::ranges::find_if(overlaps, [](const auto value) {
               return value != OverlapCheck::disjoint;
           }) != overlaps.end()) {
        state_ = State::error;
        terminal_error_ = {core::ErrorCode::invalid_argument, 0, 0};
    }
}

core::ProcessResult LzssContextualRansFrameStreamingDecoder::fail(
    const core::ErrorCode code, const std::size_t consumed,
    const std::size_t produced) noexcept {
    state_ = State::error;
    terminal_error_ = {code, input_position_, 0};
    return {consumed, produced, core::StreamStatus::error, terminal_error_};
}

bool LzssContextualRansFrameStreamingDecoder::
parse_collected_stream_header() noexcept {
    std::size_t consumed{};
    const auto error = parse_lzss_contextual_rans_stream_header(
        stream_header_bytes_, limits_, stream_, consumed);
    preparation_error_ =
        error == LzssContextualRansStreamHeaderError::limit_exceeded
        ? core::ErrorCode::limit_exceeded
        : core::ErrorCode::malformed_stream;
    return error == LzssContextualRansStreamHeaderError::none
        && consumed == lzss_contextual_rans_stream_header_size;
}

bool LzssContextualRansFrameStreamingDecoder::prepare_collected_frame()
    noexcept {
    preparation_error_ = core::ErrorCode::malformed_stream;
    const LzssContextualRansFrameValidationContext context{
        stream_, limits_, frame_sequence_, output_committed_};
    std::size_t consumed{};
    const auto header_error = parse_lzss_contextual_rans_frame_header(
        frame_header_bytes_, context, frame_, consumed);
    if (header_error != LzssContextualRansFrameHeaderError::none
        || consumed != lzss_contextual_rans_frame_header_size) {
        preparation_error_ = header_error
                == LzssContextualRansFrameHeaderError::limit_exceeded
            ? core::ErrorCode::limit_exceeded
            : core::ErrorCode::malformed_stream;
        return false;
    }

    std::size_t serialized_size{};
    std::size_t table_bytes{};
    std::size_t token_bytes{};
    if (!core::checked_add(
            lzss_contextual_rans_frame_header_size,
            entropy::internal::contextual_rans_descriptor_size,
            serialized_size)
        || !core::checked_add(
            serialized_size, static_cast<std::size_t>(frame_.payload_size),
            serialized_size)
        || !core::checked_multiply(
            entropy::internal::contextual_rans_decode_table_entries,
            sizeof(entropy::internal::RansDecodeEntry), table_bytes)
        || !core::checked_multiply(
            static_cast<std::size_t>(frame_.token_count),
            sizeof(dictionary::internal::LzssTypedToken), token_bytes)) {
        preparation_error_ = core::ErrorCode::internal_error;
        return false;
    }
    if (serialized_size > serialized_frame_workspace_.size()
        || table_workspace_.size()
            < entropy::internal::contextual_rans_decode_table_entries
        || frame_.token_count > token_workspace_.size()
        || frame_.uncompressed_size > raw_frame_workspace_.size()) {
        preparation_error_ = core::ErrorCode::out_of_memory;
        return false;
    }
    std::uint64_t buffered_bytes{};
    if (!core::checked_add(
            static_cast<std::uint64_t>(serialized_size),
            static_cast<std::uint64_t>(table_bytes), buffered_bytes)
        || !core::checked_add(
            buffered_bytes, static_cast<std::uint64_t>(token_bytes),
            buffered_bytes)
        || !core::checked_add(
            buffered_bytes,
            static_cast<std::uint64_t>(frame_.uncompressed_size),
            buffered_bytes)) {
        preparation_error_ = core::ErrorCode::internal_error;
        return false;
    }
    if (buffered_bytes > limits_.max_internal_buffered_bytes) {
        preparation_error_ = core::ErrorCode::limit_exceeded;
        return false;
    }

    frame_serialized_size_ = serialized_size;
    std::memmove(
        serialized_frame_workspace_.data(), frame_header_bytes_.data(),
        lzss_contextual_rans_frame_header_size);
    frame_collected_ = lzss_contextual_rans_frame_header_size;
    return true;
}

bool LzssContextualRansFrameStreamingDecoder::decode_collected_frame()
    noexcept {
    const LzssContextualRansFrameValidationContext context{
        stream_, limits_, frame_sequence_, output_committed_};
    const auto decoded = decode_lzss_contextual_rans_frame(
        serialized_frame_workspace_.first(frame_serialized_size_), context,
        table_workspace_.first(
            entropy::internal::contextual_rans_decode_table_entries),
        token_workspace_.first(frame_.token_count),
        raw_frame_workspace_.first(frame_.uncompressed_size));
    if (decoded.error != LzssContextualRansFrameDecodeError::none) {
        preparation_error_ = decoded.error
                == LzssContextualRansFrameDecodeError::reconstruction_error
            ? core::ErrorCode::internal_error
            : core::ErrorCode::malformed_stream;
        return false;
    }
    decoded_size_ = decoded.required_raw_size;
    output_offset_ = 0;
    output_committed_ += decoded_size_;
    ++frame_sequence_;
    header_collected_ = 0;
    frame_collected_ = 0;
    state_ = State::draining_frame;
    return true;
}

core::ProcessResult LzssContextualRansFrameStreamingDecoder::process(
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
    if (regions_overlap(
            output.data(), output.size(), raw_frame_workspace_.data(),
            raw_frame_workspace_.size()) != OverlapCheck::disjoint) {
        return fail(core::ErrorCode::invalid_argument, 0, 0);
    }

    const bool end_requested =
        (flags & core::flag_value(core::ProcessFlags::end_input)) != 0;
    std::size_t consumed{};
    std::size_t produced{};

    while (true) {
        if (state_ == State::draining_frame) {
            const auto remaining = decoded_size_ - output_offset_;
            const auto count = std::min(
                remaining, output.size() - produced);
            if (count != 0) {
                std::memmove(
                    output.data() + produced,
                    raw_frame_workspace_.data() + output_offset_, count);
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
                ? State::awaiting_end
                : State::collecting_frame_header;
            continue;
        }

        if (state_ == State::awaiting_end) {
            if (consumed != input.size()) {
                return fail(
                    core::ErrorCode::malformed_stream, consumed, produced);
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
            const auto needed =
                lzss_contextual_rans_stream_header_size - header_collected_;
            const auto count = std::min(
                needed, input.size() - consumed);
            if (count != 0) {
                std::memmove(
                    stream_header_bytes_.data() + header_collected_,
                    input.data() + consumed, count);
                header_collected_ += count;
                consumed += count;
                input_position_ += count;
            }
            if (header_collected_
                != lzss_contextual_rans_stream_header_size) {
                if ((end_seen_ || end_requested)
                    && consumed == input.size()) {
                    return fail(
                        core::ErrorCode::malformed_stream, consumed, produced);
                }
                return {consumed, produced,
                        consumed != 0 ? core::StreamStatus::progress
                                      : core::StreamStatus::need_input,
                        {}};
            }
            if (!parse_collected_stream_header()) {
                return fail(preparation_error_, consumed, produced);
            }
            header_collected_ = 0;
            state_ = stream_.original_size == 0
                ? State::awaiting_end
                : State::collecting_frame_header;
            if (state_ == State::awaiting_end && consumed != input.size()) {
                return fail(
                    core::ErrorCode::malformed_stream, consumed, produced);
            }
            if (state_ == State::awaiting_end && end_requested) {
                end_seen_ = true;
            }
            continue;
        }

        if (state_ == State::collecting_frame_header) {
            const auto needed =
                lzss_contextual_rans_frame_header_size - header_collected_;
            const auto count = std::min(
                needed, input.size() - consumed);
            if (count != 0) {
                std::memmove(
                    frame_header_bytes_.data() + header_collected_,
                    input.data() + consumed, count);
                header_collected_ += count;
                consumed += count;
                input_position_ += count;
            }
            if (header_collected_
                != lzss_contextual_rans_frame_header_size) {
                if ((end_seen_ || end_requested)
                    && consumed == input.size()) {
                    return fail(
                        core::ErrorCode::malformed_stream, consumed, produced);
                }
                return {consumed, produced,
                        (consumed != 0 || produced != 0)
                            ? core::StreamStatus::progress
                            : core::StreamStatus::need_input,
                        {}};
            }
            if (!prepare_collected_frame()) {
                return fail(preparation_error_, consumed, produced);
            }
            state_ = State::collecting_frame_body;
            continue;
        }

        const auto needed = frame_serialized_size_ - frame_collected_;
        const auto count = std::min(needed, input.size() - consumed);
        if (count != 0) {
            std::memmove(
                serialized_frame_workspace_.data() + frame_collected_,
                input.data() + consumed, count);
            frame_collected_ += count;
            consumed += count;
            input_position_ += count;
        }
        if (frame_collected_ != frame_serialized_size_) {
            if ((end_seen_ || end_requested) && consumed == input.size()) {
                return fail(
                    core::ErrorCode::malformed_stream, consumed, produced);
            }
            return {consumed, produced,
                    (consumed != 0 || produced != 0)
                        ? core::StreamStatus::progress
                        : core::StreamStatus::need_input,
                    {}};
        }
        const bool final_frame = frame_.uncompressed_size
            == stream_.original_size - output_committed_;
        if (final_frame && consumed != input.size()) {
            return fail(
                core::ErrorCode::malformed_stream, consumed, produced);
        }
        if (end_requested && consumed == input.size()) end_seen_ = true;
        if (!decode_collected_frame()) {
            return fail(preparation_error_, consumed, produced);
        }
    }
}

} // namespace marc::frame::internal

#include "frame/lzss_contextual_tans_frame_streaming_encoder.hpp"

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

[[nodiscard]] bool extent(
    const std::size_t count, const std::size_t element_size,
    std::size_t& bytes) noexcept {
    return core::checked_multiply(count, element_size, bytes);
}

[[nodiscard]] bool is_limit_failure(
    const LzssContextualTansFrameEncodeResult& result) noexcept {
    return result.error
            == LzssContextualTansFrameEncodeError::workspace_limit
        || (result.error
                == LzssContextualTansFrameEncodeError::token_encode_error
            && (result.token_encode.error
                    == dictionary::internal::LzssTypedEncodeError::
                        input_limit_exceeded
                || result.token_encode.error
                    == dictionary::internal::LzssTypedEncodeError::
                        token_storage_limit_exceeded
                || result.token_encode.match_finder_error
                    == dictionary::internal::LzssHashChainError::
                        workspace_limit_exceeded))
        || (result.error
                == LzssContextualTansFrameEncodeError::entropy_encode_error
            && result.entropy_encode.error
                == context::internal::LzssContextualTansEncodeError::
                    limit_exceeded)
        || (result.error
                == LzssContextualTansFrameEncodeError::header_error
            && result.header_error
                == LzssContextualTansFrameHeaderError::limit_exceeded)
        || (result.error
                == LzssContextualTansFrameEncodeError::descriptor_error
            && result.descriptor_error
                == entropy::internal::ContextualTansFormatError::
                    limit_exceeded);
}

[[nodiscard]] bool is_capacity_failure(
    const LzssContextualTansFrameEncodeResult& result) noexcept {
    return result.error
            == LzssContextualTansFrameEncodeError::token_staging_too_small
        || result.error
            == LzssContextualTansFrameEncodeError::table_staging_too_small
        || result.error
            == LzssContextualTansFrameEncodeError::
                serialized_output_too_small
        || (result.error == LzssContextualTansFrameEncodeError::token_encode_error
            && result.token_encode.match_finder_error
                == dictionary::internal::LzssHashChainError::workspace_too_small);
}

[[nodiscard]] core::ErrorCode preparation_error(
    const LzssContextualTansFrameEncodeResult& result) noexcept {
    if (result.error == LzssContextualTansFrameEncodeError::none) {
        return core::ErrorCode::none;
    }
    if (is_limit_failure(result)) return core::ErrorCode::limit_exceeded;
    if (is_capacity_failure(result)) {
        return core::ErrorCode::out_of_memory;
    }
    if (result.error
        == LzssContextualTansFrameEncodeError::input_size_mismatch) {
        return core::ErrorCode::invalid_argument;
    }
    return core::ErrorCode::internal_error;
}

} // namespace

LzssContextualTansFrameStreamingEncoder::
LzssContextualTansFrameStreamingEncoder(
    const LzssContextualTansStreamHeader stream,
    const core::DecoderLimits limits,
    const std::span<std::byte> raw_frame_workspace,
    const std::span<dictionary::internal::LzssTypedToken> token_workspace,
    const std::span<std::uint16_t> table_workspace,
    const std::span<std::byte> serialized_frame_workspace) noexcept
    : LzssContextualTansFrameStreamingEncoder(
        stream, limits, raw_frame_workspace, token_workspace,
        table_workspace, {}, serialized_frame_workspace) {}

LzssContextualTansFrameStreamingEncoder::
LzssContextualTansFrameStreamingEncoder(
    const LzssContextualTansStreamHeader stream,
    const core::DecoderLimits limits,
    const std::span<std::byte> raw_frame_workspace,
    const std::span<dictionary::internal::LzssTypedToken> token_workspace,
    const std::span<std::uint16_t> table_workspace,
    const std::span<std::byte> match_finder_workspace,
    const std::span<std::byte> serialized_frame_workspace) noexcept
    : stream_(stream), limits_(limits),
      raw_frame_workspace_(raw_frame_workspace),
      token_workspace_(token_workspace), table_workspace_(table_workspace),
      match_finder_workspace_(match_finder_workspace),
      serialized_frame_workspace_(serialized_frame_workspace) {
    std::size_t token_bytes{};
    std::size_t table_bytes{};
    const bool valid_extents = extent(
        token_workspace_.size(),
        sizeof(dictionary::internal::LzssTypedToken), token_bytes)
        && extent(table_workspace_.size(), sizeof(std::uint16_t), table_bytes);
    const std::array overlaps{
        valid_extents
            ? regions_overlap(
                raw_frame_workspace_.data(), raw_frame_workspace_.size(),
                token_workspace_.data(), token_bytes)
            : OverlapCheck::arithmetic_overflow,
        valid_extents
            ? regions_overlap(
                raw_frame_workspace_.data(), raw_frame_workspace_.size(),
                table_workspace_.data(), table_bytes)
            : OverlapCheck::arithmetic_overflow,
        regions_overlap(
            raw_frame_workspace_.data(), raw_frame_workspace_.size(),
            serialized_frame_workspace_.data(),
            serialized_frame_workspace_.size()),
        valid_extents
            ? regions_overlap(
                token_workspace_.data(), token_bytes,
                table_workspace_.data(), table_bytes)
            : OverlapCheck::arithmetic_overflow,
        valid_extents
            ? regions_overlap(
                token_workspace_.data(), token_bytes,
                serialized_frame_workspace_.data(),
                serialized_frame_workspace_.size())
            : OverlapCheck::arithmetic_overflow,
        valid_extents
            ? regions_overlap(
                table_workspace_.data(), table_bytes,
                serialized_frame_workspace_.data(),
                serialized_frame_workspace_.size())
            : OverlapCheck::arithmetic_overflow,
        regions_overlap(raw_frame_workspace_.data(), raw_frame_workspace_.size(),
                        match_finder_workspace_.data(), match_finder_workspace_.size()),
        valid_extents ? regions_overlap(token_workspace_.data(), token_bytes,
                        match_finder_workspace_.data(), match_finder_workspace_.size())
                      : OverlapCheck::arithmetic_overflow,
        valid_extents ? regions_overlap(table_workspace_.data(), table_bytes,
                        match_finder_workspace_.data(), match_finder_workspace_.size())
                      : OverlapCheck::arithmetic_overflow,
        regions_overlap(serialized_frame_workspace_.data(), serialized_frame_workspace_.size(),
                        match_finder_workspace_.data(), match_finder_workspace_.size()),
    };
    const auto required_raw = std::min<std::uint64_t>(
        stream_.original_size, stream_.frame_size);
    const auto stream_error = validate_lzss_contextual_tans_stream_header(
        stream_, limits_);
    const auto serialization_error =
        serialize_lzss_contextual_tans_stream_header(
            stream_, limits_, stream_header_);
    if (!valid_extents
        || stream_error != LzssContextualTansStreamHeaderError::none
        || stream_.context_variant == static_cast<std::uint16_t>(
            context::internal::LzssFieldContextVariant::field_context_16m)
        || raw_frame_workspace_.size() < required_raw
        || std::ranges::find_if(overlaps, [](const auto value) {
               return value != OverlapCheck::disjoint;
           }) != overlaps.end()
        || serialization_error
            != LzssContextualTansStreamHeaderError::none) {
        state_ = State::error;
        terminal_error_ = {core::ErrorCode::invalid_argument, 0, 0};
    }
}

core::ProcessResult LzssContextualTansFrameStreamingEncoder::fail(
    const core::ErrorCode code, const std::size_t consumed,
    const std::size_t produced) noexcept {
    state_ = State::error;
    terminal_error_ = {code, input_received_, 0};
    return {consumed, produced, core::StreamStatus::error, terminal_error_};
}

bool LzssContextualTansFrameStreamingEncoder::output_is_disjoint(
    const std::span<std::byte> output) const noexcept {
    std::size_t token_bytes{};
    std::size_t table_bytes{};
    if (!extent(
            token_workspace_.size(),
            sizeof(dictionary::internal::LzssTypedToken), token_bytes)
        || !extent(
            table_workspace_.size(), sizeof(std::uint16_t), table_bytes)) {
        return false;
    }
    const std::array overlaps{
        regions_overlap(
            output.data(), output.size(), raw_frame_workspace_.data(),
            raw_frame_workspace_.size()),
        regions_overlap(
            output.data(), output.size(), token_workspace_.data(),
            token_bytes),
        regions_overlap(
            output.data(), output.size(), table_workspace_.data(),
            table_bytes),
        regions_overlap(output.data(), output.size(),
                        match_finder_workspace_.data(), match_finder_workspace_.size()),
        regions_overlap(
            output.data(), output.size(),
            serialized_frame_workspace_.data(),
            serialized_frame_workspace_.size()),
    };
    return std::ranges::find_if(overlaps, [](const auto value) {
               return value != OverlapCheck::disjoint;
           }) == overlaps.end();
}

bool LzssContextualTansFrameStreamingEncoder::prepare_frame() noexcept {
    preparation_error_ = core::ErrorCode::internal_error;
    const auto encoded = encode_lzss_contextual_tans_frame_hash_chain(
        stream_, limits_, frame_sequence_, input_committed_,
        raw_frame_workspace_.first(raw_frame_size_), token_workspace_,
        table_workspace_, match_finder_workspace_, serialized_frame_workspace_);
    preparation_error_ = preparation_error(encoded);
    if (preparation_error_ != core::ErrorCode::none) return false;
    pending_size_ = encoded.serialized_size;
    pending_offset_ = 0;
    input_committed_ += raw_frame_size_;
    ++frame_sequence_;
    raw_frame_size_ = 0;
    state_ = State::draining_frame;
    return true;
}

core::ProcessResult LzssContextualTansFrameStreamingEncoder::process(
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
    if (!output_is_disjoint(output)) {
        return fail(core::ErrorCode::invalid_argument, 0, 0);
    }
    if (input.size() > stream_.original_size - input_received_) {
        return fail(core::ErrorCode::invalid_argument, 0, 0);
    }
    const bool end_requested =
        (flags & core::flag_value(core::ProcessFlags::end_input)) != 0;
    if (end_requested
        && input.size() != stream_.original_size - input_received_) {
        return fail(core::ErrorCode::invalid_argument, 0, 0);
    }
    if (end_requested && input_received_ == stream_.original_size) {
        end_seen_ = true;
    }

    std::size_t consumed{};
    std::size_t produced{};
    while (true) {
        if (state_ == State::draining_header
            || state_ == State::draining_frame) {
            const auto remaining = pending_size_ - pending_offset_;
            const auto count = std::min(
                remaining, output.size() - produced);
            if (count != 0) {
                const auto* source = state_ == State::draining_header
                    ? stream_header_.data()
                    : serialized_frame_workspace_.data();
                std::memmove(
                    output.data() + produced, source + pending_offset_, count);
                pending_offset_ += count;
                produced += count;
            }
            if (pending_offset_ != pending_size_) {
                return {consumed, produced, core::StreamStatus::need_output,
                        {}};
            }
            pending_offset_ = 0;
            pending_size_ = 0;
            if (state_ == State::draining_header) {
                state_ = stream_.original_size == 0
                    ? (end_seen_ ? State::ended : State::awaiting_end)
                    : State::collecting;
            } else if (input_committed_ == stream_.original_size) {
                state_ = end_seen_ ? State::ended : State::awaiting_end;
            } else {
                state_ = State::collecting;
            }
            if (state_ == State::ended) {
                return {consumed, produced,
                        core::StreamStatus::end_of_stream, {}};
            }
            continue;
        }

        if (state_ == State::awaiting_end) {
            if (consumed != input.size()) {
                return fail(
                    core::ErrorCode::invalid_argument, consumed, produced);
            }
            if (end_seen_ || end_requested) {
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

        const auto remaining_stream =
            stream_.original_size - input_committed_;
        const auto expected_frame = static_cast<std::size_t>(
            std::min<std::uint64_t>(stream_.frame_size, remaining_stream));
        const auto count = std::min(
            expected_frame - raw_frame_size_, input.size() - consumed);
        if (count != 0) {
            std::memmove(
                raw_frame_workspace_.data() + raw_frame_size_,
                input.data() + consumed, count);
            raw_frame_size_ += count;
            input_received_ += count;
            consumed += count;
        }
        if (end_requested && consumed == input.size()) end_seen_ = true;
        if (raw_frame_size_ == expected_frame) {
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

} // namespace marc::frame::internal

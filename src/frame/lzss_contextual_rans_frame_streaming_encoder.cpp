#include "frame/lzss_contextual_rans_frame_streaming_encoder.hpp"

#include "core/checked_math.hpp"

#include <algorithm>
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

[[nodiscard]] bool native_extent(
    const std::size_t count, const std::size_t element_size,
    std::size_t& extent) noexcept {
    return core::checked_multiply(count, element_size, extent);
}

[[nodiscard]] bool is_limit_failure(
    const LzssContextualRansFrameEncodeResult& result) noexcept {
    return result.error
            == LzssContextualRansFrameEncodeError::workspace_limit
        || (result.error
                == LzssContextualRansFrameEncodeError::token_encode_error
            && (result.token_encode.error
                    == dictionary::internal::
                        LzssTypedEncodeError::input_limit_exceeded
                || result.token_encode.error
                    == dictionary::internal::LzssTypedEncodeError::
                        token_storage_limit_exceeded
                || result.token_encode.match_finder_error
                    == dictionary::internal::LzssHashChainError::
                        workspace_limit_exceeded))
        || (result.error
                == LzssContextualRansFrameEncodeError::entropy_encode_error
            && result.entropy_encode.error
                == context::internal::
                    LzssContextualRansEncodeError::limit_exceeded)
        || (result.error == LzssContextualRansFrameEncodeError::header_error
            && result.header_error
                == LzssContextualRansFrameHeaderError::limit_exceeded)
        || (result.error
                == LzssContextualRansFrameEncodeError::descriptor_error
            && result.descriptor_error
                == entropy::internal::ContextualRansFormatError::
                    limit_exceeded);
}

[[nodiscard]] bool is_capacity_failure(
    const LzssContextualRansFrameEncodeResult& result) noexcept {
    return result.error
            == LzssContextualRansFrameEncodeError::token_staging_too_small
        || result.error
            == LzssContextualRansFrameEncodeError::
                serialized_output_too_small
        || (result.error
                == LzssContextualRansFrameEncodeError::token_encode_error
            && result.token_encode.match_finder_error
                == dictionary::internal::
                    LzssHashChainError::workspace_too_small);
}

template <typename Result>
[[nodiscard]] core::ErrorCode preparation_error(
    const Result& result) noexcept {
    if (result.error == LzssContextualRansFrameEncodeError::none) {
        return core::ErrorCode::none;
    }
    if (is_limit_failure(result)) return core::ErrorCode::limit_exceeded;
    if (is_capacity_failure(result)) {
        return core::ErrorCode::out_of_memory;
    }
    if (result.error
        == LzssContextualRansFrameEncodeError::input_size_mismatch) {
        return core::ErrorCode::invalid_argument;
    }
    return core::ErrorCode::internal_error;
}

} // namespace

LzssContextualRansFrameStreamingEncoder::
LzssContextualRansFrameStreamingEncoder(
    const LzssContextualRansStreamHeader stream,
    const core::DecoderLimits limits,
    const std::span<std::byte> raw_frame_workspace,
    const std::span<dictionary::internal::LzssTypedToken> token_workspace,
    const std::span<std::byte> match_finder_workspace,
    const std::span<std::byte> serialized_frame_workspace) noexcept
    : stream_(stream), limits_(limits),
      raw_frame_workspace_(raw_frame_workspace),
      token_workspace_(token_workspace),
      match_finder_workspace_(match_finder_workspace),
      serialized_frame_workspace_(serialized_frame_workspace) {
    std::size_t token_bytes{};
    const bool valid_extent = native_extent(
        token_workspace_.size(),
        sizeof(dictionary::internal::LzssTypedToken), token_bytes);
    const auto raw_tokens = valid_extent
        ? regions_overlap(
            raw_frame_workspace_.data(), raw_frame_workspace_.size(),
            token_workspace_.data(), token_bytes)
        : OverlapCheck::arithmetic_overflow;
    const auto raw_serialized = regions_overlap(
        raw_frame_workspace_.data(), raw_frame_workspace_.size(),
        serialized_frame_workspace_.data(),
        serialized_frame_workspace_.size());
    const auto raw_match_finder = regions_overlap(
        raw_frame_workspace_.data(), raw_frame_workspace_.size(),
        match_finder_workspace_.data(), match_finder_workspace_.size());
    const auto tokens_serialized = valid_extent
        ? regions_overlap(
            token_workspace_.data(), token_bytes,
            serialized_frame_workspace_.data(),
            serialized_frame_workspace_.size())
        : OverlapCheck::arithmetic_overflow;
    const auto tokens_match_finder = valid_extent
        ? regions_overlap(
            token_workspace_.data(), token_bytes,
            match_finder_workspace_.data(), match_finder_workspace_.size())
        : OverlapCheck::arithmetic_overflow;
    const auto match_finder_serialized = regions_overlap(
        match_finder_workspace_.data(), match_finder_workspace_.size(),
        serialized_frame_workspace_.data(),
        serialized_frame_workspace_.size());
    const auto required_raw = std::min<std::uint64_t>(
        stream_.original_size, stream_.frame_size);
    const auto stream_error =
        validate_lzss_contextual_rans_stream_header(stream_, limits_);
    const auto serialization_error =
        serialize_lzss_contextual_rans_stream_header(
            stream_, limits_, stream_header_);
    if (!valid_extent
        || stream_error != LzssContextualRansStreamHeaderError::none
        || stream_.dictionary_variant != 2
        || stream_.context_algorithm != 1
        || stream_.context_variant != 1
        || raw_frame_workspace_.size() < required_raw
        || raw_tokens != OverlapCheck::disjoint
        || raw_serialized != OverlapCheck::disjoint
        || raw_match_finder != OverlapCheck::disjoint
        || tokens_serialized != OverlapCheck::disjoint
        || tokens_match_finder != OverlapCheck::disjoint
        || match_finder_serialized != OverlapCheck::disjoint
        || serialization_error
            != LzssContextualRansStreamHeaderError::none) {
        state_ = State::error;
        terminal_error_ = {core::ErrorCode::invalid_argument, 0, 0};
    }
}

core::ProcessResult LzssContextualRansFrameStreamingEncoder::fail(
    const core::ErrorCode code, const std::size_t consumed,
    const std::size_t produced) noexcept {
    state_ = State::error;
    terminal_error_ = {code, input_received_, 0};
    return {consumed, produced, core::StreamStatus::error, terminal_error_};
}

bool LzssContextualRansFrameStreamingEncoder::output_is_disjoint(
    const std::span<std::byte> output) const noexcept {
    std::size_t token_bytes{};
    if (!native_extent(
            token_workspace_.size(),
            sizeof(dictionary::internal::LzssTypedToken), token_bytes)) {
        return false;
    }
    return regions_overlap(
               output.data(), output.size(), raw_frame_workspace_.data(),
               raw_frame_workspace_.size()) == OverlapCheck::disjoint
        && regions_overlap(
               output.data(), output.size(), token_workspace_.data(),
               token_bytes) == OverlapCheck::disjoint
        && regions_overlap(
               output.data(), output.size(), match_finder_workspace_.data(),
               match_finder_workspace_.size()) == OverlapCheck::disjoint
        && regions_overlap(
               output.data(), output.size(),
               serialized_frame_workspace_.data(),
               serialized_frame_workspace_.size())
            == OverlapCheck::disjoint;
}

bool LzssContextualRansFrameStreamingEncoder::prepare_frame() noexcept {
    preparation_error_ = core::ErrorCode::internal_error;
    const auto raw = raw_frame_workspace_.first(raw_frame_size_);
    std::size_t serialized_size{};
    const auto encoded = encode_lzss_contextual_rans_frame_hash_chain(
        stream_, limits_, frame_sequence_, input_committed_, raw,
        token_workspace_, match_finder_workspace_,
        serialized_frame_workspace_);
    preparation_error_ = preparation_error(encoded);
    serialized_size = encoded.serialized_size;
    if (preparation_error_ != core::ErrorCode::none) return false;
    pending_size_ = serialized_size;
    pending_offset_ = 0;
    input_committed_ += raw_frame_size_;
    ++frame_sequence_;
    raw_frame_size_ = 0;
    state_ = State::draining_frame;
    return true;
}

core::ProcessResult LzssContextualRansFrameStreamingEncoder::process(
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
                if (stream_.original_size == 0) {
                    state_ = end_seen_ ? State::ended : State::awaiting_end;
                } else {
                    state_ = State::collecting;
                }
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

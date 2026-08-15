#include "frame/lzss_contextual_blocked_huffman_frame_encoder.hpp"

#include "core/checked_math.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace marc::frame::internal {
namespace {

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

[[nodiscard]] bool exact_input_size(
    const LzssContextualBlockedHuffmanStreamHeader& stream,
    const std::uint64_t output_already_committed,
    const std::size_t input_size) noexcept {
    if (output_already_committed >= stream.original_size) return false;
    const auto remaining = stream.original_size - output_already_committed;
    const auto expected = std::min<std::uint64_t>(stream.frame_size, remaining);
    return input_size == expected && input_size != 0
        && input_size <= std::numeric_limits<std::uint32_t>::max();
}

[[nodiscard]] LzssContextualBlockedHuffmanFrameEncodeResult fail_overlap(
    LzssContextualBlockedHuffmanFrameEncodeResult result,
    const OverlapCheck overlap) noexcept {
    result.error = overlap == OverlapCheck::arithmetic_overflow
        ? LzssContextualBlockedHuffmanFrameEncodeError::arithmetic_overflow
        : LzssContextualBlockedHuffmanFrameEncodeError::overlapping_workspaces;
    return result;
}

[[nodiscard]] LzssContextualBlockedHuffmanFrameHeader make_header(
    const std::uint64_t sequence, const std::size_t raw_size,
    const LzssContextualBlockedHuffmanFrameEncodeResult& result) noexcept {
    LzssContextualBlockedHuffmanFrameHeader header{};
    header.sequence = sequence;
    header.uncompressed_size = static_cast<std::uint32_t>(raw_size);
    header.token_count = static_cast<std::uint32_t>(result.token_count);
    header.event_count = static_cast<std::uint32_t>(result.event_count);
    header.decision_count = result.decision_count;
    header.payload_size = static_cast<std::uint32_t>(result.payload_size);
    header.descriptor_size = static_cast<std::uint32_t>(result.descriptor_size);
    return header;
}

[[nodiscard]] bool token_capacity_bytes(
    const std::span<dictionary::internal::LzssTypedToken> tokens,
    std::size_t& bytes) noexcept {
    return core::checked_multiply(
        tokens.size(), sizeof(dictionary::internal::LzssTypedToken), bytes);
}

} // namespace

template <bool UseHashChain>
[[nodiscard]] LzssContextualBlockedHuffmanFrameEncodeResult plan_frame(
    const LzssContextualBlockedHuffmanStreamHeader& stream,
    const core::DecoderLimits& limits, const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> raw_input,
    const std::span<dictionary::internal::LzssTypedToken> private_tokens,
    const std::span<std::byte> match_finder_workspace,
    dictionary::internal::LzssMatchFinderStatistics* const statistics)
    noexcept {
    LzssContextualBlockedHuffmanFrameEncodeResult result{};
    std::size_t token_capacity{};
    if (!token_capacity_bytes(private_tokens, token_capacity)) {
        result.error =
            LzssContextualBlockedHuffmanFrameEncodeError::arithmetic_overflow;
        return result;
    }
    const auto overlap = regions_overlap(
        raw_input.data(), raw_input.size(), private_tokens.data(),
        token_capacity);
    if (overlap != OverlapCheck::disjoint) {
        return fail_overlap(result, overlap);
    }
    if constexpr (UseHashChain) {
        const std::array finder_overlaps{
            regions_overlap(raw_input.data(), raw_input.size(),
                            match_finder_workspace.data(),
                            match_finder_workspace.size()),
            regions_overlap(private_tokens.data(), token_capacity,
                            match_finder_workspace.data(),
                            match_finder_workspace.size()),
        };
        for (const auto finder_overlap : finder_overlaps) {
            if (finder_overlap != OverlapCheck::disjoint)
                return fail_overlap(result, finder_overlap);
        }
    }
    if (validate_lzss_contextual_blocked_huffman_stream_header(stream, limits)
        != LzssContextualBlockedHuffmanStreamHeaderError::none) {
        result.error =
            LzssContextualBlockedHuffmanFrameEncodeError::invalid_stream;
        return result;
    }
    const auto selected = context::internal::select_lzss_field_context_layout(
        stream.dictionary_variant, stream.context_algorithm,
        stream.context_variant);
    if (selected.error
        != context::internal::LzssFieldContextLayoutError::none) {
        result.error =
            LzssContextualBlockedHuffmanFrameEncodeError::invalid_stream;
        return result;
    }
    if (!exact_input_size(stream, output_already_committed, raw_input.size())) {
        result.error =
            LzssContextualBlockedHuffmanFrameEncodeError::input_size_mismatch;
        return result;
    }

    if constexpr (UseHashChain) {
        result.token_encode = dictionary::internal::
            encode_lzss_typed_tokens_hash_chain_single_pass(
                raw_input, stream.dictionary, limits, private_tokens,
                match_finder_workspace, statistics,
                selected.layout.dictionary_variant);
    } else {
        result.token_encode = dictionary::internal::encode_lzss_typed_tokens(
            raw_input, stream.dictionary, limits, private_tokens,
            selected.layout.dictionary_variant);
    }
    result.token_count = result.token_encode.token_count;
    if (result.token_encode.error
        != dictionary::internal::LzssTypedEncodeError::none) {
        result.error = result.token_encode.error
                == dictionary::internal::LzssTypedEncodeError::output_too_small
            ? LzssContextualBlockedHuffmanFrameEncodeError::
                token_staging_too_small
            : LzssContextualBlockedHuffmanFrameEncodeError::token_encode_error;
        return result;
    }
    if (result.token_count > UINT32_MAX) {
        result.error =
            LzssContextualBlockedHuffmanFrameEncodeError::arithmetic_overflow;
        return result;
    }
    const auto tokens = private_tokens.first(result.token_count);
    const dictionary::internal::LzssTypedFrameValidationContext token_context{
        static_cast<std::uint32_t>(result.token_count),
        static_cast<std::uint32_t>(raw_input.size()), output_already_committed};
    entropy::internal::ContextualBlockedHuffmanDescriptor descriptor{};
    result.entropy_encode =
        context::internal::plan_lzss_contextual_blocked_huffman_tokens(
            tokens, stream.dictionary, token_context, limits, descriptor,
            selected.layout.context_variant);
    result.event_count = result.entropy_encode.event_count;
    result.decision_count = result.entropy_encode.decision_count;
    result.descriptor_size = result.entropy_encode.descriptor_size;
    result.payload_size = result.entropy_encode.payload_size;
    if (result.entropy_encode.error
        != context::internal::LzssContextualBlockedHuffmanEncodeError::none) {
        result.error =
            LzssContextualBlockedHuffmanFrameEncodeError::entropy_encode_error;
        return result;
    }
    if (result.event_count > UINT32_MAX || result.descriptor_size > UINT32_MAX
        || result.payload_size > UINT32_MAX) {
        result.error =
            LzssContextualBlockedHuffmanFrameEncodeError::arithmetic_overflow;
        return result;
    }
    result.descriptor_error =
        entropy::internal::validate_contextual_blocked_huffman_descriptor(
            descriptor, result.decision_count,
            static_cast<std::uint32_t>(result.payload_size), limits,
            result.descriptor_size, selected.layout.context_variant);
    if (result.descriptor_error
        != entropy::internal::ContextualBlockedHuffmanFormatError::none) {
        result.error =
            LzssContextualBlockedHuffmanFrameEncodeError::descriptor_error;
        return result;
    }

    const auto header = make_header(sequence, raw_input.size(), result);
    const LzssContextualBlockedHuffmanFrameValidationContext frame_context{
        stream, limits, sequence, output_already_committed};
    result.header_error =
        validate_lzss_contextual_blocked_huffman_frame_header(
            header, frame_context);
    if (result.header_error
        != LzssContextualBlockedHuffmanFrameHeaderError::none) {
        result.error = LzssContextualBlockedHuffmanFrameEncodeError::header_error;
        return result;
    }
    if (!core::checked_add(
            lzss_contextual_blocked_huffman_frame_header_size,
            result.descriptor_size, result.serialized_size)
        || !core::checked_add(
            result.serialized_size, result.payload_size,
            result.serialized_size)) {
        result.error =
            LzssContextualBlockedHuffmanFrameEncodeError::arithmetic_overflow;
        return result;
    }
    std::size_t token_workspace = result.token_encode.token_storage_size;
    if constexpr (UseHashChain) {
        if (!core::checked_multiply(
                raw_input.size(),
                sizeof(dictionary::internal::LzssTypedToken),
                token_workspace)) {
            result.error = LzssContextualBlockedHuffmanFrameEncodeError::
                arithmetic_overflow;
            return result;
        }
    }
    std::size_t workspace{};
    if (!core::checked_add(
            raw_input.size(), token_workspace, workspace)
        || !core::checked_add(
            workspace, result.serialized_size, workspace)) {
        result.error =
            LzssContextualBlockedHuffmanFrameEncodeError::arithmetic_overflow;
        return result;
    }
    if constexpr (UseHashChain) {
        const auto required = dictionary::internal::
            calculate_lzss_hash_chain_workspace(
                raw_input.size(), stream.dictionary, limits);
        if (required.error
            != dictionary::internal::LzssHashChainError::none) {
            result.error =
                LzssContextualBlockedHuffmanFrameEncodeError::token_encode_error;
            result.token_encode.error = dictionary::internal::
                LzssTypedEncodeError::match_finder_error;
            result.token_encode.match_finder_error = required.error;
            return result;
        }
        if (!core::checked_add(
                workspace, required.workspace_size, workspace)) {
            result.error = LzssContextualBlockedHuffmanFrameEncodeError::
                arithmetic_overflow;
            return result;
        }
    }
    if (workspace > limits.max_internal_buffered_bytes) {
        result.error =
            LzssContextualBlockedHuffmanFrameEncodeError::workspace_limit;
    }
    return result;
}

template <bool UseHashChain>
[[nodiscard]] LzssContextualBlockedHuffmanFrameEncodeResult encode_frame(
    const LzssContextualBlockedHuffmanStreamHeader& stream,
    const core::DecoderLimits& limits, const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> raw_input,
    const std::span<dictionary::internal::LzssTypedToken> private_tokens,
    const std::span<std::byte> match_finder_workspace,
    const std::span<std::byte> serialized_output,
    dictionary::internal::LzssMatchFinderStatistics* const statistics)
    noexcept {
    LzssContextualBlockedHuffmanFrameEncodeResult result{};
    std::size_t token_capacity{};
    if (!token_capacity_bytes(private_tokens, token_capacity)) {
        result.error =
            LzssContextualBlockedHuffmanFrameEncodeError::arithmetic_overflow;
        return result;
    }
    const std::array overlaps{
        regions_overlap(serialized_output.data(), serialized_output.size(),
                        raw_input.data(), raw_input.size()),
        regions_overlap(serialized_output.data(), serialized_output.size(),
                        private_tokens.data(), token_capacity),
    };
    if (std::ranges::find(overlaps, OverlapCheck::arithmetic_overflow)
        != overlaps.end()) {
        return fail_overlap(result, OverlapCheck::arithmetic_overflow);
    }
    if (std::ranges::find(overlaps, OverlapCheck::overlap) != overlaps.end()) {
        return fail_overlap(result, OverlapCheck::overlap);
    }
    if constexpr (UseHashChain) {
        const auto finder_overlap = regions_overlap(
            serialized_output.data(), serialized_output.size(),
            match_finder_workspace.data(), match_finder_workspace.size());
        if (finder_overlap != OverlapCheck::disjoint)
            return fail_overlap(result, finder_overlap);
    }
    result = plan_frame<UseHashChain>(
        stream, limits, sequence, output_already_committed, raw_input,
        private_tokens, match_finder_workspace, statistics);
    if (result.error
        != LzssContextualBlockedHuffmanFrameEncodeError::none) {
        return result;
    }
    const auto selected = context::internal::select_lzss_field_context_layout(
        stream.dictionary_variant, stream.context_algorithm,
        stream.context_variant);
    if (selected.error
        != context::internal::LzssFieldContextLayoutError::none) {
        result.error =
            LzssContextualBlockedHuffmanFrameEncodeError::invalid_stream;
        return result;
    }
    if (serialized_output.size() < result.serialized_size) {
        result.error = LzssContextualBlockedHuffmanFrameEncodeError::
            serialized_output_too_small;
        return result;
    }

    const auto output = serialized_output.first(result.serialized_size);
    const auto tokens = private_tokens.first(result.token_count);
    std::size_t payload_offset{};
    if (!core::checked_add(
            lzss_contextual_blocked_huffman_frame_header_size,
            result.descriptor_size, payload_offset)) {
        result.error =
            LzssContextualBlockedHuffmanFrameEncodeError::internal_error;
        return result;
    }
    const dictionary::internal::LzssTypedFrameValidationContext token_context{
        static_cast<std::uint32_t>(result.token_count),
        static_cast<std::uint32_t>(raw_input.size()), output_already_committed};
    entropy::internal::ContextualBlockedHuffmanDescriptor descriptor{};
    result.entropy_encode =
        context::internal::encode_lzss_contextual_blocked_huffman_tokens(
            tokens, stream.dictionary, token_context, limits,
            output.subspan(payload_offset, result.payload_size), descriptor,
            selected.layout.context_variant);
    if (result.entropy_encode.error
            != context::internal::
                LzssContextualBlockedHuffmanEncodeError::none
        || result.entropy_encode.event_count != result.event_count
        || result.entropy_encode.decision_count != result.decision_count
        || result.entropy_encode.descriptor_size != result.descriptor_size
        || result.entropy_encode.payload_size != result.payload_size) {
        result.error =
            LzssContextualBlockedHuffmanFrameEncodeError::internal_error;
        return result;
    }
    std::size_t descriptor_written{};
    result.descriptor_error =
        entropy::internal::serialize_contextual_blocked_huffman_descriptor(
            descriptor, result.decision_count,
            static_cast<std::uint32_t>(result.payload_size), limits,
            output.subspan(
                lzss_contextual_blocked_huffman_frame_header_size,
                result.descriptor_size),
            descriptor_written, selected.layout.context_variant);
    if (result.descriptor_error
            != entropy::internal::ContextualBlockedHuffmanFormatError::none
        || descriptor_written != result.descriptor_size) {
        result.error =
            LzssContextualBlockedHuffmanFrameEncodeError::internal_error;
        return result;
    }
    const auto header = make_header(sequence, raw_input.size(), result);
    const LzssContextualBlockedHuffmanFrameValidationContext frame_context{
        stream, limits, sequence, output_already_committed};
    result.header_error =
        serialize_lzss_contextual_blocked_huffman_frame_header(
            header, frame_context,
            std::span<std::byte,
                      lzss_contextual_blocked_huffman_frame_header_size>{
                output.data(),
                lzss_contextual_blocked_huffman_frame_header_size});
    if (result.header_error
        != LzssContextualBlockedHuffmanFrameHeaderError::none) {
        result.error =
            LzssContextualBlockedHuffmanFrameEncodeError::internal_error;
    }
    return result;
}

LzssContextualBlockedHuffmanFrameEncodeResult
plan_lzss_contextual_blocked_huffman_frame(
    const LzssContextualBlockedHuffmanStreamHeader& stream,
    const core::DecoderLimits& limits, const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> raw_input,
    const std::span<dictionary::internal::LzssTypedToken> private_tokens)
    noexcept {
    return plan_frame<false>(
        stream, limits, sequence, output_already_committed, raw_input,
        private_tokens, {}, nullptr);
}

LzssContextualBlockedHuffmanFrameEncodeResult
encode_lzss_contextual_blocked_huffman_frame(
    const LzssContextualBlockedHuffmanStreamHeader& stream,
    const core::DecoderLimits& limits, const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> raw_input,
    const std::span<dictionary::internal::LzssTypedToken> private_tokens,
    const std::span<std::byte> serialized_output) noexcept {
    return encode_frame<false>(
        stream, limits, sequence, output_already_committed, raw_input,
        private_tokens, {}, serialized_output, nullptr);
}

LzssContextualBlockedHuffmanFrameEncodeResult
plan_lzss_contextual_blocked_huffman_frame_hash_chain(
    const LzssContextualBlockedHuffmanStreamHeader& stream,
    const core::DecoderLimits& limits, const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> raw_input,
    const std::span<dictionary::internal::LzssTypedToken> private_tokens,
    const std::span<std::byte> match_finder_workspace,
    dictionary::internal::LzssMatchFinderStatistics* const statistics)
    noexcept {
    return plan_frame<true>(
        stream, limits, sequence, output_already_committed, raw_input,
        private_tokens, match_finder_workspace, statistics);
}

LzssContextualBlockedHuffmanFrameEncodeResult
encode_lzss_contextual_blocked_huffman_frame_hash_chain(
    const LzssContextualBlockedHuffmanStreamHeader& stream,
    const core::DecoderLimits& limits, const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> raw_input,
    const std::span<dictionary::internal::LzssTypedToken> private_tokens,
    const std::span<std::byte> match_finder_workspace,
    const std::span<std::byte> serialized_output,
    dictionary::internal::LzssMatchFinderStatistics* const statistics)
    noexcept {
    return encode_frame<true>(
        stream, limits, sequence, output_already_committed, raw_input,
        private_tokens, match_finder_workspace, serialized_output,
        statistics);
}

} // namespace marc::frame::internal

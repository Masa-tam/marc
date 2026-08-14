#include "frame/lzss_contextual_tans_frame_encoder.hpp"

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
    const LzssContextualTansStreamHeader& stream,
    const std::uint64_t output_already_committed,
    const std::size_t input_size) noexcept {
    if (output_already_committed >= stream.original_size) return false;
    const auto remaining = stream.original_size - output_already_committed;
    const auto expected = std::min<std::uint64_t>(stream.frame_size, remaining);
    return input_size == expected && input_size != 0
        && input_size <= std::numeric_limits<std::uint32_t>::max();
}

[[nodiscard]] LzssContextualTansFrameEncodeResult fail_overlap(
    LzssContextualTansFrameEncodeResult result,
    const OverlapCheck overlap) noexcept {
    result.error = overlap == OverlapCheck::arithmetic_overflow
        ? LzssContextualTansFrameEncodeError::arithmetic_overflow
        : LzssContextualTansFrameEncodeError::overlapping_workspaces;
    return result;
}

[[nodiscard]] LzssContextualTansFrameHeader make_header(
    const std::uint64_t sequence, const std::size_t raw_size,
    const LzssContextualTansFrameEncodeResult& result) noexcept {
    LzssContextualTansFrameHeader header{};
    header.sequence = sequence;
    header.uncompressed_size = static_cast<std::uint32_t>(raw_size);
    header.token_count = static_cast<std::uint32_t>(result.token_count);
    header.event_count = static_cast<std::uint32_t>(result.event_count);
    header.decision_count = result.decision_count;
    header.payload_size = static_cast<std::uint32_t>(result.payload_size);
    header.descriptor_size = static_cast<std::uint32_t>(
        result.descriptor_size);
    return header;
}

[[nodiscard]] bool calculate_capacity_bytes(
    const std::span<dictionary::internal::LzssTypedToken> tokens,
    const std::span<std::uint16_t> tables,
    std::size_t& token_bytes,
    std::size_t& table_bytes) noexcept {
    return core::checked_multiply(
               tokens.size(),
               sizeof(dictionary::internal::LzssTypedToken), token_bytes)
        && core::checked_multiply(
            tables.size(), sizeof(std::uint16_t), table_bytes);
}

} // namespace

template <bool UseHashChain>
[[nodiscard]] LzssContextualTansFrameEncodeResult plan_frame(
    const LzssContextualTansStreamHeader& stream,
    const core::DecoderLimits& limits, const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> raw_input,
    const std::span<dictionary::internal::LzssTypedToken> private_tokens,
    const std::span<std::uint16_t> private_encode_tables,
    const std::span<std::byte> match_finder_workspace,
    dictionary::internal::LzssMatchFinderStatistics* const statistics)
    noexcept {
    LzssContextualTansFrameEncodeResult result{};
    std::size_t token_capacity_bytes{};
    std::size_t table_capacity_bytes{};
    if (!calculate_capacity_bytes(
            private_tokens, private_encode_tables, token_capacity_bytes,
            table_capacity_bytes)) {
        result.error =
            LzssContextualTansFrameEncodeError::arithmetic_overflow;
        return result;
    }
    const std::array overlaps{
        regions_overlap(raw_input.data(), raw_input.size(),
                        private_tokens.data(), token_capacity_bytes),
        regions_overlap(raw_input.data(), raw_input.size(),
                        private_encode_tables.data(), table_capacity_bytes),
        regions_overlap(private_tokens.data(), token_capacity_bytes,
                        private_encode_tables.data(), table_capacity_bytes),
    };
    if (std::ranges::find(overlaps, OverlapCheck::arithmetic_overflow)
        != overlaps.end()) {
        return fail_overlap(result, OverlapCheck::arithmetic_overflow);
    }
    if (std::ranges::find(overlaps, OverlapCheck::overlap)
        != overlaps.end()) {
        return fail_overlap(result, OverlapCheck::overlap);
    }
    if constexpr (UseHashChain) {
        const std::array finder_overlaps{
            regions_overlap(raw_input.data(), raw_input.size(),
                            match_finder_workspace.data(),
                            match_finder_workspace.size()),
            regions_overlap(private_tokens.data(), token_capacity_bytes,
                            match_finder_workspace.data(),
                            match_finder_workspace.size()),
            regions_overlap(private_encode_tables.data(),
                            table_capacity_bytes,
                            match_finder_workspace.data(),
                            match_finder_workspace.size()),
        };
        for (const auto overlap : finder_overlaps) {
            if (overlap != OverlapCheck::disjoint)
                return fail_overlap(result, overlap);
        }
    }
    if (private_encode_tables.size() < result.required_table_entries) {
        result.error =
            LzssContextualTansFrameEncodeError::table_staging_too_small;
        return result;
    }
    if (validate_lzss_contextual_tans_stream_header(stream, limits)
        != LzssContextualTansStreamHeaderError::none) {
        result.error = LzssContextualTansFrameEncodeError::invalid_stream;
        return result;
    }
    const auto selected = context::internal::select_lzss_field_context_layout(
        stream.dictionary_variant, stream.context_algorithm,
        stream.context_variant);
    if (selected.error
        != context::internal::LzssFieldContextLayoutError::none) {
        result.error = LzssContextualTansFrameEncodeError::invalid_stream;
        return result;
    }
    if (!exact_input_size(stream, output_already_committed,
                          raw_input.size())) {
        result.error =
            LzssContextualTansFrameEncodeError::input_size_mismatch;
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
            ? LzssContextualTansFrameEncodeError::token_staging_too_small
            : LzssContextualTansFrameEncodeError::token_encode_error;
        return result;
    }
    if (result.token_count > std::numeric_limits<std::uint32_t>::max()) {
        result.error =
            LzssContextualTansFrameEncodeError::arithmetic_overflow;
        return result;
    }
    const auto tokens = private_tokens.first(result.token_count);
    const dictionary::internal::LzssTypedFrameValidationContext token_context{
        static_cast<std::uint32_t>(result.token_count),
        static_cast<std::uint32_t>(raw_input.size()),
        output_already_committed};
    entropy::internal::ContextualTansDescriptor descriptor{};
    result.entropy_encode =
        context::internal::plan_lzss_contextual_tans_tokens(
            tokens, stream.dictionary, token_context, limits,
            private_encode_tables, descriptor,
            selected.layout.context_variant);
    result.event_count = result.entropy_encode.event_count;
    result.decision_count = result.entropy_encode.decision_count;
    result.payload_size = result.entropy_encode.payload_size;
    if (result.entropy_encode.error
        != context::internal::LzssContextualTansEncodeError::none) {
        result.error =
            LzssContextualTansFrameEncodeError::entropy_encode_error;
        return result;
    }
    if (result.event_count > std::numeric_limits<std::uint32_t>::max()
        || result.payload_size > std::numeric_limits<std::uint32_t>::max()) {
        result.error =
            LzssContextualTansFrameEncodeError::arithmetic_overflow;
        return result;
    }
    result.descriptor_error =
        entropy::internal::validate_contextual_tans_descriptor(
            descriptor, result.decision_count,
            static_cast<std::uint32_t>(result.payload_size), limits,
            result.descriptor_size, selected.layout.context_variant);
    if (result.descriptor_error
        != entropy::internal::ContextualTansFormatError::none) {
        result.error = LzssContextualTansFrameEncodeError::descriptor_error;
        return result;
    }
    if (result.descriptor_size > std::numeric_limits<std::uint32_t>::max()) {
        result.error =
            LzssContextualTansFrameEncodeError::arithmetic_overflow;
        return result;
    }

    const auto header = make_header(sequence, raw_input.size(), result);
    const LzssContextualTansFrameValidationContext frame_context{
        stream, limits, sequence, output_already_committed};
    result.header_error = validate_lzss_contextual_tans_frame_header(
        header, frame_context);
    if (result.header_error != LzssContextualTansFrameHeaderError::none) {
        result.error = LzssContextualTansFrameEncodeError::header_error;
        return result;
    }
    if (!core::checked_add(
            lzss_contextual_tans_frame_header_size, result.descriptor_size,
            result.serialized_size)
        || !core::checked_add(
            result.serialized_size, result.payload_size,
            result.serialized_size)) {
        result.error =
            LzssContextualTansFrameEncodeError::arithmetic_overflow;
        return result;
    }

    std::size_t token_workspace = result.token_encode.token_storage_size;
    if constexpr (UseHashChain) {
        if (!core::checked_multiply(
                raw_input.size(),
                sizeof(dictionary::internal::LzssTypedToken),
                token_workspace)) {
            result.error =
                LzssContextualTansFrameEncodeError::arithmetic_overflow;
            return result;
        }
    }
    std::size_t required_table_bytes{};
    std::size_t workspace{};
    if (!core::checked_multiply(
            result.required_table_entries, sizeof(std::uint16_t),
            required_table_bytes)
        || !core::checked_add(
            raw_input.size(), token_workspace, workspace)
        || !core::checked_add(
            workspace, required_table_bytes, workspace)
        || !core::checked_add(
            workspace, result.serialized_size, workspace)) {
        result.error =
            LzssContextualTansFrameEncodeError::arithmetic_overflow;
        return result;
    }
    if constexpr (UseHashChain) {
        const auto required = dictionary::internal::
            calculate_lzss_hash_chain_workspace(
                raw_input.size(), stream.dictionary, limits);
        if (required.error
            != dictionary::internal::LzssHashChainError::none) {
            result.error =
                LzssContextualTansFrameEncodeError::token_encode_error;
            result.token_encode.error = dictionary::internal::
                LzssTypedEncodeError::match_finder_error;
            result.token_encode.match_finder_error = required.error;
            return result;
        }
        if (!core::checked_add(
                workspace, required.workspace_size, workspace)) {
            result.error =
                LzssContextualTansFrameEncodeError::arithmetic_overflow;
            return result;
        }
    }
    if (workspace > limits.max_internal_buffered_bytes) {
        result.error = LzssContextualTansFrameEncodeError::workspace_limit;
    }
    return result;
}

template <bool UseHashChain>
[[nodiscard]] LzssContextualTansFrameEncodeResult encode_frame(
    const LzssContextualTansStreamHeader& stream,
    const core::DecoderLimits& limits, const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> raw_input,
    const std::span<dictionary::internal::LzssTypedToken> private_tokens,
    const std::span<std::uint16_t> private_encode_tables,
    const std::span<std::byte> match_finder_workspace,
    const std::span<std::byte> serialized_output,
    dictionary::internal::LzssMatchFinderStatistics* const statistics)
    noexcept {
    LzssContextualTansFrameEncodeResult result{};
    std::size_t token_capacity_bytes{};
    std::size_t table_capacity_bytes{};
    if (!calculate_capacity_bytes(
            private_tokens, private_encode_tables, token_capacity_bytes,
            table_capacity_bytes)) {
        result.error =
            LzssContextualTansFrameEncodeError::arithmetic_overflow;
        return result;
    }
    const std::array overlaps{
        regions_overlap(serialized_output.data(), serialized_output.size(),
                        raw_input.data(), raw_input.size()),
        regions_overlap(serialized_output.data(), serialized_output.size(),
                        private_tokens.data(), token_capacity_bytes),
        regions_overlap(serialized_output.data(), serialized_output.size(),
                        private_encode_tables.data(), table_capacity_bytes),
    };
    if (std::ranges::find(overlaps, OverlapCheck::arithmetic_overflow)
        != overlaps.end()) {
        return fail_overlap(result, OverlapCheck::arithmetic_overflow);
    }
    if (std::ranges::find(overlaps, OverlapCheck::overlap)
        != overlaps.end()) {
        return fail_overlap(result, OverlapCheck::overlap);
    }
    if constexpr (UseHashChain) {
        const auto overlap = regions_overlap(
            serialized_output.data(), serialized_output.size(),
            match_finder_workspace.data(), match_finder_workspace.size());
        if (overlap != OverlapCheck::disjoint)
            return fail_overlap(result, overlap);
    }

    result = plan_frame<UseHashChain>(
        stream, limits, sequence, output_already_committed, raw_input,
        private_tokens, private_encode_tables, match_finder_workspace,
        statistics);
    if (result.error != LzssContextualTansFrameEncodeError::none) {
        return result;
    }
    if (serialized_output.size() < result.serialized_size) {
        result.error =
            LzssContextualTansFrameEncodeError::serialized_output_too_small;
        return result;
    }

    const auto output = serialized_output.first(result.serialized_size);
    const auto selected = context::internal::select_lzss_field_context_layout(
        stream.dictionary_variant, stream.context_algorithm,
        stream.context_variant);
    if (selected.error
        != context::internal::LzssFieldContextLayoutError::none) {
        result.error = LzssContextualTansFrameEncodeError::internal_error;
        return result;
    }
    const auto tokens = private_tokens.first(result.token_count);
    std::size_t payload_offset{};
    if (!core::checked_add(
            lzss_contextual_tans_frame_header_size, result.descriptor_size,
            payload_offset)) {
        result.error = LzssContextualTansFrameEncodeError::internal_error;
        return result;
    }
    const dictionary::internal::LzssTypedFrameValidationContext token_context{
        static_cast<std::uint32_t>(result.token_count),
        static_cast<std::uint32_t>(raw_input.size()),
        output_already_committed};
    entropy::internal::ContextualTansDescriptor descriptor{};
    result.entropy_encode =
        context::internal::encode_lzss_contextual_tans_tokens(
            tokens, stream.dictionary, token_context, limits,
            private_encode_tables,
            output.subspan(payload_offset, result.payload_size), descriptor,
            selected.layout.context_variant);
    if (result.entropy_encode.error
            != context::internal::LzssContextualTansEncodeError::none
        || result.entropy_encode.event_count != result.event_count
        || result.entropy_encode.decision_count != result.decision_count
        || result.entropy_encode.payload_size != result.payload_size) {
        result.error = LzssContextualTansFrameEncodeError::internal_error;
        return result;
    }

    std::size_t descriptor_written{};
    result.descriptor_error =
        entropy::internal::serialize_contextual_tans_descriptor(
            descriptor, result.decision_count,
            static_cast<std::uint32_t>(result.payload_size), limits,
            output.subspan(
                lzss_contextual_tans_frame_header_size,
                result.descriptor_size),
            descriptor_written, selected.layout.context_variant);
    if (result.descriptor_error
            != entropy::internal::ContextualTansFormatError::none
        || descriptor_written != result.descriptor_size) {
        result.error = LzssContextualTansFrameEncodeError::internal_error;
        return result;
    }

    const auto header = make_header(sequence, raw_input.size(), result);
    const LzssContextualTansFrameValidationContext frame_context{
        stream, limits, sequence, output_already_committed};
    result.header_error = serialize_lzss_contextual_tans_frame_header(
        header, frame_context,
        std::span<std::byte, lzss_contextual_tans_frame_header_size>{
            output.data(), lzss_contextual_tans_frame_header_size});
    if (result.header_error != LzssContextualTansFrameHeaderError::none) {
        result.error = LzssContextualTansFrameEncodeError::internal_error;
    }
    return result;
}

LzssContextualTansFrameEncodeResult plan_lzss_contextual_tans_frame(
    const LzssContextualTansStreamHeader& stream,
    const core::DecoderLimits& limits, const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> raw_input,
    const std::span<dictionary::internal::LzssTypedToken> private_tokens,
    const std::span<std::uint16_t> private_encode_tables) noexcept {
    return plan_frame<false>(
        stream, limits, sequence, output_already_committed, raw_input,
        private_tokens, private_encode_tables, {}, nullptr);
}

LzssContextualTansFrameEncodeResult encode_lzss_contextual_tans_frame(
    const LzssContextualTansStreamHeader& stream,
    const core::DecoderLimits& limits, const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> raw_input,
    const std::span<dictionary::internal::LzssTypedToken> private_tokens,
    const std::span<std::uint16_t> private_encode_tables,
    const std::span<std::byte> serialized_output) noexcept {
    return encode_frame<false>(
        stream, limits, sequence, output_already_committed, raw_input,
        private_tokens, private_encode_tables, {}, serialized_output,
        nullptr);
}

LzssContextualTansFrameEncodeResult
plan_lzss_contextual_tans_frame_hash_chain(
    const LzssContextualTansStreamHeader& stream,
    const core::DecoderLimits& limits, const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> raw_input,
    const std::span<dictionary::internal::LzssTypedToken> private_tokens,
    const std::span<std::uint16_t> private_encode_tables,
    const std::span<std::byte> match_finder_workspace,
    dictionary::internal::LzssMatchFinderStatistics* const statistics)
    noexcept {
    return plan_frame<true>(
        stream, limits, sequence, output_already_committed, raw_input,
        private_tokens, private_encode_tables, match_finder_workspace,
        statistics);
}

LzssContextualTansFrameEncodeResult
encode_lzss_contextual_tans_frame_hash_chain(
    const LzssContextualTansStreamHeader& stream,
    const core::DecoderLimits& limits, const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> raw_input,
    const std::span<dictionary::internal::LzssTypedToken> private_tokens,
    const std::span<std::uint16_t> private_encode_tables,
    const std::span<std::byte> match_finder_workspace,
    const std::span<std::byte> serialized_output,
    dictionary::internal::LzssMatchFinderStatistics* const statistics)
    noexcept {
    return encode_frame<true>(
        stream, limits, sequence, output_already_committed, raw_input,
        private_tokens, private_encode_tables, match_finder_workspace,
        serialized_output, statistics);
}

} // namespace marc::frame::internal

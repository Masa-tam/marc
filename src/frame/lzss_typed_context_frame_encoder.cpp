#include "frame/lzss_typed_context_frame_encoder.hpp"

#include "core/checked_math.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

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
    if (!core::checked_add(first_begin,
                           static_cast<std::uintptr_t>(first_size), first_end)
        || !core::checked_add(second_begin,
                              static_cast<std::uintptr_t>(second_size),
                              second_end)) {
        return OverlapCheck::arithmetic_overflow;
    }
    return first_begin < second_end && second_begin < first_end
        ? OverlapCheck::overlap
        : OverlapCheck::disjoint;
}

[[nodiscard]] bool exact_input_size(
    const TypedContextStreamHeader& stream,
    const std::uint64_t output_already_committed,
    const std::size_t input_size) noexcept {
    if (output_already_committed >= stream.original_size) return false;
    const auto remaining = stream.original_size - output_already_committed;
    const auto expected = std::min<std::uint64_t>(stream.frame_size, remaining);
    return input_size == expected && input_size != 0
        && input_size <= std::numeric_limits<std::uint32_t>::max();
}

[[nodiscard]] bool checked_native_extent(
    const std::size_t count, const std::size_t element_size,
    std::size_t& extent) noexcept {
    return core::checked_multiply(count, element_size, extent);
}

[[nodiscard]] LzssTypedContextFrameEncodeResult fail_overlap(
    LzssTypedContextFrameEncodeResult result,
    const OverlapCheck overlap) noexcept {
    result.error = overlap == OverlapCheck::arithmetic_overflow
        ? LzssTypedContextFrameEncodeError::arithmetic_overflow
        : LzssTypedContextFrameEncodeError::overlapping_workspaces;
    return result;
}

} // namespace

template <bool UseHashChain>
[[nodiscard]] LzssTypedContextFrameEncodeResult plan_frame(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> raw_input,
    const std::span<dictionary::internal::LzssTypedToken> private_tokens,
    const std::span<context::internal::ModeledOperation> private_operations,
    const std::span<std::byte> match_finder_workspace,
    dictionary::internal::LzssMatchFinderStatistics* const statistics)
    noexcept {
    LzssTypedContextFrameEncodeResult result{};
    std::size_t token_capacity_bytes{};
    std::size_t operation_capacity_bytes{};
    if (!checked_native_extent(
            private_tokens.size(),
            sizeof(dictionary::internal::LzssTypedToken),
            token_capacity_bytes)
        || !checked_native_extent(
            private_operations.size(),
            sizeof(context::internal::ModeledOperation),
            operation_capacity_bytes)) {
        result.error = LzssTypedContextFrameEncodeError::arithmetic_overflow;
        return result;
    }
    for (const auto overlap : {
             regions_overlap(raw_input.data(), raw_input.size(),
                             private_tokens.data(), token_capacity_bytes),
             regions_overlap(raw_input.data(), raw_input.size(),
                             private_operations.data(),
                             operation_capacity_bytes),
             regions_overlap(private_tokens.data(), token_capacity_bytes,
                             private_operations.data(),
                             operation_capacity_bytes)}) {
        if (overlap != OverlapCheck::disjoint) {
            return fail_overlap(result, overlap);
        }
    }
    if constexpr (UseHashChain) {
        for (const auto overlap : {
                 regions_overlap(raw_input.data(), raw_input.size(),
                                 match_finder_workspace.data(),
                                 match_finder_workspace.size()),
                 regions_overlap(private_tokens.data(), token_capacity_bytes,
                                 match_finder_workspace.data(),
                                 match_finder_workspace.size()),
                 regions_overlap(private_operations.data(),
                                 operation_capacity_bytes,
                                 match_finder_workspace.data(),
                                 match_finder_workspace.size())}) {
            if (overlap != OverlapCheck::disjoint)
                return fail_overlap(result, overlap);
        }
    }
    if (validate_typed_context_stream_header(stream, limits)
        != TypedContextStreamHeaderError::none) {
        result.error = LzssTypedContextFrameEncodeError::invalid_stream;
        return result;
    }
    const auto selected = context::internal::select_lzss_field_context_layout(
        stream.dictionary_variant, stream.context_algorithm,
        stream.context_variant);
    if (selected.error
        != context::internal::LzssFieldContextLayoutError::none) {
        result.error = LzssTypedContextFrameEncodeError::invalid_stream;
        return result;
    }
    if (!exact_input_size(stream, output_already_committed,
                          raw_input.size())) {
        result.error = LzssTypedContextFrameEncodeError::input_size_mismatch;
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
        if (result.token_encode.error
            == dictionary::internal::LzssTypedEncodeError::output_too_small) {
            result.error =
                LzssTypedContextFrameEncodeError::token_staging_too_small;
            return result;
        }
        result.error = LzssTypedContextFrameEncodeError::token_encode_error;
        return result;
    }
    const auto tokens = private_tokens.first(result.token_count);
    if (result.token_count > std::numeric_limits<std::uint32_t>::max()) {
        result.error = LzssTypedContextFrameEncodeError::arithmetic_overflow;
        return result;
    }

    const dictionary::internal::LzssTypedFrameValidationContext token_context{
        static_cast<std::uint32_t>(result.token_count),
        static_cast<std::uint32_t>(raw_input.size()),
        output_already_committed};
    result.context_encode =
        context::internal::model_lzss_field_context_tokens(
            tokens, stream.dictionary, token_context, limits,
            private_operations, selected.layout.context_variant);
    result.operation_count = result.context_encode.operation_count;
    result.decision_count = result.context_encode.decision_count;
    if (result.context_encode.error
        != context::internal::LzssFieldContextError::none) {
        if (result.context_encode.error
            == context::internal::LzssFieldContextError::output_too_small) {
            result.error =
                LzssTypedContextFrameEncodeError::operation_staging_too_small;
            return result;
        }
        result.error = LzssTypedContextFrameEncodeError::context_encode_error;
        return result;
    }
    if (result.operation_count > std::numeric_limits<std::uint32_t>::max()) {
        result.error = LzssTypedContextFrameEncodeError::arithmetic_overflow;
        return result;
    }
    const auto operations = private_operations.first(result.operation_count);
    std::size_t operation_bytes{};
    if (!checked_native_extent(operations.size(),
                               sizeof(context::internal::ModeledOperation),
                               operation_bytes)) {
        result.error = LzssTypedContextFrameEncodeError::arithmetic_overflow;
        return result;
    }
    entropy::internal::ContextualDynamicRangeDescriptor descriptor{};
    result.entropy_encode =
        entropy::internal::plan_contextual_dynamic_range_operations(
            operations, limits, descriptor,
            selected.layout.context_variant);
    result.payload_size = result.entropy_encode.payload_size;
    if (result.entropy_encode.error
        != entropy::internal::ContextualDynamicRangeEncodeError::none) {
        result.error = LzssTypedContextFrameEncodeError::entropy_encode_error;
        return result;
    }
    if (result.entropy_encode.decision_count != result.decision_count
        || result.payload_size > std::numeric_limits<std::uint32_t>::max()) {
        result.error = LzssTypedContextFrameEncodeError::internal_error;
        return result;
    }

    TypedContextFrameHeader header{};
    header.sequence = sequence;
    header.uncompressed_size = static_cast<std::uint32_t>(raw_input.size());
    header.token_count = static_cast<std::uint32_t>(result.token_count);
    header.event_count = static_cast<std::uint32_t>(result.operation_count);
    header.decision_count = result.decision_count;
    header.payload_size = static_cast<std::uint32_t>(result.payload_size);
    header.descriptor_size = typed_context_range_descriptor_size;
    const TypedContextFrameValidationContext frame_context{
        stream, limits, sequence, output_already_committed};
    result.header_error =
        validate_typed_context_frame_header(header, frame_context);
    if (result.header_error != TypedContextFrameHeaderError::none) {
        result.error = LzssTypedContextFrameEncodeError::header_error;
        return result;
    }
    result.descriptor_error = validate_typed_context_range_descriptor(
        descriptor, header, limits);
    if (result.descriptor_error
        != TypedContextRangeDescriptorError::none) {
        result.error = LzssTypedContextFrameEncodeError::descriptor_error;
        return result;
    }
    if (!core::checked_add(
            typed_context_frame_header_size,
            typed_context_range_descriptor_size, result.serialized_size)
        || !core::checked_add(result.serialized_size, result.payload_size,
                              result.serialized_size)) {
        result.error = LzssTypedContextFrameEncodeError::arithmetic_overflow;
        return result;
    }

    std::size_t workspace{};
    std::size_t token_workspace = result.token_encode.token_storage_size;
    if constexpr (UseHashChain) {
        if (!core::checked_multiply(
                raw_input.size(),
                sizeof(dictionary::internal::LzssTypedToken),
                token_workspace)) {
            result.error = LzssTypedContextFrameEncodeError::arithmetic_overflow;
            return result;
        }
    }
    if (!core::checked_add(raw_input.size(), token_workspace, workspace)
        || !core::checked_add(workspace, operation_bytes, workspace)
        || !core::checked_add(workspace, result.serialized_size, workspace)) {
        result.error = LzssTypedContextFrameEncodeError::arithmetic_overflow;
        return result;
    }
    if constexpr (UseHashChain) {
        const auto required = dictionary::internal::
            calculate_lzss_hash_chain_workspace(
                raw_input.size(), stream.dictionary, limits);
        if (required.error
            != dictionary::internal::LzssHashChainError::none) {
            result.error = LzssTypedContextFrameEncodeError::token_encode_error;
            return result;
        }
        if (!core::checked_add(
                workspace, required.workspace_size, workspace)) {
            result.error = LzssTypedContextFrameEncodeError::arithmetic_overflow;
            return result;
        }
    }
    if (workspace > limits.max_internal_buffered_bytes) {
        result.error = LzssTypedContextFrameEncodeError::workspace_limit;
    }
    return result;
}

template <bool UseHashChain>
[[nodiscard]] LzssTypedContextFrameEncodeResult encode_frame(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> raw_input,
    const std::span<dictionary::internal::LzssTypedToken> private_tokens,
    const std::span<context::internal::ModeledOperation> private_operations,
    const std::span<std::byte> match_finder_workspace,
    const std::span<std::byte> serialized_output,
    dictionary::internal::LzssMatchFinderStatistics* const statistics) noexcept {
    LzssTypedContextFrameEncodeResult result{};
    std::size_t token_capacity_bytes{};
    std::size_t operation_capacity_bytes{};
    if (!checked_native_extent(
            private_tokens.size(),
            sizeof(dictionary::internal::LzssTypedToken),
            token_capacity_bytes)
        || !checked_native_extent(
            private_operations.size(),
            sizeof(context::internal::ModeledOperation),
            operation_capacity_bytes)) {
        result.error = LzssTypedContextFrameEncodeError::arithmetic_overflow;
        return result;
    }
    for (const auto overlap : {
             regions_overlap(serialized_output.data(),
                             serialized_output.size(), raw_input.data(),
                             raw_input.size()),
             regions_overlap(serialized_output.data(),
                             serialized_output.size(), private_tokens.data(),
                             token_capacity_bytes),
             regions_overlap(serialized_output.data(),
                             serialized_output.size(),
                             private_operations.data(),
                             operation_capacity_bytes)}) {
        if (overlap != OverlapCheck::disjoint) {
            return fail_overlap(result, overlap);
        }
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
        private_tokens, private_operations, match_finder_workspace,
        statistics);
    if (result.error != LzssTypedContextFrameEncodeError::none) return result;
    if (serialized_output.size() < result.serialized_size) {
        result.error =
            LzssTypedContextFrameEncodeError::serialized_output_too_small;
        return result;
    }
    const auto output = serialized_output.first(result.serialized_size);
    const auto operations = private_operations.first(result.operation_count);
    const auto selected = context::internal::select_lzss_field_context_layout(
        stream.dictionary_variant, stream.context_algorithm,
        stream.context_variant);
    if (selected.error
        != context::internal::LzssFieldContextLayoutError::none) {
        result.error = LzssTypedContextFrameEncodeError::internal_error;
        return result;
    }

    entropy::internal::ContextualDynamicRangeDescriptor descriptor{};
    const auto payload_offset = typed_context_frame_header_size
        + typed_context_range_descriptor_size;
    result.entropy_encode =
        entropy::internal::encode_contextual_dynamic_range_operations(
            operations, limits,
            output.subspan(payload_offset, result.payload_size), descriptor,
            selected.layout.context_variant);
    if (result.entropy_encode.error
            != entropy::internal::ContextualDynamicRangeEncodeError::none
        || result.entropy_encode.decision_count != result.decision_count
        || result.entropy_encode.payload_size != result.payload_size) {
        result.error = LzssTypedContextFrameEncodeError::internal_error;
        return result;
    }

    TypedContextFrameHeader header{};
    header.sequence = sequence;
    header.uncompressed_size = static_cast<std::uint32_t>(raw_input.size());
    header.token_count = static_cast<std::uint32_t>(result.token_count);
    header.event_count = static_cast<std::uint32_t>(result.operation_count);
    header.decision_count = result.decision_count;
    header.payload_size = static_cast<std::uint32_t>(result.payload_size);
    header.descriptor_size = typed_context_range_descriptor_size;
    const TypedContextFrameValidationContext frame_context{
        stream, limits, sequence, output_already_committed};
    result.header_error = serialize_typed_context_frame_header(
        header, frame_context,
        std::span<std::byte, typed_context_frame_header_size>{
            output.data(), typed_context_frame_header_size});
    if (result.header_error != TypedContextFrameHeaderError::none) {
        result.error = LzssTypedContextFrameEncodeError::internal_error;
        return result;
    }
    result.descriptor_error = serialize_typed_context_range_descriptor(
        descriptor, header, limits,
        std::span<std::byte, typed_context_range_descriptor_size>{
            output.data() + typed_context_frame_header_size,
            typed_context_range_descriptor_size});
    if (result.descriptor_error
        != TypedContextRangeDescriptorError::none) {
        result.error = LzssTypedContextFrameEncodeError::internal_error;
    }
    return result;
}

LzssTypedContextFrameEncodeResult plan_lzss_typed_context_frame(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> raw_input,
    const std::span<dictionary::internal::LzssTypedToken> private_tokens,
    const std::span<context::internal::ModeledOperation> private_operations)
    noexcept {
    return plan_frame<false>(
        stream, limits, sequence, output_already_committed, raw_input,
        private_tokens, private_operations, {}, nullptr);
}

LzssTypedContextFrameEncodeResult encode_lzss_typed_context_frame(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> raw_input,
    const std::span<dictionary::internal::LzssTypedToken> private_tokens,
    const std::span<context::internal::ModeledOperation> private_operations,
    const std::span<std::byte> serialized_output) noexcept {
    return encode_frame<false>(
        stream, limits, sequence, output_already_committed, raw_input,
        private_tokens, private_operations, {}, serialized_output, nullptr);
}

LzssTypedContextFrameEncodeResult plan_lzss_typed_context_frame_hash_chain(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> raw_input,
    const std::span<dictionary::internal::LzssTypedToken> private_tokens,
    const std::span<context::internal::ModeledOperation> private_operations,
    const std::span<std::byte> match_finder_workspace,
    dictionary::internal::LzssMatchFinderStatistics* const statistics)
    noexcept {
    return plan_frame<true>(
        stream, limits, sequence, output_already_committed, raw_input,
        private_tokens, private_operations, match_finder_workspace,
        statistics);
}

LzssTypedContextFrameEncodeResult encode_lzss_typed_context_frame_hash_chain(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    const std::uint64_t sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> raw_input,
    const std::span<dictionary::internal::LzssTypedToken> private_tokens,
    const std::span<context::internal::ModeledOperation> private_operations,
    const std::span<std::byte> match_finder_workspace,
    const std::span<std::byte> serialized_output,
    dictionary::internal::LzssMatchFinderStatistics* const statistics)
    noexcept {
    return encode_frame<true>(
        stream, limits, sequence, output_already_committed, raw_input,
        private_tokens, private_operations, match_finder_workspace,
        serialized_output, statistics);
}

} // namespace marc::frame::internal

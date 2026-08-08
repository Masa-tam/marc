#ifndef MARC_FRAME_LZSS_TYPED_CONTEXT_FRAME_ENCODER_HPP
#define MARC_FRAME_LZSS_TYPED_CONTEXT_FRAME_ENCODER_HPP

#include "context/lzss_field_context.hpp"
#include "dictionary/lzss_typed_encoder.hpp"
#include "entropy/contextual_dynamic_range_encoder.hpp"
#include "frame/typed_context_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame::internal {

enum class LzssTypedContextFrameEncodeError : std::uint8_t {
    none,
    invalid_stream,
    input_size_mismatch,
    token_staging_too_small,
    operation_staging_too_small,
    serialized_output_too_small,
    overlapping_workspaces,
    workspace_limit,
    token_encode_error,
    context_encode_error,
    entropy_encode_error,
    header_error,
    descriptor_error,
    arithmetic_overflow,
    internal_error,
};

struct LzssTypedContextFrameEncodeResult {
    std::size_t serialized_size{};
    std::size_t token_count{};
    std::size_t operation_count{};
    std::uint32_t decision_count{};
    std::size_t payload_size{};
    dictionary::internal::LzssTypedEncodeResult token_encode{};
    context::internal::LzssFieldContextResult context_encode{};
    entropy::internal::ContextualDynamicRangeEncodeResult entropy_encode{};
    TypedContextFrameHeaderError header_error{
        TypedContextFrameHeaderError::none};
    TypedContextRangeDescriptorError descriptor_error{
        TypedContextRangeDescriptorError::none};
    LzssTypedContextFrameEncodeError error{
        LzssTypedContextFrameEncodeError::none};
};

// Planning materializes private token and operation staging but never writes
// serialized output.
[[nodiscard]] LzssTypedContextFrameEncodeResult
plan_lzss_typed_context_frame(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> raw_input,
    std::span<dictionary::internal::LzssTypedToken> private_tokens,
    std::span<context::internal::ModeledOperation> private_operations) noexcept;

[[nodiscard]] LzssTypedContextFrameEncodeResult
encode_lzss_typed_context_frame(
    const TypedContextStreamHeader& stream,
    const core::DecoderLimits& limits,
    std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> raw_input,
    std::span<dictionary::internal::LzssTypedToken> private_tokens,
    std::span<context::internal::ModeledOperation> private_operations,
    std::span<std::byte> serialized_output) noexcept;

} // namespace marc::frame::internal

#endif

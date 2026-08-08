#ifndef MARC_CONTEXT_LZSS_FIELD_CONTEXT_HPP
#define MARC_CONTEXT_LZSS_FIELD_CONTEXT_HPP

#include "context/lzss_field_context_format.hpp"
#include "core/limits.hpp"
#include "dictionary/lzss_typed_token.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::context::internal {

enum class ModeledOperationKind : std::uint8_t {
    symbol,
    bypass_bits,
};

struct ModeledOperation {
    ModeledOperationKind kind{ModeledOperationKind::symbol};
    std::uint16_t context_id{};
    std::uint16_t alphabet_size{};
    std::uint32_t value{};
    std::uint8_t bit_count{};
};

struct LzssFieldContextValidationContext {
    std::uint32_t declared_token_count{};
    std::uint32_t declared_event_count{};
    std::uint32_t declared_decision_count{};
    std::uint32_t declared_raw_size{};
    std::uint64_t output_already_committed{};
};

enum class LzssFieldContextError : std::uint8_t {
    none,
    invalid_parameters,
    event_count_mismatch,
    token_count_mismatch,
    decision_count_mismatch,
    raw_size_mismatch,
    truncated_token,
    trailing_operations,
    unexpected_operation_kind,
    unexpected_context,
    unexpected_alphabet,
    invalid_symbol,
    invalid_bypass_width,
    nonzero_unused_field,
    invalid_token,
    trailing_tokens,
    output_too_small,
    overlapping_buffers,
    limit_exceeded,
    arithmetic_overflow,
};

struct LzssFieldContextResult {
    std::size_t operation_count{};
    std::size_t operation_index{};
    std::size_t token_count{};
    std::size_t token_index{};
    std::uint32_t decision_count{};
    std::uint64_t raw_size{};
    dictionary::internal::LzssTypedTokenError token_error{
        dictionary::internal::LzssTypedTokenError::none};
    LzssFieldContextError error{LzssFieldContextError::none};
};

[[nodiscard]] LzssFieldContextResult plan_lzss_field_context_operations(
    std::span<const dictionary::internal::LzssTypedToken> tokens,
    const dictionary::internal::LzssParameters& parameters,
    const dictionary::internal::LzssTypedFrameValidationContext& context,
    const core::DecoderLimits& limits) noexcept;

[[nodiscard]] LzssFieldContextResult model_lzss_field_context_tokens(
    std::span<const dictionary::internal::LzssTypedToken> tokens,
    const dictionary::internal::LzssParameters& parameters,
    const dictionary::internal::LzssTypedFrameValidationContext& context,
    const core::DecoderLimits& limits,
    std::span<ModeledOperation> private_operations) noexcept;

[[nodiscard]] LzssFieldContextResult validate_lzss_field_context_operations(
    std::span<const ModeledOperation> operations,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits) noexcept;

[[nodiscard]] LzssFieldContextResult invert_lzss_field_context_operations(
    std::span<const ModeledOperation> operations,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context,
    const core::DecoderLimits& limits,
    std::span<dictionary::internal::LzssTypedToken> private_tokens) noexcept;

} // namespace marc::context::internal

#endif

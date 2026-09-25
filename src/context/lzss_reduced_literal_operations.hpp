#ifndef MARC_CONTEXT_LZSS_REDUCED_LITERAL_OPERATIONS_HPP
#define MARC_CONTEXT_LZSS_REDUCED_LITERAL_OPERATIONS_HPP

#include "context/lzss_short_length_escape_operations.hpp"

namespace marc::context::internal {

// Private context-8 mapping. Token grammar/counts are unchanged from context 7.
// Validate the whole frame and output region before writing; no allocation.
[[nodiscard]] inline LzssFieldContextResult plan_lzss_reduced_literal_operations(
    const std::span<const dictionary::internal::LzssTypedToken> tokens,
    const dictionary::internal::LzssParameters& parameters,
    const dictionary::internal::LzssTypedFrameValidationContext& context,
    const core::DecoderLimits& limits) noexcept {
    return plan_lzss_short_length_escape_operations(tokens, parameters, context, limits);
}

[[nodiscard]] inline LzssFieldContextResult model_lzss_reduced_literal_tokens(
    const std::span<const dictionary::internal::LzssTypedToken> tokens,
    const dictionary::internal::LzssParameters& parameters,
    const dictionary::internal::LzssTypedFrameValidationContext& context,
    const core::DecoderLimits& limits,
    const std::span<ModeledOperation> operations) noexcept {
    const auto result = model_lzss_short_length_escape_tokens(
        tokens, parameters, context, limits, operations);
    if (result.error != LzssFieldContextError::none) return result;
    // Only rewrite successfully generated symbols. The old mapper has already
    // applied the last-Literal state rules; merging adjacent high-nibble bins
    // is exactly previous_literal >> 5. Alphabets and bypass bits do not change.
    for (auto& operation : operations.first(result.operation_count)) {
        if (operation.kind != ModeledOperationKind::symbol) continue;
        auto& id = operation.context_id;
        if (id >= 20) id = static_cast<std::uint16_t>(id - 8);
        else if (id >= 4) id = static_cast<std::uint16_t>(4 + (id - 4) / 2);
    }
    return result;
}

} // namespace marc::context::internal
#endif

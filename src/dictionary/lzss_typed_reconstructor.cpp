#include "dictionary/lzss_typed_reconstructor.hpp"

#include "core/checked_math.hpp"

#include <cstdint>
#include <utility>

namespace marc::dictionary::internal {
namespace {

enum class OverlapCheck : std::uint8_t {
    disjoint,
    overlap,
    arithmetic_overflow,
};

[[nodiscard]] OverlapCheck token_output_overlap(
    const std::span<const LzssTypedToken> tokens,
    const std::span<std::byte> output) noexcept {
    if (tokens.empty() || output.empty()) return OverlapCheck::disjoint;

    std::size_t token_bytes{};
    if (!core::checked_multiply(tokens.size(), sizeof(LzssTypedToken),
                                token_bytes)) {
        return OverlapCheck::arithmetic_overflow;
    }
    const auto token_begin =
        reinterpret_cast<std::uintptr_t>(tokens.data());
    const auto output_begin =
        reinterpret_cast<std::uintptr_t>(output.data());
    std::uintptr_t token_end{};
    std::uintptr_t output_end{};
    if (!core::checked_add(token_begin,
                           static_cast<std::uintptr_t>(token_bytes), token_end)
        || !core::checked_add(output_begin,
                              static_cast<std::uintptr_t>(output.size()),
                              output_end)) {
        return OverlapCheck::arithmetic_overflow;
    }
    return token_begin < output_end && output_begin < token_end
        ? OverlapCheck::overlap
        : OverlapCheck::disjoint;
}

} // namespace

LzssTypedReconstructResult reconstruct_lzss_typed_frame(
    const std::span<const LzssTypedToken> tokens,
    const LzssParameters& parameters,
    const LzssTypedFrameValidationContext& context,
    const core::DecoderLimits& limits,
    const std::span<std::byte> private_raw_output,
    const LzssTypedTokenVariant variant) noexcept {
    LzssTypedReconstructResult result{};
    result.validation =
        validate_lzss_typed_frame(tokens, parameters, context, limits,
                                  variant);
    if (result.validation.error != LzssTypedFrameValidationError::none) {
        result.error = LzssTypedReconstructError::invalid_token_frame;
        return result;
    }
    if (!std::in_range<std::size_t>(context.declared_raw_size)) {
        result.error = LzssTypedReconstructError::output_size_unsupported;
        return result;
    }
    result.output_size = static_cast<std::size_t>(context.declared_raw_size);
    if (private_raw_output.size() < result.output_size) {
        result.error = LzssTypedReconstructError::output_too_small;
        return result;
    }
    const auto output = private_raw_output.first(result.output_size);
    const auto overlap = token_output_overlap(tokens, output);
    if (overlap == OverlapCheck::arithmetic_overflow) {
        result.error = LzssTypedReconstructError::arithmetic_overflow;
        return result;
    }
    if (overlap == OverlapCheck::overlap) {
        result.error = LzssTypedReconstructError::overlapping_buffers;
        return result;
    }

    std::size_t produced{};
    for (const auto& token : tokens) {
        if (token.kind == LzssTypedTokenKind::literal) {
            output[produced++] = static_cast<std::byte>(token.literal);
            continue;
        }
        for (std::uint32_t copied = 0; copied < token.length; ++copied) {
            output[produced] = output[produced - token.distance];
            ++produced;
        }
    }
    return result;
}

} // namespace marc::dictionary::internal

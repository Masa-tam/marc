#ifndef MARC_CONTEXT_LZSS_POSITION_RANS_32M_FULL_LITERAL_TOKENS_HPP
#define MARC_CONTEXT_LZSS_POSITION_RANS_32M_FULL_LITERAL_TOKENS_HPP
#include "context/lzss_field_context.hpp"
#include "entropy/position_rans_32m_full_literal_encoder.hpp"
#include "entropy/position_rans_32m_full_literal_decoder.hpp"

namespace marc::context::internal {
enum class LzssPositionRans32mFullLiteralTokenError : std::uint8_t {
    none, invalid_parameters, invalid_counts, invalid_token, raw_size_mismatch,
    entropy_error, limit_exceeded, arithmetic_overflow, output_too_small,
    overlapping_buffers
};
struct LzssPositionRans32mFullLiteralTokenResult {
    std::uint32_t token_count{}, event_count{}, decision_count{};
    std::uint64_t raw_size{};
    std::size_t descriptor_size{}, payload_size{};
    dictionary::internal::LzssTypedTokenError token_error{};
    entropy::internal::PositionRans32mFullLiteralEncodeError encode_error{};
    entropy::internal::PositionRans32mFullLiteralDecodeError decode_error{};
    LzssPositionRans32mFullLiteralTokenError error{};
};
inline constexpr std::size_t lzss_position_rans_32m_full_literal_fixed_working_bytes = 131072;
[[nodiscard]] LzssPositionRans32mFullLiteralTokenResult plan_lzss_position_rans_32m_full_literal_tokens(
    std::span<const dictionary::internal::LzssTypedToken> tokens,
    const dictionary::internal::LzssParameters& parameters,
    const dictionary::internal::LzssTypedFrameValidationContext& context,
    const core::DecoderLimits& limits,
    entropy::internal::PositionRans32mFullLiteralDescriptor& descriptor) noexcept;
[[nodiscard]] LzssPositionRans32mFullLiteralTokenResult encode_lzss_position_rans_32m_full_literal_tokens(
    std::span<const dictionary::internal::LzssTypedToken> tokens,
    const dictionary::internal::LzssParameters& parameters,
    const dictionary::internal::LzssTypedFrameValidationContext& context,
    const core::DecoderLimits& limits,
    std::span<std::byte> descriptor_output, std::span<std::byte> payload_output) noexcept;
[[nodiscard]] LzssPositionRans32mFullLiteralTokenResult validate_lzss_position_rans_32m_full_literal_tokens(
    std::span<const std::byte> descriptor, std::span<const std::byte> payload,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context, const core::DecoderLimits& limits) noexcept;
[[nodiscard]] LzssPositionRans32mFullLiteralTokenResult decode_lzss_position_rans_32m_full_literal_tokens(
    std::span<const std::byte> descriptor, std::span<const std::byte> payload,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context, const core::DecoderLimits& limits,
    std::span<dictionary::internal::LzssTypedToken> output) noexcept;
// A failed call may leave a prefix in private scratch; discard on failure.
[[nodiscard]] LzssPositionRans32mFullLiteralTokenResult decode_lzss_position_rans_32m_full_literal_token_scratch(
    std::span<const std::byte> descriptor, std::span<const std::byte> payload,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context, const core::DecoderLimits& limits,
    std::span<dictionary::internal::LzssTypedToken> scratch) noexcept;
} // namespace marc::context::internal
#endif

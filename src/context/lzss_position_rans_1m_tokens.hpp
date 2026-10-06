#ifndef MARC_CONTEXT_LZSS_POSITION_RANS_1M_TOKENS_HPP
#define MARC_CONTEXT_LZSS_POSITION_RANS_1M_TOKENS_HPP
#include "context/lzss_field_context.hpp"
#include "entropy/position_rans_1m_encoder.hpp"
#include "entropy/position_rans_1m_decoder.hpp"

namespace marc::context::internal {
enum class LzssPositionRans1mTokenError : std::uint8_t {
    none, invalid_parameters, invalid_counts, invalid_token, raw_size_mismatch,
    entropy_error, limit_exceeded, arithmetic_overflow, output_too_small,
    overlapping_buffers
};
struct LzssPositionRans1mTokenResult {
    std::uint32_t token_count{}, event_count{}, decision_count{};
    std::uint64_t raw_size{};
    std::size_t descriptor_size{}, payload_size{};
    dictionary::internal::LzssTypedTokenError token_error{};
    entropy::internal::PositionRans1mEncodeError encode_error{};
    entropy::internal::PositionRans1mDecodeError decode_error{};
    LzssPositionRans1mTokenError error{};
};
inline constexpr std::size_t lzss_position_rans_1m_fixed_working_bytes = 65536;
[[nodiscard]] LzssPositionRans1mTokenResult plan_lzss_position_rans_1m_tokens(
    std::span<const dictionary::internal::LzssTypedToken> tokens,
    const dictionary::internal::LzssParameters& parameters,
    const dictionary::internal::LzssTypedFrameValidationContext& context,
    const core::DecoderLimits& limits,
    entropy::internal::PositionRans1mDescriptor& descriptor) noexcept;
[[nodiscard]] LzssPositionRans1mTokenResult encode_lzss_position_rans_1m_tokens(
    std::span<const dictionary::internal::LzssTypedToken> tokens,
    const dictionary::internal::LzssParameters& parameters,
    const dictionary::internal::LzssTypedFrameValidationContext& context,
    const core::DecoderLimits& limits,
    std::span<std::byte> descriptor_output, std::span<std::byte> payload_output) noexcept;
[[nodiscard]] LzssPositionRans1mTokenResult validate_lzss_position_rans_1m_tokens(
    std::span<const std::byte> descriptor, std::span<const std::byte> payload,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context, const core::DecoderLimits& limits) noexcept;
[[nodiscard]] LzssPositionRans1mTokenResult decode_lzss_position_rans_1m_tokens(
    std::span<const std::byte> descriptor, std::span<const std::byte> payload,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context, const core::DecoderLimits& limits,
    std::span<dictionary::internal::LzssTypedToken> output) noexcept;
// A failed call may leave a prefix in private scratch; discard on failure.
[[nodiscard]] LzssPositionRans1mTokenResult decode_lzss_position_rans_1m_token_scratch(
    std::span<const std::byte> descriptor, std::span<const std::byte> payload,
    const dictionary::internal::LzssParameters& parameters,
    const LzssFieldContextValidationContext& context, const core::DecoderLimits& limits,
    std::span<dictionary::internal::LzssTypedToken> scratch) noexcept;
} // namespace marc::context::internal
#endif

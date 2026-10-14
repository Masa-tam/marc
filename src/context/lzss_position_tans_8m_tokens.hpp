#ifndef MARC_CONTEXT_LZSS_POSITION_TANS_8M_TOKENS_HPP
#define MARC_CONTEXT_LZSS_POSITION_TANS_8M_TOKENS_HPP
#include "dictionary/lzss_typed_token.hpp"
#include "entropy/position_tans_8m.hpp"

namespace marc::context::internal {
struct PositionTans8mTokenContext {
    std::uint32_t token_count{}, raw_size{}, decision_count{};
    std::uint64_t output_already_committed{};
};
enum class PositionTans8mTokenError : std::uint8_t {
    none, invalid_counts, invalid_token, entropy_error, output_too_small,
    limit_exceeded, arithmetic_overflow, overlapping_buffers
};
struct PositionTans8mTokenResult {
    // Counts describe private validation progress, not published bytes/tokens.
    std::uint32_t token_count{}, raw_size{}, decision_count{};
    std::size_t descriptor_size{}, payload_size{};
    dictionary::internal::LzssTypedTokenError token_error{};
    entropy::internal::PositionTans8mError entropy_error{};
    PositionTans8mTokenError error{};
};
// Tables and payload scratch may change on failure; discard their contents.
// Ordinary descriptor/encoded bytes/token destinations commit only on success.
[[nodiscard]] PositionTans8mTokenResult plan_position_tans_8m_tokens(
    std::span<const dictionary::internal::LzssTypedToken>,
    const dictionary::internal::LzssTypedFrameValidationContext&,
    const core::DecoderLimits&, std::span<std::uint16_t> private_tables,
    entropy::internal::PositionTans8mDescriptor&) noexcept;
[[nodiscard]] PositionTans8mTokenResult encode_position_tans_8m_tokens(
    std::span<const dictionary::internal::LzssTypedToken>,
    const dictionary::internal::LzssTypedFrameValidationContext&,
    const core::DecoderLimits&, std::span<std::uint16_t> private_tables,
    std::span<std::byte> private_payload, std::span<std::byte> descriptor_output,
    std::span<std::byte> payload_output) noexcept;
[[nodiscard]] PositionTans8mTokenResult decode_position_tans_8m_token_scratch(
    std::span<const std::byte> descriptor, std::span<const std::byte> payload,
    const PositionTans8mTokenContext&, const core::DecoderLimits&,
    std::span<entropy::internal::TansDecodeEntry> private_tables,
    std::span<dictionary::internal::LzssTypedToken> private_tokens) noexcept;
[[nodiscard]] PositionTans8mTokenResult decode_position_tans_8m_tokens(
    std::span<const std::byte> descriptor, std::span<const std::byte> payload,
    const PositionTans8mTokenContext&, const core::DecoderLimits&,
    std::span<entropy::internal::TansDecodeEntry> private_tables,
    std::span<dictionary::internal::LzssTypedToken> private_tokens,
    std::span<dictionary::internal::LzssTypedToken> output) noexcept;
} // namespace marc::context::internal
#endif




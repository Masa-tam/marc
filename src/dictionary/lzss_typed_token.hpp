#ifndef MARC_DICTIONARY_LZSS_TYPED_TOKEN_HPP
#define MARC_DICTIONARY_LZSS_TYPED_TOKEN_HPP

#include "core/limits.hpp"
#include "dictionary/lzss_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

enum class LzssTypedTokenVariant : std::uint16_t {
    field_context_64k = 2,
    field_context_1m = 3,
};

enum class LzssTypedTokenKind : std::uint8_t {
    literal = 0,
    match = 1,
};

struct LzssTypedToken {
    LzssTypedTokenKind kind{LzssTypedTokenKind::literal};
    std::uint8_t literal{};
    std::uint32_t distance{};
    std::uint32_t length{};
};

struct LzssTypedTokenValidationContext {
    std::uint64_t raw_already_produced{};
    std::uint64_t declared_raw_size{};
};

enum class LzssTypedTokenError : std::uint8_t {
    none,
    invalid_parameters,
    unknown_kind,
    nonzero_unused_field,
    invalid_distance,
    invalid_length,
    output_size_mismatch,
    limit_exceeded,
    arithmetic_overflow,
};

[[nodiscard]] LzssTypedTokenError validate_lzss_typed_parameters(
    const LzssParameters& parameters,
    const core::DecoderLimits& limits,
    LzssTypedTokenVariant variant =
        LzssTypedTokenVariant::field_context_64k) noexcept;

[[nodiscard]] LzssTypedTokenError validate_lzss_typed_token(
    const LzssTypedToken& token,
    const LzssParameters& parameters,
    const LzssTypedTokenValidationContext& context,
    const core::DecoderLimits& limits,
    std::uint64_t& next_raw_size,
    LzssTypedTokenVariant variant =
        LzssTypedTokenVariant::field_context_64k) noexcept;

struct LzssTypedFrameValidationContext {
    std::uint32_t declared_token_count{};
    std::uint32_t declared_raw_size{};
    std::uint64_t output_already_committed{};
};

enum class LzssTypedFrameValidationError : std::uint8_t {
    none,
    invalid_parameters,
    token_count_mismatch,
    token_error,
    premature_end,
    trailing_tokens,
    limit_exceeded,
    arithmetic_overflow,
};

struct LzssTypedFrameValidationResult {
    std::size_t token_count{};
    std::size_t token_index{};
    std::uint64_t raw_size{};
    LzssTypedTokenError token_error{LzssTypedTokenError::none};
    LzssTypedFrameValidationError error{
        LzssTypedFrameValidationError::none};
};

[[nodiscard]] LzssTypedFrameValidationResult validate_lzss_typed_frame(
    std::span<const LzssTypedToken> tokens,
    const LzssParameters& parameters,
    const LzssTypedFrameValidationContext& context,
    const core::DecoderLimits& limits,
    LzssTypedTokenVariant variant =
        LzssTypedTokenVariant::field_context_64k) noexcept;

} // namespace marc::dictionary::internal

#endif

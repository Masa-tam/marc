#ifndef MARC_DICTIONARY_LZSS_TYPED_ENCODER_HPP
#define MARC_DICTIONARY_LZSS_TYPED_ENCODER_HPP

#include "dictionary/lzss_typed_token.hpp"
#include "dictionary/lzss_hash_chain_match_finder.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

enum class LzssTypedEncodeError : std::uint8_t {
    none,
    invalid_parameters,
    input_limit_exceeded,
    token_storage_limit_exceeded,
    output_too_small,
    overlapping_buffers,
    arithmetic_overflow,
    match_finder_error,
    internal_error,
};

struct LzssTypedEncodeResult {
    std::size_t input_size{};
    std::size_t token_count{};
    std::size_t token_storage_size{};
    LzssTypedTokenError token_error{LzssTypedTokenError::none};
    LzssTypedEncodeError error{LzssTypedEncodeError::none};
    LzssHashChainError match_finder_error{LzssHashChainError::none};
};

[[nodiscard]] LzssTypedEncodeResult plan_lzss_typed_tokens(
    std::span<const std::byte> input,
    const LzssParameters& parameters,
    const core::DecoderLimits& limits,
    LzssTypedTokenVariant variant =
        LzssTypedTokenVariant::field_context_64k) noexcept;

[[nodiscard]] LzssTypedEncodeResult encode_lzss_typed_tokens(
    std::span<const std::byte> input,
    const LzssParameters& parameters,
    const core::DecoderLimits& limits,
    std::span<LzssTypedToken> private_tokens,
    LzssTypedTokenVariant variant =
        LzssTypedTokenVariant::field_context_64k) noexcept;

[[nodiscard]] LzssTypedEncodeResult plan_lzss_typed_tokens_hash_chain(
    std::span<const std::byte> input, const LzssParameters& parameters,
    const core::DecoderLimits& limits,
    std::span<std::byte> match_finder_workspace,
    LzssTypedTokenVariant variant =
        LzssTypedTokenVariant::field_context_64k) noexcept;

[[nodiscard]] LzssTypedEncodeResult encode_lzss_typed_tokens_hash_chain(
    std::span<const std::byte> input, const LzssParameters& parameters,
    const core::DecoderLimits& limits,
    std::span<LzssTypedToken> private_tokens,
    std::span<std::byte> match_finder_workspace,
    LzssTypedTokenVariant variant =
        LzssTypedTokenVariant::field_context_64k) noexcept;

[[nodiscard]] LzssTypedEncodeResult
encode_lzss_typed_tokens_hash_chain_single_pass(
    std::span<const std::byte> input, const LzssParameters& parameters,
    const core::DecoderLimits& limits,
    std::span<LzssTypedToken> private_tokens,
    std::span<std::byte> match_finder_workspace,
    LzssMatchFinderStatistics* statistics = nullptr,
    LzssTypedTokenVariant variant =
        LzssTypedTokenVariant::field_context_64k) noexcept;

} // namespace marc::dictionary::internal

#endif

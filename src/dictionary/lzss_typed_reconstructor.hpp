#ifndef MARC_DICTIONARY_LZSS_TYPED_RECONSTRUCTOR_HPP
#define MARC_DICTIONARY_LZSS_TYPED_RECONSTRUCTOR_HPP

#include "dictionary/lzss_typed_token.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

enum class LzssTypedReconstructError : std::uint8_t {
    none,
    invalid_token_frame,
    output_size_unsupported,
    output_too_small,
    overlapping_buffers,
    arithmetic_overflow,
};

struct LzssTypedReconstructResult {
    std::size_t output_size{};
    LzssTypedFrameValidationResult validation{};
    LzssTypedReconstructError error{LzssTypedReconstructError::none};
};

[[nodiscard]] LzssTypedReconstructResult reconstruct_lzss_typed_frame(
    std::span<const LzssTypedToken> tokens,
    const LzssParameters& parameters,
    const LzssTypedFrameValidationContext& context,
    const core::DecoderLimits& limits,
    std::span<std::byte> private_raw_output,
    LzssTypedTokenVariant variant =
        LzssTypedTokenVariant::field_context_64k) noexcept;

} // namespace marc::dictionary::internal

#endif

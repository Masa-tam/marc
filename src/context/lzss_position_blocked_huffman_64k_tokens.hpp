#ifndef MARC_CONTEXT_LZSS_POSITION_BLOCKED_HUFFMAN_64K_TOKENS_HPP
#define MARC_CONTEXT_LZSS_POSITION_BLOCKED_HUFFMAN_64K_TOKENS_HPP
#include "dictionary/lzss_typed_token.hpp"
#include "entropy/position_blocked_huffman_64k.hpp"

namespace marc::context::internal {
// Conservative simultaneous workspace charges; caller spans/control objects
// are additional. These are neither measured process peaks nor public grants.
inline constexpr std::size_t position_blocked_huffman_64k_plan_workspace=
    sizeof(entropy::internal::PositionBlockedHuffman64kModelBuilder)+2*sizeof(entropy::internal::PositionBlockedHuffman64kDescriptor)+131072+2518;
inline constexpr std::size_t position_blocked_huffman_64k_encode_workspace=
    position_blocked_huffman_64k_plan_workspace+sizeof(entropy::internal::PositionBlockedHuffman64kDescriptor)
    +sizeof(entropy::internal::PositionBlockedHuffman64kPrivateWriter)+2*2518;
inline constexpr std::size_t position_blocked_huffman_64k_decode_workspace=
    2*sizeof(entropy::internal::PositionBlockedHuffman64kDescriptor)+sizeof(entropy::internal::PositionBlockedHuffman64kDecoder)+16384+2518;
struct PositionBlockedHuffman64kTokenContext {
    std::uint32_t token_count{},raw_size{},decision_count{};
    std::uint64_t output_already_committed{};
};
enum class PositionBlockedHuffman64kTokenError : std::uint8_t {
    none,invalid_counts,invalid_token,entropy_error,output_too_small,
    limit_exceeded,arithmetic_overflow,overlapping_buffers
};
struct PositionBlockedHuffman64kTokenResult {
    // Private validation progress only, never publication counts on failure.
    std::uint32_t token_count{},raw_size{},decision_count{};
    std::size_t descriptor_size{},payload_size{};
    dictionary::internal::LzssTypedTokenError token_error{};
    entropy::internal::PositionBlockedHuffmanError entropy_error{};
    PositionBlockedHuffman64kTokenError error{};
};
// Ordinary descriptor/payload/token destinations commit only on success.
// All private tables/payload/tokens may change on failure and must be discarded.
[[nodiscard]] PositionBlockedHuffman64kTokenResult plan_position_blocked_huffman_64k_tokens(
    std::span<const dictionary::internal::LzssTypedToken>,
    const dictionary::internal::LzssTypedFrameValidationContext&,const core::DecoderLimits&,
    entropy::internal::PositionBlockedHuffman64kDescriptor&) noexcept;
[[nodiscard]] PositionBlockedHuffman64kTokenResult encode_position_blocked_huffman_64k_tokens(
    std::span<const dictionary::internal::LzssTypedToken>,
    const dictionary::internal::LzssTypedFrameValidationContext&,const core::DecoderLimits&,
    std::span<entropy::internal::CanonicalHuffmanTable> private_tables,
    std::span<std::byte> private_payload,std::span<std::byte> descriptor_output,
    std::span<std::byte> payload_output) noexcept;
// Decode once into private token scratch. No raw frame is published here.
[[nodiscard]] PositionBlockedHuffman64kTokenResult decode_position_blocked_huffman_64k_token_scratch(
    std::span<const std::byte> descriptor,std::span<const std::byte> payload,
    const PositionBlockedHuffman64kTokenContext&,const core::DecoderLimits&,
    std::span<entropy::internal::HuffmanDecodeTable> private_tables,
    std::span<dictionary::internal::LzssTypedToken> private_tokens) noexcept;
[[nodiscard]] PositionBlockedHuffman64kTokenResult decode_position_blocked_huffman_64k_tokens(
    std::span<const std::byte> descriptor,std::span<const std::byte> payload,
    const PositionBlockedHuffman64kTokenContext&,const core::DecoderLimits&,
    std::span<entropy::internal::HuffmanDecodeTable> private_tables,
    std::span<dictionary::internal::LzssTypedToken> private_tokens,
    std::span<dictionary::internal::LzssTypedToken> output) noexcept;
} // namespace marc::context::internal
#endif

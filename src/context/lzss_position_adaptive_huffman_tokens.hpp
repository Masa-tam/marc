#ifndef MARC_CONTEXT_LZSS_POSITION_ADAPTIVE_HUFFMAN_TOKENS_HPP
#define MARC_CONTEXT_LZSS_POSITION_ADAPTIVE_HUFFMAN_TOKENS_HPP
#include "dictionary/lzss_typed_token.hpp"
#include "entropy/position_adaptive_huffman.hpp"

namespace marc::context::internal {
struct PositionAdaptiveHuffmanTokenContext {
    std::uint32_t token_count{},raw_size{},decision_count{};
    std::uint64_t output_already_committed{};
};
enum class PositionAdaptiveHuffmanTokenError : std::uint8_t {
    none,invalid_counts,invalid_token,entropy_error,output_too_small,
    limit_exceeded,arithmetic_overflow,overlapping_buffers,invalid_profile
};
struct PositionAdaptiveHuffmanTokenResult {
    // Success counts only. Every failure returns zero sizes and counts.
    std::uint32_t token_count{},raw_size{},decision_count{};
    std::size_t descriptor_size{},payload_size{};
    dictionary::internal::LzssTypedTokenError token_error{};
    entropy::internal::PositionAdaptiveHuffmanError entropy_error{};
    PositionAdaptiveHuffmanTokenError error{};
};
// Nodes, symbol indices, scratch payload/tokens are private and may change on
// error. Ordinary descriptors/payloads/tokens and their unused tails commit
// only after the whole operation has succeeded. Complete raw-frame publication
// additionally requires reconstruction and frame validation at the outer layer.
[[nodiscard]] PositionAdaptiveHuffmanTokenResult plan_position_adaptive_huffman_tokens(
    entropy::internal::PositionAdaptiveHuffmanProfile,std::span<const dictionary::internal::LzssTypedToken>,
    const dictionary::internal::LzssTypedFrameValidationContext&,const core::DecoderLimits&,
    std::span<entropy::internal::AdaptiveHuffmanNode>,std::span<std::uint16_t>,
    entropy::internal::PositionAdaptiveHuffmanDescriptor&) noexcept;
[[nodiscard]] PositionAdaptiveHuffmanTokenResult encode_position_adaptive_huffman_tokens(
    entropy::internal::PositionAdaptiveHuffmanProfile,std::span<const dictionary::internal::LzssTypedToken>,
    const dictionary::internal::LzssTypedFrameValidationContext&,const core::DecoderLimits&,
    std::span<entropy::internal::AdaptiveHuffmanNode>,std::span<std::uint16_t>,
    std::span<std::byte> private_payload,std::span<std::byte> descriptor_output,
    std::span<std::byte> payload_output) noexcept;
// Frame integration may reuse a validated plan. This entry still checks typed
// tokens, descriptor bounds and exact emitted decisions/bits before committing.
[[nodiscard]] PositionAdaptiveHuffmanTokenResult write_position_adaptive_huffman_tokens(
    entropy::internal::PositionAdaptiveHuffmanProfile,std::span<const dictionary::internal::LzssTypedToken>,
    const dictionary::internal::LzssTypedFrameValidationContext&,
    const entropy::internal::PositionAdaptiveHuffmanDescriptor&,const core::DecoderLimits&,
    std::span<entropy::internal::AdaptiveHuffmanNode>,std::span<std::uint16_t>,
    std::span<std::byte> private_payload,std::span<std::byte> descriptor_output,
    std::span<std::byte> payload_output) noexcept;
// Owned-frame entry: the payload is private and may change on failure. No
// ordinary descriptor or payload is published; the validated plan is retained
// by the caller. Emitted counts remain zero on failure.
[[nodiscard]] PositionAdaptiveHuffmanTokenResult write_position_adaptive_huffman_token_scratch(
    entropy::internal::PositionAdaptiveHuffmanProfile,std::span<const dictionary::internal::LzssTypedToken>,
    const dictionary::internal::LzssTypedFrameValidationContext&,
    const entropy::internal::PositionAdaptiveHuffmanDescriptor&,const core::DecoderLimits&,
    std::span<entropy::internal::AdaptiveHuffmanNode>,std::span<std::uint16_t>,
    std::span<std::byte> private_payload) noexcept;
[[nodiscard]] PositionAdaptiveHuffmanTokenResult decode_position_adaptive_huffman_token_scratch(
    entropy::internal::PositionAdaptiveHuffmanProfile,std::span<const std::byte> descriptor,
    std::span<const std::byte> payload,const PositionAdaptiveHuffmanTokenContext&,
    const core::DecoderLimits&,std::span<entropy::internal::AdaptiveHuffmanNode>,
    std::span<std::uint16_t>,std::span<dictionary::internal::LzssTypedToken> private_tokens) noexcept;
[[nodiscard]] PositionAdaptiveHuffmanTokenResult decode_position_adaptive_huffman_tokens(
    entropy::internal::PositionAdaptiveHuffmanProfile,std::span<const std::byte> descriptor,
    std::span<const std::byte> payload,const PositionAdaptiveHuffmanTokenContext&,
    const core::DecoderLimits&,std::span<entropy::internal::AdaptiveHuffmanNode>,
    std::span<std::uint16_t>,std::span<dictionary::internal::LzssTypedToken> private_tokens,
    std::span<dictionary::internal::LzssTypedToken> output) noexcept;
} // namespace marc::context::internal
#endif

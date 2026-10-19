#ifndef MARC_ENTROPY_POSITION_ADAPTIVE_HUFFMAN_HPP
#define MARC_ENTROPY_POSITION_ADAPTIVE_HUFFMAN_HPP
#include "entropy/position_adaptive_huffman_format.hpp"
#include "entropy/contextual_adaptive_huffman_tree.hpp"
#include <array>

namespace marc::entropy::internal {
// Minimum conservative control+descriptor+model+payload charge, not RSS.
// Caller buffers larger than these minima are charged at their supplied sizes.
[[nodiscard]] PositionAdaptiveHuffmanError position_adaptive_huffman_workspace(
    PositionAdaptiveHuffmanProfile,std::size_t payload_bytes,std::size_t& bytes) noexcept;
class PositionAdaptiveHuffmanModelBank {
public:
    [[nodiscard]] PositionAdaptiveHuffmanError initialize(PositionAdaptiveHuffmanProfile,
        const core::DecoderLimits&,std::span<AdaptiveHuffmanNode>,std::span<std::uint16_t>) noexcept;
    [[nodiscard]] ContextualAdaptiveHuffmanTree* tree(int context) noexcept;
    [[nodiscard]] bool validate() const noexcept;
private:
    std::array<ContextualAdaptiveHuffmanTree,58> trees_{};
    const PositionAdaptiveHuffmanSpec* spec_{};
};
class PositionAdaptiveHuffmanEncoder {
public:
    [[nodiscard]] PositionAdaptiveHuffmanError begin_plan(PositionAdaptiveHuffmanProfile,
        const core::DecoderLimits&,std::span<AdaptiveHuffmanNode>,std::span<std::uint16_t>) noexcept;
    // All output and model workspaces are PRIVATE scratch. Discard on failure.
    // The output span is exactly the planned payload size; a caller may pass
    // the appropriate prefix of a larger allocation.
    [[nodiscard]] PositionAdaptiveHuffmanError begin_write(PositionAdaptiveHuffmanProfile,
        const PositionAdaptiveHuffmanDescriptor&,const core::DecoderLimits&,
        std::span<AdaptiveHuffmanNode>,std::span<std::uint16_t>,std::span<std::byte>) noexcept;
    [[nodiscard]] PositionAdaptiveHuffmanError emit(int context,std::uint32_t symbol) noexcept;
    // Ordinary descriptor commits only when the entire planning pass succeeds.
    [[nodiscard]] PositionAdaptiveHuffmanError finish_plan(PositionAdaptiveHuffmanDescriptor&) noexcept;
    [[nodiscard]] PositionAdaptiveHuffmanError finish_write() noexcept;
private:
    [[nodiscard]] PositionAdaptiveHuffmanError begin(PositionAdaptiveHuffmanProfile,
        const core::DecoderLimits&,std::span<AdaptiveHuffmanNode>,std::span<std::uint16_t>,
        std::span<std::byte>,const PositionAdaptiveHuffmanDescriptor*) noexcept;
    PositionAdaptiveHuffmanModelBank bank_{};
    PositionAdaptiveHuffmanProfile profile_{};
    const PositionAdaptiveHuffmanSpec* spec_{};
    core::DecoderLimits limits_{};
    std::span<AdaptiveHuffmanNode> nodes_{};
    std::span<std::uint16_t> symbols_{};
    std::span<std::byte> output_{};
    PositionAdaptiveHuffmanDescriptor expected_{};
    std::size_t bits_{};
    std::uint32_t decisions_{};
    PositionAdaptiveHuffmanError error_{PositionAdaptiveHuffmanError::not_started};
    bool writing_{},finished_{};
};
class PositionAdaptiveHuffmanDecoder {
public:
    [[nodiscard]] PositionAdaptiveHuffmanError begin(PositionAdaptiveHuffmanProfile,
        const PositionAdaptiveHuffmanDescriptor&,std::span<const std::byte>,
        const core::DecoderLimits&,std::span<AdaptiveHuffmanNode>,std::span<std::uint16_t>) noexcept;
    // Each ordinary symbol destination commits only on success. Tokens and raw
    // frames still remain private until their own full validation succeeds.
    [[nodiscard]] PositionAdaptiveHuffmanError read(int context,std::uint32_t&) noexcept;
    [[nodiscard]] PositionAdaptiveHuffmanError finish() noexcept;
private:
    PositionAdaptiveHuffmanModelBank bank_{};
    const PositionAdaptiveHuffmanSpec* spec_{};
    PositionAdaptiveHuffmanDescriptor expected_{};
    std::span<const std::byte> payload_{};
    std::span<AdaptiveHuffmanNode> nodes_{};
    std::span<std::uint16_t> symbols_{};
    std::size_t bits_{},cursor_{};
    std::uint32_t decisions_{};
    PositionAdaptiveHuffmanError error_{PositionAdaptiveHuffmanError::not_started};
    bool finished_{};
};
} // namespace marc::entropy::internal
#endif

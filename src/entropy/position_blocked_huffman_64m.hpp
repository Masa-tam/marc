#ifndef MARC_ENTROPY_POSITION_BLOCKED_HUFFMAN_64M_HPP
#define MARC_ENTROPY_POSITION_BLOCKED_HUFFMAN_64M_HPP
// Reuse the established model, error and decision types; no64KiB coding path
// is invoked by this specialization.
#include "entropy/position_blocked_huffman_64k.hpp"

namespace marc::entropy::internal {
inline constexpr std::size_t position_blocked_huffman_64m_contexts=58;
inline constexpr std::size_t position_blocked_huffman_64m_tables=63;
inline constexpr std::size_t position_blocked_huffman_64m_descriptor_capacity=2590;
inline constexpr auto position_blocked_huffman_64m_alphabets=[] {
    std::array<std::uint16_t,58> result{};
    for (std::size_t c=0;c<result.size();++c)
        result[c]=c<3 ? 2 : c<20 ? 256 : c<23 ? 9 : c<32 ? 27 : 2;
    return result;
}();
inline constexpr std::array<std::uint16_t,5> position_blocked_huffman_64m_fields{2,256,9,27,2};
[[nodiscard]] constexpr std::size_t position_blocked_huffman_64m_field(std::size_t c) noexcept {
    return c<3 ? 0 : c<20 ? 1 : c<23 ? 2 : c<32 ? 3 : 4;
}
struct PositionBlockedHuffman64mDescriptor {
    std::uint32_t decision_count{},payload_size{};
    std::uint8_t final_valid_bits{},field_mask{};
    std::uint64_t override_mask{};
    std::array<PositionBlockedHuffmanModel,63> models{}; // Pools0..4, overrides5..62.
    bool operator==(const PositionBlockedHuffman64mDescriptor&) const = default;
};
// All ordinary descriptor/size/serialized destinations commit only on success.
[[nodiscard]] PositionBlockedHuffmanError validate_position_blocked_huffman_64m_descriptor(
    const PositionBlockedHuffman64mDescriptor&,const core::DecoderLimits&,std::size_t&) noexcept;
[[nodiscard]] PositionBlockedHuffmanError serialize_position_blocked_huffman_64m_descriptor(
    const PositionBlockedHuffman64mDescriptor&,const core::DecoderLimits&,std::span<std::byte>,std::size_t&) noexcept;
[[nodiscard]] PositionBlockedHuffmanError parse_position_blocked_huffman_64m_descriptor(
    std::span<const std::byte>,std::uint32_t decisions,std::uint32_t payload,
    const core::DecoderLimits&,PositionBlockedHuffman64mDescriptor&) noexcept;
class PositionBlockedHuffman64mModelBuilder {
public:
    // 10*67108864 is the format-derived maximum per reset frame.
    explicit PositionBlockedHuffman64mModelBuilder(std::uint32_t maximum_decisions=671088640) noexcept;
    [[nodiscard]] PositionBlockedHuffmanError add(int context,std::uint32_t symbol) noexcept;
    // A finite accumulator: repeated finish is deterministic; subsequent add
    // remains permitted. Ordinary descriptor commits only on successful finish.
    [[nodiscard]] PositionBlockedHuffmanError finish(const core::DecoderLimits&,
        PositionBlockedHuffman64mDescriptor&) const noexcept;
private:
    std::array<HuffmanFrequencies,63> counts_{};
    std::uint32_t decisions_{},uniform_bits_{},maximum_decisions_{};
    PositionBlockedHuffmanError error_{};
};
// Private tables may change on refusal. Ordinary output is written only after
// exact sizing, symbol/model usage and all aliases/limits have been validated.
[[nodiscard]] PositionBlockedHuffmanError encode_position_blocked_huffman_64m(
    const PositionBlockedHuffman64mDescriptor&,std::span<const PositionBlockedHuffmanDecision>,
    const core::DecoderLimits&,std::span<CanonicalHuffmanTable> private_tables,
    std::span<std::byte> output,std::size_t& written) noexcept;
// All payload storage is private scratch; begin/add may change it before a
// later failure. The ordinary byte count commits only on successful finish.
class PositionBlockedHuffman64mPrivateWriter {
public:
    [[nodiscard]] PositionBlockedHuffmanError begin(const PositionBlockedHuffman64mDescriptor&,
        const core::DecoderLimits&,std::span<CanonicalHuffmanTable>,std::span<std::byte>) noexcept;
    [[nodiscard]] PositionBlockedHuffmanError add(int context,std::uint32_t symbol) noexcept;
    [[nodiscard]] PositionBlockedHuffmanError finish(std::size_t& written) noexcept;
private:
    PositionBlockedHuffman64mDescriptor descriptor_{};
    std::span<std::byte> payload_{};
    std::span<CanonicalHuffmanTable> tables_{};
    std::uint64_t cursor_{},valid_bits_{},used_overrides_{};
    std::uint32_t decisions_{};
    std::uint8_t used_fields_{};
    PositionBlockedHuffmanError error_{};
    bool begun_{},finished_{};
};
class PositionBlockedHuffman64mDecoder {
public:
    // Finite payload supplied at begin. Failed begin/read is sticky-invalid;
    // overlap preflight rejects before any state or private scratch changes.
    [[nodiscard]] PositionBlockedHuffmanError begin(const PositionBlockedHuffman64mDescriptor&,
        std::span<const std::byte>,const core::DecoderLimits&,std::span<HuffmanDecodeTable>) noexcept;
    // Symbol output commits only on success. Entire tokens/raw frames remain
    // private until entropy finish and history/reconstruction validation.
    [[nodiscard]] PositionBlockedHuffmanError read(int context,std::uint32_t&) noexcept;
    [[nodiscard]] PositionBlockedHuffmanError finish() noexcept;
private:
    PositionBlockedHuffman64mDescriptor descriptor_{};
    std::span<const std::byte> payload_{};
    std::span<HuffmanDecodeTable> tables_{};
    std::uint64_t cursor_{},valid_bits_{},used_overrides_{};
    std::uint32_t decisions_{};
    std::uint8_t used_fields_{};
    PositionBlockedHuffmanError error_{};
    bool begun_{},finished_{};
};
} // namespace marc::entropy::internal
#endif

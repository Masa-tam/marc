#ifndef MARC_ENTROPY_POSITION_BLOCKED_HUFFMAN_64K_HPP
#define MARC_ENTROPY_POSITION_BLOCKED_HUFFMAN_64K_HPP
#include "core/limits.hpp"
#include "entropy/huffman_decode_table.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {
inline constexpr std::size_t position_blocked_huffman_64k_contexts=48;
inline constexpr std::size_t position_blocked_huffman_64k_tables=53;
inline constexpr std::size_t position_blocked_huffman_64k_descriptor_capacity=2518;
inline constexpr std::uint16_t position_blocked_huffman_no_single=UINT16_MAX;
inline constexpr auto position_blocked_huffman_64k_alphabets=[] {
    std::array<std::uint16_t,48> result{};
    for (std::size_t c=0;c<result.size();++c)
        result[c]=c<3 ? 2 : c<20 ? 256 : c<23 ? 9 : c<32 ? 17 : 2;
    return result;
}();
inline constexpr std::array<std::uint16_t,5> position_blocked_huffman_64k_fields{2,256,9,17,2};
[[nodiscard]] constexpr std::size_t position_blocked_huffman_64k_field(std::size_t c) noexcept {
    return c<3 ? 0 : c<20 ? 1 : c<23 ? 2 : c<32 ? 3 : 4;
}
struct PositionBlockedHuffmanModel {
    HuffmanCodeLengths lengths{};
    std::uint16_t single_symbol{position_blocked_huffman_no_single};
    bool operator==(const PositionBlockedHuffmanModel&) const = default;
};
struct PositionBlockedHuffman64kDescriptor {
    std::uint32_t decision_count{},payload_size{};
    std::uint8_t final_valid_bits{},field_mask{};
    std::uint64_t override_mask{};
    std::array<PositionBlockedHuffmanModel,53> models{}; // Pools0..4, overrides5..52.
    bool operator==(const PositionBlockedHuffman64kDescriptor&) const = default;
};
struct PositionBlockedHuffmanDecision {
    int context{}; // -1 is a raw uniform length bit.
    std::uint32_t symbol{};
};
enum class PositionBlockedHuffmanError : std::uint8_t {
    none,truncated,invalid_metadata,invalid_model,noncanonical,trailing_data,
    limit_exceeded,output_too_small,scratch_too_small,overlapping_buffers,arithmetic_overflow,
    invalid_context,inactive_context,invalid_symbol,count_mismatch,
    nonzero_padding,trailing_bits,unused_model,invalid_code,not_started,already_finished
};
// Descriptor and ordinary size/output destinations commit only on success.
[[nodiscard]] PositionBlockedHuffmanError validate_position_blocked_huffman_64k_descriptor(
    const PositionBlockedHuffman64kDescriptor&,const core::DecoderLimits&,std::size_t&) noexcept;
[[nodiscard]] PositionBlockedHuffmanError serialize_position_blocked_huffman_64k_descriptor(
    const PositionBlockedHuffman64kDescriptor&,const core::DecoderLimits&,std::span<std::byte>,std::size_t&) noexcept;
[[nodiscard]] PositionBlockedHuffmanError parse_position_blocked_huffman_64k_descriptor(
    std::span<const std::byte>,std::uint32_t decisions,std::uint32_t payload,
    const core::DecoderLimits&,PositionBlockedHuffman64kDescriptor&) noexcept;
class PositionBlockedHuffman64kModelBuilder {
public:
    // 7*65536 is the format-derived maximum decisions per reset frame.
    explicit PositionBlockedHuffman64kModelBuilder(std::uint32_t maximum_decisions=458752) noexcept;
    [[nodiscard]] PositionBlockedHuffmanError add(int context,std::uint32_t symbol) noexcept;
    // Repeated finish is deterministic. This accumulator is not a stream
    // transform: additional add calls remain permitted after a successful plan.
    [[nodiscard]] PositionBlockedHuffmanError finish(const core::DecoderLimits&,
        PositionBlockedHuffman64kDescriptor&) const noexcept;
private:
    std::array<HuffmanFrequencies,53> counts_{};
    std::uint32_t decisions_{},uniform_bits_{},maximum_decisions_{};
    PositionBlockedHuffmanError error_{};
};
// Tables are private scratch; discarded on any failure. Payload commits only
// after a complete sizing/usage pass, and its unproduced tail is untouched.
[[nodiscard]] PositionBlockedHuffmanError encode_position_blocked_huffman_64k(
    const PositionBlockedHuffman64kDescriptor&,std::span<const PositionBlockedHuffmanDecision>,
    const core::DecoderLimits&,std::span<CanonicalHuffmanTable> private_tables,
    std::span<std::byte> output,std::size_t& written) noexcept;
class PositionBlockedHuffman64kDecoder {
public:
    // A finite block is supplied completely at begin. Failed begin/read makes
    // this instance sticky-invalid; private tables may already be modified.
    // Overlap/arithmetic preflight rejects before any state or scratch write.
    [[nodiscard]] PositionBlockedHuffmanError begin(const PositionBlockedHuffman64kDescriptor&,
        std::span<const std::byte>,const core::DecoderLimits&,std::span<HuffmanDecodeTable>) noexcept;
    // Each ordinary symbol destination commits only on success. Decoded token
    // and raw-frame storage must stay private until finish/history validation.
    [[nodiscard]] PositionBlockedHuffmanError read(int context,std::uint32_t&) noexcept;
    [[nodiscard]] PositionBlockedHuffmanError finish() noexcept;
private:
    PositionBlockedHuffman64kDescriptor descriptor_{};
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

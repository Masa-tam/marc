#include "entropy/position_adaptive_huffman_format.hpp"
#include "core/endian.hpp"
#include <algorithm>
#include <array>

namespace marc::entropy::internal {
namespace {
constexpr std::array<PositionAdaptiveHuffmanSpec,7> specs{{
    {65536,8,35,3,48,4570,16,7},
    {1048576,9,36,4,52,4614,20,8},
    {4194304,10,37,5,54,4636,22,9},
    {8388608,11,38,6,55,4647,23,9},
    {16777216,12,39,7,56,4658,24,9},
    {33554432,13,40,8,57,4669,25,10},
    {67108864,14,41,9,58,4680,26,11}
}};
using Error=PositionAdaptiveHuffmanError;
}
const PositionAdaptiveHuffmanSpec* position_adaptive_huffman_spec(
    const PositionAdaptiveHuffmanProfile profile) noexcept {
    const auto index=static_cast<std::size_t>(profile);
    return index<specs.size() ? &specs[index] : nullptr;
}
std::uint16_t position_adaptive_huffman_alphabet(
    const PositionAdaptiveHuffmanSpec& spec,const std::size_t context) noexcept {
    if (context>=spec.contexts) return 0;
    return context<3 ? 2 : context<20 ? 256 : context<23 ? 9
        : context<32 ? static_cast<std::uint16_t>(spec.distance_bits+1) : 2;
}
PositionAdaptiveHuffmanError validate_position_adaptive_huffman_descriptor(
    const PositionAdaptiveHuffmanProfile profile,const PositionAdaptiveHuffmanDescriptor& d,
    const std::uint32_t expected_decisions,const std::uint32_t expected_payload,
    const core::DecoderLimits& limits) noexcept {
    const auto* spec=position_adaptive_huffman_spec(profile);
    if (!spec) return Error::invalid_profile;
    if (d.flags || d.context_count!=spec->contexts) return Error::invalid_metadata;
    if (d.decision_count!=expected_decisions || d.payload_size!=expected_payload
        || d.decision_count>static_cast<std::uint64_t>(spec->decisions_per_byte)*spec->window_size)
        return Error::invalid_counts;
    if (d.decision_count==0) {
        if (d.payload_size || d.final_valid_bits) return Error::invalid_payload;
    } else {
        if (!d.payload_size || !d.final_valid_bits || d.final_valid_bits>8)
            return Error::invalid_payload;
        const auto bits=UINT64_C(8)*(d.payload_size-1)+d.final_valid_bits;
        if (bits<d.decision_count || bits>UINT64_C(264)*d.decision_count
            || d.payload_size>(UINT64_C(267)*spec->window_size+7)/8)
            return Error::invalid_payload;
    }
    if (core::validate_limits(limits)!=core::LimitError::none
        || d.decision_count>limits.max_block_size
        || d.payload_size>limits.max_compressed_payload_size
        || spec->node_entries()+spec->alphabet_entries>limits.max_entropy_table_entries
        || UINT64_C(16)+d.payload_size>limits.max_internal_buffered_bytes)
        return Error::limit_exceeded;
    return Error::none;
}
PositionAdaptiveHuffmanError parse_position_adaptive_huffman_descriptor(
    const PositionAdaptiveHuffmanProfile profile,const std::span<const std::byte> bytes,
    const std::uint32_t expected_decisions,const std::uint32_t expected_payload,
    const core::DecoderLimits& limits,PositionAdaptiveHuffmanDescriptor& output) noexcept {
    if (bytes.size()<16) return Error::truncated;
    if (bytes.size()!=16) return Error::invalid_metadata;
    if (!std::all_of(bytes.begin()+12,bytes.end(),[](auto b){return b==std::byte{};}))
        return Error::nonzero_reserved;
    PositionAdaptiveHuffmanDescriptor d{};
    (void)core::load_le(bytes,0,d.decision_count);
    (void)core::load_le(bytes,4,d.payload_size);
    (void)core::load_le(bytes,8,d.context_count);
    d.final_valid_bits=std::to_integer<std::uint8_t>(bytes[10]);
    d.flags=std::to_integer<std::uint8_t>(bytes[11]);
    const auto error=validate_position_adaptive_huffman_descriptor(profile,d,expected_decisions,expected_payload,limits);
    if (error==Error::none) output=d;
    return error;
}
PositionAdaptiveHuffmanError serialize_position_adaptive_huffman_descriptor(
    const PositionAdaptiveHuffmanProfile profile,const PositionAdaptiveHuffmanDescriptor& d,
    const core::DecoderLimits& limits,const std::span<std::byte> output) noexcept {
    if (const auto error=validate_position_adaptive_huffman_descriptor(profile,d,d.decision_count,d.payload_size,limits);
        error!=Error::none) return error;
    if (output.size()<16) return Error::output_too_small;
    std::array<std::byte,16> bytes{};
    (void)core::store_le(bytes,0,d.decision_count);
    (void)core::store_le(bytes,4,d.payload_size);
    (void)core::store_le(bytes,8,d.context_count);
    bytes[10]=static_cast<std::byte>(d.final_valid_bits);
    std::copy(bytes.begin(),bytes.end(),output.begin());
    return Error::none;
}
} // namespace marc::entropy::internal

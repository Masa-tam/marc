#include "frame/lzss_position_adaptive_huffman_format.hpp"
#include "core/checked_math.hpp"
#include "core/endian.hpp"
#include <algorithm>
#include <array>
#include <utility>

namespace marc::frame::internal {
namespace {
using namespace entropy::internal;
using Error=PositionAdaptiveHuffmanFormatError;
template<class T> T load(std::span<const std::byte> bytes,std::size_t offset) noexcept {
    T value{};(void)core::load_le(bytes,offset,value);return value;
}
bool zero(std::span<const std::byte> bytes) noexcept {
    return std::all_of(bytes.begin(),bytes.end(),[](auto b){return b==std::byte{};});
}
bool magic(std::span<const std::byte> bytes,const char* name) noexcept {
    for (std::size_t i=0;i<4;++i) if (bytes[i]!=static_cast<std::byte>(name[i])) return false;
    return true;
}
void put_magic(std::span<std::byte> bytes,const char* name) noexcept {
    for (std::size_t i=0;i<4;++i) bytes[i]=static_cast<std::byte>(name[i]);
}
}
PositionAdaptiveHuffmanFormatError validate_position_adaptive_huffman_stream(
    const PositionAdaptiveHuffmanStreamHeader& stream,const core::DecoderLimits& limits) noexcept {
    const auto* spec=position_adaptive_huffman_spec(stream.profile);
    if (!spec) return Error::unsupported_identity;
    if (stream.original_size==UINT64_MAX) return Error::invalid_parameters;
    if (core::validate_limits(limits)!=core::LimitError::none || stream.frame_size>limits.max_frame_size
        || stream.frame_size>limits.max_block_size || stream.original_size>limits.max_total_output_size
        || 112>limits.max_internal_buffered_bytes
        || spec->node_entries()+spec->alphabet_entries>limits.max_entropy_table_entries) return Error::limit_exceeded;
    if (!stream.frame_size || stream.frame_size>spec->window_size || stream.dictionary.window_size!=spec->window_size
        || stream.dictionary.min_match_length!=3 || stream.dictionary.max_match_length!=258 || stream.dictionary.flags)
        return Error::invalid_parameters;
    const auto e=dictionary::internal::validate_lzss_typed_parameters(stream.dictionary,limits,
        static_cast<dictionary::internal::LzssTypedTokenVariant>(spec->dictionary_variant));
    return e==dictionary::internal::LzssTypedTokenError::none ? Error::none
        : e==dictionary::internal::LzssTypedTokenError::limit_exceeded ? Error::limit_exceeded : Error::invalid_parameters;
}
PositionAdaptiveHuffmanFormatError serialize_position_adaptive_huffman_stream(
    const PositionAdaptiveHuffmanStreamHeader& stream,const core::DecoderLimits& limits,std::span<std::byte> output) noexcept {
    if (const auto e=validate_position_adaptive_huffman_stream(stream,limits);e!=Error::none) return e;
    if (output.size()<112) return Error::output_too_small;
    const auto& spec=*position_adaptive_huffman_spec(stream.profile);
    std::array<std::byte,112> bytes{};put_magic(bytes,"MARC");
    for (auto [at,value]:std::array<std::pair<std::size_t,std::uint16_t>,10>{{
        {4,2},{6,0},{8,64},{10,1},{12,2},{14,spec.dictionary_variant},{16,1},{18,spec.entropy_variant},{96,1},{98,spec.context_variant}}})
        (void)core::store_le(bytes,at,value);
    (void)core::store_le(bytes,20,stream.frame_size);
    (void)core::store_le(bytes,28,std::uint32_t{16});(void)core::store_le(bytes,32,std::uint32_t{16});
    (void)core::store_le(bytes,40,stream.original_size);(void)core::store_le(bytes,48,std::uint32_t{16});
    (void)core::store_le(bytes,64,stream.dictionary.window_size);(void)core::store_le(bytes,68,std::uint32_t{3});
    (void)core::store_le(bytes,72,std::uint32_t{258});bytes[80]=bytes[81]=std::byte{1};
    (void)core::store_le(bytes,82,spec.contexts);(void)core::store_le(bytes,84,spec.alphabet_entries);
    (void)core::store_le(bytes,88,static_cast<std::uint32_t>(spec.node_entries()));
    std::copy(bytes.begin(),bytes.end(),output.begin());return Error::none;
}
PositionAdaptiveHuffmanFormatError parse_position_adaptive_huffman_stream(
    PositionAdaptiveHuffmanProfile profile,std::span<const std::byte> input,const core::DecoderLimits& limits,
    PositionAdaptiveHuffmanStreamHeader& output,std::size_t& consumed) noexcept {
    if (input.size()<112) return Error::truncated;
    const auto bytes=input.first(112);if (!magic(bytes,"MARC")) return Error::invalid_magic;
    const auto* spec=position_adaptive_huffman_spec(profile);if (!spec) return Error::unsupported_identity;
    for (auto [at,value]:std::array<std::pair<std::size_t,std::uint16_t>,9>{{
        {4,2},{6,0},{8,64},{12,2},{14,spec->dictionary_variant},{16,1},{18,spec->entropy_variant},{96,1},{98,spec->context_variant}}})
        if (load<std::uint16_t>(bytes,at)!=value) return Error::unsupported_identity;
    if (load<std::uint16_t>(bytes,10)!=1) return Error::invalid_flags;
    if (!zero(bytes.subspan(24,4)) || !zero(bytes.subspan(36,4)) || !zero(bytes.subspan(52,12))
        || !zero(bytes.subspan(92,4)) || !zero(bytes.subspan(100,12))) return Error::nonzero_reserved;
    PositionAdaptiveHuffmanStreamHeader candidate{};candidate.profile=profile;
    candidate.frame_size=load<std::uint32_t>(bytes,20);candidate.original_size=load<std::uint64_t>(bytes,40);
    candidate.dictionary={load<std::uint32_t>(bytes,64),load<std::uint32_t>(bytes,68),load<std::uint32_t>(bytes,72),load<std::uint32_t>(bytes,76)};
    std::array<std::byte,112> canonical{};
    if (const auto e=serialize_position_adaptive_huffman_stream(candidate,limits,canonical);e!=Error::none) return e;
    if (!std::equal(canonical.begin(),canonical.end(),bytes.begin())) return Error::invalid_parameters;
    output=candidate;consumed=112;return Error::none;
}
PositionAdaptiveHuffmanFormatError validate_position_adaptive_huffman_frame_header(
    const PositionAdaptiveHuffmanFrameHeader& h,const PositionAdaptiveHuffmanFrameContext& context,
    PositionAdaptiveHuffmanFrameRequirements& output) noexcept {
    if (const auto e=validate_position_adaptive_huffman_stream(context.stream,context.limits);e!=Error::none) return e;
    if (h.flags || h.side_data_size || h.trailer_size) return Error::invalid_flags;
    if (context.raw_committed>=context.stream.original_size || context.raw_committed%context.stream.frame_size
        || context.sequence!=context.raw_committed/context.stream.frame_size || h.sequence!=context.sequence) return Error::invalid_sequence;
    const auto& spec=*position_adaptive_huffman_spec(context.stream.profile);
    const auto expected=std::min<std::uint64_t>(context.stream.frame_size,context.stream.original_size-context.raw_committed);
    const std::uint64_t raw=h.raw_size,tokens=h.token_count,decisions=h.decision_count;
    if (raw!=expected || !tokens || tokens>raw || h.event_count!=decisions || decisions<2*tokens
        || decisions>(UINT64_C(10)+spec.distance_bits)*tokens || decisions>static_cast<std::uint64_t>(spec.decisions_per_byte)*raw
        || h.descriptor_size!=16 || !h.payload_size || h.payload_size>(UINT64_C(267)*raw+7)/8)
        return Error::invalid_counts;
    core::FrameBounds bounds{};bounds.uncompressed_size=raw;bounds.compressed_payload_size=h.payload_size;
    bounds.largest_block_size=decisions;bounds.lz_distance=spec.window_size;bounds.lz_match_length=258;
    bounds.entropy_table_entries=spec.node_entries()+spec.alphabet_entries;
    bounds.model_buffered_bytes=16;bounds.payload_buffered_bytes=h.payload_size;bounds.block_count=1;
    const auto checked=core::validate_frame_bounds(context.limits,bounds,context.raw_committed);
    if (checked!=core::LimitError::none) return checked==core::LimitError::arithmetic_overflow ? Error::arithmetic_overflow : Error::limit_exceeded;
    PositionAdaptiveHuffmanFrameRequirements r{};r.token_count=h.token_count;r.raw_size=h.raw_size;r.prefix_size=80;
    r.node_entries=spec.node_entries();r.symbol_entries=spec.alphabet_entries;
    std::size_t nodes{},symbols{},tokens_bytes{};
    if (!core::checked_multiply(r.node_entries,sizeof(AdaptiveHuffmanNode),nodes)
        || !core::checked_multiply(r.symbol_entries,sizeof(std::uint16_t),symbols)
        || !core::checked_add(nodes,symbols,r.model_bytes)
        || !core::checked_add(std::size_t{80},static_cast<std::size_t>(h.payload_size),r.serialized_size)
        || !core::checked_multiply(r.token_count,sizeof(dictionary::internal::LzssTypedToken),tokens_bytes)
        || !core::checked_add(std::size_t{131072},r.model_bytes,r.aggregate_bytes)
        || !core::checked_add(r.aggregate_bytes,r.serialized_size,r.aggregate_bytes)
        || !core::checked_add(r.aggregate_bytes,tokens_bytes,r.aggregate_bytes)
        || !core::checked_add(r.aggregate_bytes,r.raw_size,r.aggregate_bytes)
        || !core::checked_add(r.aggregate_bytes,sizeof(PositionAdaptiveHuffmanFrameContext)+sizeof(PositionAdaptiveHuffmanStreamHeader)+sizeof(core::DecoderLimits),r.aggregate_bytes))
        return Error::arithmetic_overflow;
    if (r.aggregate_bytes>context.limits.max_internal_buffered_bytes) return Error::limit_exceeded;
    output=r;return Error::none;
}
PositionAdaptiveHuffmanFormatError serialize_position_adaptive_huffman_frame_header(
    const PositionAdaptiveHuffmanFrameHeader& h,const PositionAdaptiveHuffmanFrameContext& context,std::span<std::byte> output) noexcept {
    PositionAdaptiveHuffmanFrameRequirements r{};
    if (const auto e=validate_position_adaptive_huffman_frame_header(h,context,r);e!=Error::none) return e;
    if (output.size()<64) return Error::output_too_small;
    std::array<std::byte,64> bytes{};put_magic(bytes,"MRF2");(void)core::store_le(bytes,4,std::uint16_t{64});
    (void)core::store_le(bytes,8,h.sequence);
    for (auto [at,value]:std::array<std::pair<std::size_t,std::uint32_t>,6>{{
        {16,h.raw_size},{20,h.token_count},{24,h.event_count},{28,h.decision_count},{32,h.payload_size},{36,h.descriptor_size}}})
        (void)core::store_le(bytes,at,value);
    std::copy(bytes.begin(),bytes.end(),output.begin());return Error::none;
}
PositionAdaptiveHuffmanFormatError parse_position_adaptive_huffman_frame_header(
    std::span<const std::byte> input,const PositionAdaptiveHuffmanFrameContext& context,PositionAdaptiveHuffmanFrameLayout& output) noexcept {
    if (input.size()<64) return Error::truncated;
    const auto bytes=input.first(64);if (!magic(bytes,"MRF2")) return Error::invalid_magic;
    if (load<std::uint16_t>(bytes,4)!=64) return Error::unsupported_identity;
    if (!zero(bytes.subspan(48,16))) return Error::nonzero_reserved;
    PositionAdaptiveHuffmanFrameLayout candidate{};auto& h=candidate.header;
    h.flags=load<std::uint16_t>(bytes,6);h.sequence=load<std::uint64_t>(bytes,8);
    h.raw_size=load<std::uint32_t>(bytes,16);h.token_count=load<std::uint32_t>(bytes,20);
    h.event_count=load<std::uint32_t>(bytes,24);h.decision_count=load<std::uint32_t>(bytes,28);
    h.payload_size=load<std::uint32_t>(bytes,32);h.descriptor_size=load<std::uint32_t>(bytes,36);
    h.side_data_size=load<std::uint32_t>(bytes,40);h.trailer_size=load<std::uint32_t>(bytes,44);
    if (const auto e=validate_position_adaptive_huffman_frame_header(h,context,candidate.requirements);e!=Error::none) return e;
    output=candidate;return Error::none;
}
PositionAdaptiveHuffmanFormatError preflight_position_adaptive_huffman_frame_prefix(
    std::span<const std::byte> input,const PositionAdaptiveHuffmanFrameContext& context,PositionAdaptiveHuffmanFrameLayout& output) noexcept {
    PositionAdaptiveHuffmanFrameLayout candidate{};
    if (const auto e=parse_position_adaptive_huffman_frame_header(input,context,candidate);e!=Error::none) return e;
    if (input.size()<80) return Error::truncated;
    PositionAdaptiveHuffmanDescriptor d{};
    if (parse_position_adaptive_huffman_descriptor(context.stream.profile,input.subspan(64,16),candidate.header.decision_count,
            candidate.header.payload_size,context.limits,d)!=PositionAdaptiveHuffmanError::none) return Error::invalid_descriptor;
    output=candidate;return Error::none;
}
PositionAdaptiveHuffmanFormatError preflight_position_adaptive_huffman_frame(
    std::span<const std::byte> input,const PositionAdaptiveHuffmanFrameContext& context,PositionAdaptiveHuffmanFrameLayout& output) noexcept {
    PositionAdaptiveHuffmanFrameLayout candidate{};
    if (const auto e=preflight_position_adaptive_huffman_frame_prefix(input,context,candidate);e!=Error::none) return e;
    if (input.size()<candidate.requirements.serialized_size) return Error::truncated;
    output=candidate;return Error::none;
}
} // namespace marc::frame::internal

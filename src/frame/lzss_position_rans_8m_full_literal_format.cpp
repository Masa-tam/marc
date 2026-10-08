#include "frame/lzss_position_rans_8m_full_literal_format.hpp"
#include "core/endian.hpp"
#include <algorithm>
#include <utility>

namespace marc::frame::internal {
namespace {
using Error = PositionRans8mFullLiteralFormatError;
constexpr auto variant = dictionary::internal::LzssTypedTokenVariant::field_context_8m_short_length_escape;
template<class T> T load(std::span<const std::byte> bytes,std::size_t offset) noexcept {
    T value{}; (void)core::load_le(bytes,offset,value); return value;
}
bool zero(std::span<const std::byte> bytes) noexcept {
    return std::all_of(bytes.begin(),bytes.end(),[](auto v) { return v == std::byte{0}; });
}
bool magic(std::span<const std::byte> bytes,const char* expected) noexcept {
    for (std::size_t i=0;i<4;++i) if (bytes[i] != static_cast<std::byte>(expected[i])) return false;
    return true;
}
void put_magic(std::span<std::byte> bytes,const char* value) noexcept {
    for (std::size_t i=0;i<4;++i) bytes[i] = static_cast<std::byte>(value[i]);
}
} // namespace

PositionRans8mFullLiteralFormatError validate_position_rans_8m_full_literal_stream(
    const PositionRans8mFullLiteralStreamHeader& stream,const core::DecoderLimits& limits) noexcept {
    if (core::validate_limits(limits) != core::LimitError::none
        || stream.frame_size > limits.max_frame_size || stream.frame_size > limits.max_block_size
        || stream.original_size > limits.max_total_output_size
        || position_rans_8m_full_literal_stream_header_size > limits.max_internal_buffered_bytes
        || limits.max_entropy_table_entries < 4647) return Error::limit_exceeded;
    if (stream.frame_size == 0 || stream.frame_size > position_rans_8m_full_literal_window_size
        || stream.dictionary.window_size != position_rans_8m_full_literal_window_size
        || stream.dictionary.min_match_length != 3 || stream.dictionary.max_match_length != 258
        || stream.dictionary.flags != 0) return Error::invalid_parameters;
    const auto dictionary_error = dictionary::internal::validate_lzss_typed_parameters(stream.dictionary,limits,variant);
    if (dictionary_error != dictionary::internal::LzssTypedTokenError::none)
        return dictionary_error == dictionary::internal::LzssTypedTokenError::limit_exceeded
            ? Error::limit_exceeded : Error::invalid_parameters;
    return Error::none;
}
PositionRans8mFullLiteralFormatError serialize_position_rans_8m_full_literal_stream(
    const PositionRans8mFullLiteralStreamHeader& stream,const core::DecoderLimits& limits,
    const std::span<std::byte> output) noexcept {
    if (const auto error=validate_position_rans_8m_full_literal_stream(stream,limits);error!=Error::none) return error;
    if (output.size()<112) return Error::output_too_small;
    std::array<std::byte,112> bytes{};
    put_magic(bytes,"MARC");
    for (const auto& [offset,value] : std::array<std::pair<std::size_t,std::uint16_t>,10>{
        {{4,2},{6,0},{8,64},{10,1},{12,2},{14,11},{16,4},{18,8},{96,1},{98,17}}})
        (void)core::store_le(bytes,offset,value);
    (void)core::store_le(bytes,20,stream.frame_size);
    (void)core::store_le(bytes,28,std::uint32_t{16});
    (void)core::store_le(bytes,32,std::uint32_t{16});
    (void)core::store_le(bytes,40,stream.original_size);
    (void)core::store_le(bytes,48,std::uint32_t{16});
    (void)core::store_le(bytes,64,stream.dictionary.window_size);
    (void)core::store_le(bytes,68,stream.dictionary.min_match_length);
    (void)core::store_le(bytes,72,stream.dictionary.max_match_length);
    bytes[80]=std::byte{12}; bytes[81]=std::byte{1};
    (void)core::store_le(bytes,82,std::uint16_t{55});
    (void)core::store_le(bytes,84,std::uint32_t{4647});
    std::copy(bytes.begin(),bytes.end(),output.begin());
    return Error::none;
}
PositionRans8mFullLiteralFormatError parse_position_rans_8m_full_literal_stream(
    const std::span<const std::byte> input,const core::DecoderLimits& limits,
    PositionRans8mFullLiteralStreamHeader& stream,std::size_t& consumed) noexcept {
    if (input.size()<112) return Error::truncated;
    const auto bytes=input.first(112);
    if (!magic(bytes,"MARC")) return Error::invalid_magic;
    for (const auto& [offset,value] : std::array<std::pair<std::size_t,std::uint16_t>,9>{
        {{4,2},{6,0},{8,64},{12,2},{14,11},{16,4},{18,8},{96,1},{98,17}}})
        if (load<std::uint16_t>(bytes,offset)!=value) return Error::unsupported_identity;
    if (load<std::uint16_t>(bytes,10)!=1) return Error::invalid_flags;
    if (load<std::uint32_t>(bytes,24)!=0 || load<std::uint32_t>(bytes,28)!=16
        || load<std::uint32_t>(bytes,32)!=16 || load<std::uint32_t>(bytes,36)!=0
        || load<std::uint32_t>(bytes,48)!=16 || bytes[80]!=std::byte{12} || bytes[81]!=std::byte{1}
        || load<std::uint16_t>(bytes,82)!=55 || load<std::uint32_t>(bytes,84)!=4647
        || load<std::uint32_t>(bytes,88)!=0 || load<std::uint32_t>(bytes,100)!=0)
        return Error::invalid_parameters;
    if (!zero(bytes.subspan(52,12)) || !zero(bytes.subspan(92,4)) || !zero(bytes.subspan(104,8)))
        return Error::nonzero_reserved;
    PositionRans8mFullLiteralStreamHeader candidate{};
    candidate.frame_size=load<std::uint32_t>(bytes,20);
    candidate.original_size=load<std::uint64_t>(bytes,40);
    candidate.dictionary={load<std::uint32_t>(bytes,64),load<std::uint32_t>(bytes,68),
        load<std::uint32_t>(bytes,72),load<std::uint32_t>(bytes,76)};
    const auto error=validate_position_rans_8m_full_literal_stream(candidate,limits);
    if (error!=Error::none) return error;
    stream=candidate; consumed=112;
    return Error::none;
}
PositionRans8mFullLiteralFormatError validate_position_rans_8m_full_literal_frame_header(
    const PositionRans8mFullLiteralFrameHeader& h,const PositionRans8mFullLiteralFrameContext& c,
    PositionRans8mFullLiteralFrameRequirements& requirements) noexcept {
    if (const auto error=validate_position_rans_8m_full_literal_stream(c.stream,c.limits);error!=Error::none) return error;
    if (h.flags || h.side_data_size || h.trailer_size) return Error::invalid_flags;
    if (c.raw_committed>=c.stream.original_size || c.raw_committed%c.stream.frame_size
        || c.sequence!=c.raw_committed/c.stream.frame_size || h.sequence!=c.sequence) return Error::invalid_sequence;
    const auto expected=std::min<std::uint64_t>(c.stream.frame_size,c.stream.original_size-c.raw_committed);
    const std::uint64_t t=h.token_count,e=h.event_count,d=h.decision_count,f=h.raw_size;
    if (f!=expected || t==0 || t>f || e<2*t || e>5*t || e>2*f || d<e || d>33*t || d>9*f
        || h.descriptor_size<23 || h.descriptor_size>9262 || h.payload_size<8
        || h.payload_size>2*d+8 || h.payload_size>18*f+8) return Error::invalid_counts;
    core::FrameBounds bounds{};
    bounds.uncompressed_size=f; bounds.compressed_payload_size=h.payload_size;
    bounds.largest_block_size=d;
    bounds.lz_distance=c.stream.dictionary.window_size;
    bounds.lz_match_length=c.stream.dictionary.max_match_length;
    bounds.entropy_table_entries=4647; bounds.model_buffered_bytes=h.descriptor_size;
    bounds.payload_buffered_bytes=h.payload_size; bounds.block_count=1;
    const auto checked=core::validate_frame_bounds(c.limits,bounds,c.raw_committed);
    if (checked!=core::LimitError::none) return checked==core::LimitError::arithmetic_overflow
        ? Error::arithmetic_overflow : Error::limit_exceeded;
    PositionRans8mFullLiteralFrameRequirements r{};
    r.token_count=h.token_count; r.raw_size=h.raw_size;
    if (!core::checked_add(std::size_t{64},static_cast<std::size_t>(h.descriptor_size),r.prefix_size)
        || !core::checked_add(r.prefix_size,static_cast<std::size_t>(h.payload_size),r.serialized_size)
        || !core::checked_multiply(r.token_count,sizeof(dictionary::internal::LzssTypedToken),r.aggregate_bytes)
        || !core::checked_add(r.aggregate_bytes,r.serialized_size,r.aggregate_bytes)
        || !core::checked_add(r.aggregate_bytes,r.raw_size,r.aggregate_bytes)
        || !core::checked_add(r.aggregate_bytes,context::internal::lzss_position_rans_8m_full_literal_fixed_working_bytes,r.aggregate_bytes))
        return Error::arithmetic_overflow;
    if (r.aggregate_bytes>c.limits.max_internal_buffered_bytes) return Error::limit_exceeded;
    requirements=r;
    return Error::none;
}
PositionRans8mFullLiteralFormatError serialize_position_rans_8m_full_literal_frame_header(
    const PositionRans8mFullLiteralFrameHeader& h,const PositionRans8mFullLiteralFrameContext& context,
    const std::span<std::byte> output) noexcept {
    PositionRans8mFullLiteralFrameRequirements requirements{};
    if (const auto error=validate_position_rans_8m_full_literal_frame_header(h,context,requirements);error!=Error::none) return error;
    if (output.size()<64) return Error::output_too_small;
    std::array<std::byte,64> bytes{};
    put_magic(bytes,"MRF2");
    (void)core::store_le(bytes,4,std::uint16_t{64});
    (void)core::store_le(bytes,8,h.sequence);
    for (const auto& [offset,value] : std::array<std::pair<std::size_t,std::uint32_t>,6>{
        {{16,h.raw_size},{20,h.token_count},{24,h.event_count},{28,h.decision_count},{32,h.payload_size},{36,h.descriptor_size}}})
        (void)core::store_le(bytes,offset,value);
    std::copy(bytes.begin(),bytes.end(),output.begin());
    return Error::none;
}
PositionRans8mFullLiteralFormatError parse_position_rans_8m_full_literal_frame_header(
    const std::span<const std::byte> input,const PositionRans8mFullLiteralFrameContext& context,
    PositionRans8mFullLiteralFrameLayout& layout) noexcept {
    if (input.size()<64) return Error::truncated;
    const auto bytes=input.first(64);
    if (!magic(bytes,"MRF2")) return Error::invalid_magic;
    if (load<std::uint16_t>(bytes,4)!=64) return Error::unsupported_identity;
    if (!zero(bytes.subspan(48,16))) return Error::nonzero_reserved;
    PositionRans8mFullLiteralFrameLayout candidate{};
    auto& h=candidate.header;
    h.flags=load<std::uint16_t>(bytes,6); h.sequence=load<std::uint64_t>(bytes,8);
    h.raw_size=load<std::uint32_t>(bytes,16); h.token_count=load<std::uint32_t>(bytes,20);
    h.event_count=load<std::uint32_t>(bytes,24); h.decision_count=load<std::uint32_t>(bytes,28);
    h.payload_size=load<std::uint32_t>(bytes,32); h.descriptor_size=load<std::uint32_t>(bytes,36);
    h.side_data_size=load<std::uint32_t>(bytes,40); h.trailer_size=load<std::uint32_t>(bytes,44);
    const auto error=validate_position_rans_8m_full_literal_frame_header(h,context,candidate.requirements);
    if (error!=Error::none) return error;
    layout=candidate;
    return Error::none;
}
PositionRans8mFullLiteralFormatError preflight_position_rans_8m_full_literal_frame_prefix(
    const std::span<const std::byte> input,const PositionRans8mFullLiteralFrameContext& context,
    PositionRans8mFullLiteralFrameLayout& layout) noexcept {
    PositionRans8mFullLiteralFrameLayout candidate{};
    if (const auto error=parse_position_rans_8m_full_literal_frame_header(input,context,candidate);error!=Error::none) return error;
    if (input.size()<candidate.requirements.prefix_size) return Error::truncated;
    entropy::internal::PositionRans8mFullLiteralDescriptor model{};
    if (entropy::internal::parse_position_rans_8m_full_literal_descriptor(input.subspan(64,candidate.header.descriptor_size),
        candidate.header.decision_count,candidate.header.payload_size,context.limits,model)
        !=entropy::internal::PositionRans8mFullLiteralFormatError::none) return Error::invalid_descriptor;
    layout=candidate;
    return Error::none;
}
PositionRans8mFullLiteralFormatError preflight_position_rans_8m_full_literal_frame(
    const std::span<const std::byte> input,const PositionRans8mFullLiteralFrameContext& context,
    PositionRans8mFullLiteralFrameLayout& layout) noexcept {
    PositionRans8mFullLiteralFrameLayout candidate{};
    if (const auto error=preflight_position_rans_8m_full_literal_frame_prefix(input,context,candidate);error!=Error::none) return error;
    if (input.size()<candidate.requirements.serialized_size) return Error::truncated;
    layout=candidate;
    return Error::none;
}
} // namespace marc::frame::internal

#include "frame/lzss_position_distance_8m_preflight.hpp"
#include "dictionary/lzss_typed_token.hpp"
#include "core/buffer_overlap.hpp"
#include "core/endian.hpp"
#include <algorithm>
#include <array>
#include <utility>
namespace marc::frame::internal {
namespace {
using Error=LzssPositionDistance8mPreflightError;
struct Region {const void* data;std::size_t size;};
template<std::size_t W,std::size_t R>
Error output_preflight(const std::array<Region,W>& writes,const std::array<Region,R>& reads) noexcept {
    for(const auto& a:writes) {
        for(const auto& b:reads) {
            auto o=core::check_buffer_overlap(a.data,a.size,b.data,b.size);
            if(o!=core::BufferOverlap::disjoint)return o==core::BufferOverlap::arithmetic_overflow?Error::arithmetic_overflow:Error::overlapping_output;
        }
    }
    for(std::size_t i=0;i<W;++i)for(std::size_t j=i+1;j<W;++j)
        if(core::check_buffer_overlap(writes[i].data,writes[i].size,writes[j].data,writes[j].size)!=core::BufferOverlap::disjoint)return Error::overlapping_output;
    return Error::none;
}
bool zero(std::span<const std::byte> s) noexcept {
    return std::ranges::all_of(s,[](std::byte b){return b==std::byte{};});
}
template<class T>T read(std::span<const std::byte> s,std::size_t offset) noexcept {
    T v{};static_cast<void>(core::load_le(s,offset,v));return v;
}
bool magic(std::span<const std::byte> s,std::byte a,std::byte b,std::byte c,std::byte d) noexcept {
    return s[0]==a&&s[1]==b&&s[2]==c&&s[3]==d;
}
} // namespace
Error validate_lzss_position_distance_8m_stream_semantics(const TypedContextStreamHeader& s,const core::DecoderLimits& l) noexcept {
    if(core::validate_limits(l)!=core::LimitError::none)return Error::limit_exceeded;
    if(s.dictionary_variant!=11||s.context_algorithm!=1||s.context_variant!=12||s.context_count!=47||s.range_model_total!=32768
        ||!s.frame_size||s.frame_size>8388608||!s.dictionary.window_size||s.dictionary.window_size>8388608
        ||s.dictionary.min_match_length!=3||s.dictionary.max_match_length<3||s.dictionary.max_match_length>258||s.dictionary.flags)return Error::invalid_stream;
    if(s.frame_size>l.max_frame_size||s.original_size>l.max_total_output_size||s.dictionary.window_size>l.max_lz_distance
        ||s.dictionary.max_match_length>l.max_lz_match_length||2599>l.max_entropy_table_entries||32768>l.max_range_model_total
        ||112>l.max_internal_buffered_bytes||sizeof(entropy::internal::LzssPositionDistance8mRangeState)>l.max_internal_buffered_bytes)return Error::limit_exceeded;
    return Error::none;
}
Error parse_lzss_position_distance_8m_stream_header(std::span<const std::byte> input,const core::DecoderLimits& limits,
    TypedContextStreamHeader& output,std::size_t& consumed) noexcept {
    const auto alias=output_preflight(std::array{Region{&output,sizeof(output)},Region{&consumed,sizeof(consumed)}},
        std::array{Region{input.data(),input.size()},Region{&limits,sizeof(limits)}});
    if(alias!=Error::none)return alias;
    if(input.size()<112)return Error::truncated_stream_header;
    auto b=input.first(112);
    if(!magic(b,std::byte{'M'},std::byte{'A'},std::byte{'R'},std::byte{'C'}))return Error::invalid_magic;
    if(read<std::uint16_t>(b,4)!=2||read<std::uint16_t>(b,6)!=0)return Error::unsupported_version;
    if(read<std::uint16_t>(b,8)!=64)return Error::invalid_header_size;
    if(read<std::uint16_t>(b,10)!=1||read<std::uint32_t>(b,24)||read<std::uint32_t>(b,28)!=16||read<std::uint32_t>(b,32)!=16
        ||read<std::uint32_t>(b,36)||read<std::uint32_t>(b,48)!=16||read<std::uint16_t>(b,86)||read<std::uint32_t>(b,100))return Error::unsupported_feature;
    if(!zero(b.subspan(52,12))||!zero(b.subspan(88,8))||!zero(b.subspan(104,8)))return Error::nonzero_reserved;
    if(read<std::uint16_t>(b,12)!=2||read<std::uint16_t>(b,14)!=11||read<std::uint16_t>(b,16)!=3||read<std::uint16_t>(b,18)!=2
        ||read<std::uint16_t>(b,96)!=1||read<std::uint16_t>(b,98)!=12)return Error::unsupported_format;
    TypedContextStreamHeader s{};
    s.frame_size=read<std::uint32_t>(b,20);s.original_size=read<std::uint64_t>(b,40);
    s.dictionary={read<std::uint32_t>(b,64),read<std::uint32_t>(b,68),read<std::uint32_t>(b,72),read<std::uint32_t>(b,76)};
    s.range_model_total=read<std::uint32_t>(b,80);s.context_count=read<std::uint16_t>(b,84);
    s.dictionary_variant=11;s.context_algorithm=1;s.context_variant=12;
    const auto error=validate_lzss_position_distance_8m_stream_semantics(s,limits);
    if(error!=Error::none)return error;
    output=s;consumed=112;return Error::none;
}
Error preflight_lzss_position_distance_8m_frame_prefix(std::span<const std::byte> input,const TypedContextFrameValidationContext& c,
    TypedContextFrameLayout& output,LzssPositionDistance8mFrameRequirements& requirements,std::size_t retained) noexcept {
    const auto alias=output_preflight(std::array{Region{&output,sizeof(output)},Region{&requirements,sizeof(requirements)}},
        std::array{Region{input.data(),input.size()},Region{&c,sizeof(c)},Region{&c.stream,sizeof(c.stream)},Region{&c.limits,sizeof(c.limits)}});
    if(alias!=Error::none)return alias;
    auto error=validate_lzss_position_distance_8m_stream_semantics(c.stream,c.limits);if(error!=Error::none)return error;
    if(input.size()<64)return Error::truncated_frame_header;
    const auto b=input.first(64);
    if(!magic(b,std::byte{'M'},std::byte{'R'},std::byte{'F'},std::byte{'2'}))return Error::invalid_magic;
    if(read<std::uint16_t>(b,4)!=64)return Error::invalid_header_size;
    if(read<std::uint16_t>(b,6)||read<std::uint32_t>(b,40)||read<std::uint32_t>(b,44))return Error::unsupported_feature;
    if(!zero(b.subspan(48,16)))return Error::nonzero_reserved;
    if(input.size()<80)return Error::truncated_descriptor;
    auto d=input.subspan(64,16);
    if(read<std::uint16_t>(d,10))return Error::unsupported_feature;
    if(!zero(d.subspan(12,4)))return Error::nonzero_reserved;
    TypedContextFrameLayout result{};auto& f=result.header;
    f.sequence=read<std::uint64_t>(b,8);f.uncompressed_size=read<std::uint32_t>(b,16);f.token_count=read<std::uint32_t>(b,20);
    f.event_count=read<std::uint32_t>(b,24);f.decision_count=read<std::uint32_t>(b,28);f.payload_size=read<std::uint32_t>(b,32);
    f.descriptor_size=read<std::uint32_t>(b,36);
    result.descriptor={read<std::uint32_t>(d,0),read<std::uint32_t>(d,4),read<std::uint16_t>(d,8)};
    if(c.output_already_committed%c.stream.frame_size||c.expected_sequence!=c.output_already_committed/c.stream.frame_size
        ||f.sequence!=c.expected_sequence)return Error::unexpected_sequence;
    if(c.output_already_committed>=c.stream.original_size||f.uncompressed_size!=std::min<std::uint64_t>(c.stream.frame_size,c.stream.original_size-c.output_already_committed))return Error::unexpected_frame_size;
    const std::uint64_t raw=f.uncompressed_size,t=f.token_count,e=f.event_count,n=f.decision_count,p=f.payload_size;
    // Stream capacity already bounds these values; widen before products.
    if(!t||t>raw||e<2*t||e>std::min(2*raw,5*t)||n<e||n>std::min(9*raw,33*t)||p<5||p>std::min(2*n+5,18*raw+5)
        ||f.descriptor_size!=16)return Error::contradictory_counts;
    if(result.descriptor.decision_count!=n||result.descriptor.payload_size!=p||result.descriptor.context_count!=47)return Error::invalid_descriptor;
    std::uint64_t serialized{},tokens{},working{},model{},aggregate{};
    if(!core::checked_add(p,std::uint64_t{80},serialized)||!core::checked_multiply(t,std::uint64_t{sizeof(dictionary::internal::LzssTypedToken)},tokens)
        ||!core::checked_add(serialized,tokens,working)||!core::checked_add(working,raw,working)
        ||!core::checked_add(std::uint64_t{sizeof(entropy::internal::LzssPositionDistance8mRangeState)},static_cast<std::uint64_t>(retained),model)
        ||!core::checked_add(working,model,aggregate)||!std::in_range<std::size_t>(aggregate)||!std::in_range<std::size_t>(serialized))return Error::arithmetic_overflow;
    const core::FrameBounds bounds{raw,0,p,raw,0,c.stream.dictionary.window_size,c.stream.dictionary.max_match_length,0,2599,32768,model,working,1};
    const auto limit=core::validate_frame_bounds(c.limits,bounds,c.output_already_committed);
    if(limit!=core::LimitError::none)return limit==core::LimitError::arithmetic_overflow?Error::arithmetic_overflow:Error::limit_exceeded;
    result.serialized_size=static_cast<std::size_t>(serialized);
    const LzssPositionDistance8mFrameRequirements r{result.serialized_size,static_cast<std::size_t>(t),static_cast<std::size_t>(raw),static_cast<std::size_t>(aggregate)};
    output=result;requirements=r;return Error::none;
}
} // namespace marc::frame::internal

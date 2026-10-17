#include "entropy/position_blocked_huffman_64m.hpp"
#include "core/buffer_overlap.hpp"
#include "core/endian.hpp"
#include <algorithm>
#include <bit>

namespace marc::entropy::internal {
namespace {
using Error=PositionBlockedHuffmanError;
using Descriptor=PositionBlockedHuffman64mDescriptor;
using Buffer=std::array<std::byte,2590>;
bool active(const Descriptor& d,std::size_t i) noexcept {
    return i<5 ? (d.field_mask&(1U<<i))!=0 : (d.override_mask&(UINT64_C(1)<<(i-5)))!=0;
}
std::uint16_t alphabet(std::size_t i) noexcept {
    return i<5 ? position_blocked_huffman_64m_fields[i] : position_blocked_huffman_64m_alphabets[i-5];
}
Error overlap(const void* a,std::size_t n,const void* b,std::size_t m) noexcept {
    const auto result=core::check_buffer_overlap(a,n,b,m);
    return result==core::BufferOverlap::disjoint ? Error::none
        : result==core::BufferOverlap::arithmetic_overflow ? Error::arithmetic_overflow : Error::overlapping_buffers;
}
template<class T> T load(std::span<const std::byte> b,std::size_t at) noexcept {
    T value{};(void)core::load_le(b,at,value);return value;
}
Error pack(const Descriptor& d,const core::DecoderLimits& limits,Buffer& b,std::size_t& n) noexcept {
    if (d.field_mask>=32 || (d.override_mask>>58)
        || (d.payload_size==0 ? d.final_valid_bits!=0 : d.final_valid_bits==0 || d.final_valid_bits>8)
        || d.payload_size>(UINT64_C(15)*d.decision_count+7)/8
        || (!d.decision_count && (d.payload_size || d.field_mask || d.override_mask))) return Error::invalid_metadata;
    if (core::validate_limits(limits)!=core::LimitError::none
        || limits.max_huffman_code_length<15 || d.decision_count>limits.max_block_size
        || d.payload_size>limits.max_compressed_payload_size
        || UINT64_C(511)*(std::popcount(d.field_mask)+std::popcount(d.override_mask))>limits.max_entropy_table_entries)
        return Error::limit_exceeded;
    for (std::size_t c=0;c<58;++c)
        if ((d.override_mask&(UINT64_C(1)<<c)) && !(d.field_mask&(1U<<position_blocked_huffman_64m_field(c))))
            return Error::invalid_metadata;
    (void)core::store_le(b,0,d.decision_count);(void)core::store_le(b,4,d.payload_size);
    b[8]=static_cast<std::byte>(d.final_valid_bits);b[9]=std::byte{15};b[10]=std::byte{5};
    b[11]=static_cast<std::byte>(d.field_mask);
    (void)core::store_le(b,12,std::uint16_t{58});(void)core::store_le(b,16,std::uint32_t{4680});
    for (std::size_t i=0;i<8;++i) b[24+i]=static_cast<std::byte>((d.override_mask>>(8*i))&255);
    std::size_t cursor=32;
    for (std::size_t i=0;i<63;++i) {
        const auto& m=d.models[i]; const auto a=alphabet(i);
        const bool clear=std::all_of(m.lengths.begin(),m.lengths.end(),[](auto x){return x==0;});
        if (!active(d,i)) {
            if (!clear || m.single_symbol!=position_blocked_huffman_no_single) return Error::noncanonical;
            continue;
        }
        if (m.single_symbol!=position_blocked_huffman_no_single) {
            if (!clear || m.single_symbol>=a) return Error::invalid_model;
            b[cursor++]=std::byte{0};b[cursor++]=static_cast<std::byte>(m.single_symbol);continue;
        }
        if (std::any_of(m.lengths.begin()+a,m.lengths.end(),[](auto x){return x!=0;})
            || validate_code_lengths(m.lengths)!=HuffmanTableError::none) return Error::invalid_model;
        const auto present=static_cast<std::uint16_t>(std::count_if(m.lengths.begin(),m.lengths.end(),[](auto x){return x!=0;}));
        if (present<2) return Error::noncanonical;
        if (1+(a+1)/2<=3+2*present) {
            b[cursor++]=std::byte{1};
            for (std::size_t s=0;s<a;s+=2)
                b[cursor++]=static_cast<std::byte>(m.lengths[s]|((s+1<a ? m.lengths[s+1] : 0)<<4));
        } else {
            b[cursor++]=std::byte{2};(void)core::store_le(b,cursor,present);cursor+=2;
            for (std::size_t s=0;s<a;++s) if (m.lengths[s]) {
                b[cursor++]=static_cast<std::byte>(s);b[cursor++]=static_cast<std::byte>(m.lengths[s]);
            }
        }
    }
    if (sizeof(Descriptor)+UINT64_C(2590)+cursor+d.payload_size>limits.max_internal_buffered_bytes)
        return Error::limit_exceeded;
    n=cursor;return Error::none;
}
} // namespace
PositionBlockedHuffmanError validate_position_blocked_huffman_64m_descriptor(
    const Descriptor& d,const core::DecoderLimits& limits,std::size_t& size) noexcept {
    for (const auto e : {overlap(&d,sizeof(d),&size,sizeof(size)),overlap(&limits,sizeof(limits),&size,sizeof(size))})
        if (e!=Error::none) return e;
    Buffer b{};std::size_t n{};const auto error=pack(d,limits,b,n);
    if (error==Error::none) size=n;
    return error;
}
PositionBlockedHuffmanError serialize_position_blocked_huffman_64m_descriptor(
    const Descriptor& d,const core::DecoderLimits& limits,std::span<std::byte> output,std::size_t& written) noexcept {
    for (const auto e : {overlap(&d,sizeof(d),output.data(),output.size()),
         overlap(&limits,sizeof(limits),output.data(),output.size()),overlap(&d,sizeof(d),&written,sizeof(written)),
         overlap(&limits,sizeof(limits),&written,sizeof(written)),overlap(output.data(),output.size(),&written,sizeof(written))})
        if (e!=Error::none) return e;
    Buffer b{};std::size_t n{};if (const auto e=pack(d,limits,b,n);e!=Error::none) return e;
    if (output.size()<n) return Error::output_too_small;
    std::copy_n(b.begin(),n,output.begin());written=n;return Error::none;
}
PositionBlockedHuffmanError parse_position_blocked_huffman_64m_descriptor(
    std::span<const std::byte> input,std::uint32_t decisions,std::uint32_t payload,
    const core::DecoderLimits& limits,Descriptor& output) noexcept {
    for (const auto e : {overlap(input.data(),input.size(),&output,sizeof(output)),overlap(&limits,sizeof(limits),&output,sizeof(output))})
        if (e!=Error::none) return e;
    if (input.size()<32) return Error::truncated;
    if (input.size()>2590) return Error::trailing_data;
    if (input.size()+sizeof(Descriptor)+UINT64_C(2590)>limits.max_internal_buffered_bytes) return Error::limit_exceeded;
    if (load<std::uint32_t>(input,0)!=decisions || load<std::uint32_t>(input,4)!=payload
        || input[9]!=std::byte{15} || input[10]!=std::byte{5} || load<std::uint16_t>(input,12)!=58
        || load<std::uint16_t>(input,14) || load<std::uint32_t>(input,16)!=4680 || load<std::uint32_t>(input,20))
        return Error::invalid_metadata;
    Descriptor d{};d.decision_count=decisions;d.payload_size=payload;
    d.final_valid_bits=std::to_integer<std::uint8_t>(input[8]);d.field_mask=std::to_integer<std::uint8_t>(input[11]);
    for (std::size_t i=0;i<8;++i) d.override_mask|=std::uint64_t{std::to_integer<unsigned>(input[24+i])}<<(8*i);
    std::size_t cursor=32;
    for (std::size_t i=0;i<63;++i) {
        if (!active(d,i)) continue;
        if (cursor==input.size()) return Error::truncated;
        auto& m=d.models[i];const auto a=alphabet(i);const auto mode=std::to_integer<unsigned>(input[cursor++]);
        if (mode==0) {
            if (cursor==input.size()) return Error::truncated;
            m.single_symbol=std::to_integer<std::uint8_t>(input[cursor++]);
        } else if (mode==1) {
            const std::size_t extent=(a+1)/2;
            if (extent>input.size()-cursor) return Error::truncated;
            for (std::size_t s=0;s<a;++s)
                m.lengths[s]=static_cast<std::uint8_t>((std::to_integer<unsigned>(input[cursor+s/2])>>(4*(s%2)))&15);
            if ((a%2) && (std::to_integer<unsigned>(input[cursor+extent-1])>>4)) return Error::noncanonical;
            cursor+=extent;
        } else if (mode==2) {
            if (input.size()-cursor<2) return Error::truncated;
            const auto present=load<std::uint16_t>(input,cursor);cursor+=2;
            if (present<2 || present>a) return Error::invalid_model;
            if (std::size_t{2}*present>input.size()-cursor) return Error::truncated;
            int previous=-1;
            for (std::size_t k=0;k<present;++k) {
                const auto s=std::to_integer<unsigned>(input[cursor++]);
                const auto length=std::to_integer<std::uint8_t>(input[cursor++]);
                if (static_cast<int>(s)<=previous || s>=a || !length || length>15) return Error::invalid_model;
                m.lengths[s]=length;previous=static_cast<int>(s);
            }
        } else return Error::invalid_model;
    }
    if (cursor!=input.size()) return Error::trailing_data;
    Buffer canonical{};std::size_t n{};
    if (const auto e=pack(d,limits,canonical,n);e!=Error::none) return e;
    if (n!=input.size() || !std::equal(input.begin(),input.end(),canonical.begin())) return Error::noncanonical;
    output=d;return Error::none;
}
} // namespace marc::entropy::internal

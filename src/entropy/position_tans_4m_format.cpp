#include "entropy/position_tans_4m.hpp"
#include "core/endian.hpp"
#include <algorithm>

namespace marc::entropy::internal {
namespace {
using Error=PositionTans4mError;
using Buffer=std::array<std::byte,position_tans_4m_descriptor_capacity>;
Error pack(const PositionTans4mDescriptor& d, const core::DecoderLimits& limits,
           Buffer& b, std::size_t& extent) noexcept {
    const std::uint64_t ceiling=2+(UINT64_C(12)*d.decision_count+7)/8;
    if (d.payload_size<2 || d.payload_size>ceiling
        || (d.payload_size==2 ? d.final_valid_bits!=0
                             : d.final_valid_bits==0 || d.final_valid_bits>8))
        return Error::invalid_payload_size;
    if (core::validate_limits(limits)!=core::LimitError::none
        || d.decision_count>limits.max_block_size
        || d.payload_size>limits.max_compressed_payload_size
        || position_tans_4m_table_entries>limits.max_entropy_table_entries)
        return Error::limit_exceeded;
    (void)core::store_le(b,0,d.decision_count);
    (void)core::store_le(b,4,d.payload_size);
    b[8]=std::byte{12}; b[9]=static_cast<std::byte>(d.final_valid_bits);
    (void)core::store_le(b,12,std::uint16_t{54});
    (void)core::store_le(b,16,std::uint32_t{4636});
    std::size_t cursor=31;
    for (std::size_t c=0;c<54;++c) {
        const auto begin=position_tans_4m_offsets[c];
        const auto n=position_tans_4m_alphabets[c];
        std::size_t present{},last{};
        std::uint32_t sum{};
        for (std::size_t s=0;s<n;++s) {
            const auto f=d.frequencies[begin+s]; sum+=f;
            if (f) { ++present; last=s; }
        }
        if (!present) continue;
        if (sum!=4096 || d.decision_count==0) return Error::invalid_frequencies;
        b[24+c/8]|=static_cast<std::byte>(1U<<(c%8));
        if (present==1) {
            b[cursor++]=std::byte{0};b[cursor++]=static_cast<std::byte>(last);
        } else if (1+2*(n-1)<=1+3*present) {
            b[cursor++]=std::byte{1};
            for (std::size_t s=0;s+1<n;++s) {
                (void)core::store_le(b,cursor,d.frequencies[begin+s]);cursor+=2;
            }
        } else {
            b[cursor++]=std::byte{2};
            (void)core::store_le(b,cursor,static_cast<std::uint16_t>(present));cursor+=2;
            for (std::size_t s=0;s<n;++s) {
                if (!d.frequencies[begin+s]) continue;
                b[cursor++]=static_cast<std::byte>(s);
                if (s!=last) { (void)core::store_le(b,cursor,d.frequencies[begin+s]);cursor+=2; }
            }
        }
    }
    if (static_cast<std::uint64_t>(cursor)+d.payload_size>limits.max_internal_buffered_bytes)
        return Error::limit_exceeded;
    extent=cursor;
    return Error::none;
}
}
PositionTans4mError validate_position_tans_4m_descriptor(
    const PositionTans4mDescriptor& d,const core::DecoderLimits& limits,std::size_t& size) noexcept {
    Buffer b{};std::size_t n{};
    const auto error=pack(d,limits,b,n);
    if (error==Error::none) size=n;
    return error;
}
PositionTans4mError serialize_position_tans_4m_descriptor(
    const PositionTans4mDescriptor& d,const core::DecoderLimits& limits,
    std::span<std::byte> output,std::size_t& written) noexcept {
    Buffer b{};std::size_t n{};
    if (const auto e=pack(d,limits,b,n);e!=Error::none) return e;
    if (output.size()<n) return Error::output_too_small;
    std::copy_n(b.begin(),n,output.begin());written=n;return Error::none;
}
PositionTans4mError parse_position_tans_4m_descriptor(
    std::span<const std::byte> input,std::uint32_t expected_decisions,
    std::uint32_t expected_payload,const core::DecoderLimits& limits,
    PositionTans4mDescriptor& output) noexcept {
    if (input.size()<31) return Error::truncated;
    if (input.size()>9249) return Error::trailing_data;
    if (input.size()>limits.max_internal_buffered_bytes) return Error::limit_exceeded;
    PositionTans4mDescriptor d{};
    std::uint16_t flags{},contexts{},reserved{};
    std::uint32_t frequencies{},tail{};
    (void)core::load_le(input,0,d.decision_count);(void)core::load_le(input,4,d.payload_size);
    d.final_valid_bits=std::to_integer<std::uint8_t>(input[9]);
    (void)core::load_le(input,10,flags);(void)core::load_le(input,12,contexts);
    (void)core::load_le(input,14,reserved);(void)core::load_le(input,16,frequencies);
    (void)core::load_le(input,20,tail);
    if (d.decision_count!=expected_decisions || d.payload_size!=expected_payload
        || input[8]!=std::byte{12} || flags || contexts!=54 || reserved
        || frequencies!=4636 || tail
        // The final seven-byte mask carries six valid context bits (54 total);
        // only its upper two bits are reserved.
        || (std::to_integer<unsigned>(input[30])&0xc0U)) return Error::invalid_metadata;
    std::size_t cursor=31;
    for (std::size_t c=0;c<54;++c) {
        if ((std::to_integer<unsigned>(input[24+c/8])&(1U<<(c%8)))==0) continue;
        if (cursor==input.size()) return Error::truncated;
        const auto mode=std::to_integer<unsigned>(input[cursor++]);
        const auto begin=position_tans_4m_offsets[c];
        const auto alphabet=position_tans_4m_alphabets[c];
        if (mode==0) {
            if (cursor==input.size()) return Error::truncated;
            const auto symbol=std::to_integer<unsigned>(input[cursor++]);
            if (symbol>=alphabet) return Error::invalid_frequencies;
            d.frequencies[begin+symbol]=4096;
        } else if (mode==1) {
            if (2U*(alphabet-1)>input.size()-cursor) return Error::truncated;
            std::uint32_t sum{};
            for (std::size_t s=0;s+1<alphabet;++s) {
                (void)core::load_le(input,cursor,d.frequencies[begin+s]);cursor+=2;
                sum+=d.frequencies[begin+s];
            }
            if (sum>4096) return Error::invalid_frequencies;
            d.frequencies[begin+alphabet-1]=static_cast<std::uint16_t>(4096-sum);
        } else if (mode==2) {
            std::uint16_t count{};
            if (!core::load_le(input,cursor,count)) return Error::truncated;
            cursor+=2;
            if (count<2 || count>alphabet) return Error::invalid_frequencies;
            if (3U*(count-1)+1>input.size()-cursor) return Error::truncated;
            int previous=-1;std::uint32_t sum{};
            for (std::size_t i=0;i<count;++i) {
                const auto s=std::to_integer<unsigned>(input[cursor++]);
                if (static_cast<int>(s)<=previous || s>=alphabet) return Error::invalid_frequencies;
                previous=static_cast<int>(s);
                std::uint16_t f{};
                if (i+1<count) { (void)core::load_le(input,cursor,f);cursor+=2; }
                else { if (sum>=4096) return Error::invalid_frequencies;f=static_cast<std::uint16_t>(4096-sum); }
                if (!f || f>4096 || sum+f>4096) return Error::invalid_frequencies;
                d.frequencies[begin+s]=f;sum+=f;
            }
        } else return Error::invalid_mode;
    }
    if (cursor!=input.size()) return Error::trailing_data;
    Buffer canonical{};std::size_t n{};
    if (const auto e=pack(d,limits,canonical,n);e!=Error::none) return e;
    if (n!=input.size() || !std::equal(input.begin(),input.end(),canonical.begin()))
        return Error::noncanonical;
    output=d;return Error::none;
}
} // namespace marc::entropy::internal




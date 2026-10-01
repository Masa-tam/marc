#include "lzss_position_distance_4m_diagnostic_finder.hpp"
#include "dictionary/lzss_position_distance_4m_five_prefix_finder.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <memory>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace marc::dictionary::internal;
using F=LzssPositionDistance4mDiagnosticFinder;
using E=LzssShortPrefixError;
constexpr LzssParameters params{4194304,3,258,0};
std::size_t cases{};
void require(bool condition,const std::source_location location=std::source_location::current()) {
    if(!condition)throw std::runtime_error("diagnostic check failed at line "+std::to_string(location.line()));
}
marc::core::DecoderLimits limits() {
    marc::core::DecoderLimits l{};l.max_block_size=4194304;
    l.max_compressed_payload_size=75497477;l.max_internal_buffered_bytes=512U*1024U*1024U;return l;
}
void differential(std::span<const std::byte> raw,LzssParameters p) {
    const auto q=calculate_lzss_position_distance_4m_diagnostic_workspace(raw.size(),p,limits());
    require(q.error==E::none);
    std::vector<std::uint32_t> storage(q.workspace_size/4+4,0xa5a5a5a5),reference(storage);
    F f;LzssPositionDistance4mFivePrefixFinder original;
    require(initialize_lzss_position_distance_4m_diagnostic_finder(raw,p,limits(),std::as_writable_bytes(std::span{storage}),f)==E::none);
    require(initialize_lzss_position_distance_4m_five_prefix_finder(raw,p,limits(),std::as_writable_bytes(std::span{reference}),original)==E::none);
    std::size_t tokens{};
    for(std::size_t position=0;position<raw.size();) {
        const auto a=f.find_match(position),b=original.find_match(position);
        require(a==b);const auto step=a.length>=3?a.length:1U;
        f.advance(position,position+step);original.advance(position,position+step);
        position+=step;++tokens;
    }
    const auto c=f.counters();
    require(!c.overflow&&!c.invalid_find_calls&&!c.invalid_advance_calls);
    require(c.find_calls==tokens&&c.advance_calls==tokens&&c.advanced_positions==raw.size());
    require(c.initialized_words==q.workspace_size/4);
    for(std::size_t i=0;i<3;++i)require(c.insertions[i]==(raw.size()>=i+3?raw.size()-(i+2):0));
    require(c.extension_equal_bytes<=c.extension_comparisons);
    require(storage==reference); // Includes every link/head and unborrowed tail.
    ++cases;
}
void query_and_failures() {
    for(std::size_t n:{0U,1U,2U,3U,4U,5U,4194304U}) {
        const auto q=calculate_lzss_position_distance_4m_diagnostic_workspace(n,params,limits());
        require(q.error==E::none&&q.workspace_size==(n<3?0:12*(65536+n)));
        auto l=limits();l.max_block_size=std::max<std::size_t>(n,1);
        l.max_internal_buffered_bytes=n+q.workspace_size+sizeof(F);
        require(calculate_lzss_position_distance_4m_diagnostic_workspace(n,params,l).error==E::none);
        --l.max_internal_buffered_bytes;
        require(calculate_lzss_position_distance_4m_diagnostic_workspace(n,params,l).error==E::workspace_limit_exceeded);
        ++cases;
    }
    require(calculate_lzss_position_distance_4m_diagnostic_workspace(4194305,params,limits()).error==E::input_limit_exceeded);
    std::array<std::byte,64> raw{};
    const auto q=calculate_lzss_position_distance_4m_diagnostic_workspace(raw.size(),params,limits());
    std::vector<std::uint32_t> words(q.workspace_size/4+4,0xa5a5a5a5);
    const auto bytes=std::as_writable_bytes(std::span{words});F f;
    require(initialize_lzss_position_distance_4m_diagnostic_finder(raw,params,limits(),bytes,f)==E::none);
    f.advance(0,1);require(f.find_match(1)==(LzssMatch{1,63}));
    std::array<std::byte,sizeof(F)> snapshot{};std::memcpy(snapshot.data(),&f,sizeof(f));
    const auto before=words;
    const auto check=[&](E actual,E expected) {
        require(actual==expected&&words==before&&std::memcmp(snapshot.data(),&f,sizeof(f))==0);++cases;
    };
    check(initialize_lzss_position_distance_4m_diagnostic_finder(raw,params,limits(),bytes.first(q.workspace_size-1),f),E::workspace_too_small);
    check(initialize_lzss_position_distance_4m_diagnostic_finder(raw,params,limits(),bytes.subspan(1),f),E::misaligned_workspace);
    check(initialize_lzss_position_distance_4m_diagnostic_finder(bytes.first(3),params,limits(),bytes,f),E::overlapping_buffers);
    check(initialize_lzss_position_distance_4m_diagnostic_finder(std::as_bytes(std::span{&f,1}).first(3),params,limits(),bytes,f),E::overlapping_buffers);
    check(initialize_lzss_position_distance_4m_diagnostic_finder(raw,params,limits(),bytes,f,LzssTypedTokenVariant::field_context_1m_short_length_escape),E::invalid_parameters);
    auto l=limits();
    // Metadata is constructed inside a real, sufficiently large active buffer.
    std::vector<std::byte> metadata(q.workspace_size+alignof(F),std::byte{0xa5});
    void* aligned=metadata.data();std::size_t space=metadata.size();
    require(std::align(alignof(F),q.workspace_size,aligned,space)!=nullptr);
    auto active=std::span<std::byte>{static_cast<std::byte*>(aligned),q.workspace_size};
    auto* p=std::construct_at(static_cast<LzssParameters*>(aligned),params);
    auto meta_before=metadata;
    check(initialize_lzss_position_distance_4m_diagnostic_finder(raw,*p,l,active,f),E::overlapping_buffers);
    require(metadata==meta_before);std::destroy_at(p);
    auto* embedded_limits=std::construct_at(static_cast<marc::core::DecoderLimits*>(aligned),l);
    meta_before=metadata;
    check(initialize_lzss_position_distance_4m_diagnostic_finder(raw,params,*embedded_limits,active,f),E::overlapping_buffers);
    require(metadata==meta_before);std::destroy_at(embedded_limits);
    auto* embedded=std::construct_at(static_cast<F*>(aligned));
    meta_before=metadata;
    require(initialize_lzss_position_distance_4m_diagnostic_finder(raw,params,l,active,*embedded)==E::overlapping_buffers);
    require(metadata==meta_before);std::destroy_at(embedded);++cases;
    l.max_block_size=raw.size();l.max_internal_buffered_bytes=raw.size()+q.workspace_size+sizeof(F)-1;
    check(initialize_lzss_position_distance_4m_diagnostic_finder(raw,params,l,bytes,f),E::workspace_limit_exceeded);
    for(int field=0;field<3;++field) {
        l=limits();if(field==0)l.max_frame_size=2;else if(field==1)l.max_block_size=2;else l.max_total_output_size=2;
        check(initialize_lzss_position_distance_4m_diagnostic_finder(raw,params,l,bytes,f),E::input_limit_exceeded);
    }
    require(f.find_match(2)==LzssMatch{});f.advance(0,2);require(f.find_match(1)==LzssMatch{});
    require(f.counters().invalid_find_calls==2&&f.counters().invalid_advance_calls==1);
    require(initialize_lzss_position_distance_4m_diagnostic_finder(raw,params,limits(),bytes,f)==E::none);
    require(f.counters().find_calls==0&&f.counters().advance_calls==0);
    f.advance(0,65);require(f.find_match(0)==LzssMatch{});++cases;
}
void wide_and_ties() {
    for(std::size_t distance:{65535U,65536U,65537U,1048575U,1048576U,1048577U,2097152U,4194299U,4194301U})
    for(std::size_t length:{3U,4U,5U}) {
        if(distance+length>4194304)continue;
        std::vector<std::byte> raw(distance+length);
        for(std::size_t i=0;i<length;++i)raw[i]=raw[distance+i]=std::byte(0xa0+i);
        const auto q=calculate_lzss_position_distance_4m_diagnostic_workspace(raw.size(),params,limits());
        std::vector<std::uint32_t> words(q.workspace_size/4);F f;
        require(initialize_lzss_position_distance_4m_diagnostic_finder(raw,params,limits(),std::as_writable_bytes(std::span{words}),f)==E::none);
        f.advance(0,distance);
        require(f.find_match(distance)==(LzssMatch{static_cast<std::uint32_t>(distance),static_cast<std::uint32_t>(length)}));
        ++cases;
    }
    std::array<std::byte,30> raw{};const auto q=calculate_lzss_position_distance_4m_diagnostic_workspace(raw.size(),params,limits());
    std::vector<std::uint32_t> words(q.workspace_size/4);F f;
    require(initialize_lzss_position_distance_4m_diagnostic_finder(raw,params,limits(),std::as_writable_bytes(std::span{words}),f)==E::none);
    f.advance(0,20);require(f.find_match(20)==(LzssMatch{1,10}));++cases;
    // Hand-checkable overlap: literal then length-8 match, all interior insertion.
    raw={};const auto small=std::span<const std::byte>{raw}.first(9);
    require(initialize_lzss_position_distance_4m_diagnostic_finder(small,params,limits(),std::as_writable_bytes(std::span{words}),f)==E::none);
    require(f.find_match(0)==LzssMatch{});f.advance(0,1);
    require(f.find_match(1)==(LzssMatch{1,8}));f.advance(1,9);
    const auto c=f.counters();
    require(c.initialized_words==3*(65536+9)&&c.find_calls==2&&c.advance_calls==2&&c.advanced_positions==9);
    require(c.chain_visits==std::array<std::uint64_t,3>{1,1,1});
    require(c.prefix_comparisons==std::array<std::uint64_t,3>{3,4,5});
    require(c.insertions==std::array<std::uint64_t,3>{7,6,5});
    require(c.fast_path_comparisons==2&&c.candidate_filter_comparisons==1);
    require(c.extension_comparisons==3&&c.extension_equal_bytes==3);++cases;
}
}
int main() {
    try {
        query_and_failures();wide_and_ties();
        for(std::size_t n:{0U,1U,2U,3U,4U,5U,7U,16U,31U,64U,127U,259U,513U})
        for(unsigned pattern=0;pattern<3;++pattern) {
            std::vector<std::byte> raw(n);std::uint32_t rng=1390;
            for(std::size_t i=0;i<n;++i) {rng=rng*1664525+1013904223;raw[i]=pattern==0?std::byte{}:pattern==1?std::byte(i%7):std::byte(rng>>24);}
            for(unsigned window:{1U,3U,17U,4194304U})for(unsigned maximum:{3U,4U,5U,258U}) {
                auto p=params;p.window_size=window;p.max_match_length=maximum;differential(raw,p);
            }
        }
        std::vector<std::byte> full(4194304);std::uint32_t rng=1390;
        for(std::size_t i=0;i<full.size();++i) {rng=rng*1664525+1013904223;full[i]=i<70000?std::byte(rng>>24):full[i%70000];}
        differential(full,params);
        std::cout<<"PASS diagnostic cases="<<cases<<'\n';return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}

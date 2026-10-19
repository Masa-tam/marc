#include "frame/lzss_position_adaptive_huffman_owned.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace marc::frame::internal;
using Profile=marc::entropy::internal::PositionAdaptiveHuffmanProfile;
using Code=marc::core::ErrorCode;
using Status=marc::core::StreamStatus;
auto profile=Profile::window_1m;
std::uint32_t extent=1048576,decisions_per_byte=8;
constexpr auto end=marc::core::flag_value(marc::core::ProcessFlags::end_input);
void check(bool condition,const std::source_location where=std::source_location::current()) {
    if (!condition) throw std::runtime_error("native FGK owner check failed at line "+std::to_string(where.line()));
}
PositionAdaptiveHuffmanStreamHeader header(std::uint32_t frame,std::size_t original) {
    PositionAdaptiveHuffmanStreamHeader h{};h.profile=profile;h.frame_size=frame;
    h.original_size=original;h.dictionary={extent,3,258,0};return h;
}
marc::core::DecoderLimits limits(std::uint32_t frame) {
    marc::core::DecoderLimits l{};l.max_frame_size=frame;l.max_lz_distance=extent;
    l.max_block_size=decisions_per_byte*static_cast<std::uint64_t>(frame);
    l.max_compressed_payload_size=(267*static_cast<std::uint64_t>(frame)+7)/8;
    // Query only: allocation is always admitted against the returned exact size.
    l.max_internal_buffered_bytes=std::numeric_limits<std::uint64_t>::max();return l;
}
std::vector<std::byte> drive(marc::core::Transform& codec,const std::vector<std::byte>& input,
    std::size_t input_chunk,std::size_t output_chunk,bool late_end=false) {
    std::vector<std::byte> result,output(output_chunk);std::size_t consumed{},offered{};
    const auto maximum_calls=8*(input.size()+extent+1);
    for (std::size_t calls=0;calls<maximum_calls;++calls) {
        if (consumed==offered) offered=std::min(input.size(),offered+input_chunk);
        const auto in=std::span(input).subspan(consumed,offered-consumed);
        std::fill(output.begin(),output.end(),std::byte{0x6d});
        const auto flags=offered==input.size() && (!late_end || in.empty()) ? end : 0;
        const auto r=codec.process(in,output,flags);
        check(r.input_consumed<=in.size() && r.output_produced<=output.size());
        check(r.status!=Status::error && (r.status!=Status::progress || r.input_consumed || r.output_produced));
        check(std::all_of(output.begin()+r.output_produced,output.end(),[](auto b){return b==std::byte{0x6d};}));
        consumed+=r.input_consumed;result.insert(result.end(),output.begin(),output.begin()+r.output_produced);
        if (r.status==Status::end_of_stream) {
            check(consumed==input.size());std::fill(output.begin(),output.end(),std::byte{0x6d});
            const auto stable=codec.process({},output,0);
            check(stable.status==Status::end_of_stream && !stable.input_consumed && !stable.output_produced);
            check(std::all_of(output.begin(),output.end(),[](auto b){return b==std::byte{0x6d};}));return result;
        }
    }
    check(false);return {};
}
std::vector<std::byte> encode(const std::vector<std::byte>& raw,std::uint32_t frame,
    std::size_t input_chunk=65536,std::size_t output_chunk=65536,bool late=false) {
    auto l=limits(frame);const auto h=header(frame,raw.size());PositionAdaptiveHuffmanWorkspace w{};
    check(PositionAdaptiveHuffmanOwnedEncoder::requirements(h,l,w)==Code::none);
    l.max_internal_buffered_bytes=w.aggregate_bytes;Code error{};
    auto codec=PositionAdaptiveHuffmanOwnedEncoder::create(h,l,error);check(codec && error==Code::none);
    return drive(*codec,raw,input_chunk,output_chunk,late);
}
void roundtrip(const std::vector<std::byte>& raw,std::uint32_t frame,bool chunks) {
    const auto wire=encode(raw,frame);auto l=limits(frame);PositionAdaptiveHuffmanWorkspace w{};
    check(PositionAdaptiveHuffmanOwnedDecoder::requirements(profile,frame,l,w)==Code::none);
    l.max_internal_buffered_bytes=w.aggregate_bytes;
    PositionAdaptiveHuffmanStreamHeader parsed{};std::size_t used{};
    check(parse_position_adaptive_huffman_stream(profile,wire,l,parsed,used)==PositionAdaptiveHuffmanFormatError::none);
    check(used==112 && parsed.dictionary.window_size==extent && parsed.frame_size==frame && parsed.original_size==raw.size());
    for (auto ic:chunks ? std::vector<std::size_t>{1,7,65536} : std::vector<std::size_t>{65536}) {
        for (auto oc:chunks ? std::vector<std::size_t>{1,19,65536} : std::vector<std::size_t>{65536}) {
            check(encode(raw,frame,ic,oc,true)==wire);Code error{};
            auto decoder=PositionAdaptiveHuffmanOwnedDecoder::create(profile,frame,l,error);
            check(decoder && error==Code::none && drive(*decoder,wire,ic,oc,true)==raw);
        }
    }
}
void admission(std::uint32_t frame) {
    auto h=header(frame,frame+1);auto l=limits(frame);PositionAdaptiveHuffmanWorkspace e{},d{};
    check(PositionAdaptiveHuffmanOwnedEncoder::requirements(h,l,e)==Code::none);
    check(PositionAdaptiveHuffmanOwnedDecoder::requirements(profile,frame,l,d)==Code::none);
    std::vector<std::byte> raw(frame+1,std::byte{'A'});Code error{};
    l.max_internal_buffered_bytes=e.aggregate_bytes;
    {
        auto encoder=PositionAdaptiveHuffmanOwnedEncoder::create(h,l,error);check(encoder && error==Code::none);
        const auto wire=drive(*encoder,raw,65536,65536);check(!wire.empty());
    }
    --l.max_internal_buffered_bytes;auto sentinel=e;sentinel.aggregate_bytes=17;
    check(PositionAdaptiveHuffmanOwnedEncoder::requirements(h,l,sentinel)==Code::limit_exceeded && sentinel.aggregate_bytes==17);
    check(!PositionAdaptiveHuffmanOwnedEncoder::create(h,l,error) && error==Code::limit_exceeded);
    l.max_internal_buffered_bytes=d.aggregate_bytes;
    {
        // The encoder is complete before the decoder is admitted; simultaneous
        // retention is not part of the per-instance exact-quota assertion.
        const auto wire=encode(raw,frame);
        auto decoder=PositionAdaptiveHuffmanOwnedDecoder::create(profile,frame,l,error);check(decoder && error==Code::none);
        check(drive(*decoder,wire,65536,65536)==raw);
    }
    --l.max_internal_buffered_bytes;sentinel=d;sentinel.aggregate_bytes=17;
    check(PositionAdaptiveHuffmanOwnedDecoder::requirements(profile,frame,l,sentinel)==Code::limit_exceeded && sentinel.aggregate_bytes==17);
    check(!PositionAdaptiveHuffmanOwnedDecoder::create(profile,frame,l,error) && error==Code::limit_exceeded);
    std::cout<<"window="<<extent<<" capacity="<<frame<<" aggregate encode="<<e.aggregate_bytes<<" decode="<<d.aggregate_bytes
        <<" finder="<<e.finder_bytes<<" owned encode="<<e.owned_bytes<<" decode="<<d.owned_bytes<<'\n';
}
void rejected(const std::vector<std::byte>& wire,std::size_t expected_prefix) {
    auto l=limits(5);PositionAdaptiveHuffmanWorkspace w{};
    check(PositionAdaptiveHuffmanOwnedDecoder::requirements(profile,5,l,w)==Code::none);
    l.max_internal_buffered_bytes=w.aggregate_bytes;Code error{};
    auto decoder=PositionAdaptiveHuffmanOwnedDecoder::create(profile,5,l,error);check(static_cast<bool>(decoder));
    std::array<std::byte,32> out{};std::vector<std::byte> published;std::size_t used{};
    for (std::size_t calls=0;calls<100;++calls) {
        out.fill(std::byte{0x5a});const auto r=decoder->process(std::span(wire).subspan(used),out,end);
        check(r.input_consumed<=wire.size()-used && r.output_produced<=out.size());used+=r.input_consumed;
        if (r.status==Status::error) {
            check(!r.output_produced && std::all_of(out.begin(),out.end(),[](auto b){return b==std::byte{0x5a};}));
            check(published.size()==expected_prefix);
            for (std::size_t i=0;i<published.size();++i) check(published[i]==static_cast<std::byte>("ABABA"[i]));
            const auto stable=decoder->process({},out,0);
            check(stable.status==Status::error && stable.error.code==r.error.code && !stable.input_consumed && !stable.output_produced);
            check(std::all_of(out.begin(),out.end(),[](auto b){return b==std::byte{0x5a};}));return;
        }
        check(r.status!=Status::end_of_stream);
        check(r.status!=Status::progress || r.input_consumed || r.output_produced);
        published.insert(published.end(),out.begin(),out.begin()+r.output_produced);
    }
    check(false);
}
void failed_frame() {
    std::vector<std::byte> raw;for (char c:std::string("ABABAABABA")) raw.push_back(static_cast<std::byte>(c));
    auto wire=encode(raw,5);constexpr std::size_t second=112+80+5;
    check(wire.size()==second+80+5 && wire[second+32]==std::byte{5} && wire[second+74]==std::byte{1});
    // ABABA needs33 bits. Its metadata now promises only32, failing after four
    // private tokens while retaining the earlier successfully published frame.
    wire[second+32]=wire[second+68]=std::byte{4};wire[second+74]=std::byte{8};wire.pop_back();rejected(wire,5);
    const auto valid=encode(raw,5);
    for (std::size_t n=0;n<valid.size();++n)
        rejected(std::vector<std::byte>(valid.begin(),valid.begin()+n),n>=second ? 5 : 0);
    auto padding=valid;padding.back()|=std::byte{0x80};rejected(padding,5);
    auto trailing=valid;trailing.push_back(std::byte{0});
    // Both valid frames may drain before the strict trailing-data error.
    auto l=limits(5);Code error{};auto decoder=PositionAdaptiveHuffmanOwnedDecoder::create(profile,5,l,error);
    check(static_cast<bool>(decoder));std::array<std::byte,32> out{};std::size_t used{},published{};bool failed{};
    for (unsigned calls=0;calls<100;++calls) {
        out.fill(std::byte{0x37});const auto r=decoder->process(std::span(trailing).subspan(used),out,end);used+=r.input_consumed;
        if (r.status==Status::error) {check(!r.output_produced && published==raw.size());failed=true;
            check(std::all_of(out.begin(),out.end(),[](auto b){return b==std::byte{0x37};}));break;}
        check(r.status!=Status::end_of_stream);published+=r.output_produced;
    }
    check(failed);
}
void failed_encode() {
    auto l=limits(5);l.max_compressed_payload_size=1;auto h=header(5,5);Code error{};
    auto encoder=PositionAdaptiveHuffmanOwnedEncoder::create(h,l,error);check(encoder && error==Code::none);
    std::vector<std::byte> raw(5,std::byte{'A'}),out(128,std::byte{0x61});
    const auto first=encoder->process(raw,out,end);
    check(first.status==Status::progress && first.output_produced==112 && !first.input_consumed);
    std::fill(out.begin(),out.end(),std::byte{0x61});
    const auto rejected=encoder->process(raw,out,end);
    check(rejected.status==Status::error && !rejected.output_produced && rejected.input_consumed==raw.size());
    check(std::all_of(out.begin(),out.end(),[](auto b){return b==std::byte{0x61};}));
    const auto stable=encoder->process({},out,0);
    check(stable.status==Status::error && stable.error.code==rejected.error.code && !stable.input_consumed && !stable.output_produced);
    check(std::all_of(out.begin(),out.end(),[](auto b){return b==std::byte{0x61};}));
}
}
int main(int argc,char** argv) {
    try {
        if (argc==2) {
            const std::string selected=argv[1];
            if (selected=="4m") {profile=Profile::window_4m;extent=4194304;decisions_per_byte=9;}
            else if (selected=="8m") {profile=Profile::window_8m;extent=8388608;decisions_per_byte=9;}
            else if (selected=="16m") {profile=Profile::window_16m;extent=16777216;decisions_per_byte=9;}
            else if (selected=="32m") {profile=Profile::window_32m;extent=33554432;decisions_per_byte=10;}
            else {check(selected=="64m");profile=Profile::window_64m;extent=67108864;decisions_per_byte=11;}
        }
        else check(argc==1);
        roundtrip({},5,true);roundtrip({std::byte{0}},5,true);roundtrip({std::byte{255}},5,true);
        std::vector<std::byte> values;for (unsigned i=0;i<256;++i) values.push_back(static_cast<std::byte>(i));
        roundtrip(values,17,true);
        for (auto n:{extent-1,extent,extent+1}) roundtrip(std::vector<std::byte>(n,std::byte{'A'}),extent,false);
        std::vector<std::byte> mixed(1025);std::uint32_t state=17;
        for (auto& b:mixed) {state=state*1664525u+1013904223u;b=static_cast<std::byte>(state>>24);}
        roundtrip(mixed,257,true);admission(257);admission(extent);failed_frame();failed_encode();
        std::cout<<"native FGK owned checks passed\n";
    } catch (const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}

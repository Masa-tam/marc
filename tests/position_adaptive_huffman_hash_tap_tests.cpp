#include <marc/marc.h>
#include "core/hash_tap.hpp"
#include "core/sha256.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <source_location>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
using Digest=std::array<std::byte,32>;
using Bytes=std::vector<std::uint8_t>;
void check(bool ok,const std::source_location where=std::source_location::current()) {
    if(!ok) throw std::runtime_error("native HashTap check at line "+std::to_string(where.line()));
}
Digest hash(std::span<const std::uint8_t> bytes) {
    marc::core::Sha256 sha;Digest result{};
    check(sha.update(std::as_bytes(bytes)) && sha.finalize(result));return result;
}
struct Result {Bytes bytes;Digest input_hash{},output_hash{};marc_status status{};};
Result run(std::uint32_t profile,marc_direction direction,const Bytes& input,std::uint32_t frame,std::size_t chunk) {
    marc_lzss_position_distance_adaptive_huffman_config c{};
    check(marc_lzss_position_distance_adaptive_huffman_config_init(direction,&c)==MARC_STATUS_OK);
    check(marc_lzss_position_distance_adaptive_huffman_config_apply_profile(&c,profile)==MARC_STATUS_OK);
    constexpr std::array<unsigned,7> decisions{7,8,9,9,9,10,11};
    static_assert(decisions.size()==MARC_LZSS_POSITION_DISTANCE_ADAPTIVE_HUFFMAN_PROFILE_64M+1);
    c.frame_size=c.max_frame_size=frame;c.max_block_size=static_cast<std::uint64_t>(decisions[profile])*frame;
    c.max_total_output_size=4096;c.max_compressed_payload_size=(UINT64_C(267)*frame+7)/8;
    c.original_size=direction==MARC_DIRECTION_ENCODE ? input.size() : 0;
    c.input_capacity_bytes=c.output_capacity_bytes=64;c.external_retained_bytes=65536;
    c.max_internal_buffered_bytes=UINT64_MAX;
    marc_lzss_position_distance_adaptive_huffman_resources resources{};
    check(marc_lzss_position_distance_adaptive_huffman_resource_requirements(&c,&resources)==MARC_STATUS_OK);
    c.max_internal_buffered_bytes=resources.minimum_aggregate_bytes;
    marc_transform* raw{};check(marc_lzss_position_distance_adaptive_huffman_create(&c,&raw)==MARC_STATUS_OK && raw);
    std::unique_ptr<marc_transform,decltype(&marc_transform_destroy)> codec(raw,&marc_transform_destroy);
    marc::core::Sha256 in_sha,out_sha;marc::core::HashTap in_tap(in_sha),out_tap(out_sha);
    Result result;std::size_t consumed{};
    for(std::size_t call=0;call<8*(input.size()+4097);++call) {
        const auto count=std::min(chunk,input.size()-consumed),capacity=call%3 ? chunk : 0;
        const auto available=std::span(input).subspan(consumed,count);
        std::array<std::uint8_t,66> storage;storage.fill(0xa7);
        const auto out=std::span(storage).subspan(1,capacity);
        const auto flags=(consumed+count==input.size() ? MARC_PROCESS_END_INPUT : 0)|(call%2 ? MARC_PROCESS_FLUSH : 0);
        const auto r=marc_transform_process(codec.get(),{available.data(),available.size()},{out.data(),out.size()},flags);
        check(r.input_consumed<=count && r.output_produced<=capacity);
        check(r.status!=MARC_STATUS_PROGRESS || r.input_consumed || r.output_produced);
        check(storage.front()==0xa7 && std::all_of(storage.begin()+1+r.output_produced,storage.end(),[](auto b){return b==0xa7;}));
        if(r.status>=100)check(!r.output_produced);
        check(in_tap.commit(std::as_bytes(available),r.input_consumed)==marc::core::HashTapStatus::ok);
        check(out_tap.commit(std::as_bytes(out),r.output_produced)==marc::core::HashTapStatus::ok);
        consumed+=r.input_consumed;result.bytes.insert(result.bytes.end(),out.begin(),out.begin()+r.output_produced);
        check(in_tap.total_committed()==consumed && out_tap.total_committed()==result.bytes.size());
        if(r.status==MARC_STATUS_END_OF_STREAM || r.status>=100) {
            const auto again=marc_transform_process(codec.get(),{nullptr,0},{nullptr,0},0);
            check(again.status==r.status && !again.input_consumed && !again.output_produced && again.error_byte_position==r.error_byte_position);
            check(in_tap.finalize(result.input_hash)==marc::core::HashTapStatus::ok);
            check(out_tap.finalize(result.output_hash)==marc::core::HashTapStatus::ok);
            result.status=r.status;return result;
        }
    }
    check(false);return result;
}
std::uint32_t u32(const Bytes& data,std::size_t at) {
    check(at+4<=data.size());std::uint32_t result{};
    for(unsigned i=0;i<4;++i)result|=static_cast<std::uint32_t>(data[at+i])<<(8*i);
    return result;
}
void put32(Bytes& data,std::size_t at,std::uint32_t value) {
    check(at+4<=data.size());for(unsigned i=0;i<4;++i)data[at+i]=static_cast<std::uint8_t>(value>>(8*i));
}
void profile(std::uint32_t selected) {
    Bytes values(1025);for(std::size_t i=0;i<values.size();++i)values[i]=static_cast<std::uint8_t>(i);
    for(const auto& raw:std::array<Bytes,3>{Bytes{},values,Bytes(513,65)}) {
        const auto whole=run(selected,MARC_DIRECTION_ENCODE,raw,257,64);
        check(whole.status==MARC_STATUS_END_OF_STREAM && whole.input_hash==hash(raw) && whole.output_hash==hash(whole.bytes));
        for(const auto chunk:{std::size_t{1},std::size_t{7},std::size_t{64}}) {
            const auto encoded=run(selected,MARC_DIRECTION_ENCODE,raw,257,chunk);
            check(encoded.status==MARC_STATUS_END_OF_STREAM && encoded.bytes==whole.bytes && encoded.input_hash==whole.input_hash && encoded.output_hash==whole.output_hash);
            const auto decoded=run(selected,MARC_DIRECTION_DECODE,encoded.bytes,257,chunk);
            check(decoded.status==MARC_STATUS_END_OF_STREAM && decoded.bytes==raw && decoded.input_hash==encoded.output_hash && decoded.output_hash==encoded.input_hash);
        }
    }
    const Bytes raw{'A','B','A','B','A','A','B','A','B','A'};
    auto broken=run(selected,MARC_DIRECTION_ENCODE,raw,5,64).bytes;
    check(u32(broken,112+32)==5);
    const auto second=112+80+u32(broken,112+32);
    check(u32(broken,second+32)==5 && broken[second+64+10]==1);
    put32(broken,second+32,4);put32(broken,second+64+4,4);broken[second+64+10]=8;broken.pop_back();
    const Bytes prefix(raw.begin(),raw.begin()+5);
    for(const auto chunk:{std::size_t{1},std::size_t{7},std::size_t{64}}) {
        const auto rejected=run(selected,MARC_DIRECTION_DECODE,broken,257,chunk);
        check(rejected.status==MARC_STATUS_MALFORMED_STREAM && rejected.bytes==prefix && rejected.output_hash==hash(prefix));
    }
}
}
int main() {try {
    for(std::uint32_t selected=0;selected<=MARC_LZSS_POSITION_DISTANCE_ADAPTIVE_HUFFMAN_PROFILE_64M;++selected)profile(selected);
    std::cout<<"Verified seven native Adaptive Huffman HashTap profiles and failed-frame withholding\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}

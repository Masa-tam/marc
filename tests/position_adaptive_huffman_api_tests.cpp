#include <marc/marc.h>
#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <iostream>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>

namespace {thread_local bool tracked{};thread_local std::size_t allocations{},fail_at{},live{};}
void* operator new(std::size_t n) {
    if (tracked && ++allocations==fail_at) throw std::bad_alloc();
    if (auto p=std::malloc(n ? n : 1)) {++live;return p;}throw std::bad_alloc();
}
void* operator new[](std::size_t n) {return ::operator new(n);}
void operator delete(void* p) noexcept {if (p) {--live;std::free(p);}}
void operator delete[](void* p) noexcept {::operator delete(p);}
void operator delete(void* p,std::size_t) noexcept {::operator delete(p);}
void operator delete[](void* p,std::size_t) noexcept {::operator delete(p);}
void* operator new(std::size_t n,const std::nothrow_t&) noexcept {try {return ::operator new(n);}catch(...) {return nullptr;}}
void* operator new[](std::size_t n,const std::nothrow_t&) noexcept {return ::operator new(n,std::nothrow);}
void operator delete(void* p,const std::nothrow_t&) noexcept {::operator delete(p);}
void operator delete[](void* p,const std::nothrow_t&) noexcept {::operator delete(p);}
namespace {
using Config=marc_lzss_position_distance_adaptive_huffman_config;
using Resources=marc_lzss_position_distance_adaptive_huffman_resources;
marc_lzss_position_distance_adaptive_huffman_profile selected_profile=MARC_LZSS_POSITION_DISTANCE_ADAPTIVE_HUFFMAN_PROFILE_64K;
std::uint32_t window=65536;
std::uint32_t decisions_per_byte=7,model_entries=13758;
void check(bool b,const std::source_location p=std::source_location::current()) {
    if (!b) throw std::runtime_error("native FGK public check failed at line "+std::to_string(p.line()));
}
Config config(marc_direction direction,std::uint32_t frame=257) {
    Config c{};check(marc_lzss_position_distance_adaptive_huffman_config_init(direction,&c)==MARC_STATUS_OK);
    check(marc_lzss_position_distance_adaptive_huffman_config_apply_profile(&c,selected_profile)==MARC_STATUS_OK);
    c.frame_size=frame;c.max_frame_size=frame;c.max_block_size=static_cast<std::uint64_t>(decisions_per_byte)*frame;
    c.input_capacity_bytes=c.output_capacity_bytes=7;
    c.external_retained_bytes=UINT64_C(4)*window+(UINT64_C(2)<<20);
    // The test retains additional source/result vectors outside the transform.
    if (selected_profile) c.max_internal_buffered_bytes+=c.external_retained_bytes;
    return c;
}
struct Result {std::vector<std::uint8_t> bytes;marc_status status{};std::size_t consumed{};};
Result run(marc_transform* p,const std::vector<std::uint8_t>& input,std::size_t chunk=7) {
    Result result;
    for (std::size_t calls=0;calls<8*(input.size()+window+1);++calls) {
        const auto size=std::min(chunk,input.size()-result.consumed),capacity=calls%3 ? chunk : 0;
        std::array<std::uint8_t,9> out;out.fill(0x63);allocations=0;fail_at=0;tracked=true;
        const auto r=marc_transform_process(p,{input.empty() ? nullptr : input.data()+result.consumed,size},{out.data()+1,capacity},
            (result.consumed+size==input.size() ? MARC_PROCESS_END_INPUT : 0)|(calls%2 ? MARC_PROCESS_FLUSH : 0));
        tracked=false;check(!allocations && r.input_consumed<=size && r.output_produced<=capacity);
        check(r.status!=MARC_STATUS_PROGRESS || r.input_consumed || r.output_produced);
        check(out.front()==0x63 && std::all_of(out.begin()+1+r.output_produced,out.end(),[](auto b){return b==0x63;}));
        if (r.status>=100) check(!r.output_produced);
        result.consumed+=r.input_consumed;result.bytes.insert(result.bytes.end(),out.begin()+1,out.begin()+1+r.output_produced);
        if (r.status==MARC_STATUS_END_OF_STREAM || r.status>=100) {
            const auto again=marc_transform_process(p,{nullptr,0},{nullptr,0},0);
            check(again.status==r.status && !again.input_consumed && !again.output_produced && again.error_byte_position==r.error_byte_position);
            result.status=r.status;return result;
        }
    }
    check(false);return result;
}
Result transform(const std::vector<std::uint8_t>& input,marc_direction direction,std::uint32_t frame,std::size_t chunk=7) {
    auto c=config(direction,frame);c.original_size=direction==MARC_DIRECTION_ENCODE ? input.size() : 0;
    marc_transform* p{};check(marc_lzss_position_distance_adaptive_huffman_create(&c,&p)==MARC_STATUS_OK && p);
    auto r=run(p,input,chunk);marc_transform_destroy(p);return r;
}
void resources() {
    for (const auto direction:{MARC_DIRECTION_ENCODE,MARC_DIRECTION_DECODE}) for (const auto frame:{257u,window}) {
        auto c=config(direction,frame);Resources r{};check(marc_lzss_position_distance_adaptive_huffman_resource_requirements(&c,&r)==MARC_STATUS_OK);
        check(r.owned_buffer_bytes==r.raw_bytes+r.token_bytes+r.serialized_bytes+r.finder_bytes+r.private_payload_bytes
            +r.private_serialized_bytes+r.node_bytes+r.symbol_bytes);
        check(r.private_admission_bytes==r.owned_buffer_bytes+r.owner_bytes+r.fixed_working_bytes);
        check(r.minimum_aggregate_bytes==std::max(r.private_admission_bytes,c.max_block_size)+r.external_charge_bytes);
        c.max_internal_buffered_bytes=r.minimum_aggregate_bytes-1;const auto saved=r;
        check(marc_lzss_position_distance_adaptive_huffman_resource_requirements(&c,&r)==MARC_STATUS_LIMIT_EXCEEDED && !std::memcmp(&saved,&r,sizeof(r)));
        marc_transform* p=reinterpret_cast<marc_transform*>(std::uintptr_t{1});allocations=0;tracked=true;
        const auto refused=marc_lzss_position_distance_adaptive_huffman_create(&c,&p);tracked=false;
        check(refused==MARC_STATUS_LIMIT_EXCEEDED && !p && !allocations);
        c.max_internal_buffered_bytes=r.minimum_aggregate_bytes;allocations=0;tracked=true;
        const auto ok=marc_lzss_position_distance_adaptive_huffman_create(&c,&p);tracked=false;
        check(ok==MARC_STATUS_OK && p);const auto allocation_count=allocations;marc_transform_destroy(p);
        for (std::size_t n=1;n<=allocation_count;++n) {
            const auto before=live;allocations=0;fail_at=n;tracked=true;
            const auto failed=marc_lzss_position_distance_adaptive_huffman_create(&c,&p);tracked=false;fail_at=0;
            check(failed==MARC_STATUS_OUT_OF_MEMORY && !p && live==before && allocations==n);
        }
        std::vector<std::uint8_t> raw(frame,65);
        const auto source=direction==MARC_DIRECTION_ENCODE ? raw : transform(raw,MARC_DIRECTION_ENCODE,frame).bytes;
        c.original_size=frame;
        check(marc_lzss_position_distance_adaptive_huffman_create(&c,&p)==MARC_STATUS_OK && p);
        const auto completed=run(p,source);marc_transform_destroy(p);
        check(completed.status==MARC_STATUS_END_OF_STREAM && (direction==MARC_DIRECTION_ENCODE || completed.bytes==raw));
        const auto before=c;
        check(marc_lzss_position_distance_adaptive_huffman_create(&c,reinterpret_cast<marc_transform**>(&c.original_size))==MARC_STATUS_INVALID_ARGUMENT
            && !std::memcmp(&before,&c,sizeof(c)));
        check(marc_lzss_position_distance_adaptive_huffman_resource_requirements(&c,reinterpret_cast<Resources*>(&c))==MARC_STATUS_INVALID_ARGUMENT
            && !std::memcmp(&before,&c,sizeof(c)));
        c.external_retained_bytes=UINT64_MAX;check(marc_lzss_position_distance_adaptive_huffman_resource_requirements(&c,&r)==MARC_STATUS_LIMIT_EXCEEDED);
    }
}
void invalid_configurations() {
    const auto initial=config(MARC_DIRECTION_ENCODE);auto init=initial;
    check(marc_lzss_position_distance_adaptive_huffman_config_init(99,&init)==MARC_STATUS_INVALID_ARGUMENT
        && !std::memcmp(&initial,&init,sizeof(init)));
    Resources r{};check(marc_lzss_position_distance_adaptive_huffman_resource_requirements(&initial,&r)==MARC_STATUS_OK);
    for (unsigned n=0;n<12;++n) {
        auto c=initial;marc_status expected=MARC_STATUS_INVALID_ARGUMENT;
        switch(n) {
        case 0:c.struct_size=0;break;
        case 1:c.abi_version=99;break;
        case 2:c.direction=99;break;
        case 3:c.profile=99;break;
        case 4:c.reserved2=1;break;
        case 5:c.frame_size=0;break;
        case 6:c.frame_size=window+1;break;
        case 7:c.max_lz_distance=window-1;expected=MARC_STATUS_LIMIT_EXCEEDED;break;
        case 8:c.max_entropy_table_entries=model_entries-1;expected=MARC_STATUS_LIMIT_EXCEEDED;break;
        case 9:c.max_compressed_payload_size=0;break;
        case 10:c.original_size=UINT64_MAX;break;
        default:c.original_size=c.max_total_output_size+1;expected=MARC_STATUS_LIMIT_EXCEEDED;break;
        }
        const auto before=r;
        check(marc_lzss_position_distance_adaptive_huffman_resource_requirements(&c,&r)==expected && !std::memcmp(&before,&r,sizeof(r)));
        marc_transform* p=reinterpret_cast<marc_transform*>(std::uintptr_t{1});allocations=0;tracked=true;
        const auto refused=marc_lzss_position_distance_adaptive_huffman_create(&c,&p);tracked=false;
        check(refused==expected && !p && !allocations);
    }
    auto c=initial;c.profile=MARC_LZSS_POSITION_DISTANCE_ADAPTIVE_HUFFMAN_PROFILE_64M+1;const auto saved=c;
    check(marc_lzss_position_distance_adaptive_huffman_config_apply_profile(&c,c.profile)==MARC_STATUS_INVALID_ARGUMENT
        && !std::memcmp(&saved,&c,sizeof(c)));
    check(marc_lzss_position_distance_adaptive_huffman_resource_requirements(&c,&r)==MARC_STATUS_INVALID_ARGUMENT);
    check(marc_lzss_position_distance_adaptive_huffman_config_apply_profile(&c,99)==MARC_STATUS_INVALID_ARGUMENT
        && !std::memcmp(&saved,&c,sizeof(c)));
    check(marc_lzss_position_distance_adaptive_huffman_config_apply_profile(&c,MARC_LZSS_POSITION_DISTANCE_ADAPTIVE_HUFFMAN_PROFILE_64K)==MARC_STATUS_OK
        && c.profile==MARC_LZSS_POSITION_DISTANCE_ADAPTIVE_HUFFMAN_PROFILE_64K && c.frame_size==65536);
}
void directed() {
    resources();invalid_configurations();
    for (const auto size:{0u,1u,256u,257u,258u,1029u,window-1,window,window+1}) {
        std::vector<std::uint8_t> raw(size);for (std::size_t i=0;i<size;++i) raw[i]=static_cast<std::uint8_t>(i%13);
        const auto frame=size>1029 ? window : 257u;
        const auto a=transform(raw,MARC_DIRECTION_ENCODE,frame);check(a.status==MARC_STATUS_END_OF_STREAM);
        check(a.bytes[14]==8+selected_profile && a.bytes[16]==1 && a.bytes[18]==3+selected_profile && a.bytes[98]==35+selected_profile);
        check(transform(raw,MARC_DIRECTION_ENCODE,frame,1).bytes==a.bytes);
        const auto b=transform(a.bytes,MARC_DIRECTION_DECODE,frame,1);check(b.status==MARC_STATUS_END_OF_STREAM && b.bytes==raw);
    }
    auto one=transform({65},MARC_DIRECTION_ENCODE,1).bytes;
    for (std::size_t cut=0;cut<one.size();++cut) {
        const auto r=transform({one.begin(),one.begin()+cut},MARC_DIRECTION_DECODE,1,1);
        check(r.status==MARC_STATUS_MALFORMED_STREAM && r.bytes.empty());
    }
    auto two=transform({65,66},MARC_DIRECTION_ENCODE,1).bytes;two.back()|=0x80;
    const auto late=transform(two,MARC_DIRECTION_DECODE,1,7);check(late.status==MARC_STATUS_MALFORMED_STREAM && late.bytes==std::vector<std::uint8_t>{65});
    one[98]=1;const auto foreign=transform(one,MARC_DIRECTION_DECODE,1);check(foreign.status==MARC_STATUS_UNSUPPORTED && foreign.bytes.empty());
    auto c=config(MARC_DIRECTION_ENCODE,1);c.original_size=1;marc_transform* p{};
    check(marc_lzss_position_distance_adaptive_huffman_create(&c,&p)==MARC_STATUS_OK);
    std::array<std::uint8_t,8> out{};const auto excessive=marc_transform_process(p,{nullptr,0},{out.data(),8},0);
    check(excessive.status==MARC_STATUS_LIMIT_EXCEEDED && !excessive.input_consumed && !excessive.output_produced);marc_transform_destroy(p);
}
}
int main(int argc,char** argv) {try {
    if (argc==2) {
        if (std::string(argv[1])=="1m") {selected_profile=MARC_LZSS_POSITION_DISTANCE_ADAPTIVE_HUFFMAN_PROFILE_1M;window=1048576;decisions_per_byte=8;model_entries=13894;}
        else if (std::string(argv[1])=="4m") {selected_profile=MARC_LZSS_POSITION_DISTANCE_ADAPTIVE_HUFFMAN_PROFILE_4M;window=4194304;decisions_per_byte=9;model_entries=13962;}
        else if (std::string(argv[1])=="8m") {selected_profile=MARC_LZSS_POSITION_DISTANCE_ADAPTIVE_HUFFMAN_PROFILE_8M;window=8388608;decisions_per_byte=9;model_entries=13996;}
        else if (std::string(argv[1])=="16m") {selected_profile=MARC_LZSS_POSITION_DISTANCE_ADAPTIVE_HUFFMAN_PROFILE_16M;window=16777216;decisions_per_byte=9;model_entries=14030;}
        else if (std::string(argv[1])=="32m") {selected_profile=MARC_LZSS_POSITION_DISTANCE_ADAPTIVE_HUFFMAN_PROFILE_32M;window=33554432;decisions_per_byte=10;model_entries=14064;}
        else {check(std::string(argv[1])=="64m");selected_profile=MARC_LZSS_POSITION_DISTANCE_ADAPTIVE_HUFFMAN_PROFILE_64M;window=67108864;decisions_per_byte=11;model_entries=14098;}
    }
    else check(argc==1);
    directed();std::cout<<"native FGK public checks passed\n";return 0;}
    catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}

#include <marc/marc.h>
#include "core/endian.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <fstream>
#include <string>
#include <new>
#include <vector>

namespace { thread_local bool tracked{}; thread_local std::size_t allocations{},fail_at{},live{}; }
void* operator new(std::size_t n) {
    if (tracked && ++allocations==fail_at) throw std::bad_alloc();
    if (auto p=std::malloc(n ? n : 1)) { ++live; return p; } throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { if (p) { --live; std::free(p); } }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete(void* p,std::size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p,std::size_t) noexcept { ::operator delete(p); }
void* operator new(std::size_t n,const std::nothrow_t&) noexcept { try { return ::operator new(n); } catch (...) { return nullptr; } }
void* operator new[](std::size_t n,const std::nothrow_t&) noexcept { return ::operator new(n,std::nothrow); }
void operator delete(void* p,const std::nothrow_t&) noexcept { ::operator delete(p); }
void operator delete[](void* p,const std::nothrow_t&) noexcept { ::operator delete(p); }
namespace {
using Config=marc_lzss_position_distance_blocked_huffman_16m_config;
using Resources=marc_lzss_position_distance_blocked_huffman_16m_resources;
static_assert(sizeof(Config)==sizeof(marc_lzss_position_distance_blocked_huffman_config));
static_assert(sizeof(Resources)==sizeof(marc_lzss_position_distance_blocked_huffman_resources));
int failures{};
void check(bool good,const char* message) { if (!good) { ++failures; std::cerr<<message<<'\n'; } }
Config config(marc_direction direction,std::uint32_t frame=257) {
    Config c{}; check(marc_lzss_position_distance_blocked_huffman_16m_config_init(direction,&c)==MARC_STATUS_OK,"config init");
    c.frame_size=frame; c.max_frame_size=frame; c.max_block_size=9*frame;
    c.input_capacity_bytes=c.output_capacity_bytes=7; return c;
}
struct Result { std::vector<std::uint8_t> bytes; marc_process_result result{}; std::size_t consumed{}; };
Result run(marc_transform* p,const std::vector<std::uint8_t>& input,std::size_t chunk=7,
           std::size_t retained_base=0,std::size_t retained_limit=SIZE_MAX) {
    Result r;
    for (std::size_t call=0;call<input.size()*3+33554432;++call) {
        const auto count=std::min(chunk,input.size()-r.consumed);
        std::array<std::uint8_t,9> output{}; output.fill(0xa5);
        const auto capacity=call%3 ? chunk : 0;
        const auto flags=(r.consumed+count==input.size() ? MARC_PROCESS_END_INPUT : 0) | (call%2 ? MARC_PROCESS_FLUSH : 0);
        allocations=0; fail_at=0; tracked=true;
        r.result=marc_transform_process(p,{input.empty() ? nullptr : input.data()+r.consumed,count},{output.data()+1,capacity},flags);
        tracked=false;
        check(!allocations,"process allocated");
        check(r.result.input_consumed<=count && r.result.output_produced<=capacity,"process counts");
        check(r.result.status!=MARC_STATUS_PROGRESS || r.result.input_consumed || r.result.output_produced,"progress without counts");
        check(output.front()==0xa5 && std::all_of(output.begin()+1+r.result.output_produced,output.end(),[](auto b) { return b==0xa5; }),"output guards");
        r.consumed+=r.result.input_consumed;
        const auto previous_capacity=r.bytes.capacity();
        r.bytes.insert(r.bytes.end(),output.begin()+1,output.begin()+1+r.result.output_produced);
        const auto transient_old=r.bytes.capacity()!=previous_capacity ? previous_capacity : 0;
        check(retained_base<=retained_limit && r.bytes.capacity()<=retained_limit-retained_base
            && transient_old<=retained_limit-retained_base-r.bytes.capacity(),"declared caller retention bounds actual vector capacities");
        if (r.result.status==MARC_STATUS_END_OF_STREAM || r.result.status>=100) {
            const auto again=marc_transform_process(p,{nullptr,0},{nullptr,0},0);
            check(again.status==r.result.status && !again.input_consumed && !again.output_produced
                && again.error_byte_position==r.result.error_byte_position,"sticky terminal");
            return r;
        }
    }
    check(false,"nontermination"); return r;
}
Result encode(const std::vector<std::uint8_t>& input,std::uint32_t frame=257,std::size_t chunk=7) {
    auto c=config(MARC_DIRECTION_ENCODE,frame); c.original_size=input.size();
    marc_transform* p{}; check(marc_lzss_position_distance_blocked_huffman_16m_create(&c,&p)==MARC_STATUS_OK && p,"encoder create");
    if (!p) return {};
    const auto r=run(p,input,chunk); marc_transform_destroy(p); return r;
}
void budget_tests() {
    for (auto direction : {MARC_DIRECTION_ENCODE,MARC_DIRECTION_DECODE}) {
        auto c=config(direction); c.original_size=1028;
        Resources r{}; check(marc_lzss_position_distance_blocked_huffman_16m_resource_requirements(&c,&r)==MARC_STATUS_OK,"query");
        auto bad=c; bad.max_internal_buffered_bytes=r.minimum_aggregate_bytes-1;
        const auto before=r;
        check(marc_lzss_position_distance_blocked_huffman_16m_resource_requirements(&bad,&r)==MARC_STATUS_LIMIT_EXCEEDED
            && std::memcmp(&before,&r,sizeof(r))==0,"query failure retains output");
        marc_transform* p=reinterpret_cast<marc_transform*>(std::uintptr_t{1});
        allocations=0; fail_at=0; tracked=true;
        const auto status=marc_lzss_position_distance_blocked_huffman_16m_create(&bad,&p);
        tracked=false;
        check(status==MARC_STATUS_LIMIT_EXCEEDED && !p && !allocations,"budget before allocation");
        c.max_internal_buffered_bytes=r.minimum_aggregate_bytes;
        for (std::size_t failure=1;failure<=(direction==MARC_DIRECTION_ENCODE ? 10u : 7u);++failure) {
            const auto count=live;
            allocations=0; fail_at=failure; tracked=true;
            const auto failed=marc_lzss_position_distance_blocked_huffman_16m_create(&c,&p);
            tracked=false; fail_at=0;
            check(failed==MARC_STATUS_OUT_OF_MEMORY && !p && live==count && allocations==failure,"factory fault cleanup");
        }
        check(marc_lzss_position_distance_blocked_huffman_16m_create(&c,&p)==MARC_STATUS_OK && p,"exact budget create");
        marc_transform_destroy(p);
        const auto saved=c;
        check(marc_lzss_position_distance_blocked_huffman_16m_create(&c,reinterpret_cast<marc_transform**>(&c.original_size))==MARC_STATUS_INVALID_ARGUMENT
            && std::memcmp(&c,&saved,sizeof(c))==0,"aliased handle output");
        check(marc_lzss_position_distance_blocked_huffman_16m_resource_requirements(&c,reinterpret_cast<Resources*>(&c))==MARC_STATUS_INVALID_ARGUMENT
            && std::memcmp(&c,&saved,sizeof(c))==0,"aliased resources");
        c.external_retained_bytes=UINT64_MAX;
        check(marc_lzss_position_distance_blocked_huffman_16m_resource_requirements(&c,&r)==MARC_STATUS_LIMIT_EXCEEDED,"external overflow");
    }
}
void refusal_tests() {
    auto initial=config(MARC_DIRECTION_ENCODE);
    const auto saved=initial;
    check(marc_lzss_position_distance_blocked_huffman_16m_config_init(99,&initial)==MARC_STATUS_INVALID_ARGUMENT
        && std::memcmp(&initial,&saved,sizeof(initial))==0,"invalid init preserves config");
    Resources resources{};
    check(marc_lzss_position_distance_blocked_huffman_16m_resource_requirements(&initial,&resources)==MARC_STATUS_OK,"refusal baseline query");
    for (unsigned index=0;index<8;++index) {
        auto invalid=initial;
        switch(index) {
        case 0:invalid.struct_size=0;break;
        case 1:invalid.abi_version=99;break;
        case 2:invalid.reserved=1;break;
        case 3:invalid.reserved2=1;break;
        case 4:invalid.direction=99;break;
        case 5:invalid.frame_size=0;break;
        case 6:invalid.frame_size=16777217;break;
        default:invalid.max_lz_distance=16777215;break;
        }
        const auto before=resources;
        check(marc_lzss_position_distance_blocked_huffman_16m_resource_requirements(&invalid,&resources)
            ==(index==7 ? MARC_STATUS_LIMIT_EXCEEDED : MARC_STATUS_INVALID_ARGUMENT)
            && std::memcmp(&before,&resources,sizeof(resources))==0,"invalid config query preserves result");
        marc_transform* instance=reinterpret_cast<marc_transform*>(std::uintptr_t{1});
        allocations=0;fail_at=0;tracked=true;
        const auto status=marc_lzss_position_distance_blocked_huffman_16m_create(&invalid,&instance);
        tracked=false;
        check(status==(index==7 ? MARC_STATUS_LIMIT_EXCEEDED : MARC_STATUS_INVALID_ARGUMENT)
            && !instance && !allocations,"invalid config refuses before allocations");
    }
}
}
int fixture(const char* path,bool raw_encode) {
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    if (!file) return 2;
    const auto extent=file.tellg();
    if (extent<120 || extent>static_cast<std::streamoff>(UINT64_C(120)+283118159+16777216)) return 3;
    file.seekg(0);std::vector<std::uint8_t> bytes(static_cast<std::size_t>(extent));
    if (!file.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()))) return 4;
    std::uint32_t fs{},rs{},frame{};
    const auto view=std::as_bytes(std::span(bytes));
    if (!marc::core::load_le(view,0,fs) || !marc::core::load_le(view,4,rs)
        || !marc::core::load_le(view,28,frame)) return 5;
    if (rs>16777216 || fs>283118159 || UINT64_C(120)+fs+rs!=bytes.size()
        || !frame || frame>16777216 || ((rs==0)!=(fs==0))) return 5;
    std::vector<std::uint8_t> archive(bytes.begin()+8,bytes.begin()+120+fs);
    std::vector<std::uint8_t> raw(bytes.begin()+120+fs,bytes.end());
    auto c=config(MARC_DIRECTION_DECODE,frame);
    // Bound original fixture, archive/raw copies, result-vector growth and
    // controls, including transient old/new vector allocations between calls.
    // Every actual vector capacity and transient reallocation is checked below.
    c.external_retained_bytes=UINT64_C(128)<<20;c.max_internal_buffered_bytes=(UINT64_C(1280)<<20)+c.external_retained_bytes;
    Resources resources{};
    if (marc_lzss_position_distance_blocked_huffman_16m_resource_requirements(&c,&resources)!=MARC_STATUS_OK) return 6;
    c.max_internal_buffered_bytes=resources.minimum_aggregate_bytes;
    marc_transform* decoder{};
    if (marc_lzss_position_distance_blocked_huffman_16m_create(&c,&decoder)!=MARC_STATUS_OK || !decoder) return 7;
    const auto retained=bytes.capacity()+archive.capacity()+raw.capacity()+16777216;
    const auto decoded=run(decoder,archive,1,retained,static_cast<std::size_t>(c.external_retained_bytes));
    marc_transform_destroy(decoder);
    if (decoded.result.status!=MARC_STATUS_END_OF_STREAM || decoded.bytes!=raw || failures) return 8;
    if (raw_encode) {
        auto e=config(MARC_DIRECTION_ENCODE,frame);e.original_size=raw.size();
        e.external_retained_bytes=UINT64_C(128)<<20;e.max_internal_buffered_bytes=(UINT64_C(1280)<<20)+e.external_retained_bytes;
        if (marc_lzss_position_distance_blocked_huffman_16m_resource_requirements(&e,&resources)!=MARC_STATUS_OK) return 9;
        e.max_internal_buffered_bytes=resources.minimum_aggregate_bytes;
        marc_transform* encoder{};
        if (marc_lzss_position_distance_blocked_huffman_16m_create(&e,&encoder)!=MARC_STATUS_OK || !encoder) return 10;
        const auto encoded=run(encoder,raw,1,retained+decoded.bytes.capacity(),static_cast<std::size_t>(e.external_retained_bytes));
        marc_transform_destroy(encoder);
        if (encoded.result.status!=MARC_STATUS_END_OF_STREAM || failures) return 11;
        std::ofstream wire(std::string(path)+".encoded",std::ios::binary);
        wire.write(reinterpret_cast<const char*>(encoded.bytes.data()),static_cast<std::streamsize>(encoded.bytes.size()));
        if (!wire) return 12;
    }
    return 0;
}
int main(int argc,char** argv) {
    if (argc==3 && std::string(argv[1])=="--public-fixture") return fixture(argv[2],false);
    if (argc==3 && std::string(argv[1])=="--public-raw-fixture") return fixture(argv[2],true);
    budget_tests();
    refusal_tests();
    for (auto direction : {MARC_DIRECTION_ENCODE,MARC_DIRECTION_DECODE}) {
        Config c{};Resources r{};
        check(marc_lzss_position_distance_blocked_huffman_16m_config_init(direction,&c)==MARC_STATUS_OK,"public default init");
        check(c.max_internal_buffered_bytes==(UINT64_C(1280)<<20)
            && c.max_block_size==150994944 && c.max_compressed_payload_size==283115520
            && c.max_entropy_table_entries==31171 && c.frame_size==16777216
            && c.input_capacity_bytes==65536 && c.output_capacity_bytes==65536,"Huffman checked logical default bounds");
        check(marc_lzss_position_distance_blocked_huffman_16m_resource_requirements(&c,&r)==MARC_STATUS_OK,"public default query");
        check(r.owned_buffer_bytes==r.raw_bytes+r.token_bytes+r.serialized_bytes+r.finder_bytes
            +r.table_bytes+r.private_payload_bytes+r.private_serialized_bytes,"owned components");
        check(r.private_admission_bytes==std::max(r.metadata_floor_bytes,
            r.owned_buffer_bytes+r.owner_bytes+r.fixed_working_bytes),"private admission distinct from metadata floor");
        check(r.minimum_aggregate_bytes==std::max(r.private_admission_bytes,c.max_block_size)
            +r.external_charge_bytes,"public grant uses max block floor rather than a second buffer");
        c.max_internal_buffered_bytes=UINT64_C(512)<<20;
        check(marc_lzss_position_distance_blocked_huffman_16m_resource_requirements(&c,&r)==(direction==MARC_DIRECTION_ENCODE ? MARC_STATUS_LIMIT_EXCEEDED : MARC_STATUS_OK),"512MiB admission differs by checked direction requirements");
        c.external_retained_bytes=16777216;
        const auto admitted=marc_lzss_position_distance_blocked_huffman_16m_resource_requirements(&c,&r);
        check(admitted==(direction==MARC_DIRECTION_ENCODE ? MARC_STATUS_LIMIT_EXCEEDED : MARC_STATUS_OK),
            "512MiB external-retained qualification is direction dependent");
    }
    for (auto direction : {MARC_DIRECTION_ENCODE,MARC_DIRECTION_DECODE}) {
        Config small{};
        check(marc_lzss_position_distance_blocked_huffman_16m_config_init(direction,&small)==MARC_STATUS_OK,"small default init");
        small.frame_size=1;
        Resources queried{};
        check(marc_lzss_position_distance_blocked_huffman_16m_resource_requirements(&small,&queried)==MARC_STATUS_OK,"small default query");
        check(queried.minimum_aggregate_bytes>=small.max_block_size+queried.external_charge_bytes,"decision-ceiling budget floor");
        small.max_internal_buffered_bytes=queried.minimum_aggregate_bytes;
        marc_transform* instance{};
        check(marc_lzss_position_distance_blocked_huffman_16m_create(&small,&instance)==MARC_STATUS_OK && instance,"small default exact query budget");
        marc_transform_destroy(instance);
        --small.max_internal_buffered_bytes;
        const auto retained=queried;
        check(marc_lzss_position_distance_blocked_huffman_16m_resource_requirements(&small,&queried)==MARC_STATUS_LIMIT_EXCEEDED
            && std::memcmp(&queried,&retained,sizeof(queried))==0,"decision floor query refusal retains result");
        allocations=0; fail_at=0; tracked=true;
        const auto refused=marc_lzss_position_distance_blocked_huffman_16m_create(&small,&instance);
        tracked=false;
        check(refused==MARC_STATUS_LIMIT_EXCEEDED && !instance && !allocations,"decision floor refuses before allocation");
    }
    for (auto direction : {MARC_DIRECTION_ENCODE,MARC_DIRECTION_DECODE}) {
        auto flags_config=config(direction,1); flags_config.original_size=1;
        marc_transform* flags_instance{};
        check(marc_lzss_position_distance_blocked_huffman_16m_create(&flags_config,&flags_instance)==MARC_STATUS_OK,"flags create");
        std::array<std::uint8_t,7> guarded{}; guarded.fill(0xa5);
        const auto refused=marc_transform_process(flags_instance,{nullptr,0},{guarded.data(),guarded.size()},MARC_PROCESS_RESET_BLOCK);
        check(refused.status==MARC_STATUS_UNSUPPORTED && !refused.input_consumed && !refused.output_produced
            && std::all_of(guarded.begin(),guarded.end(),[](auto value){ return value==0xa5; }),"public reset block refused unchanged");
        const auto sticky=marc_transform_process(flags_instance,{nullptr,0},{nullptr,0},MARC_PROCESS_NONE);
        check(sticky.status==refused.status && !sticky.input_consumed && !sticky.output_produced,"public flags sticky");
        marc_transform_destroy(flags_instance);
    }
    for (const std::size_t size : {0u,1u,256u,257u,258u,1029u,16777215u,16777216u,16777217u}) {
        std::vector<std::uint8_t> raw(size);
        for (std::size_t i=0;i<size;++i) raw[i]=static_cast<std::uint8_t>(i%13);
        const auto frame=size>1029 ? 16777216u : 257u;
        const auto wire=encode(raw,frame);
        check(wire.result.status==MARC_STATUS_END_OF_STREAM,"encoded finish");
        check(encode(raw,frame,1).bytes==wire.bytes,"public chunk determinism");
        auto c=config(MARC_DIRECTION_DECODE,frame); marc_transform* p{};
        check(marc_lzss_position_distance_blocked_huffman_16m_create(&c,&p)==MARC_STATUS_OK,"decoder create");
        const auto decoded=run(p,wire.bytes,1); marc_transform_destroy(p);
        check(decoded.result.status==MARC_STATUS_END_OF_STREAM && decoded.bytes==raw,"public roundtrip");
    }
    auto c=config(MARC_DIRECTION_ENCODE,1); c.original_size=1;
    marc_transform* p{}; check(marc_lzss_position_distance_blocked_huffman_16m_create(&c,&p)==MARC_STATUS_OK,"capacity create");
    std::array<std::uint8_t,8> bytes{};
    const auto excessive=marc_transform_process(p,{bytes.data(),bytes.size()},{nullptr,0},0);
    check(excessive.status==MARC_STATUS_LIMIT_EXCEEDED && !excessive.input_consumed && !excessive.output_produced,"call capacity refusal");
    marc_transform_destroy(p);
    auto wire=encode({65,66},1).bytes;
    for (std::size_t cut=0;cut<wire.size();++cut) {
        auto cuts=config(MARC_DIRECTION_DECODE,1);marc_transform* decoder{};
        check(marc_lzss_position_distance_blocked_huffman_16m_create(&cuts,&decoder)==MARC_STATUS_OK,"cut create");
        const auto result=run(decoder,{wire.begin(),wire.begin()+cut},1);
        marc_transform_destroy(decoder);
        check(result.result.status==MARC_STATUS_MALFORMED_STREAM
            && result.bytes==(cut>=211 ? std::vector<std::uint8_t>{65} : std::vector<std::uint8_t>{}),
            "cut publishes only complete prior frame");
    }
    wire[wire.size()-3]=1;
    c=config(MARC_DIRECTION_DECODE,1);
    check(marc_lzss_position_distance_blocked_huffman_16m_create(&c,&p)==MARC_STATUS_OK,"later failure create");
    const auto failed=run(p,wire,1); marc_transform_destroy(p);
    check(failed.result.status==MARC_STATUS_MALFORMED_STREAM && failed.bytes==std::vector<std::uint8_t>{65},"failed public frame withheld");
    // The active-codebook quota is checked at the complete descriptor prefix,
    // rather than rejecting a smaller cap at creation or reading its payload.
    auto limited=config(MARC_DIRECTION_DECODE,1);limited.max_entropy_table_entries=511;
    check(marc_lzss_position_distance_blocked_huffman_16m_create(&limited,&p)==MARC_STATUS_OK && p,"partial model quota create");
    const auto quota_failed=run(p,encode({65},1).bytes,1);marc_transform_destroy(p);
    check(quota_failed.result.status==MARC_STATUS_LIMIT_EXCEEDED && quota_failed.bytes.empty()
        && quota_failed.consumed==211,"active model quota before payload/publication");
    auto wrong_identity=encode({65},1).bytes;wrong_identity[98]=21;
    limited=config(MARC_DIRECTION_DECODE,1);
    check(marc_lzss_position_distance_blocked_huffman_16m_create(&limited,&p)==MARC_STATUS_OK,"identity refusal create");
    const auto foreign=run(p,wrong_identity,1);marc_transform_destroy(p);
    check(foreign.result.status==MARC_STATUS_UNSUPPORTED && foreign.bytes.empty() && foreign.consumed==112,"foreign native context refused");
    for (auto direction : {MARC_DIRECTION_ENCODE,MARC_DIRECTION_DECODE}) {
        Config queried{}; Resources resources{};
        check(marc_lzss_position_distance_blocked_huffman_16m_config_init(direction,&queried)==MARC_STATUS_OK,"default resource init");
        queried.external_retained_bytes=65536;
        check(marc_lzss_position_distance_blocked_huffman_16m_resource_requirements(&queried,&resources)==MARC_STATUS_OK,"default resource query");
        std::cout<<"resources direction="<<direction<<" config_size="<<sizeof(Config)<<" resources_size="<<sizeof(Resources)
            <<" aggregate="<<resources.minimum_aggregate_bytes<<" raw="<<resources.raw_bytes
            <<" token="<<resources.token_bytes<<" serialized="<<resources.serialized_bytes<<" finder="<<resources.finder_bytes
            <<" working="<<resources.fixed_working_bytes<<" external="<<resources.external_charge_bytes<<'\n';
    }
    std::cout<<"failures="<<failures<<'\n'; return failures ? 1 : 0;
}

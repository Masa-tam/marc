#include <marc/marc.h>
#include "core/endian.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
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
namespace {
using Config=marc_lzss_position_distance_rans_4m_config;
using Resources=marc_lzss_position_distance_rans_4m_resources;
int failures{};
void check(bool good,const char* message) { if (!good) { ++failures; std::cerr<<message<<'\n'; } }
Config config(marc_direction direction,std::uint32_t frame=257) {
    Config c{}; check(marc_lzss_position_distance_rans_4m_config_init(direction,&c)==MARC_STATUS_OK,"config init");
    c.frame_size=frame; c.max_frame_size=frame; c.max_block_size=9*frame;
    c.input_capacity_bytes=c.output_capacity_bytes=7; return c;
}
struct Result { std::vector<std::uint8_t> bytes; marc_process_result result{}; std::size_t consumed{}; };
Result run(marc_transform* p,const std::vector<std::uint8_t>& input,std::size_t chunk=7) {
    Result r;
    for (std::size_t call=0;call<input.size()*3+1048576;++call) {
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
        r.bytes.insert(r.bytes.end(),output.begin()+1,output.begin()+1+r.result.output_produced);
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
    marc_transform* p{}; check(marc_lzss_position_distance_rans_4m_create(&c,&p)==MARC_STATUS_OK && p,"encoder create");
    if (!p) return {};
    const auto r=run(p,input,chunk); marc_transform_destroy(p); return r;
}
void budget_tests() {
    for (auto direction : {MARC_DIRECTION_ENCODE,MARC_DIRECTION_DECODE}) {
        auto c=config(direction); c.original_size=1028;
        Resources r{}; check(marc_lzss_position_distance_rans_4m_resource_requirements(&c,&r)==MARC_STATUS_OK,"query");
        auto bad=c; bad.max_internal_buffered_bytes=r.minimum_aggregate_bytes-1;
        const auto before=r;
        check(marc_lzss_position_distance_rans_4m_resource_requirements(&bad,&r)==MARC_STATUS_LIMIT_EXCEEDED
            && std::memcmp(&before,&r,sizeof(r))==0,"query failure retains output");
        marc_transform* p=reinterpret_cast<marc_transform*>(std::uintptr_t{1});
        allocations=0; fail_at=0; tracked=true;
        const auto status=marc_lzss_position_distance_rans_4m_create(&bad,&p);
        tracked=false;
        check(status==MARC_STATUS_LIMIT_EXCEEDED && !p && !allocations,"budget before allocation");
        c.max_internal_buffered_bytes=r.minimum_aggregate_bytes;
        for (std::size_t failure=1;failure<=(direction==MARC_DIRECTION_ENCODE ? 7u : 6u);++failure) {
            const auto count=live;
            allocations=0; fail_at=failure; tracked=true;
            const auto failed=marc_lzss_position_distance_rans_4m_create(&c,&p);
            tracked=false; fail_at=0;
            check(failed==MARC_STATUS_OUT_OF_MEMORY && !p && live==count && allocations==failure,"factory fault cleanup");
        }
        check(marc_lzss_position_distance_rans_4m_create(&c,&p)==MARC_STATUS_OK && p,"exact budget create");
        marc_transform_destroy(p);
        const auto saved=c;
        check(marc_lzss_position_distance_rans_4m_create(&c,reinterpret_cast<marc_transform**>(&c.original_size))==MARC_STATUS_INVALID_ARGUMENT
            && std::memcmp(&c,&saved,sizeof(c))==0,"aliased handle output");
        check(marc_lzss_position_distance_rans_4m_resource_requirements(&c,reinterpret_cast<Resources*>(&c))==MARC_STATUS_INVALID_ARGUMENT
            && std::memcmp(&c,&saved,sizeof(c))==0,"aliased resources");
        c.external_retained_bytes=UINT64_MAX;
        check(marc_lzss_position_distance_rans_4m_resource_requirements(&c,&r)==MARC_STATUS_LIMIT_EXCEEDED,"external overflow");
    }
}
}
int main() {
    budget_tests();
    for (auto direction : {MARC_DIRECTION_ENCODE,MARC_DIRECTION_DECODE}) {
        Config small{};
        check(marc_lzss_position_distance_rans_4m_config_init(direction,&small)==MARC_STATUS_OK,"small default init");
        small.frame_size=1;
        Resources queried{};
        check(marc_lzss_position_distance_rans_4m_resource_requirements(&small,&queried)==MARC_STATUS_OK,"small default query");
        check(queried.minimum_aggregate_bytes>=small.max_block_size+queried.external_charge_bytes,"decision-ceiling budget floor");
        small.max_internal_buffered_bytes=queried.minimum_aggregate_bytes;
        marc_transform* instance{};
        check(marc_lzss_position_distance_rans_4m_create(&small,&instance)==MARC_STATUS_OK && instance,"small default exact query budget");
        marc_transform_destroy(instance);
        --small.max_internal_buffered_bytes;
        const auto retained=queried;
        check(marc_lzss_position_distance_rans_4m_resource_requirements(&small,&queried)==MARC_STATUS_LIMIT_EXCEEDED
            && std::memcmp(&queried,&retained,sizeof(queried))==0,"decision floor query refusal retains result");
        allocations=0; fail_at=0; tracked=true;
        const auto refused=marc_lzss_position_distance_rans_4m_create(&small,&instance);
        tracked=false;
        check(refused==MARC_STATUS_LIMIT_EXCEEDED && !instance && !allocations,"decision floor refuses before allocation");
    }
    for (auto direction : {MARC_DIRECTION_ENCODE,MARC_DIRECTION_DECODE}) {
        auto flags_config=config(direction,1); flags_config.original_size=1;
        marc_transform* flags_instance{};
        check(marc_lzss_position_distance_rans_4m_create(&flags_config,&flags_instance)==MARC_STATUS_OK,"flags create");
        std::array<std::uint8_t,7> guarded{}; guarded.fill(0xa5);
        const auto refused=marc_transform_process(flags_instance,{nullptr,0},{guarded.data(),guarded.size()},MARC_PROCESS_RESET_BLOCK);
        check(refused.status==MARC_STATUS_UNSUPPORTED && !refused.input_consumed && !refused.output_produced
            && std::all_of(guarded.begin(),guarded.end(),[](auto value){ return value==0xa5; }),"public reset block refused unchanged");
        const auto sticky=marc_transform_process(flags_instance,{nullptr,0},{nullptr,0},MARC_PROCESS_NONE);
        check(sticky.status==refused.status && !sticky.input_consumed && !sticky.output_produced,"public flags sticky");
        marc_transform_destroy(flags_instance);
    }
    for (const std::size_t size : {0u,1u,256u,257u,258u,1029u}) {
        std::vector<std::uint8_t> raw(size);
        for (std::size_t i=0;i<size;++i) raw[i]=static_cast<std::uint8_t>(i%13);
        const auto wire=encode(raw);
        check(wire.result.status==MARC_STATUS_END_OF_STREAM,"encoded finish");
        check(encode(raw,257,1).bytes==wire.bytes,"public chunk determinism");
        auto c=config(MARC_DIRECTION_DECODE); marc_transform* p{};
        check(marc_lzss_position_distance_rans_4m_create(&c,&p)==MARC_STATUS_OK,"decoder create");
        const auto decoded=run(p,wire.bytes,1); marc_transform_destroy(p);
        check(decoded.result.status==MARC_STATUS_END_OF_STREAM && decoded.bytes==raw,"public roundtrip");
    }
    auto c=config(MARC_DIRECTION_ENCODE,1); c.original_size=1;
    marc_transform* p{}; check(marc_lzss_position_distance_rans_4m_create(&c,&p)==MARC_STATUS_OK,"capacity create");
    std::array<std::uint8_t,8> bytes{};
    const auto excessive=marc_transform_process(p,{bytes.data(),bytes.size()},{nullptr,0},0);
    check(excessive.status==MARC_STATUS_LIMIT_EXCEEDED && !excessive.input_consumed && !excessive.output_produced,"call capacity refusal");
    marc_transform_destroy(p);
    auto wire=encode({65,66},1).bytes;
    (void)marc::core::store_le<std::uint64_t>(std::as_writable_bytes(std::span(wire)),wire.size()-8,(UINT64_C(1)<<31)+1);
    c=config(MARC_DIRECTION_DECODE,1);
    check(marc_lzss_position_distance_rans_4m_create(&c,&p)==MARC_STATUS_OK,"later failure create");
    const auto failed=run(p,wire,1); marc_transform_destroy(p);
    check(failed.result.status==MARC_STATUS_MALFORMED_STREAM && failed.bytes==std::vector<std::uint8_t>{65},"failed public frame withheld");
    for (auto direction : {MARC_DIRECTION_ENCODE,MARC_DIRECTION_DECODE}) {
        Config measured{}; Resources resources{};
        check(marc_lzss_position_distance_rans_4m_config_init(direction,&measured)==MARC_STATUS_OK,"default measure init");
        measured.external_retained_bytes=65536;
        check(marc_lzss_position_distance_rans_4m_resource_requirements(&measured,&resources)==MARC_STATUS_OK,"default measure query");
        std::cout<<"resources direction="<<direction<<" config_size="<<sizeof(Config)<<" resources_size="<<sizeof(Resources)
            <<" aggregate="<<resources.minimum_aggregate_bytes<<" raw="<<resources.raw_bytes
            <<" token="<<resources.token_bytes<<" serialized="<<resources.serialized_bytes<<" finder="<<resources.finder_bytes
            <<" working="<<resources.fixed_working_bytes<<" external="<<resources.external_charge_bytes<<'\n';
    }
    std::cout<<"failures="<<failures<<'\n'; return failures ? 1 : 0;
}

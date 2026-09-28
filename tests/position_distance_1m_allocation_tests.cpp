#include "frame/lzss_position_distance_1m_owned_decoder.hpp"
#include "frame/lzss_position_distance_1m_frame.hpp"
#include "core/endian.hpp"
#include <array>
#include "test_assert.h"
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <new>

// Isolated executable only: no allocation controls enter the production ABI.
namespace {
bool armed{};
unsigned fail_at{}, attempts{}, released{};
void* live[4]{};
void release(void* p) noexcept {
    if (p != nullptr) for (auto& slot : live) {
        if (slot == p) { slot = nullptr; ++released; break; }
    }
    std::free(p);
}
}
void* operator new(std::size_t n) {
    assert(!armed); // An unexpected throwing allocation must not evade tracking.
    if (void* p = std::malloc(n ? n : 1)) return p;
    std::abort(); // Unexpected real exhaustion, not the injected failure path.
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { release(p); }
void operator delete[](void* p) noexcept { release(p); }
void operator delete(void* p, std::size_t) noexcept { release(p); }
void operator delete[](void* p, std::size_t) noexcept { release(p); }
void* operator new(std::size_t n, const std::nothrow_t&) noexcept {
    if (armed && ++attempts == fail_at) return nullptr;
    void* p = std::malloc(n ? n : 1);
    assert(p != nullptr);
    if (armed) {
        assert(attempts >= 1 && attempts <= 4);
        live[attempts-1] = p;
    }
    return p;
}
void* operator new[](std::size_t n, const std::nothrow_t& tag) noexcept {
    return ::operator new(n, tag);
}
void operator delete(void* p, const std::nothrow_t&) noexcept { release(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { release(p); }


int main() {
    using namespace marc::frame::internal;
    using Owner=LzssPositionDistance1mOwnedDecoder;
    using Code=marc::core::ErrorCode;
    using Status=marc::core::StreamStatus;
    using Token=marc::dictionary::internal::LzssTypedToken;
    using Kind=marc::dictionary::internal::LzssTypedTokenKind;
    const TypedContextStreamHeader stream{4,4,{1048576,3,258,0},32768,44,9,1,10};
    std::array<std::byte,256> archive{};
    archive[0]=std::byte{'M'};archive[1]=std::byte{'A'};archive[2]=std::byte{'R'};archive[3]=std::byte{'C'};
    const auto put=[&](std::size_t offset,auto value) {assert(marc::core::store_le(std::span{archive},offset,value));};
    for(const auto pair:std::array<std::array<std::uint16_t,2>,10>{
        {{4,2},{8,64},{10,1},{12,2},{14,9},{16,3},{18,2},{84,44},{96,1},{98,10}}}) put(pair[0],pair[1]);
    put(20,UINT32_C(4));put(28,UINT32_C(16));put(32,UINT32_C(16));put(40,UINT64_C(4));
    put(48,UINT32_C(16));put(64,UINT32_C(1048576));put(68,UINT32_C(3));put(72,UINT32_C(258));put(80,UINT32_C(32768));
    const std::array tokens{Token{Kind::literal,65,0,0},Token{Kind::match,0,1,3}};
    std::array<marc::context::internal::ModeledOperation,10> ops{};
    const auto encoded=encode_lzss_position_distance_1m_frame(stream,{},0,0,tokens,ops,std::span{archive}.subspan(112));
    assert(encoded.error==LzssShortMatchFrameEncodeError::none);
    const auto size=112+encoded.serialized_size;
    for(unsigned failure:{0U,1U,2U,3U,4U,0U}) {
        attempts=released=0;fail_at=failure;armed=true;
        LzssPositionDistance1mDecodeWorkspace r{};
        assert(Owner::requirements(4,{},r)==Code::none && attempts==0);
        Code error{};auto p=Owner::create(4,{},error);
        if(failure) {
            assert(!p && error==Code::out_of_memory && attempts==failure && released==failure-1);
        } else {
            assert(p && error==Code::none && attempts==4);
            std::size_t consumed{},produced{};
            Status status=Status::need_input;
            for(unsigned call=0;call<512;++call) {
                std::array<std::byte,1> output{};
                const auto input=std::span{archive}.first(size).subspan(consumed);
                const auto q=p->process(input,output,marc::core::flag_value(marc::core::ProcessFlags::end_input));
                assert(marc::core::is_valid(q,input.size(),1));
                assert(q.status!=Status::error && attempts==4);
                consumed+=q.input_consumed;produced+=q.output_produced;
                if(q.output_produced) assert(output[0]==std::byte{65});
                status=q.status;
                if(status==Status::end_of_stream) break;
            }
            assert(status==Status::end_of_stream && consumed==size && produced==4);
            p.reset();assert(released==4);
        }
        for(auto slot:live) assert(slot==nullptr);
        armed=false;
    }
    attempts=released=0;fail_at=0;armed=true;
    Code error{};
    auto p=Owner::create(0,{},error);
    assert(!p && error==Code::invalid_argument && attempts==0);
    LzssPositionDistance1mDecodeWorkspace r{};
    assert(Owner::requirements(4,{},r)==Code::none && attempts==0);
    auto limits=marc::core::DecoderLimits{};
    limits.max_block_size=4;limits.max_internal_buffered_bytes=r.aggregate_bytes-1;
    p=Owner::create(4,limits,error);
    assert(!p && error==Code::limit_exceeded && attempts==0);
    armed=false;
}

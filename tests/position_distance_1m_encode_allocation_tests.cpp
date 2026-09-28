#include "frame/lzss_position_distance_1m_owned_encoder.hpp"
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
    using Owner=LzssPositionDistance1mOwnedEncoder;
    using Code=marc::core::ErrorCode;
    using Status=marc::core::StreamStatus;
    const TypedContextStreamHeader stream{4,9,{1048576,3,258,0},32768,44,9,1,10};
    std::array<std::byte,9> raw{};
    using Search=marc::dictionary::internal::LzssPositionDistance1mSearch;
    for(auto search:{Search::indexed,Search::indexed_five_prefix}) {
    for(unsigned failure:{0U,1U,2U,3U,4U,0U}) {
        attempts=released=0;fail_at=failure;armed=true;
        LzssPositionDistanceWorkspaceRequirements r{};
        assert(Owner::requirements(stream,{},r,search)==Code::none && attempts==0);
        Code error{};auto p=Owner::create(stream,{},error,3,search);
        if(failure) assert(!p && error==Code::out_of_memory && attempts==failure && released==failure-1);
        else {
            assert(p && error==Code::none && attempts==4);
            std::size_t consumed{},produced{};Status status=Status::need_input;
            for(unsigned call=0;call<4096;++call) {
                std::array<std::byte,1> output{};
                const auto input=std::span{raw}.subspan(consumed);
                const auto q=p->process(input,output,marc::core::flag_value(marc::core::ProcessFlags::end_input));
                assert(marc::core::is_valid(q,input.size(),1));
                assert(q.status!=Status::error && attempts==4);
                consumed+=q.input_consumed;produced+=q.output_produced;status=q.status;
                if(status==Status::end_of_stream) break;
            }
            assert(status==Status::end_of_stream && consumed==raw.size() && produced>112);
            p.reset();assert(released==4);
        }
        for(auto slot:live) assert(slot==nullptr);
        armed=false;
    }
    attempts=released=0;fail_at=0;armed=true;Code error{};
    auto invalid=stream;invalid.frame_size=0;
    assert(!Owner::create(invalid,{},error) && error==Code::invalid_argument && attempts==0);
    assert(!Owner::create(stream,{},error,2) && error==Code::invalid_argument && attempts==0);
    LzssPositionDistanceWorkspaceRequirements r{};
    assert(Owner::requirements(stream,{},r,search)==Code::none && attempts==0);
    auto limits=marc::core::DecoderLimits{};limits.max_block_size=4;
    limits.max_internal_buffered_bytes=r.aggregate_bytes-1;
    assert(!Owner::create(stream,limits,error,3,search) && error==Code::limit_exceeded && attempts==0);
    assert(!Owner::create(stream,{},error,3,static_cast<Search>(99)) && error==Code::invalid_argument && attempts==0);
    armed=false;
    }
}

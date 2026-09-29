#include "frame/lzss_position_distance_1m_bounded18_owned_encoder.hpp"
#include "test_assert.h"
#include <array>
#include <cstdlib>
#include <new>
#include <vector>
namespace { bool armed{}; }
void* operator new(std::size_t n) {
    assert(!armed);
    if(auto* p=std::malloc(n?n:1)) return p;
    std::abort();
}
void* operator new[](std::size_t n) {return ::operator new(n);}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete[](void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void operator delete[](void* p,std::size_t) noexcept {std::free(p);}
void* operator new(std::size_t n,const std::nothrow_t&) noexcept {return ::operator new(n);}
void* operator new[](std::size_t n,const std::nothrow_t&) noexcept {return ::operator new(n);}
void operator delete(void* p,const std::nothrow_t&) noexcept {std::free(p);}
void operator delete[](void* p,const std::nothrow_t&) noexcept {std::free(p);}
int main() {
    using namespace marc::frame::internal;
    using Owner=LzssPositionDistance1mBounded18OwnedEncoder;
    using Code=marc::core::ErrorCode;using Status=marc::core::StreamStatus;
    const TypedContextStreamHeader stream{64,193,{1048576,3,258,0},32768,44,9,1,10};
    LzssPositionDistanceWorkspaceRequirements r{};marc::core::DecoderLimits limits{};
    assert(Owner::requirements(stream,limits,r)==Code::none);
    limits.max_internal_buffered_bytes=r.aggregate_bytes-1;Code error{};
    armed=true;
    assert(!Owner::create(stream,limits,error) && error==Code::limit_exceeded);
    armed=false;++limits.max_internal_buffered_bytes;
    auto encoder=Owner::create(stream,limits,error);assert(encoder && error==Code::none);
    std::array<std::byte,193> input{};std::array<std::byte,1> output{};
    std::size_t consumed{};bool ended{};
    armed=true;
    for(unsigned call=0;call<10000;++call) {
        auto in=std::span{input}.subspan(consumed);
        auto out=call%7==0?std::span{output}.first(0):std::span{output};
        const auto result=encoder->process(in,out,marc::core::flag_value(marc::core::ProcessFlags::end_input));
        assert(marc::core::is_valid(result,in.size(),out.size()));assert(result.status!=Status::error);
        consumed+=result.input_consumed;
        if(result.status==Status::end_of_stream) {ended=true;break;}
    }
    assert(ended && consumed==input.size());encoder.reset();armed=false;
}

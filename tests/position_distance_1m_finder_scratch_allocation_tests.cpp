#include "frame/lzss_position_distance_1m_finder_scratch_raw_frame_encoder.hpp"
#include "dictionary/lzss_position_distance_1m_five_prefix_finder.hpp"
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
    using namespace marc::dictionary::internal;
    constexpr unsigned size=256;
    const TypedContextStreamHeader stream{size,size,{1048576,3,258,0},32768,44,9,1,10};
    const auto r=calculate_lzss_position_distance_1m_five_prefix_workspace(size,stream.dictionary,{});
    assert(r.error==LzssShortPrefixError::none);
    std::vector<std::max_align_t> storage((r.workspace_size+sizeof(std::max_align_t)-1)/sizeof(std::max_align_t));
    auto finder=std::as_writable_bytes(std::span{storage}).first(r.workspace_size);
    std::array<std::byte,size> raw{};
    std::array<LzssTypedToken,size> tokens{};
    std::array<marc::context::internal::ModeledOperation,2*size> operations{};
    std::array<std::byte,18*size+85> output{};
    armed=true;
    for(unsigned i=0;i<128;++i) {
        const auto result=encode_lzss_position_distance_1m_finder_scratch_raw_frame(
            stream,{},0,0,raw,3,LzssPositionDistance1mSearch::indexed_five_prefix,tokens,operations,finder,output);
        assert(result.error==LzssPositionDistanceRawFrameError::none && result.used_finder_scratch);
    }
    armed=false;
}

#include "dictionary/lzss_position_distance_1m_five_prefix_candidate.hpp"
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
    using namespace marc::dictionary::internal;
    const LzssParameters params{1048576,3,258,0};std::array<std::byte,64> raw{};
    const auto q=calculate_lzss_position_distance_1m_five_prefix_workspace(raw.size(),params,{});
    assert(q.error==LzssShortPrefixError::none);
    std::vector<std::uint32_t> words(q.workspace_size/4);std::array<LzssTypedToken,64> tokens{};
    armed=true;
    for(unsigned i=0;i<128;++i) {
        assert(calculate_lzss_position_distance_1m_five_prefix_workspace(raw.size(),params,{}).error==LzssShortPrefixError::none);
        const auto result=tokenize_lzss_position_distance_1m_five_prefix_candidate(raw,params,{},3,tokens,std::as_writable_bytes(std::span{words}));
        assert(result.error==LzssShortMatchCandidateError::none && result.token_count==2);
        assert(tokens[1].distance==1 && tokens[1].length==63);
    }
    armed=false;
}

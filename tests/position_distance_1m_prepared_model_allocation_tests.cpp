#include "context/lzss_position_distance_1m_prepared_model.hpp"
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
    using namespace marc::context::internal;using namespace marc::dictionary::internal;
    const LzssParameters p{1048576,3,258,0};const LzssTypedFrameValidationContext context{2,4,0};
    const marc::core::DecoderLimits limits{};
    std::array<LzssTypedToken,2> tokens{{{LzssTypedTokenKind::literal,65,0,0},{LzssTypedTokenKind::match,0,1,3}}};
    std::array<ModeledOperation,8> output{};PreparedLzssPositionDistance1mModel prepared;
    armed=true;
    for(unsigned i=0;i<128;++i) {
        assert(prepared.prepare(tokens,p,context,limits).error==LzssFieldContextError::none);
        assert(prepared.write(output).error==LzssFieldContextError::none);
    }
    armed=false;
}

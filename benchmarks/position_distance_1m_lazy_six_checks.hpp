#ifndef MARC_BENCHMARK_POSITION_DISTANCE_1M_LAZY_SIX_CHECKS_HPP
#define MARC_BENCHMARK_POSITION_DISTANCE_1M_LAZY_SIX_CHECKS_HPP
#include "position_distance_1m_lazy_six.hpp"
namespace marc::benchmark {
inline bool check_lazy_six_boundaries() {
    using namespace dictionary::internal;
    const LzssParameters parameters{1048576,3,258,0};
    for(std::size_t n=0;n<=32;++n) {
        LazySixPrefixFinder finder(n);
        for(unsigned pattern=0;pattern<3;++pattern) {
            std::vector<std::byte> raw(n);std::uint32_t state=719;
            for(std::size_t i=0;i<n;++i) {
                state=state*1664525U+1013904223U;
                raw[i]=pattern==0?std::byte{0}:pattern==1?std::byte(i%7):std::byte(state>>24);
            }
            LzssExhaustiveMatchFinder oracle(raw,parameters);
            for(std::size_t checkpoint=0;checkpoint<=n;++checkpoint) {
                if(!finder.reset(raw) || finder.active() || finder.activate(1)) return false;
                for(std::size_t p=0;p<n;++p) {
                    if(p==checkpoint) {
                        const bool expected=n-p>=6;
                        if(finder.activate(p)!=expected || finder.active()!=expected) return false;
                        if(expected && finder.activate(p)) return false;
                    }
                    if(finder.find_match(p)!=oracle.find_match(p) || !finder.advance(p,p+1)) return false;
                }
                if(finder.activate(n) || finder.advance(n,n+1)) return false;
            }
        }
    }
    for(std::size_t n:{257U,258U,259U,513U}) {
        std::vector<std::byte> raw(n);
        for(std::size_t i=0;i<n;++i) raw[i]=std::byte(i%7);
        LazySixPrefixFinder finder(n);LzssExhaustiveMatchFinder oracle(raw,parameters);
        for(const auto checkpoint:std::array<std::size_t,5>{0,1,n/2,n-6,n}) {
            if(!finder.reset(raw)) return false;
            for(std::size_t p=0;p<n;++p) {
                if(p==checkpoint && !finder.activate(p)) return false;
                if(finder.find_match(p)!=oracle.find_match(p) || !finder.advance(p,p+1)) return false;
            }
        }
    }
    // The nearest prior position is inside the previously emitted length-258 match.
    std::vector<std::byte> zeros(600);
    LazySixPrefixFinder finder(600);
    if(!finder.reset(zeros) || !finder.advance(0,1) || finder.find_match(1)!=LzssMatch{1,258}
        || !finder.advance(1,259) || !finder.activate(259) || finder.find_match(259)!=LzssMatch{1,258}) return false;
    std::vector<std::byte> oversized(601);
    if(finder.reset(oversized) || finder.find_match(259)!=LzssMatch{1,258}) return false;
    for(std::size_t n=0;n<=7;++n) {
        if(!finder.reset(std::span{zeros}.first(n)) || finder.active()) return false;
        LzssExhaustiveMatchFinder oracle(std::span{zeros}.first(n),parameters);
        for(std::size_t p=0;p<n;++p) {
            if(finder.find_match(p)!=oracle.find_match(p) || !finder.advance(p,p+1)) return false;
        }
    }
    for(std::size_t distance:{65535U,65536U,65537U,1048570U}) {
        std::vector<std::byte> raw(distance+6);
        for(std::size_t i=0;i<6;++i) raw[i]=raw[distance+i]=std::byte(0xa1+i);
        LazySixPrefixFinder wide(raw.size());
        if(!wide.reset(raw) || !wide.advance(0,distance) || !wide.activate(distance)
            || wide.find_match(distance)!=LzssMatch{static_cast<std::uint32_t>(distance),6}) return false;
    }
    return true;
}
}
#endif

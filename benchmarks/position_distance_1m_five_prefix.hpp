#ifndef MARC_BENCHMARK_POSITION_DISTANCE_1M_FIVE_PREFIX_HPP
#define MARC_BENCHMARK_POSITION_DISTANCE_1M_FIVE_PREFIX_HPP
#include "dictionary/lzss_position_distance_1m_match_finder.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <vector>
namespace marc::benchmark {
// Experiment only. Caller supplies sequential positions inside one bounded frame.
class FivePrefixFinder {
    static constexpr auto empty=std::numeric_limits<std::uint32_t>::max();
    std::span<const std::byte> input_;
    std::array<std::vector<std::uint32_t>,3> heads_,links_;
    std::size_t bucket(std::size_t p,std::size_t n) const {
        std::uint32_t key{};
        for(std::size_t i=0;i<std::min<std::size_t>(n,4);++i)
            key|=std::to_integer<std::uint32_t>(input_[p+i])<<(8*i);
        // Fold the fifth byte before the same bounded bucket mixer.
        if(n==5) key^=std::to_integer<std::uint32_t>(input_[p+4])*UINT32_C(2246822519);
        key^=key>>11;
        return (key*UINT32_C(2654435761))>>16;
    }
    bool equal(std::size_t p,std::size_t c,std::size_t n) const {
        for(std::size_t i=0;i<n;++i) if(input_[p+i]!=input_[c+i]) return false;
        return true;
    }
public:
    explicit FivePrefixFinder(std::size_t capacity) {
        for(auto& h:heads_) h.resize(65536);
        for(auto& l:links_) l.resize(capacity);
    }
    void reset(std::span<const std::byte> input) {
        input_=input;
        for(auto& h:heads_) std::fill(h.begin(),h.end(),empty);
        for(auto& l:links_) std::fill_n(l.begin(),input.size(),empty);
    }
    dictionary::internal::LzssMatch find_match(std::size_t p) const {
        dictionary::internal::LzssMatch best{};
        const auto maximum=std::min<std::size_t>(258,input_.size()-p);
        if(maximum<3) return best;
        auto nearest=empty;
        for(auto c=heads_[0][bucket(p,3)];c!=empty;c=links_[0][c]) {
            if(equal(p,c,3)) {nearest=c;best={static_cast<std::uint32_t>(p-c),3};break;}
        }
        if(!best.length || maximum==3) return best;
        auto first=input_[p+3]==input_[nearest+3] ? nearest : heads_[1][bucket(p,4)];
        nearest=empty;
        for(auto c=first;c!=empty;c=links_[1][c]) {
            if(equal(p,c,4)) {nearest=c;best={static_cast<std::uint32_t>(p-c),4};break;}
        }
        if(nearest==empty || maximum==4) return best;
        first=input_[p+4]==input_[nearest+4] ? nearest : heads_[2][bucket(p,5)];
        for(auto c=first;c!=empty;c=links_[2][c]) {
            if(input_[p+best.length]!=input_[c+best.length] || !equal(p,c,5)) continue;
            std::size_t length=5;
            while(length<maximum && input_[p+length]==input_[c+length]) ++length;
            if(length>best.length) {
                best={static_cast<std::uint32_t>(p-c),static_cast<std::uint32_t>(length)};
                if(length==maximum) break;
            }
        }
        return best;
    }
    void advance(std::size_t p,std::size_t next) {
        for(;p<next;++p) for(std::size_t i=0;i<3;++i) {
            if(input_.size()-p<i+3) break;
            const auto b=bucket(p,i+3);links_[i][p]=heads_[i][b];heads_[i][b]=static_cast<std::uint32_t>(p);
        }
    }
};
}
#endif

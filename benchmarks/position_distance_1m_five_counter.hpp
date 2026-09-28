#ifndef MARC_BENCHMARK_POSITION_DISTANCE_1M_FIVE_COUNTER_HPP
#define MARC_BENCHMARK_POSITION_DISTANCE_1M_FIVE_COUNTER_HPP
#include "dictionary/lzss_position_distance_1m_five_prefix_finder.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <vector>
namespace marc::benchmark {
struct FiveFinderCounts {
    std::uint64_t short_visits{},middle_visits{},long_visits{};
    std::uint64_t exact_long_prefix{},colliding_long_prefix{},probe_rejects{},prefix_rejects{};
    std::uint64_t extension_compares{},extension_equal{},improvements{},maximum_exits{};
    std::uint64_t short_inserts{},middle_inserts{},long_inserts{};
};
// Untimed first-party replay, restricted to sequential full-window frames <=1 MiB.
// Counts describe logical work, not CPU time. Codec output is the oracle.
class FiveCounterFinder {
    static constexpr auto empty=std::numeric_limits<std::uint32_t>::max();
    std::span<const std::byte> input_;
    std::array<std::vector<std::uint32_t>,3> heads_,links_;
    FiveFinderCounts& counts_;
    std::size_t bucket(std::size_t p,std::size_t n) const {
        std::uint32_t key{};
        for(std::size_t i=0;i<std::min<std::size_t>(n,4);++i)
            key|=std::to_integer<std::uint32_t>(input_[p+i])<<(8*i);
        if(n==5) key^=std::to_integer<std::uint32_t>(input_[p+4])*UINT32_C(2246822519);
        key^=key>>11;return (key*UINT32_C(2654435761))>>16;
    }
    bool equal(std::size_t p,std::size_t c,std::size_t n) const {
        for(std::size_t i=0;i<n;++i) if(input_[p+i]!=input_[c+i]) return false;
        return true;
    }
public:
    FiveCounterFinder(std::span<const std::byte> input,FiveFinderCounts& counts)
        : input_(input),counts_(counts) {
        for(auto& h:heads_) h.assign(65536,empty);
        for(auto& l:links_) l.assign(input.size(),empty);
    }
    dictionary::internal::LzssMatch find(std::size_t p) {
        dictionary::internal::LzssMatch best{};
        const auto maximum=std::min<std::size_t>(258,input_.size()-p);
        if(maximum<3) return best;
        auto nearest=empty;
        for(auto c=heads_[0][bucket(p,3)];c!=empty;c=links_[0][c]) {
            ++counts_.short_visits;
            if(equal(p,c,3)) {nearest=c;best={static_cast<std::uint32_t>(p-c),3};break;}
        }
        if(!best.length || maximum==3) return best;
        auto first=input_[p+3]==input_[nearest+3]?nearest:heads_[1][bucket(p,4)];
        nearest=empty;
        for(auto c=first;c!=empty;c=links_[1][c]) {
            ++counts_.middle_visits;
            if(equal(p,c,4)) {nearest=c;best={static_cast<std::uint32_t>(p-c),4};break;}
        }
        if(nearest==empty || maximum==4) return best;
        first=input_[p+4]==input_[nearest+4]?nearest:heads_[2][bucket(p,5)];
        for(auto c=first;c!=empty;c=links_[2][c]) {
            ++counts_.long_visits;
            const bool prefix=equal(p,c,5);
            if(prefix) ++counts_.exact_long_prefix;else ++counts_.colliding_long_prefix;
            if(input_[p+best.length]!=input_[c+best.length]) {++counts_.probe_rejects;continue;}
            if(!prefix) {++counts_.prefix_rejects;continue;}
            std::size_t length=5;
            while(length<maximum) {
                ++counts_.extension_compares;
                if(input_[p+length]!=input_[c+length]) break;
                ++counts_.extension_equal;++length;
            }
            if(length>best.length) {
                ++counts_.improvements;
                best={static_cast<std::uint32_t>(p-c),static_cast<std::uint32_t>(length)};
                if(length==maximum) {++counts_.maximum_exits;break;}
            }
        }
        return best;
    }
    void advance(std::size_t p,std::size_t next) {
        for(;p<next;++p) for(std::size_t i=0;i<3;++i) {
            if(input_.size()-p<i+3) break;
            const auto b=bucket(p,i+3);
            links_[i][p]=heads_[i][b];heads_[i][b]=static_cast<std::uint32_t>(p);
            if(i==0) ++counts_.short_inserts;
            else if(i==1) ++counts_.middle_inserts;
            else ++counts_.long_inserts;
        }
    }
};
}
#endif

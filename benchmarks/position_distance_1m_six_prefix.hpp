#ifndef MARC_BENCHMARK_POSITION_DISTANCE_1M_SIX_PREFIX_HPP
#define MARC_BENCHMARK_POSITION_DISTANCE_1M_SIX_PREFIX_HPP
#include "dictionary/lzss_position_distance_1m_match_finder.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <vector>
namespace marc::benchmark {
// Experiment only. Caller supplies sequential positions inside one bounded frame.
template<bool SharedAdvance, bool OmitFive = false, bool RollingKey = false, bool FifthFirst = false>
class SixPrefixFinderImpl {
    static constexpr auto empty=std::numeric_limits<std::uint32_t>::max();
    std::span<const std::byte> input_;
    static constexpr std::size_t long_index = OmitFive ? 2 : 3;
    std::array<std::vector<std::uint32_t>, OmitFive ? 3 : 4> heads_,links_;
    std::size_t bucket(std::size_t p,std::size_t n) const {
        std::uint32_t key{};
        for(std::size_t i=0;i<std::min<std::size_t>(n,4);++i)
            key|=std::to_integer<std::uint32_t>(input_[p+i])<<(8*i);
        // Fold the fifth byte before the same bounded bucket mixer.
        if(n>=5) key^=std::to_integer<std::uint32_t>(input_[p+4])*UINT32_C(2246822519);
        if(n==6) key^=std::to_integer<std::uint32_t>(input_[p+5])*UINT32_C(3266489917);
        key^=key>>11;
        return (key*UINT32_C(2654435761))>>16;
    }
    bool equal(std::size_t p,std::size_t c,std::size_t n) const {
        for(std::size_t i=0;i<n;++i) if(input_[p+i]!=input_[c+i]) return false;
        return true;
    }
public:
    explicit SixPrefixFinderImpl(std::size_t capacity) {
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
        // The compact experiment searches the four-byte chain for its nearest
        // five-byte fallback; it has no separate five-byte index.
        constexpr std::size_t fallback_index = OmitFive ? 1 : 2;
        first=input_[p+4]==input_[nearest+4] ? nearest
            : heads_[fallback_index][bucket(p,OmitFive ? 4 : 5)];
        nearest=empty;
        for(auto c=first;c!=empty;c=links_[fallback_index][c]) {
            if constexpr (FifthFirst) {
                if(input_[p+4]!=input_[c+4]) continue;
            }
            if(equal(p,c,FifthFirst ? 4 : 5)) {nearest=c;best={static_cast<std::uint32_t>(p-c),5};break;}
        }
        if(nearest==empty || maximum==5) return best;
        first=input_[p+5]==input_[nearest+5] ? nearest : heads_[long_index][bucket(p,6)];
        for(auto c=first;c!=empty;c=links_[long_index][c]) {
            if(input_[p+best.length]!=input_[c+best.length] || !equal(p,c,6)) continue;
            std::size_t length=6;
            while(length<maximum && input_[p+length]==input_[c+length]) ++length;
            if(length>best.length) {
                best={static_cast<std::uint32_t>(p-c),static_cast<std::uint32_t>(length)};
                if(length==maximum) break;
            }
        }
        return best;
    }
    void advance(std::size_t p,std::size_t next) {
        if constexpr (SharedAdvance) {
            const auto end=std::min(next,input_.size()>=6?input_.size()-5:0);
            const auto mix=[](std::uint32_t key) {
                key^=key>>11;
                return (key*UINT32_C(2654435761))>>16;
            };
            if constexpr (RollingKey) {
                std::uint32_t rolling{};
                if(p<end) for(std::size_t i=0;i<4;++i)
                    rolling|=std::to_integer<std::uint32_t>(input_[p+i])<<(8*i);
                for(;p<end;++p) {
                    const auto four=rolling;
                    const auto three=four & UINT32_C(0x00ffffff);
                    const auto fifth=std::to_integer<std::uint32_t>(input_[p+4]);
                    const auto five=four ^ (fifth*UINT32_C(2246822519));
                    const std::array<std::uint32_t,4> buckets{mix(three),mix(four),mix(five),
                        mix(five ^ (std::to_integer<std::uint32_t>(input_[p+5])*UINT32_C(3266489917)))};
                    // Carry the next four-byte key only within this bounded advance.
                    rolling=(four>>8) | (fifth<<24);
                    for(std::size_t i=0;i<heads_.size();++i) {
                        const auto b=buckets[OmitFive && i==2 ? 3 : i];
                        links_[i][p]=heads_[i][b];
                        heads_[i][b]=static_cast<std::uint32_t>(p);
                    }
                }
            } else {
                for(;p<end;++p) {
                    const auto three=std::to_integer<std::uint32_t>(input_[p])
                        | (std::to_integer<std::uint32_t>(input_[p+1])<<8)
                        | (std::to_integer<std::uint32_t>(input_[p+2])<<16);
                    const auto four=three | (std::to_integer<std::uint32_t>(input_[p+3])<<24);
                    const std::array<std::uint32_t,4> buckets{mix(three),mix(four),
                        mix(four ^ (std::to_integer<std::uint32_t>(input_[p+4])*UINT32_C(2246822519))),
                        mix(four ^ (std::to_integer<std::uint32_t>(input_[p+4])*UINT32_C(2246822519))
                            ^ (std::to_integer<std::uint32_t>(input_[p+5])*UINT32_C(3266489917)))};
                    for(std::size_t i=0;i<heads_.size();++i) {
                        const auto b=buckets[OmitFive && i==2 ? 3 : i];
                        links_[i][p]=heads_[i][b];
                        heads_[i][b]=static_cast<std::uint32_t>(p);
                    }
                }
            }
        }
        for(;p<next;++p) for(std::size_t i=0;i<heads_.size();++i) {
            const auto width=OmitFive && i==2 ? 6 : i+3;
            if(input_.size()-p<width) break;
            const auto b=bucket(p,width);links_[i][p]=heads_[i][b];heads_[i][b]=static_cast<std::uint32_t>(p);
        }
    }
};
using SixPrefixFinder = SixPrefixFinderImpl<true>;
using FifthFirstSixPrefixFinder = SixPrefixFinderImpl<true, false, false, true>;
using RollingSixPrefixFinder = SixPrefixFinderImpl<true, false, true>;
using CompactSixPrefixFinder = SixPrefixFinderImpl<true, true>;
}
#endif

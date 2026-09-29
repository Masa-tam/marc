#ifndef MARC_BENCHMARK_POSITION_DISTANCE_1M_RUNTIME_WIDTH_HPP
#define MARC_BENCHMARK_POSITION_DISTANCE_1M_RUNTIME_WIDTH_HPP
#include "dictionary/lzss_position_distance_1m_match_finder.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <vector>
namespace marc::benchmark {
// Experiment only. Caller supplies sequential positions inside one bounded frame.
class RuntimeWidthFinder {
    unsigned long_bits_{16};
    std::size_t capacity_,next_{};
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
        return (key*UINT32_C(2654435761))>>(n==5 ? 32-long_bits_ : 16);
    }
    bool equal(std::size_t p,std::size_t c,std::size_t n) const {
        for(std::size_t i=0;i<n;++i) if(input_[p+i]!=input_[c+i]) return false;
        return true;
    }
public:
    unsigned active_bits() const noexcept {return long_bits_;}
    explicit RuntimeWidthFinder(std::size_t capacity):capacity_(std::min<std::size_t>(capacity,1048576)) {
        for(std::size_t i=0;i<3;++i) heads_[i].resize(i==2 ? std::size_t{1}<<20 : 65536);
        for(auto& l:links_) l.resize(capacity_);
    }
    bool reset(std::span<const std::byte> input,unsigned bits) {
        if(bits!=16 && bits!=18 && bits!=20) return false;
        if(input.size()>capacity_ || input.size()>1048576) return false;
        input_=input;next_=0;long_bits_=bits;
        for(std::size_t i=0;i<3;++i)
            std::fill_n(heads_[i].begin(),i==2 ? std::size_t{1}<<long_bits_ : 65536,empty);
        for(auto& l:links_) std::fill_n(l.begin(),input.size(),empty);
        return true;
    }
    dictionary::internal::LzssMatch find_match(std::size_t p) const {
        dictionary::internal::LzssMatch best{};
        if(p!=next_ || p>=input_.size()) return best;
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
    bool advance(std::size_t p,std::size_t next) {
        if(p!=next_ || next<p || next>input_.size()) return false;
        {
            const auto end=std::min(next,input_.size()>=5?input_.size()-4:0);
            const auto mix=[](std::uint32_t key) {
                key^=key>>11;
                return key*UINT32_C(2654435761);
            };
            for(;p<end;++p) {
                const auto three=std::to_integer<std::uint32_t>(input_[p])
                    | (std::to_integer<std::uint32_t>(input_[p+1])<<8)
                    | (std::to_integer<std::uint32_t>(input_[p+2])<<16);
                const auto four=three | (std::to_integer<std::uint32_t>(input_[p+3])<<24);
                const std::array<std::uint32_t,3> buckets{mix(three)>>16,mix(four)>>16,
                    mix(four ^ (std::to_integer<std::uint32_t>(input_[p+4])*UINT32_C(2246822519)))>>(32-long_bits_)};
                for(std::size_t i=0;i<3;++i) {
                    links_[i][p]=heads_[i][buckets[i]];
                    heads_[i][buckets[i]]=static_cast<std::uint32_t>(p);
                }
            }
        }
        for(;p<next;++p) for(std::size_t i=0;i<3;++i) {
            if(input_.size()-p<i+3) break;
            const auto b=bucket(p,i+3);links_[i][p]=heads_[i][b];heads_[i][b]=static_cast<std::uint32_t>(p);
        }
        next_=next;return true;
    }
};
}
#endif

#ifndef MARC_BENCHMARK_POSITION_DISTANCE_1M_LAZY_SIX_HPP
#define MARC_BENCHMARK_POSITION_DISTANCE_1M_LAZY_SIX_HPP
#include "dictionary/lzss_position_distance_1m_match_finder.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <limits>
#include <vector>
namespace marc::benchmark {
// Benchmark only: immutable full-window frames, maximum match 258, minimum 3.
// Caller constructs with capacity <= 1 MiB. All arrays are reserved up front.
struct LazyActivationTimes {double initialize{},catch_up{};};
class LazySixPrefixFinder {
    static constexpr auto empty=std::numeric_limits<std::uint32_t>::max();
    std::span<const std::byte> input_;
    std::array<std::vector<std::uint32_t>,4> heads_,links_;
    std::size_t capacity_,next_{};
    bool six_{};
    static std::uint32_t mix(std::uint32_t key) {
        key^=key>>11;return (key*UINT32_C(2654435761))>>16;
    }
    std::size_t bucket(std::size_t p,std::size_t n) const {
        std::uint32_t key{};
        for(std::size_t i=0;i<std::min<std::size_t>(n,4);++i)
            key|=std::to_integer<std::uint32_t>(input_[p+i])<<(8*i);
        if(n>=5) key^=std::to_integer<std::uint32_t>(input_[p+4])*UINT32_C(2246822519);
        if(n==6) key^=std::to_integer<std::uint32_t>(input_[p+5])*UINT32_C(3266489917);
        return mix(key);
    }
    bool equal(std::size_t p,std::size_t c,std::size_t n) const {
        for(std::size_t i=0;i<n;++i) if(input_[p+i]!=input_[c+i]) return false;
        return true;
    }
    template<bool Six>
    void advance_indexes(std::size_t p,std::size_t next) {
        constexpr std::size_t width=Six?6:5;
        constexpr std::size_t count=Six?4:3;
        const auto end=std::min(next,input_.size()>=width?input_.size()-(width-1):0);
        for(;p<end;++p) {
            const auto three=std::to_integer<std::uint32_t>(input_[p])
                | (std::to_integer<std::uint32_t>(input_[p+1])<<8)
                | (std::to_integer<std::uint32_t>(input_[p+2])<<16);
            const auto four=three | (std::to_integer<std::uint32_t>(input_[p+3])<<24);
            const auto five=four ^ (std::to_integer<std::uint32_t>(input_[p+4])*UINT32_C(2246822519));
            std::array<std::uint32_t,count> buckets{mix(three),mix(four),mix(five)};
            if constexpr(Six) buckets[3]=mix(five ^ (std::to_integer<std::uint32_t>(input_[p+5])*UINT32_C(3266489917)));
            for(std::size_t i=0;i<count;++i) {
                links_[i][p]=heads_[i][buckets[i]];heads_[i][buckets[i]]=static_cast<std::uint32_t>(p);
            }
        }
        for(;p<next;++p) for(std::size_t i=0;i<count;++i) {
            if(input_.size()-p<i+3) break;
            const auto b=bucket(p,i+3);links_[i][p]=heads_[i][b];heads_[i][b]=static_cast<std::uint32_t>(p);
        }
    }
public:
    explicit LazySixPrefixFinder(std::size_t capacity):capacity_(capacity) {
        for(std::size_t i=0;i<4;++i) if(capacity>=3 && (i<3 || capacity>=6)) {
            heads_[i].resize(65536);links_[i].resize(capacity);
        }
    }
    bool reset(std::span<const std::byte> input) {
        if(input.size()>capacity_ || input.size()>1048576) return false;
        input_=input;next_=0;six_=false;
        if(input.size()>=3) for(std::size_t i=0;i<3;++i) {
            std::fill(heads_[i].begin(),heads_[i].end(),empty);
            std::fill_n(links_[i].begin(),input.size(),empty);
        }
        return true;
    }
private:
    template<bool Measure>
    bool activate_impl(std::size_t p,LazyActivationTimes* times) {
        if(six_ || p!=next_ || p>=input_.size() || input_.size()-p<6) return false;
        using Clock=std::chrono::steady_clock;
        Clock::time_point start,middle;
        if constexpr(Measure) start=Clock::now();
        std::fill(heads_[3].begin(),heads_[3].end(),empty);
        std::fill_n(links_[3].begin(),input_.size(),empty);
        if constexpr(Measure) middle=Clock::now();
        // Insert every byte position, including interiors of previous matches.
        const auto end=std::min(p,input_.size()-5);
        for(std::size_t c=0;c<end;++c) {
            const auto b=bucket(c,6);links_[3][c]=heads_[3][b];heads_[3][b]=static_cast<std::uint32_t>(c);
        }
        if constexpr(Measure) {
            const auto end_time=Clock::now();
            times->initialize=std::chrono::duration<double>(middle-start).count();
            times->catch_up=std::chrono::duration<double>(end_time-middle).count();
        }
        six_=true;return true;
    }
public:
    bool activate(std::size_t p) {return activate_impl<false>(p,nullptr);}
    bool activate_measured(std::size_t p,LazyActivationTimes& times) {return activate_impl<true>(p,&times);}
    bool active() const {return six_;}
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
        if(!six_) {
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
        nearest=empty;
        for(auto c=first;c!=empty;c=links_[2][c]) {
            if(equal(p,c,5)) {nearest=c;best={static_cast<std::uint32_t>(p-c),5};break;}
        }
        if(nearest==empty || maximum==5) return best;
        first=input_[p+5]==input_[nearest+5] ? nearest : heads_[3][bucket(p,6)];
        for(auto c=first;c!=empty;c=links_[3][c]) {
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
    bool advance(std::size_t p,std::size_t next) {
        if(p!=next_ || next<p || next>input_.size()) return false;
        if(six_) advance_indexes<true>(p,next);else advance_indexes<false>(p,next);
        next_=next;return true;
    }
};
}
#endif

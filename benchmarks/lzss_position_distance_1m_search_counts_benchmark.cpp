#include "position_distance_1m_five_counter.hpp"
#include "position_distance_1m_six_counter.hpp"
#include "position_distance_1m_six_prefix.hpp"
#include "dictionary/lzss_position_distance_1m_candidate.hpp"
#include <fstream>
#include <iostream>
using namespace marc::dictionary::internal;
using namespace marc::benchmark;

bool hand_checks() {
    for(std::size_t n:{6U,7U}) {
        std::vector<std::byte> raw(n);
        FiveFinderCounts a; SixFinderCounts b;
        FiveCounterFinder five(raw,a); SixCounterFinder six(raw,b);
        if(five.find(0)!=LzssMatch{} || six.find(0)!=LzssMatch{}) return false;
        five.advance(0,1);six.advance(0,1);
        const LzssMatch expected{1,static_cast<std::uint32_t>(n-1)};
        if(five.find(1)!=expected || six.find(1)!=expected) return false;
        if(a.short_visits!=1 || a.middle_visits!=1 || a.long_visits!=1
            || b.short_visits!=1 || b.middle_visits!=1 || b.fallback_visits!=1
            || b.long_visits!=(n==7?1U:0U) || a.extension_compares!=(n==7?1U:0U)
            || b.extension_compares!=0 || b.fallback_matches!=1
            || b.fallback_fifth_rejects!=0 || b.fallback_prefix_rejects!=0) return false;
    }
    return true;
}
template<class Counts>
bool valid(const Counts& c) {
    return c.long_visits==c.exact_long_prefix+c.colliding_long_prefix
        && c.probe_rejects+c.prefix_rejects<=c.long_visits
        && c.extension_equal<=c.extension_compares && c.maximum_exits<=c.improvements;
}
template<class Counts>
void report(const char* prefix,const Counts& c) {
#define COUNT(name) std::cout<<prefix<<#name<<'='<<c.name<<'\n'
    COUNT(short_visits);COUNT(middle_visits);COUNT(long_visits);
    COUNT(exact_long_prefix);COUNT(colliding_long_prefix);COUNT(probe_rejects);COUNT(prefix_rejects);
    COUNT(extension_compares);COUNT(extension_equal);COUNT(improvements);COUNT(maximum_exits);
    COUNT(short_inserts);COUNT(middle_inserts);COUNT(long_inserts);
#undef COUNT
}
int main(int argc,char** argv) {
    if(argc!=2) return 2;
    std::ifstream file(argv[1],std::ios::binary|std::ios::ate);if(!file) return 2;
    const auto size=file.tellg();if(size<=0 || size>64*1024*1024) return 2;
    std::vector<std::byte> input(static_cast<std::size_t>(size));file.seekg(0);
    if(!file.read(reinterpret_cast<char*>(input.data()),size)) return 2;
    if(!hand_checks()) return 1;
    constexpr std::size_t capacity=1048576;
    const LzssParameters parameters{capacity,3,258,0};const marc::core::DecoderLimits limits{};
    const auto q=calculate_lzss_position_distance_1m_five_prefix_workspace(capacity,parameters,limits);
    if(q.error!=LzssShortPrefixError::none) return 1;
    std::vector<std::uint32_t> storage(q.workspace_size/4);
    const auto workspace=std::as_writable_bytes(std::span{storage});
    std::vector<LzssTypedToken> tokens(capacity);
    FiveFinderCounts a;SixFinderCounts b;std::uint64_t total_tokens{};
    std::array<std::uint64_t,7> lengths{};
    SixPrefixFinder original(capacity);
    for(std::size_t offset=0;offset<input.size();offset+=capacity) {
        const auto raw=std::span{input}.subspan(offset,std::min(capacity,input.size()-offset));
        const auto oracle=tokenize_lzss_position_distance_1m_candidate(raw,parameters,limits,3,
            LzssPositionDistance1mSearch::indexed,tokens,workspace);
        if(oracle.error!=LzssShortMatchCandidateError::none) return 1;
        LzssPositionDistance1mFivePrefixFinder bounded;
        if(initialize_lzss_position_distance_1m_five_prefix_finder(raw,parameters,limits,workspace,bounded)!=LzssShortPrefixError::none) return 1;
        original.reset(raw);FiveCounterFinder five(raw,a);SixCounterFinder six(raw,b);
        std::size_t p{};
        for(const auto& t:std::span{tokens}.first(oracle.token_count)) {
            const auto x=bounded.find_match(p),y=original.find_match(p);
            if(x!=y || x!=five.find(p) || x!=six.find(p)) return 1;
            if(t.kind==LzssTypedTokenKind::literal) {
                if(x.length>=3 || t.literal!=std::to_integer<std::uint8_t>(raw[p])) return 1;
                ++lengths[0];
            } else {
                if(x.length!=t.length || x.distance!=t.distance) return 1;
                ++lengths[std::min<std::size_t>(t.length,6)];
            }
            const auto next=p+(t.kind==LzssTypedTokenKind::literal?1:t.length);
            if(next>raw.size()) return 1;
            bounded.advance(p,next);original.advance(p,next);five.advance(p,next);six.advance(p,next);p=next;
        }
        if(p!=raw.size()) return 1;
        total_tokens+=oracle.token_count;
    }
    if(!valid(a) || !valid(b) || a.short_visits!=b.short_visits || a.middle_visits!=b.middle_visits
        || a.short_inserts!=b.short_inserts || a.middle_inserts!=b.middle_inserts
        || a.long_inserts!=b.fallback_inserts
        || b.fallback_visits!=b.fallback_fifth_rejects+b.fallback_prefix_rejects+b.fallback_matches) return 1;
    std::cout<<"input_bytes="<<input.size()<<"\ntokens="<<total_tokens<<"\ncounter_verified=1\nhand_verified=1\n";
    report("five_",a);report("six_",b);
    std::cout<<"six_fallback_visits="<<b.fallback_visits<<"\nsix_fallback_inserts="<<b.fallback_inserts<<'\n';
    std::cout<<"six_fallback_fifth_rejects="<<b.fallback_fifth_rejects
        <<"\nsix_fallback_prefix_rejects="<<b.fallback_prefix_rejects
        <<"\nsix_fallback_matches="<<b.fallback_matches<<'\n';
    for(auto i:{0U,3U,4U,5U,6U}) std::cout<<"token_length_"<<i<<"="<<lengths[i]<<'\n';
}

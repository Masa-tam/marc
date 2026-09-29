#ifndef MARC_BENCHMARK_POSITION_DISTANCE_1M_ONLINE_SIX_HPP
#define MARC_BENCHMARK_POSITION_DISTANCE_1M_ONLINE_SIX_HPP
#include "position_distance_1m_lazy_six.hpp"
namespace marc::benchmark {
struct OnlineDecisionEvent {
    std::uint64_t position{},width{},visits{},queries{},benefit{},cost{};
    bool selected{};
    bool operator==(const OnlineDecisionEvent&) const = default;
};
struct OnlineDecisionTrace {
    std::array<OnlineDecisionEvent,9> events{};
    std::size_t count{},activation{};
    bool ok{true};
    bool operator==(const OnlineDecisionTrace&) const = default;
};
class OnlineSixFinder {
    ObservedLazySixPrefixFinder finder_;
    std::size_t size_{},target_{},previous_position_{};
    LazySearchObservations previous_{};
    unsigned saving_{},scale_{};
    bool monitor_only_{};
    OnlineDecisionTrace trace_{};
public:
    explicit OnlineSixFinder(std::size_t capacity):finder_(capacity) {}
    bool reset(std::span<const std::byte> input,unsigned saving,unsigned scale,bool monitor_only=false) {
        if(saving<1 || saving>3 || (scale!=1 && scale!=4 && scale!=16)) return false;
        if(!finder_.reset(input)) return false;
        size_=input.size();target_=std::max<std::size_t>(65536,(size_+3)/4);
        previous_position_=0;previous_={};saving_=saving;scale_=scale;monitor_only_=monitor_only;
        trace_={};trace_.activation=size_;return true;
    }
    auto find_match(std::size_t p) const {return finder_.find_match(p);}
    bool advance(std::size_t p,std::size_t next) {
        if(!finder_.advance(p,next)) {trace_.ok=false;return false;}
        if(finder_.active() || target_>3*size_/4 || next<target_) return true;
        if(size_-next<std::max<std::size_t>(65536,(size_+7)/8)) {target_=size_+1;return true;}
        if(trace_.count>=trace_.events.size()) {trace_.ok=false;return false;}
        const auto now=finder_.observations();
        auto& event=trace_.events[trace_.count++];
        event.position=next;event.width=next-previous_position_;
        event.visits=now.long_visits-previous_.long_visits;event.queries=now.queries-previous_.queries;
        const std::uint64_t remaining=size_-next;
        // N<=2^20, V<=N*N, saving<=3, scale<=16: all products fit uint64.
        event.benefit=event.queries ? saving_*event.visits*remaining : 0;
        event.cost=4*event.width*scale_*(UINT64_C(65536)+size_+2*next+remaining);
        event.selected=!monitor_only_ && event.benefit>event.cost;
        previous_=now;previous_position_=next;target_+=65536;
        if(event.selected) {
            if(!finder_.activate(next)) {trace_.ok=false;return false;}
            trace_.activation=next;
        }
        return true;
    }
    const auto& trace() const {return trace_;}
};
}
#endif

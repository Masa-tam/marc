#ifndef MARC_BENCHMARK_POSITION_DISTANCE_1M_COUNTER_HPP
#define MARC_BENCHMARK_POSITION_DISTANCE_1M_COUNTER_HPP
#include "dictionary/lzss_position_distance_1m_match_finder.hpp"
#include <algorithm>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace marc::benchmark {
struct FinderCounts {
    std::uint64_t short_visits{}, long_visits{}, probe_rejects{}, prefix_rejects{};
    std::uint64_t extension_compares{}, extension_equal{}, short_inserts{}, long_inserts{};
};
// Untimed repository-derived replay, restricted to validated 1 MiB frames.
// Production tokens are the oracle; this class is never used by a codec.
class CounterFinder {
    static constexpr auto empty = std::numeric_limits<std::uint32_t>::max();
    std::span<const std::byte> input_;
    std::vector<std::uint32_t> heads_, long_heads_, links_, long_links_;
    FinderCounts& counts_;
    std::size_t bucket(std::size_t p, bool four = false) const {
        auto key = std::to_integer<std::uint32_t>(input_[p])
            | (std::to_integer<std::uint32_t>(input_[p+1]) << 8)
            | (std::to_integer<std::uint32_t>(input_[p+2]) << 16);
        if (four) key |= std::to_integer<std::uint32_t>(input_[p+3]) << 24;
        key ^= key >> 11;
        return (key * UINT32_C(2654435761)) >> 16;
    }
public:
    CounterFinder(std::span<const std::byte> input, FinderCounts& counts)
        : input_(input), heads_(65536, empty), long_heads_(65536, empty),
          links_(input.size(), empty), long_links_(input.size(), empty), counts_(counts) {}
    dictionary::internal::LzssMatch find(std::size_t p) {
        dictionary::internal::LzssMatch best{};
        if (input_.size()-p < 3) return best;
        const auto maximum = std::min<std::size_t>(258, input_.size()-p);
        auto nearest = empty;
        for (auto c=heads_[bucket(p)]; c!=empty; c=links_[c]) {
            ++counts_.short_visits;
            if (input_[p]==input_[c] && input_[p+1]==input_[c+1] && input_[p+2]==input_[c+2]) {
                best={static_cast<std::uint32_t>(p-c),3}; nearest=c; break;
            }
        }
        if (!best.length || maximum==3) return best;
        auto first=input_[p+3]==input_[nearest+3] ? nearest : long_heads_[bucket(p,true)];
        for (auto c=first; c!=empty; c=long_links_[c]) {
            ++counts_.long_visits;
            if (input_[p+best.length]!=input_[c+best.length]) {++counts_.probe_rejects;continue;}
            if (input_[p]!=input_[c] || input_[p+1]!=input_[c+1]
                || input_[p+2]!=input_[c+2] || input_[p+3]!=input_[c+3]) {++counts_.prefix_rejects;continue;}
            std::size_t length=4;
            while (length<maximum) {
                ++counts_.extension_compares;
                if (input_[p+length]!=input_[c+length]) break;
                ++counts_.extension_equal; ++length;
            }
            if (length>best.length) {
                best={static_cast<std::uint32_t>(p-c),static_cast<std::uint32_t>(length)};
                if (length==maximum) break;
            }
        }
        return best;
    }
    void advance(std::size_t p, std::size_t next) {
        for (;p<next;++p) {
            if (input_.size()-p<3) break;
            const auto b=bucket(p);links_[p]=heads_[b];heads_[b]=static_cast<std::uint32_t>(p);
            ++counts_.short_inserts;
            if (input_.size()-p>=4) {
                const auto l=bucket(p,true);long_links_[p]=long_heads_[l];long_heads_[l]=static_cast<std::uint32_t>(p);
                ++counts_.long_inserts;
            }
        }
    }
};
}
#endif

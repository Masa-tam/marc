#ifndef MARC_BENCHMARKS_LZSS_SHORT_MATCH_PROBE_HPP
#define MARC_BENCHMARKS_LZSS_SHORT_MATCH_PROBE_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace marc::benchmark::internal {

struct ShortMatchPrefixFlags {
    bool has_three{};
    bool has_four{};
    std::uint32_t nearest_three{};
    std::uint32_t nearest_four{};
};

// Private diagnostic only. A frame-local exact index for 3- and 4-byte
// prefixes; it does not find the longest match or alter encoded output.
class ShortMatchPrefixIndex {
public:
    static constexpr std::size_t frame_limit = 65'536;

    ShortMatchPrefixIndex() noexcept;
    [[nodiscard]] bool ready() const noexcept;
    [[nodiscard]] bool analyze(
        std::span<const std::byte> frame,
        std::span<ShortMatchPrefixFlags> output) noexcept;

private:
    struct Entry {
        std::uint64_t epoch{};
        std::uint32_t key{};
        std::uint32_t position{};
    };
    static constexpr std::size_t table_size = 131'072;
    static constexpr std::size_t table_mask = table_size - 1;

    [[nodiscard]] static std::size_t bucket(std::uint32_t key) noexcept;
    [[nodiscard]] bool find_and_update(
        Entry* table, std::uint32_t key, std::size_t position,
        std::uint32_t& nearest_distance) noexcept;

    std::unique_ptr<Entry[]> three_{};
    std::unique_ptr<Entry[]> four_{};
    std::uint64_t epoch_{};
};

} // namespace marc::benchmark::internal

#endif

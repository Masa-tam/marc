#include "lzss_short_match_probe.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <span>

namespace marc::benchmark::internal {
namespace {

[[nodiscard]] std::uint32_t prefix_key(
    const std::span<const std::byte> frame, const std::size_t position,
    const std::size_t length) noexcept {
    std::uint32_t key{};
    for (std::size_t offset = 0; offset < length; ++offset) {
        key |= std::to_integer<std::uint32_t>(frame[position + offset])
            << static_cast<unsigned>(8 * offset);
    }
    return key;
}

} // namespace

ShortMatchPrefixIndex::ShortMatchPrefixIndex() noexcept
    : three_(new (std::nothrow) Entry[table_size]{}),
      four_(new (std::nothrow) Entry[table_size]{}) {}

bool ShortMatchPrefixIndex::ready() const noexcept {
    return three_ != nullptr && four_ != nullptr;
}

std::size_t ShortMatchPrefixIndex::bucket(std::uint32_t key) noexcept {
    // Fold both 16-bit halves before distributing keys over the table.
    key ^= key >> 16U;
    return static_cast<std::size_t>((key * UINT32_C(2654435761)) >> 15U);
}

bool ShortMatchPrefixIndex::find_and_update(
    Entry* const table, const std::uint32_t key,
    const std::size_t position,
    std::uint32_t& nearest_distance) noexcept {
    const auto start = bucket(key);
    for (std::size_t attempt = 0; attempt < table_size; ++attempt) {
        auto& entry = table[(start + attempt) & table_mask];
        if (entry.epoch != epoch_) {
            entry = {epoch_, key, static_cast<std::uint32_t>(position)};
            nearest_distance = 0;
            return true;
        }
        if (entry.key == key) {
            nearest_distance = static_cast<std::uint32_t>(
                position - entry.position);
            entry.position = static_cast<std::uint32_t>(position);
            return true;
        }
    }
    return false;
}

bool ShortMatchPrefixIndex::analyze(
    const std::span<const std::byte> frame,
    const std::span<ShortMatchPrefixFlags> output) noexcept {
    if (!ready() || frame.size() > frame_limit
        || output.size() != frame.size()) {
        return false;
    }
    if (epoch_ == std::numeric_limits<std::uint64_t>::max()) {
        std::fill_n(three_.get(), table_size, Entry{});
        std::fill_n(four_.get(), table_size, Entry{});
        epoch_ = 0;
    }
    ++epoch_;
    std::fill(output.begin(), output.end(), ShortMatchPrefixFlags{});
    for (std::size_t position = 0; position < frame.size(); ++position) {
        if (frame.size() - position >= 3) {
            auto& flags = output[position];
            if (!find_and_update(
                    three_.get(), prefix_key(frame, position, 3),
                    position, flags.nearest_three)) {
                return false;
            }
            flags.has_three = flags.nearest_three != 0;
        }
        if (frame.size() - position >= 4) {
            auto& flags = output[position];
            if (!find_and_update(
                    four_.get(), prefix_key(frame, position, 4),
                    position, flags.nearest_four)) {
                return false;
            }
            flags.has_four = flags.nearest_four != 0;
        }
    }
    return true;
}

} // namespace marc::benchmark::internal

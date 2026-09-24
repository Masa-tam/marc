#ifndef MARC_DICTIONARY_LZSS_SHORT_PREFIX_MATCH_FINDER_HPP
#define MARC_DICTIONARY_LZSS_SHORT_PREFIX_MATCH_FINDER_HPP

#include "dictionary/lzss_match_finder.hpp"
#include "dictionary/lzss_typed_token.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

enum class LzssShortPrefixError : std::uint8_t {
    none,
    invalid_parameters,
    input_limit_exceeded,
    arithmetic_overflow,
    workspace_limit_exceeded,
    workspace_too_small,
    misaligned_workspace,
    overlapping_buffers,
};

struct LzssShortPrefixWorkspaceRequirements {
    std::size_t workspace_size{};
    std::size_t workspace_alignment{alignof(std::uint32_t)};
    LzssShortPrefixError error{LzssShortPrefixError::none};
};

[[nodiscard]] LzssShortPrefixWorkspaceRequirements
calculate_lzss_short_prefix_workspace(
    std::size_t input_size, const LzssParameters& parameters,
    const core::DecoderLimits& limits) noexcept;

// Private exact index for variant 7. Hash collisions are verified against
// the source bytes; a chain is traversed nearest-first. Caller owns storage.
class LzssShortPrefixMatchFinder {
public:
    [[nodiscard]] LzssMatch find_match(std::size_t position) const noexcept;
    void advance(std::size_t position, std::size_t next_position) noexcept;

private:
    friend LzssShortPrefixError initialize_lzss_short_prefix_match_finder(
        std::span<const std::byte>, const LzssParameters&,
        const core::DecoderLimits&, std::span<std::byte>,
        LzssShortPrefixMatchFinder&) noexcept;

    std::span<const std::byte> input_{};
    LzssParameters parameters_{};
    std::span<std::uint32_t> heads_{};
    std::span<std::uint32_t> links_{};
    std::size_t next_position_{};
};

static_assert(LzssMatchFinder<LzssShortPrefixMatchFinder>);

[[nodiscard]] LzssShortPrefixError initialize_lzss_short_prefix_match_finder(
    std::span<const std::byte> input, const LzssParameters& parameters,
    const core::DecoderLimits& limits, std::span<std::byte> workspace,
    LzssShortPrefixMatchFinder& finder) noexcept;

} // namespace marc::dictionary::internal

#endif

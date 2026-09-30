#ifndef MARC_DICTIONARY_LZSS_POSITION_DISTANCE_4M_MATCH_FINDER_HPP
#define MARC_DICTIONARY_LZSS_POSITION_DISTANCE_4M_MATCH_FINDER_HPP

#include "dictionary/lzss_short_prefix_match_finder.hpp"
#include "dictionary/lzss_typed_token.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

[[nodiscard]] LzssShortPrefixWorkspaceRequirements
calculate_lzss_position_distance_4m_match_workspace(
    std::size_t input_size, const LzssParameters& parameters,
    const core::DecoderLimits& limits) noexcept;

[[nodiscard]] LzssShortPrefixWorkspaceRequirements
calculate_lzss_position_distance_4m_match_workspace(
    std::size_t input_size, const LzssParameters& parameters,
    const core::DecoderLimits& limits,
    LzssTypedTokenVariant variant) noexcept;

// Private exact three-byte-prefix reference index for variant 10; positions/links are 32-bit. Collisions are verified against
// the source bytes; a chain is traversed nearest-first. Caller owns storage.
class LzssPositionDistance4mMatchFinder {
public:
    [[nodiscard]] LzssMatch find_match(std::size_t position) const noexcept;
    void advance(std::size_t position, std::size_t next_position) noexcept;

private:
    friend LzssShortPrefixError initialize_lzss_position_distance_4m_match_finder(
        std::span<const std::byte>, const LzssParameters&,
        const core::DecoderLimits&, std::span<std::byte>,
        LzssPositionDistance4mMatchFinder&, LzssTypedTokenVariant) noexcept;

    std::span<const std::byte> input_{};
    LzssParameters parameters_{};
    std::span<std::uint32_t> heads_{};
    std::span<std::uint32_t> links_{};
    std::size_t next_position_{};
};

static_assert(LzssMatchFinder<LzssPositionDistance4mMatchFinder>);

[[nodiscard]] LzssShortPrefixError initialize_lzss_position_distance_4m_match_finder(
    std::span<const std::byte> input, const LzssParameters& parameters,
    const core::DecoderLimits& limits, std::span<std::byte> workspace,
    LzssPositionDistance4mMatchFinder& finder) noexcept;

[[nodiscard]] LzssShortPrefixError initialize_lzss_position_distance_4m_match_finder(
    std::span<const std::byte> input, const LzssParameters& parameters,
    const core::DecoderLimits& limits, std::span<std::byte> workspace,
    LzssPositionDistance4mMatchFinder& finder,
    LzssTypedTokenVariant variant) noexcept;

} // namespace marc::dictionary::internal

#endif

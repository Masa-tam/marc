#ifndef MARC_DICTIONARY_LZSS_POSITION_DISTANCE_4M_WORD4_FINDER_HPP
#define MARC_DICTIONARY_LZSS_POSITION_DISTANCE_4M_WORD4_FINDER_HPP

#include "dictionary/lzss_short_prefix_match_finder.hpp"
#include "dictionary/lzss_typed_token.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

[[nodiscard]] LzssShortPrefixWorkspaceRequirements
calculate_lzss_position_distance_4m_word4_workspace(
    std::size_t input_size, const LzssParameters& parameters,
    const core::DecoderLimits& limits) noexcept;

[[nodiscard]] LzssShortPrefixWorkspaceRequirements
calculate_lzss_position_distance_4m_word4_workspace(
    std::size_t input_size, const LzssParameters& parameters,
    const core::DecoderLimits& limits,
    LzssTypedTokenVariant variant) noexcept;

// Private candidate only; no public encoder selects it. Caller-owned arrays.
// Query charges input + active arrays + finder state. Initialize failure preserves
// finder and workspace; only the queried prefix is borrowed on success.
class LzssPositionDistance4mWord4Finder {
public:
    [[nodiscard]] LzssMatch find_match(std::size_t position) const noexcept;
    void advance(std::size_t position, std::size_t next_position) noexcept;

private:
    friend LzssShortPrefixError initialize_lzss_position_distance_4m_word4_finder(
        std::span<const std::byte>, const LzssParameters&,
        const core::DecoderLimits&, std::span<std::byte>,
        LzssPositionDistance4mWord4Finder&, LzssTypedTokenVariant) noexcept;

    std::span<const std::byte> input_{};
    LzssParameters parameters_{};
    std::array<std::span<std::uint32_t>,3> heads_{}, links_{};
    std::size_t next_position_{};
};

static_assert(LzssMatchFinder<LzssPositionDistance4mWord4Finder>);

[[nodiscard]] LzssShortPrefixError initialize_lzss_position_distance_4m_word4_finder(
    std::span<const std::byte> input, const LzssParameters& parameters,
    const core::DecoderLimits& limits, std::span<std::byte> workspace,
    LzssPositionDistance4mWord4Finder& finder) noexcept;

[[nodiscard]] LzssShortPrefixError initialize_lzss_position_distance_4m_word4_finder(
    std::span<const std::byte> input, const LzssParameters& parameters,
    const core::DecoderLimits& limits, std::span<std::byte> workspace,
    LzssPositionDistance4mWord4Finder& finder,
    LzssTypedTokenVariant variant) noexcept;

} // namespace marc::dictionary::internal

#endif

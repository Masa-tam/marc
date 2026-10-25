#ifndef MARC_DICTIONARY_LZSS_HASH_CHAIN_MATCH_FINDER_HPP
#define MARC_DICTIONARY_LZSS_HASH_CHAIN_MATCH_FINDER_HPP

#include "core/limits.hpp"
#include "dictionary/lzss_match_finder.hpp"
#include "dictionary/lzss_prefix_hash.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

enum class LzssHashChainError : std::uint8_t {
    none,
    invalid_limits,
    invalid_parameters,
    input_limit_exceeded,
    arithmetic_overflow,
    workspace_limit_exceeded,
    workspace_too_small,
    misaligned_workspace,
    overlapping_buffers,
    invalid_bucket_cap,
};

struct LzssHashChainWorkspaceRequirements {
    std::size_t workspace_size{};
    std::size_t workspace_alignment{
        alignof(std::size_t) > alignof(std::uint32_t)
            ? alignof(std::size_t) : alignof(std::uint32_t)};
    std::size_t bucket_count{};
    std::size_t link_count{};
    std::size_t link_offset{};
    LzssFormatError format_error{LzssFormatError::none};
    LzssHashChainError error{LzssHashChainError::none};
};

[[nodiscard]] LzssHashChainWorkspaceRequirements
calculate_lzss_hash_chain_workspace(
    std::size_t input_size, const LzssParameters& parameters,
    const core::DecoderLimits& limits) noexcept;

inline constexpr std::size_t lzss_hash_chain_production_bucket_cap = 262'144;
inline constexpr bool lzss_hash_chain_production_uses_best_length_probe = true;

class LzssHashChainBestLengthProbeMatchFinder;
class LzssHashChainNoProbeMatchFinder;

class LzssHashChainMatchFinder {
public:
    LzssHashChainMatchFinder() noexcept = default;

    [[nodiscard]] LzssMatch find_match(
        std::size_t position) const noexcept;
    void advance(std::size_t position, std::size_t next_position) noexcept;

private:
    [[nodiscard]] LzssMatch find_match_no_probe(std::size_t position) const noexcept;
    friend class LzssHashChainBestLengthProbeMatchFinder;
    friend class LzssHashChainNoProbeMatchFinder;
    friend LzssHashChainError initialize_lzss_hash_chain_match_finder(
        std::span<const std::byte>, const LzssParameters&,
        const core::DecoderLimits&, std::span<std::byte>,
        LzssHashChainMatchFinder&, LzssMatchFinderStatistics*) noexcept;
    template <auto CalculatePrefixHash, bool UseBestLengthProbe>
    [[nodiscard]] LzssMatch find_match_with(
        std::size_t position) const noexcept;
    template <auto CalculatePrefixHash>
    void advance_with(
        std::size_t position, std::size_t next_position) noexcept;

    std::span<const std::byte> input_{};
    LzssParameters parameters_{};
    std::span<std::size_t> heads_{};
    std::span<std::uint32_t> links_{};
    std::size_t next_position_{};
    LzssMatchFinderStatistics* statistics_{};
};

static_assert(LzssMatchFinder<LzssHashChainMatchFinder>);

// Explicit historical control; independent of the production query policy.
class LzssHashChainNoProbeMatchFinder {
public:
    [[nodiscard]] LzssMatch find_match(std::size_t position) const noexcept;
    void advance(std::size_t position, std::size_t next_position) noexcept;

private:
    friend LzssHashChainError initialize_lzss_hash_chain_no_probe_match_finder(
        std::span<const std::byte>, const LzssParameters&,
        const core::DecoderLimits&, std::span<std::byte>,
        LzssHashChainNoProbeMatchFinder&, LzssMatchFinderStatistics*) noexcept;
    LzssHashChainMatchFinder implementation_{};
};

static_assert(LzssMatchFinder<LzssHashChainNoProbeMatchFinder>);

[[nodiscard]] LzssHashChainError initialize_lzss_hash_chain_no_probe_match_finder(
    std::span<const std::byte> input, const LzssParameters& parameters,
    const core::DecoderLimits& limits, std::span<std::byte> workspace,
    LzssHashChainNoProbeMatchFinder& finder,
    LzssMatchFinderStatistics* statistics = nullptr) noexcept;

class LzssHashChainBestLengthProbeMatchFinder {
public:
    LzssHashChainBestLengthProbeMatchFinder() noexcept = default;

    [[nodiscard]] LzssMatch find_match(
        std::size_t position) const noexcept;
    void advance(std::size_t position, std::size_t next_position) noexcept;

private:
    friend LzssHashChainError
    initialize_lzss_hash_chain_best_length_probe_match_finder(
        std::span<const std::byte>, const LzssParameters&,
        const core::DecoderLimits&, std::span<std::byte>,
        LzssHashChainBestLengthProbeMatchFinder&,
        LzssMatchFinderStatistics*) noexcept;

    LzssHashChainMatchFinder implementation_{};
};

static_assert(LzssMatchFinder<LzssHashChainBestLengthProbeMatchFinder>);

[[nodiscard]] LzssHashChainError
initialize_lzss_hash_chain_best_length_probe_match_finder(
    std::span<const std::byte> input, const LzssParameters& parameters,
    const core::DecoderLimits& limits, std::span<std::byte> workspace,
    LzssHashChainBestLengthProbeMatchFinder& finder,
    LzssMatchFinderStatistics* statistics = nullptr) noexcept;

[[nodiscard]] LzssHashChainError initialize_lzss_hash_chain_match_finder(
    std::span<const std::byte> input, const LzssParameters& parameters,
    const core::DecoderLimits& limits, std::span<std::byte> workspace,
    LzssHashChainMatchFinder& finder,
    LzssMatchFinderStatistics* statistics = nullptr) noexcept;

} // namespace marc::dictionary::internal

#endif

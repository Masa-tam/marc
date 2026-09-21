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

inline constexpr std::size_t lzss_hash_chain_legacy_bucket_cap = 65'536;
inline constexpr std::size_t lzss_hash_chain_production_bucket_cap = 262'144;
inline constexpr std::size_t lzss_hash_chain_bucket_cap_262144 =
    lzss_hash_chain_production_bucket_cap;
inline constexpr std::size_t lzss_hash_chain_bucket_cap_1048576 = 1'048'576;
inline constexpr std::size_t lzss_hash_chain_bucket_cap_4194304 = 4'194'304;

static_assert(
    lzss_match_finder_max_bucket_count
    == lzss_hash_chain_legacy_bucket_cap);

[[nodiscard]] bool is_supported_lzss_hash_chain_private_bucket_cap(
    std::size_t bucket_cap) noexcept;

[[nodiscard]] LzssHashChainWorkspaceRequirements
calculate_lzss_hash_chain_workspace_with_private_bucket_cap(
    std::size_t input_size, const LzssParameters& parameters,
    const core::DecoderLimits& limits, std::size_t bucket_cap) noexcept;

class LzssHashChainMnemonicMixerV1MatchFinder;

class LzssHashChainMatchFinder {
public:
    LzssHashChainMatchFinder() noexcept = default;

    [[nodiscard]] LzssMatch find_match(
        std::size_t position) const noexcept;
    void advance(std::size_t position, std::size_t next_position) noexcept;

private:
    friend class LzssHashChainMnemonicMixerV1MatchFinder;
    friend LzssHashChainError initialize_lzss_hash_chain_match_finder(
        std::span<const std::byte>, const LzssParameters&,
        const core::DecoderLimits&, std::span<std::byte>,
        LzssHashChainMatchFinder&, LzssMatchFinderStatistics*) noexcept;
    friend LzssHashChainError
    initialize_lzss_hash_chain_match_finder_with_private_bucket_cap(
        std::span<const std::byte>, const LzssParameters&,
        const core::DecoderLimits&, std::span<std::byte>, std::size_t,
        LzssHashChainMatchFinder&, LzssMatchFinderStatistics*) noexcept;

    template <auto CalculatePrefixHash>
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

[[nodiscard]] LzssHashChainError initialize_lzss_hash_chain_match_finder(
    std::span<const std::byte> input, const LzssParameters& parameters,
    const core::DecoderLimits& limits, std::span<std::byte> workspace,
    LzssHashChainMatchFinder& finder,
    LzssMatchFinderStatistics* statistics = nullptr) noexcept;

[[nodiscard]] LzssHashChainError
initialize_lzss_hash_chain_match_finder_with_private_bucket_cap(
    std::span<const std::byte> input, const LzssParameters& parameters,
    const core::DecoderLimits& limits, std::span<std::byte> workspace,
    std::size_t bucket_cap, LzssHashChainMatchFinder& finder,
    LzssMatchFinderStatistics* statistics = nullptr) noexcept;

template <std::size_t BucketCap>
class LzssHashChainBucketScaledMatchFinder {
    static_assert(
        BucketCap == lzss_hash_chain_legacy_bucket_cap
        || BucketCap == lzss_hash_chain_bucket_cap_262144
        || BucketCap == lzss_hash_chain_bucket_cap_1048576
        || BucketCap == lzss_hash_chain_bucket_cap_4194304);

public:
    LzssHashChainBucketScaledMatchFinder() noexcept = default;

    [[nodiscard]] LzssMatch find_match(
        const std::size_t position) const noexcept {
        return implementation_.find_match(position);
    }

    void advance(
        const std::size_t position,
        const std::size_t next_position) noexcept {
        implementation_.advance(position, next_position);
    }

    [[nodiscard]] LzssHashChainError initialize(
        const std::span<const std::byte> input,
        const LzssParameters& parameters,
        const core::DecoderLimits& limits,
        const std::span<std::byte> workspace,
        LzssMatchFinderStatistics* const statistics) noexcept {
        LzssHashChainMatchFinder implementation{};
        const auto error =
            initialize_lzss_hash_chain_match_finder_with_private_bucket_cap(
                input, parameters, limits, workspace, BucketCap,
                implementation, statistics);
        if (error != LzssHashChainError::none) return error;
        implementation_ = implementation;
        return LzssHashChainError::none;
    }

private:
    LzssHashChainMatchFinder implementation_{};
};

using LzssHashChainBuckets65536MatchFinder =
    LzssHashChainBucketScaledMatchFinder<
        lzss_hash_chain_legacy_bucket_cap>;
using LzssHashChainBuckets262144MatchFinder =
    LzssHashChainBucketScaledMatchFinder<
        lzss_hash_chain_bucket_cap_262144>;
using LzssHashChainBuckets1048576MatchFinder =
    LzssHashChainBucketScaledMatchFinder<
        lzss_hash_chain_bucket_cap_1048576>;
using LzssHashChainBuckets4194304MatchFinder =
    LzssHashChainBucketScaledMatchFinder<
        lzss_hash_chain_bucket_cap_4194304>;

static_assert(LzssMatchFinder<LzssHashChainBuckets65536MatchFinder>);
static_assert(LzssMatchFinder<LzssHashChainBuckets262144MatchFinder>);
static_assert(LzssMatchFinder<LzssHashChainBuckets1048576MatchFinder>);
static_assert(LzssMatchFinder<LzssHashChainBuckets4194304MatchFinder>);

template <std::size_t BucketCap>
[[nodiscard]] LzssHashChainError
initialize_lzss_hash_chain_bucket_scaled_match_finder(
    const std::span<const std::byte> input,
    const LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::span<std::byte> workspace,
    LzssHashChainBucketScaledMatchFinder<BucketCap>& finder,
    LzssMatchFinderStatistics* const statistics = nullptr) noexcept {
    LzssHashChainBucketScaledMatchFinder<BucketCap> initialized{};
    const auto error = initialized.initialize(
        input, parameters, limits, workspace, statistics);
    if (error != LzssHashChainError::none) return error;
    finder = initialized;
    return LzssHashChainError::none;
}

class LzssHashChainMnemonicMixerV1MatchFinder {
public:
    LzssHashChainMnemonicMixerV1MatchFinder() noexcept = default;

    [[nodiscard]] LzssMatch find_match(
        std::size_t position) const noexcept;
    void advance(std::size_t position, std::size_t next_position) noexcept;

private:
    friend LzssHashChainError
    initialize_lzss_hash_chain_mnemonic_mixer_v1_match_finder(
        std::span<const std::byte>, const LzssParameters&,
        const core::DecoderLimits&, std::span<std::byte>,
        LzssHashChainMnemonicMixerV1MatchFinder&,
        LzssMatchFinderStatistics*) noexcept;

    LzssHashChainMatchFinder implementation_{};
};

static_assert(LzssMatchFinder<LzssHashChainMnemonicMixerV1MatchFinder>);

[[nodiscard]] LzssHashChainError
initialize_lzss_hash_chain_mnemonic_mixer_v1_match_finder(
    std::span<const std::byte> input, const LzssParameters& parameters,
    const core::DecoderLimits& limits, std::span<std::byte> workspace,
    LzssHashChainMnemonicMixerV1MatchFinder& finder,
    LzssMatchFinderStatistics* statistics = nullptr) noexcept;

} // namespace marc::dictionary::internal

#endif

#ifndef MARC_DICTIONARY_LZSS_HASH_TREE_MATCH_FINDER_HPP
#define MARC_DICTIONARY_LZSS_HASH_TREE_MATCH_FINDER_HPP

#include "core/limits.hpp"
#include "dictionary/lzss_match_finder.hpp"
#include "dictionary/lzss_prefix_hash.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace marc::dictionary::internal {

inline constexpr std::uint32_t lzss_hash_tree_null_node = UINT32_MAX;
inline constexpr std::size_t lzss_hash_tree_no_position =
    std::numeric_limits<std::size_t>::max();

enum class LzssHashTreeError : std::uint8_t {
    none,
    invalid_limits,
    invalid_parameters,
    input_limit_exceeded,
    arithmetic_overflow,
    workspace_limit_exceeded,
    workspace_too_small,
    misaligned_workspace,
    overlapping_buffers,
};

enum class LzssHashTreeBucketMode : std::uint8_t {
    chain = 0,
    promoted_tree = 1,
};

struct LzssHashTreeWorkspaceRequirements {
    std::size_t workspace_size{};
    std::size_t workspace_alignment{
        alignof(std::size_t) > alignof(std::uint32_t)
            ? alignof(std::size_t) : alignof(std::uint32_t)};
    std::size_t bucket_count{};
    std::size_t node_count{};
    std::size_t head_offset{};
    std::size_t link_offset{};
    std::size_t root_offset{};
    std::size_t mode_offset{};
    std::size_t left_offset{};
    std::size_t right_offset{};
    std::size_t parent_offset{};
    std::size_t height_offset{};
    std::size_t position_offset{};
    std::size_t subtree_maximum_position_offset{};
    LzssFormatError format_error{LzssFormatError::none};
    LzssHashTreeError error{LzssHashTreeError::none};
};

[[nodiscard]] LzssHashTreeWorkspaceRequirements
calculate_lzss_hash_tree_workspace(
    std::size_t input_size, const LzssParameters& parameters,
    const core::DecoderLimits& limits) noexcept;

class LzssHashTreeMatchFinder {
public:
    LzssHashTreeMatchFinder() noexcept = default;

    [[nodiscard]] bool initialized() const noexcept { return initialized_; }
    [[nodiscard]] bool state_valid() const noexcept { return state_valid_; }
    [[nodiscard]] std::size_t input_size() const noexcept {
        return input_.size();
    }
    [[nodiscard]] std::size_t bucket_count() const noexcept {
        return heads_.size();
    }
    [[nodiscard]] std::size_t node_capacity() const noexcept {
        return links_.size();
    }
    [[nodiscard]] std::size_t next_position() const noexcept {
        return next_position_;
    }

private:
    friend LzssHashTreeError initialize_lzss_hash_tree_match_finder(
        std::span<const std::byte>, const LzssParameters&,
        const core::DecoderLimits&, std::span<std::byte>,
        LzssHashTreeMatchFinder&, LzssMatchFinderStatistics*) noexcept;

    std::span<const std::byte> input_{};
    LzssParameters parameters_{};
    std::span<std::size_t> heads_{};
    std::span<std::uint32_t> links_{};
    std::span<std::uint32_t> roots_{};
    std::span<LzssHashTreeBucketMode> modes_{};
    std::span<std::uint32_t> left_{};
    std::span<std::uint32_t> right_{};
    std::span<std::uint32_t> parent_{};
    std::span<std::uint8_t> height_{};
    std::span<std::size_t> position_{};
    std::span<std::size_t> subtree_maximum_position_{};
    std::size_t next_position_{};
    LzssMatchFinderStatistics* statistics_{};
    bool initialized_{};
    bool state_valid_{};
};

[[nodiscard]] LzssHashTreeError initialize_lzss_hash_tree_match_finder(
    std::span<const std::byte> input, const LzssParameters& parameters,
    const core::DecoderLimits& limits, std::span<std::byte> workspace,
    LzssHashTreeMatchFinder& finder,
    LzssMatchFinderStatistics* statistics = nullptr) noexcept;

} // namespace marc::dictionary::internal

#endif

#ifndef MARC_DICTIONARY_LZSS_HASH_TREE_MATCH_FINDER_HPP
#define MARC_DICTIONARY_LZSS_HASH_TREE_MATCH_FINDER_HPP

#include "core/limits.hpp"
#include "dictionary/lzss_format.hpp"
#include "dictionary/lzss_prefix_hash.hpp"

#include <cstddef>
#include <cstdint>

namespace marc::dictionary::internal {

enum class LzssHashTreeError : std::uint8_t {
    none,
    invalid_limits,
    invalid_parameters,
    input_limit_exceeded,
    arithmetic_overflow,
    workspace_limit_exceeded,
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

} // namespace marc::dictionary::internal

#endif

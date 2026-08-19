#ifndef MARC_DICTIONARY_LZSS_SPARSE_HASH_TREE_POOL_HPP
#define MARC_DICTIONARY_LZSS_SPARSE_HASH_TREE_POOL_HPP

#include "core/limits.hpp"
#include "dictionary/lzss_hash_tree_match_finder.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

enum class LzssSparseHashTreeError : std::uint8_t {
    none,
    invalid_limits,
    invalid_parameters,
    input_limit_exceeded,
    invalid_pool_capacity,
    arithmetic_overflow,
    workspace_limit_exceeded,
    workspace_too_small,
    misaligned_workspace,
    invalid_state,
    invalid_node,
    double_release,
};

enum class LzssSparseHashTreeBucketMode : std::uint8_t {
    chain = 0,
    promoted_tree = 1,
    pool_rejected_chain = 2,
};

struct LzssSparseHashTreeWorkspaceRequirements {
    std::size_t workspace_size{};
    std::size_t workspace_alignment{alignof(std::uint32_t)};
    std::size_t bucket_count{};
    std::size_t chain_node_count{};
    std::size_t pool_node_capacity{};
    std::size_t head_offset{};
    std::size_t link_offset{};
    std::size_t root_offset{};
    std::size_t mode_offset{};
    std::size_t bucket_node_count_offset{};
    std::size_t left_offset{};
    std::size_t right_offset{};
    std::size_t parent_offset{};
    std::size_t height_offset{};
    std::size_t position_offset{};
    std::size_t subtree_maximum_position_offset{};
    LzssFormatError format_error{LzssFormatError::none};
    LzssSparseHashTreeError error{LzssSparseHashTreeError::none};
};

[[nodiscard]] LzssSparseHashTreeWorkspaceRequirements
calculate_lzss_sparse_hash_tree_workspace(
    std::size_t input_size, const LzssParameters& parameters,
    const core::DecoderLimits& limits,
    std::size_t pool_node_capacity) noexcept;

struct LzssSparseHashTreeNodeAllocation {
    std::uint32_t node{lzss_hash_tree_null_node};
    bool allocated{};
    LzssSparseHashTreeError error{LzssSparseHashTreeError::none};
};

struct LzssSparseHashTreeNodeArrays {
    std::span<std::uint32_t> left{};
    std::span<std::uint32_t> right{};
    std::span<std::uint32_t> parent{};
    std::span<std::uint8_t> height{};
    std::span<LzssHashTreeStoredPosition> position{};
    std::span<LzssHashTreeStoredPosition> subtree_maximum_position{};
};

class LzssSparseHashTreeNodePool {
public:
    LzssSparseHashTreeNodePool() noexcept = default;

    [[nodiscard]] bool initialized() const noexcept { return initialized_; }
    [[nodiscard]] bool state_valid() const noexcept { return state_valid_; }
    [[nodiscard]] std::size_t capacity() const noexcept {
        return left_.size();
    }
    [[nodiscard]] std::size_t free_count() const noexcept {
        return free_count_;
    }
    [[nodiscard]] std::size_t active_count() const noexcept {
        return active_count_;
    }
    [[nodiscard]] LzssSparseHashTreeError last_error() const noexcept {
        return last_error_;
    }
    [[nodiscard]] LzssSparseHashTreeNodeArrays node_arrays() noexcept {
        return {left_, right_, parent_, height_, position_,
                subtree_maximum_position_};
    }

    [[nodiscard]] LzssSparseHashTreeNodeAllocation allocate() noexcept;
    [[nodiscard]] LzssSparseHashTreeError release(
        std::uint32_t node) noexcept;

private:
    friend class LzssSparseHashTreeWorkspace;
    friend LzssSparseHashTreeError
    initialize_lzss_sparse_hash_tree_node_pool(
        std::size_t, const LzssParameters&, const core::DecoderLimits&,
        std::size_t, std::span<std::byte>,
        LzssSparseHashTreeNodePool&) noexcept;

    void mark_error(LzssSparseHashTreeError error) noexcept;
    void reset_valid_storage() noexcept;

    std::span<std::uint32_t> left_{};
    std::span<std::uint32_t> right_{};
    std::span<std::uint32_t> parent_{};
    std::span<std::uint8_t> height_{};
    std::span<LzssHashTreeStoredPosition> position_{};
    std::span<LzssHashTreeStoredPosition> subtree_maximum_position_{};
    std::uint32_t free_head_{lzss_hash_tree_null_node};
    std::size_t free_count_{};
    std::size_t active_count_{};
    LzssSparseHashTreeError last_error_{LzssSparseHashTreeError::none};
    bool initialized_{};
    bool state_valid_{};
};

class LzssSparseHashTreeWorkspace {
public:
    LzssSparseHashTreeWorkspace() noexcept = default;

    [[nodiscard]] bool initialized() const noexcept { return initialized_; }
    [[nodiscard]] std::span<LzssHashTreeStoredPosition> heads() noexcept {
        return heads_;
    }
    [[nodiscard]] std::span<const LzssHashTreeStoredPosition>
    heads() const noexcept { return heads_; }
    [[nodiscard]] std::span<std::uint32_t> links() noexcept { return links_; }
    [[nodiscard]] std::span<const std::uint32_t> links() const noexcept {
        return links_;
    }
    [[nodiscard]] std::span<std::uint32_t> roots() noexcept { return roots_; }
    [[nodiscard]] std::span<const std::uint32_t> roots() const noexcept {
        return roots_;
    }
    [[nodiscard]] std::span<LzssSparseHashTreeBucketMode> modes() noexcept {
        return modes_;
    }
    [[nodiscard]] std::span<const LzssSparseHashTreeBucketMode>
    modes() const noexcept { return modes_; }
    [[nodiscard]] std::span<std::uint32_t> bucket_node_counts() noexcept {
        return bucket_node_counts_;
    }
    [[nodiscard]] std::span<const std::uint32_t>
    bucket_node_counts() const noexcept { return bucket_node_counts_; }
    [[nodiscard]] LzssSparseHashTreeNodePool& node_pool() noexcept {
        return node_pool_;
    }
    [[nodiscard]] const LzssSparseHashTreeNodePool& node_pool() const noexcept {
        return node_pool_;
    }

    [[nodiscard]] LzssSparseHashTreeError reset_frame() noexcept;

private:
    friend LzssSparseHashTreeError initialize_lzss_sparse_hash_tree_workspace(
        std::size_t, const LzssParameters&, const core::DecoderLimits&,
        std::size_t, std::span<std::byte>,
        LzssSparseHashTreeWorkspace&) noexcept;

    std::span<LzssHashTreeStoredPosition> heads_{};
    std::span<std::uint32_t> links_{};
    std::span<std::uint32_t> roots_{};
    std::span<LzssSparseHashTreeBucketMode> modes_{};
    std::span<std::uint32_t> bucket_node_counts_{};
    LzssSparseHashTreeNodePool node_pool_{};
    bool initialized_{};
};

[[nodiscard]] LzssSparseHashTreeError
initialize_lzss_sparse_hash_tree_node_pool(
    std::size_t input_size, const LzssParameters& parameters,
    const core::DecoderLimits& limits, std::size_t pool_node_capacity,
    std::span<std::byte> workspace,
    LzssSparseHashTreeNodePool& pool) noexcept;

[[nodiscard]] LzssSparseHashTreeError
initialize_lzss_sparse_hash_tree_workspace(
    std::size_t input_size, const LzssParameters& parameters,
    const core::DecoderLimits& limits, std::size_t pool_node_capacity,
    std::span<std::byte> storage,
    LzssSparseHashTreeWorkspace& workspace) noexcept;

} // namespace marc::dictionary::internal

#endif

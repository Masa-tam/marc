#include "dictionary/lzss_hash_tree_match_finder.hpp"

#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"
#include "dictionary/lzss_hash_tree_bucket_builder.hpp"
#include "dictionary/lzss_hash_tree_bucket_mutation.hpp"
#include "dictionary/lzss_hash_tree_bucket_query.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>

namespace marc::dictionary::internal {
namespace {

[[nodiscard]] bool append_array(
    const std::size_t count, const std::size_t element_size,
    const std::size_t alignment, std::size_t& cursor,
    std::size_t& offset) noexcept {
    const auto remainder = cursor % alignment;
    const auto padding = remainder == 0 ? 0 : alignment - remainder;
    std::size_t bytes{};
    return core::checked_add(cursor, padding, offset)
        && core::checked_multiply(count, element_size, bytes)
        && core::checked_add(offset, bytes, cursor);
}

template<typename T>
[[nodiscard]] std::span<T> array_at(
    const std::span<std::byte> workspace, const std::size_t offset,
    const std::size_t count) noexcept {
    return {reinterpret_cast<T*>(workspace.data() + offset), count};
}

template<typename T>
void construct_array(const std::span<T> values, const T initial) noexcept {
    for (std::size_t index = 0; index < values.size(); ++index) {
        std::construct_at(values.data() + index, initial);
    }
}

void increment_statistic(
    LzssMatchFinderStatistics* const statistics,
    std::uint64_t& value) noexcept {
    if (statistics == nullptr) return;
    if (value == std::numeric_limits<std::uint64_t>::max()) {
        statistics->overflowed = true;
        return;
    }
    ++value;
}

void add_statistic(
    LzssMatchFinderStatistics* const statistics,
    std::uint64_t& value, const std::uint64_t increment) noexcept {
    if (statistics == nullptr || increment == 0) return;
    if (value > std::numeric_limits<std::uint64_t>::max() - increment) {
        value = std::numeric_limits<std::uint64_t>::max();
        statistics->overflowed = true;
        return;
    }
    value += increment;
}

void aggregate_builder_statistics(
    LzssMatchFinderStatistics* const statistics,
    const LzssHashTreeComponentStatistics& component) noexcept {
    if (statistics == nullptr) return;
    add_statistic(statistics,
        statistics->hash_tree_promotion_build_key_comparison_count,
        component.key_comparison_count);
    add_statistic(statistics,
        statistics->hash_tree_promotion_build_key_byte_comparison_count,
        component.key_byte_comparison_count);
    add_statistic(statistics,
        statistics->hash_tree_promotion_build_rotation_count,
        component.rotation_count);
    statistics->hash_tree_maximum_height = std::max(
        statistics->hash_tree_maximum_height, component.maximum_height);
    statistics->overflowed = statistics->overflowed || component.overflowed;
}

void aggregate_query_statistics(
    LzssMatchFinderStatistics* const statistics,
    const LzssHashTreeComponentStatistics& component) noexcept {
    if (statistics == nullptr) return;
    add_statistic(statistics,
        statistics->hash_tree_tree_query_key_comparison_count,
        component.key_comparison_count);
    add_statistic(statistics,
        statistics->hash_tree_tree_query_key_byte_comparison_count,
        component.key_byte_comparison_count);
    add_statistic(statistics,
        statistics->hash_tree_tree_query_lcp_byte_comparison_count,
        component.lcp_byte_comparison_count);
    add_statistic(statistics,
        statistics->hash_tree_tree_query_prefix_range_comparison_count,
        component.prefix_range_comparison_count);
    add_statistic(statistics,
        statistics->hash_tree_tree_query_prefix_range_byte_comparison_count,
        component.prefix_range_byte_comparison_count);
    add_statistic(statistics,
        statistics->hash_tree_tree_query_lcp_skipped_byte_count,
        component.lcp_skipped_byte_count);
    statistics->overflowed = statistics->overflowed || component.overflowed;
}

void aggregate_mutation_statistics(
    LzssMatchFinderStatistics* const statistics,
    const LzssHashTreeComponentStatistics& component) noexcept {
    if (statistics == nullptr) return;
    add_statistic(statistics,
        statistics->hash_tree_maintenance_key_comparison_count,
        component.key_comparison_count);
    add_statistic(statistics,
        statistics->hash_tree_maintenance_key_byte_comparison_count,
        component.key_byte_comparison_count);
    add_statistic(statistics, statistics->hash_tree_rotation_count,
        component.rotation_count);
    statistics->hash_tree_maximum_height = std::max(
        statistics->hash_tree_maximum_height, component.maximum_height);
    statistics->overflowed = statistics->overflowed || component.overflowed;
}

void record_hash_tree_depth(
    LzssMatchFinderStatistics* const statistics,
    std::array<std::uint64_t,
               lzss_match_finder_depth_histogram_size>& histogram,
    const std::uint64_t work_items) noexcept {
    if (statistics == nullptr) return;
    const auto raw_bin = work_items == 0 ? 0U : std::bit_width(work_items);
    const auto bin = std::min<std::size_t>(
        raw_bin, histogram.size() - 1U);
    increment_statistic(statistics, histogram[bin]);
}

void record_hash_tree_population(
    LzssMatchFinderStatistics* const statistics,
    const std::size_t promoted_buckets,
    const std::size_t promoted_nodes) noexcept {
    if (statistics == nullptr) return;
    statistics->hash_tree_maximum_promoted_buckets = std::max(
        statistics->hash_tree_maximum_promoted_buckets,
        static_cast<std::uint64_t>(promoted_buckets));
    statistics->hash_tree_maximum_promoted_nodes = std::max(
        statistics->hash_tree_maximum_promoted_nodes,
        static_cast<std::uint64_t>(promoted_nodes));
}

void record_chain_query_depth(
    LzssMatchFinderStatistics* const statistics,
    const std::uint64_t candidate_count) noexcept {
    if (statistics == nullptr) return;
    statistics->hash_chain_maximum_candidates_per_query = std::max(
        statistics->hash_chain_maximum_candidates_per_query,
        candidate_count);
    const auto raw_bin = candidate_count == 0 ? 0U
        : std::bit_width(candidate_count);
    const auto bin = std::min<std::size_t>(
        raw_bin, statistics->hash_chain_query_depth_histogram.size() - 1U);
    increment_statistic(
        statistics, statistics->hash_chain_query_depth_histogram[bin]);
}

} // namespace

void LzssHashTreeMatchFinder::mark_error(
    const LzssHashTreeError error) noexcept {
    if (last_error_ == LzssHashTreeError::none) last_error_ = error;
    state_valid_ = false;
    next_position_ = input_.size();
}

LzssMatch LzssHashTreeMatchFinder::find_match(
    const std::size_t position) noexcept {
    LzssMatch best{};
    if (!initialized_) {
        mark_error(LzssHashTreeError::invalid_state);
        return best;
    }
    if (!state_valid_) return best;
    if (position != next_position_ || position > input_.size()) {
        mark_error(LzssHashTreeError::invalid_protocol);
        return best;
    }
    if (position == input_.size()) return best;
    if (statistics_ != nullptr) {
        increment_statistic(statistics_, statistics_->query_count);
    }
    if (heads_.empty()
        || input_.size() - position < lzss_match_finder_prefix_size) {
        record_chain_query_depth(statistics_, 0);
        if (hash_tree_diagnostics_enabled_ && statistics_ != nullptr) {
            increment_statistic(
                statistics_, statistics_->hash_tree_chain_query_count);
            record_hash_tree_depth(
                statistics_,
                statistics_->hash_tree_chain_query_depth_histogram, 0);
        }
        return best;
    }

    const auto prefix_hash = calculate_lzss_prefix_hash(input_, position);
    if (!prefix_hash.valid) {
        mark_error(LzssHashTreeError::invalid_state);
        return {};
    }
    const auto bucket = static_cast<std::size_t>(prefix_hash.value)
        & (heads_.size() - 1U);
    if (modes_[bucket] == LzssHashTreeBucketMode::promoted_tree) {
        LzssHashTreeComponentStatistics component_statistics{};
        auto* const component_observer = hash_tree_diagnostics_enabled_
                && statistics_ != nullptr
            ? &component_statistics : nullptr;
        const auto query = query_lzss_hash_tree_bucket_exact({
            input_, parameters_, position, bucket, heads_.size(),
            roots_[bucket], left_, right_, parent_, height_, position_,
            subtree_maximum_position_, component_observer});
        if (query.error != LzssHashTreeBucketQueryError::none) {
            mark_error(LzssHashTreeError::tree_query_failure);
            return {};
        }
        if (hash_tree_diagnostics_enabled_ && statistics_ != nullptr) {
            aggregate_query_statistics(statistics_, component_statistics);
            increment_statistic(
                statistics_, statistics_->hash_tree_tree_query_count);
            add_statistic(
                statistics_, statistics_->hash_tree_tree_query_node_count,
                query.nodes_visited);
            statistics_->hash_tree_maximum_nodes_per_query = std::max(
                statistics_->hash_tree_maximum_nodes_per_query,
                query.nodes_visited);
            record_hash_tree_depth(
                statistics_,
                statistics_->hash_tree_tree_query_depth_histogram,
                query.nodes_visited);
        }
        return query.match;
    }
    if (modes_[bucket] != LzssHashTreeBucketMode::chain
        || roots_[bucket] != lzss_hash_tree_null_node) {
        mark_error(LzssHashTreeError::invalid_state);
        return {};
    }

    const auto maximum_length = std::min<std::size_t>(
        input_.size() - position,
        static_cast<std::size_t>(parameters_.max_match_length));
    auto candidate = heads_[bucket];
    std::uint64_t query_candidate_count{};
    while (candidate != lzss_hash_tree_no_stored_position) {
        const auto candidate_position = static_cast<std::size_t>(candidate);
        if (candidate_position >= position
            || candidate_position >= input_.size()
            || input_.size() - candidate_position
                < lzss_match_finder_prefix_size) {
            mark_error(LzssHashTreeError::invalid_state);
            return {};
        }
        const auto distance = position - candidate_position;
        if (distance > parameters_.window_size) break;
        ++query_candidate_count;
        if (statistics_ != nullptr) {
            increment_statistic(statistics_, statistics_->candidate_count);
        }

        std::size_t length{};
        while (length < maximum_length) {
            if (statistics_ != nullptr) {
                increment_statistic(
                    statistics_, statistics_->byte_comparison_count);
            }
            if (statistics_ != nullptr
                && length >= lzss_match_finder_prefix_size) {
                increment_statistic(
                    statistics_,
                    statistics_->hash_chain_extension_byte_comparison_count);
            }
            if (input_[position + length]
                != input_[candidate_position + length]) break;
            ++length;
        }
        if (statistics_ != nullptr) {
            auto& prefix_count = length >= lzss_match_finder_prefix_size
                ? statistics_->hash_chain_prefix_match_count
                : statistics_->hash_chain_prefix_mismatch_count;
            increment_statistic(statistics_, prefix_count);
        }
        if (length >= parameters_.min_match_length && length > best.length) {
            best.distance = static_cast<std::uint32_t>(distance);
            best.length = static_cast<std::uint32_t>(length);
            if (length == maximum_length) break;
        }

        const auto previous_distance =
            links_[candidate_position % links_.size()];
        if (previous_distance == 0) break;
        if (previous_distance > candidate_position) {
            mark_error(LzssHashTreeError::invalid_state);
            return {};
        }
        candidate = static_cast<LzssHashTreeStoredPosition>(
            candidate_position - previous_distance);
    }
    record_chain_query_depth(statistics_, query_candidate_count);
    const auto promotion = promotion_.record_completed_chain_query(
        bucket, query_candidate_count);
    if (promotion.error != LzssHashTreePromotionError::none) {
        mark_error(LzssHashTreeError::promotion_failure);
        return {};
    }
    if (hash_tree_diagnostics_enabled_ && statistics_ != nullptr) {
        increment_statistic(
            statistics_, statistics_->hash_tree_chain_query_count);
        add_statistic(
            statistics_, statistics_->hash_tree_chain_candidate_count,
            query_candidate_count);
        record_hash_tree_depth(
            statistics_, statistics_->hash_tree_chain_query_depth_histogram,
            query_candidate_count);
        if (promotion.pending) {
            increment_statistic(
                statistics_, statistics_->hash_tree_trigger_query_count);
            add_statistic(
                statistics_,
                statistics_->hash_tree_promotion_trigger_candidate_count,
                query_candidate_count);
            statistics_->hash_tree_promotion_maximum_trigger_candidates =
                std::max(
                    statistics_->
                        hash_tree_promotion_maximum_trigger_candidates,
                    query_candidate_count);
        }
    }
    return best;
}

void LzssHashTreeMatchFinder::advance(
    const std::size_t position,
    const std::size_t next_position) noexcept {
    if (!initialized_) {
        mark_error(LzssHashTreeError::invalid_state);
        return;
    }
    if (!state_valid_) return;
    if (position != next_position_ || next_position < position
        || next_position > input_.size()) {
        mark_error(LzssHashTreeError::invalid_protocol);
        return;
    }

    const auto pending = promotion_.begin_advance();
    if (pending.error != LzssHashTreePromotionError::none) {
        mark_error(LzssHashTreeError::promotion_failure);
        return;
    }
    if (pending.required) {
        const auto bucket = pending.bucket;
        if (bucket >= heads_.size()
            || modes_[bucket] != LzssHashTreeBucketMode::chain
            || roots_[bucket] != lzss_hash_tree_null_node) {
            mark_error(LzssHashTreeError::promotion_failure);
            return;
        }
        LzssHashTreeComponentStatistics component_statistics{};
        auto* const component_observer = hash_tree_diagnostics_enabled_
                && statistics_ != nullptr
            ? &component_statistics : nullptr;
        const auto build = build_lzss_hash_tree_bucket({
            input_, parameters_, position, bucket, heads_.size(),
            heads_[bucket] == lzss_hash_tree_no_stored_position
                ? lzss_hash_tree_no_position
                : static_cast<std::size_t>(heads_[bucket]),
            links_,
            {left_, right_, parent_, height_, position_,
             subtree_maximum_position_}, component_observer});
        if (build.error != LzssHashTreeBucketBuildError::none
            || build.root == lzss_hash_tree_null_node) {
            mark_error(LzssHashTreeError::promotion_failure);
            return;
        }
        if (promoted_bucket_count_ >= heads_.size()
            || promoted_node_count_ > left_.size()
            || build.node_count > left_.size() - promoted_node_count_) {
            mark_error(LzssHashTreeError::invalid_state);
            return;
        }
        roots_[bucket] = build.root;
        modes_[bucket] = LzssHashTreeBucketMode::promoted_tree;
        if (promotion_.commit(bucket)
            != LzssHashTreePromotionError::none) {
            mark_error(LzssHashTreeError::promotion_failure);
            return;
        }
        ++promoted_bucket_count_;
        promoted_node_count_ += build.node_count;
        if (hash_tree_diagnostics_enabled_ && statistics_ != nullptr) {
            aggregate_builder_statistics(statistics_, component_statistics);
            increment_statistic(
                statistics_, statistics_->hash_tree_promotion_count);
            add_statistic(
                statistics_, statistics_->hash_tree_promotion_build_node_count,
                static_cast<std::uint64_t>(build.node_count));
            record_hash_tree_population(
                statistics_, promoted_bucket_count_, promoted_node_count_);
        }
    }

    for (auto current = position; current < next_position; ++current) {
        if (current >= parameters_.window_size) {
            const auto expired = current - parameters_.window_size;
            if (input_.size() - expired
                >= lzss_match_finder_prefix_size) {
                const auto expired_hash = calculate_lzss_prefix_hash(
                    input_, expired);
                if (!expired_hash.valid) {
                    mark_error(LzssHashTreeError::invalid_state);
                    return;
                }
                const auto expired_bucket =
                    static_cast<std::size_t>(expired_hash.value)
                    & (heads_.size() - 1U);
                if (modes_[expired_bucket]
                    == LzssHashTreeBucketMode::promoted_tree) {
                    if (promoted_node_count_ == 0) {
                        mark_error(LzssHashTreeError::invalid_state);
                        return;
                    }
                    LzssHashTreeComponentStatistics component_statistics{};
                    auto* const component_observer =
                        hash_tree_diagnostics_enabled_ && statistics_ != nullptr
                        ? &component_statistics : nullptr;
                    const auto removed =
                        remove_lzss_hash_tree_bucket_position_v2(
                            {input_, parameters_, expired_bucket,
                             heads_.size(), left_, right_, parent_, height_,
                             position_, subtree_maximum_position_,
                             component_observer},
                            roots_[expired_bucket], expired);
                    if (removed.error
                        != LzssHashTreeBucketMutationError::none) {
                        mark_error(LzssHashTreeError::tree_mutation_failure);
                        return;
                    }
                    roots_[expired_bucket] = removed.root;
                    --promoted_node_count_;
                    if (hash_tree_diagnostics_enabled_
                        && statistics_ != nullptr) {
                        aggregate_mutation_statistics(
                            statistics_, component_statistics);
                        increment_statistic(
                            statistics_,
                            statistics_->hash_tree_retirement_count);
                    }
                }
            }
        }
        if (input_.size() - current < lzss_match_finder_prefix_size) continue;
        const auto prefix_hash = calculate_lzss_prefix_hash(input_, current);
        if (!prefix_hash.valid) {
            mark_error(LzssHashTreeError::invalid_state);
            return;
        }
        const auto bucket = static_cast<std::size_t>(prefix_hash.value)
            & (heads_.size() - 1U);
        if (modes_[bucket] != LzssHashTreeBucketMode::chain
            && modes_[bucket] != LzssHashTreeBucketMode::promoted_tree) {
            mark_error(LzssHashTreeError::invalid_state);
            return;
        }
        if (modes_[bucket] == LzssHashTreeBucketMode::chain
            && roots_[bucket] != lzss_hash_tree_null_node) {
            mark_error(LzssHashTreeError::invalid_state);
            return;
        }

        const auto previous = heads_[bucket];
        std::uint32_t previous_distance{};
        if (previous != lzss_hash_tree_no_stored_position) {
            const auto previous_position =
                static_cast<std::size_t>(previous);
            if (previous_position >= current
                || previous_position >= input_.size()
                || input_.size() - previous_position
                    < lzss_match_finder_prefix_size) {
                mark_error(LzssHashTreeError::invalid_state);
                return;
            }
            const auto distance = current - previous_position;
            if (distance <= parameters_.window_size) {
                previous_distance = static_cast<std::uint32_t>(distance);
            }
        }
        std::construct_at(
            links_.data() + current % links_.size(), previous_distance);
        heads_[bucket] = static_cast<LzssHashTreeStoredPosition>(current);
        if (modes_[bucket] == LzssHashTreeBucketMode::promoted_tree) {
            if (promoted_node_count_ >= left_.size()) {
                mark_error(LzssHashTreeError::invalid_state);
                return;
            }
            LzssHashTreeComponentStatistics component_statistics{};
            auto* const component_observer = hash_tree_diagnostics_enabled_
                    && statistics_ != nullptr
                ? &component_statistics : nullptr;
            const auto inserted = insert_lzss_hash_tree_bucket_position_v2(
                {input_, parameters_, bucket, heads_.size(), left_, right_,
                 parent_, height_, position_, subtree_maximum_position_,
                 component_observer},
                roots_[bucket], current);
            if (inserted.error
                != LzssHashTreeBucketMutationError::none) {
                mark_error(LzssHashTreeError::tree_mutation_failure);
                return;
            }
            roots_[bucket] = inserted.root;
            ++promoted_node_count_;
            if (hash_tree_diagnostics_enabled_ && statistics_ != nullptr) {
                aggregate_mutation_statistics(
                    statistics_, component_statistics);
                increment_statistic(
                    statistics_, statistics_->hash_tree_insertion_count);
                record_hash_tree_population(
                    statistics_, promoted_bucket_count_, promoted_node_count_);
            }
        }
    }
    next_position_ = next_position;
}

LzssHashTreeWorkspaceRequirements calculate_lzss_hash_tree_workspace(
    const std::size_t input_size, const LzssParameters& parameters,
    const core::DecoderLimits& limits) noexcept {
    LzssHashTreeWorkspaceRequirements result{};
    if (core::validate_limits(limits) != core::LimitError::none) {
        result.error = LzssHashTreeError::invalid_limits;
        return result;
    }
    result.format_error = validate_lzss_parameters(parameters, limits);
    if (result.format_error != LzssFormatError::none) {
        result.error = LzssHashTreeError::invalid_parameters;
        return result;
    }
    if (input_size > limits.max_frame_size
        || input_size > limits.max_total_output_size) {
        result.error = LzssHashTreeError::input_limit_exceeded;
        return result;
    }
    if (input_size
        > static_cast<std::size_t>(lzss_hash_tree_no_stored_position)) {
        result.error = LzssHashTreeError::arithmetic_overflow;
        return result;
    }
    if (input_size >= lzss_match_finder_prefix_size) {
        result.node_count = std::min<std::size_t>(
            input_size, static_cast<std::size_t>(parameters.window_size));
        const auto bucket_target = std::min(
            result.node_count, lzss_match_finder_max_bucket_count);
        result.bucket_count = std::bit_ceil(bucket_target);
    }

    std::size_t cursor{};
    if (!append_array(result.bucket_count,
                      sizeof(LzssHashTreeStoredPosition),
                      alignof(LzssHashTreeStoredPosition), cursor,
                      result.head_offset)
        || !append_array(result.node_count, sizeof(std::uint32_t),
                         alignof(std::uint32_t), cursor, result.link_offset)
        || !append_array(result.bucket_count, sizeof(std::uint32_t),
                         alignof(std::uint32_t), cursor, result.root_offset)
        || !append_array(result.bucket_count, sizeof(std::uint8_t),
                         alignof(std::uint8_t), cursor, result.mode_offset)
        || !append_array(result.node_count, sizeof(std::uint32_t),
                         alignof(std::uint32_t), cursor, result.left_offset)
        || !append_array(result.node_count, sizeof(std::uint32_t),
                         alignof(std::uint32_t), cursor, result.right_offset)
        || !append_array(result.node_count, sizeof(std::uint32_t),
                         alignof(std::uint32_t), cursor, result.parent_offset)
        || !append_array(result.node_count, sizeof(std::uint8_t),
                         alignof(std::uint8_t), cursor, result.height_offset)
        || !append_array(result.node_count,
                         sizeof(LzssHashTreeStoredPosition),
                         alignof(LzssHashTreeStoredPosition), cursor,
                         result.position_offset)
        || !append_array(
            result.node_count, sizeof(LzssHashTreeStoredPosition),
            alignof(LzssHashTreeStoredPosition), cursor,
            result.subtree_maximum_position_offset)) {
        result.error = LzssHashTreeError::arithmetic_overflow;
        return result;
    }
    result.workspace_size = cursor;

    std::size_t aggregate{};
    if (!core::checked_add(input_size, result.workspace_size, aggregate)) {
        result.error = LzssHashTreeError::arithmetic_overflow;
        return result;
    }
    if (result.workspace_size > limits.max_internal_buffered_bytes
        || aggregate > limits.max_internal_buffered_bytes) {
        result.error = LzssHashTreeError::workspace_limit_exceeded;
    }
    return result;
}

LzssHashTreeError initialize_lzss_hash_tree_match_finder(
    const std::span<const std::byte> input,
    const LzssParameters& parameters, const core::DecoderLimits& limits,
    const std::span<std::byte> workspace,
    LzssHashTreeMatchFinder& finder,
    LzssMatchFinderStatistics* const statistics,
    const LzssHashTreeOptions& options) noexcept {
    const auto required = calculate_lzss_hash_tree_workspace(
        input.size(), parameters, limits);
    if (required.error != LzssHashTreeError::none) return required.error;
    if (workspace.size() < required.workspace_size) {
        return LzssHashTreeError::workspace_too_small;
    }
    const auto active_workspace = workspace.first(required.workspace_size);
    if (!active_workspace.empty()
        && reinterpret_cast<std::uintptr_t>(active_workspace.data())
               % required.workspace_alignment != 0) {
        return LzssHashTreeError::misaligned_workspace;
    }
    const auto overlap = core::check_buffer_overlap(
        input.data(), input.size(), active_workspace.data(),
        active_workspace.size());
    if (overlap == core::BufferOverlap::overlap) {
        return LzssHashTreeError::overlapping_buffers;
    }
    if (overlap == core::BufferOverlap::arithmetic_overflow) {
        return LzssHashTreeError::arithmetic_overflow;
    }

    LzssHashTreeMatchFinder initialized{};
    initialized.input_ = input;
    initialized.parameters_ = parameters;
    initialized.statistics_ = statistics;
    initialized.hash_tree_diagnostics_enabled_ =
        options.promotion_candidate_threshold
        != std::numeric_limits<std::uint64_t>::max();
    initialize_lzss_hash_tree_promotion_state(
        required.bucket_count, options.promotion_candidate_threshold,
        initialized.promotion_);
    initialized.initialized_ = true;
    initialized.state_valid_ = true;
    if (required.workspace_size == 0) {
        finder = initialized;
        return LzssHashTreeError::none;
    }

    initialized.heads_ = array_at<LzssHashTreeStoredPosition>(
        active_workspace, required.head_offset, required.bucket_count);
    initialized.links_ = array_at<std::uint32_t>(
        active_workspace, required.link_offset, required.node_count);
    initialized.roots_ = array_at<std::uint32_t>(
        active_workspace, required.root_offset, required.bucket_count);
    initialized.modes_ = array_at<LzssHashTreeBucketMode>(
        active_workspace, required.mode_offset, required.bucket_count);
    initialized.left_ = array_at<std::uint32_t>(
        active_workspace, required.left_offset, required.node_count);
    initialized.right_ = array_at<std::uint32_t>(
        active_workspace, required.right_offset, required.node_count);
    initialized.parent_ = array_at<std::uint32_t>(
        active_workspace, required.parent_offset, required.node_count);
    initialized.height_ = array_at<std::uint8_t>(
        active_workspace, required.height_offset, required.node_count);
    initialized.position_ = array_at<LzssHashTreeStoredPosition>(
        active_workspace, required.position_offset, required.node_count);
    initialized.subtree_maximum_position_ =
        array_at<LzssHashTreeStoredPosition>(
            active_workspace, required.subtree_maximum_position_offset,
            required.node_count);

    construct_array(
        initialized.heads_, lzss_hash_tree_no_stored_position);
    construct_array(initialized.roots_, lzss_hash_tree_null_node);
    construct_array(initialized.modes_, LzssHashTreeBucketMode::chain);

    finder = initialized;
    return LzssHashTreeError::none;
}

} // namespace marc::dictionary::internal

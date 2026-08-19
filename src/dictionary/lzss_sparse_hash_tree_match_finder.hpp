#ifndef MARC_DICTIONARY_LZSS_SPARSE_HASH_TREE_MATCH_FINDER_HPP
#define MARC_DICTIONARY_LZSS_SPARSE_HASH_TREE_MATCH_FINDER_HPP

#include "core/limits.hpp"
#include "dictionary/lzss_match_finder.hpp"
#include "dictionary/lzss_sparse_hash_tree_controller.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace marc::dictionary::internal {

enum class LzssSparseHashTreeMatchFinderError : std::uint8_t {
    none,
    invalid_limits,
    invalid_parameters,
    input_limit_exceeded,
    invalid_pool_capacity,
    arithmetic_overflow,
    workspace_limit_exceeded,
    workspace_too_small,
    misaligned_workspace,
    overlapping_buffers,
    invalid_state,
    invalid_protocol,
    controller_failure,
};

struct LzssSparseHashTreeMatchFinderOptions {
    std::size_t pool_node_capacity{};
    std::uint64_t promotion_candidate_threshold{
        std::numeric_limits<std::uint64_t>::max()};
};

class LzssSparseHashTreeMatchFinder {
public:
    LzssSparseHashTreeMatchFinder() noexcept = default;

    [[nodiscard]] bool initialized() const noexcept { return initialized_; }
    [[nodiscard]] bool state_valid() const noexcept { return state_valid_; }
    [[nodiscard]] std::size_t input_size() const noexcept {
        return input_.size();
    }
    [[nodiscard]] std::size_t bucket_count() const noexcept {
        return workspace_.heads().size();
    }
    [[nodiscard]] std::size_t node_capacity() const noexcept {
        return workspace_.node_pool().capacity();
    }
    [[nodiscard]] std::size_t active_node_count() const noexcept {
        return workspace_.node_pool().active_count();
    }
    [[nodiscard]] std::size_t next_position() const noexcept {
        return advance_state_.next_position();
    }
    [[nodiscard]] LzssSparseHashTreeMatchFinderError last_error()
        const noexcept { return last_error_; }
    [[nodiscard]] LzssSparseHashTreeControllerError controller_error()
        const noexcept { return controller_error_; }

    [[nodiscard]] LzssMatch find_match(std::size_t position) noexcept;
    void advance(std::size_t position, std::size_t next_position) noexcept;

private:
    friend LzssSparseHashTreeMatchFinderError
    initialize_lzss_sparse_hash_tree_match_finder(
        std::span<const std::byte>, const LzssParameters&,
        const core::DecoderLimits&, std::span<std::byte>,
        LzssSparseHashTreeMatchFinder&,
        LzssMatchFinderStatistics*,
        const LzssSparseHashTreeMatchFinderOptions&) noexcept;

    void mark_error(LzssSparseHashTreeMatchFinderError error,
                    LzssSparseHashTreeControllerError controller_error =
                        LzssSparseHashTreeControllerError::none) noexcept;
    [[nodiscard]] LzssSparseHashTreePositionContext context() noexcept;

    std::span<const std::byte> input_{};
    LzssParameters parameters_{};
    LzssSparseHashTreeWorkspace workspace_{};
    LzssHashTreePromotionState promotion_{};
    LzssSparseHashTreeAdvanceState advance_state_{};
    LzssMatchFinderStatistics* statistics_{};
    LzssSparseHashTreeControllerError controller_error_{
        LzssSparseHashTreeControllerError::none};
    LzssSparseHashTreeMatchFinderError last_error_{
        LzssSparseHashTreeMatchFinderError::none};
    bool initialized_{};
    bool state_valid_{};
};

static_assert(LzssMatchFinder<LzssSparseHashTreeMatchFinder>);

[[nodiscard]] LzssSparseHashTreeMatchFinderError
initialize_lzss_sparse_hash_tree_match_finder(
    std::span<const std::byte> input, const LzssParameters& parameters,
    const core::DecoderLimits& limits, std::span<std::byte> workspace,
    LzssSparseHashTreeMatchFinder& finder,
    LzssMatchFinderStatistics* statistics = nullptr,
    const LzssSparseHashTreeMatchFinderOptions& options = {}) noexcept;

} // namespace marc::dictionary::internal

#endif

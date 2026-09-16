#include "dictionary/lzss_hash_tree_promotion.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace marc::dictionary::internal {

void LzssHashTreePromotionState::mark_error(
    const LzssHashTreePromotionError error) noexcept {
    if (last_error_ == LzssHashTreePromotionError::none) last_error_ = error;
    state_valid_ = false;
}

void LzssHashTreePromotionState::clear_active() noexcept {
    phase_ = LzssHashTreePromotionPhase::idle;
    active_bucket_ = lzss_hash_tree_no_promotion_bucket;
    trigger_candidate_count_ = 0;
}

LzssHashTreePromotionRecordResult
LzssHashTreePromotionState::record_completed_chain_query(
    const std::size_t bucket,
    const std::uint64_t candidate_count,
    const std::span<std::uint8_t> reuse_counts) noexcept {
    if (!initialized_) {
        mark_error(LzssHashTreePromotionError::invalid_transition);
        return {false, last_error_};
    }
    if (!state_valid_) return {false, last_error_};
    if (bucket >= bucket_count_) {
        mark_error(LzssHashTreePromotionError::invalid_bucket);
        return {false, last_error_};
    }
    const auto expected_reuse_count = reuse_threshold_ == 1
        ? std::size_t{0} : bucket_count_;
    if (reuse_counts.size() != expected_reuse_count) {
        mark_error(LzssHashTreePromotionError::invalid_reuse_counts);
        return {false, last_error_};
    }
    if (phase_ == LzssHashTreePromotionPhase::pending) {
        if (bucket == active_bucket_
            && candidate_count == trigger_candidate_count_) {
            return {true, LzssHashTreePromotionError::none};
        }
        mark_error(LzssHashTreePromotionError::invalid_transition);
        return {false, last_error_};
    }
    if (phase_ != LzssHashTreePromotionPhase::idle) {
        mark_error(LzssHashTreePromotionError::invalid_transition);
        return {false, last_error_};
    }
    if (candidate_count == 0
        || candidate_count <= candidate_threshold_) {
        if (reuse_threshold_ > 1) reuse_counts[bucket] = 0;
        return {false, LzssHashTreePromotionError::none};
    }

    if (reuse_threshold_ > 1) {
        auto& reuse_count = reuse_counts[bucket];
        if (reuse_count != std::numeric_limits<std::uint8_t>::max()) {
            ++reuse_count;
        }
        if (reuse_count < reuse_threshold_) {
            return {false, LzssHashTreePromotionError::none};
        }
    }

    phase_ = LzssHashTreePromotionPhase::pending;
    active_bucket_ = bucket;
    trigger_candidate_count_ = candidate_count;
    return {true, LzssHashTreePromotionError::none};
}

LzssHashTreePromotionBeginResult
LzssHashTreePromotionState::begin_advance() noexcept {
    if (!initialized_) {
        mark_error(LzssHashTreePromotionError::invalid_transition);
        return {false, lzss_hash_tree_no_promotion_bucket, 0, last_error_};
    }
    if (!state_valid_) {
        return {false, lzss_hash_tree_no_promotion_bucket, 0, last_error_};
    }
    if (phase_ == LzssHashTreePromotionPhase::idle) return {};
    if (phase_ != LzssHashTreePromotionPhase::pending) {
        mark_error(LzssHashTreePromotionError::invalid_transition);
        return {false, lzss_hash_tree_no_promotion_bucket, 0, last_error_};
    }

    phase_ = LzssHashTreePromotionPhase::building;
    return {true, active_bucket_, trigger_candidate_count_,
            LzssHashTreePromotionError::none};
}

LzssHashTreePromotionError LzssHashTreePromotionState::commit(
    const std::size_t bucket,
    const std::span<std::uint8_t> reuse_counts) noexcept {
    if (!initialized_) {
        mark_error(LzssHashTreePromotionError::invalid_transition);
        return last_error_;
    }
    if (!state_valid_) return last_error_;
    if (bucket >= bucket_count_) {
        mark_error(LzssHashTreePromotionError::invalid_bucket);
        return last_error_;
    }
    const auto expected_reuse_count = reuse_threshold_ == 1
        ? std::size_t{0} : bucket_count_;
    if (reuse_counts.size() != expected_reuse_count) {
        mark_error(LzssHashTreePromotionError::invalid_reuse_counts);
        return last_error_;
    }
    if (phase_ != LzssHashTreePromotionPhase::building
        || bucket != active_bucket_) {
        mark_error(LzssHashTreePromotionError::invalid_transition);
        return last_error_;
    }
    if (reuse_threshold_ > 1) reuse_counts[bucket] = 0;
    clear_active();
    return LzssHashTreePromotionError::none;
}

void initialize_lzss_hash_tree_promotion_state(
    const std::size_t bucket_count,
    const std::uint64_t candidate_threshold,
    LzssHashTreePromotionState& state,
    const std::uint8_t reuse_threshold) noexcept {
    LzssHashTreePromotionState initialized{};
    initialized.bucket_count_ = bucket_count;
    initialized.candidate_threshold_ = candidate_threshold;
    initialized.reuse_threshold_ = reuse_threshold;
    initialized.initialized_ = true;
    initialized.state_valid_ = reuse_threshold != 0;
    if (!initialized.state_valid_) {
        initialized.last_error_ =
            LzssHashTreePromotionError::invalid_reuse_threshold;
    }
    state = initialized;
}

} // namespace marc::dictionary::internal

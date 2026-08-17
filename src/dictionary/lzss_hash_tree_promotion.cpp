#include "dictionary/lzss_hash_tree_promotion.hpp"

#include <cstddef>
#include <cstdint>

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
    const std::uint64_t candidate_count) noexcept {
    if (!initialized_) {
        mark_error(LzssHashTreePromotionError::invalid_transition);
        return {false, last_error_};
    }
    if (!state_valid_) return {false, last_error_};
    if (bucket >= bucket_count_) {
        mark_error(LzssHashTreePromotionError::invalid_bucket);
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
        return {false, LzssHashTreePromotionError::none};
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
    const std::size_t bucket) noexcept {
    if (!initialized_) {
        mark_error(LzssHashTreePromotionError::invalid_transition);
        return last_error_;
    }
    if (!state_valid_) return last_error_;
    if (bucket >= bucket_count_) {
        mark_error(LzssHashTreePromotionError::invalid_bucket);
        return last_error_;
    }
    if (phase_ != LzssHashTreePromotionPhase::building
        || bucket != active_bucket_) {
        mark_error(LzssHashTreePromotionError::invalid_transition);
        return last_error_;
    }
    clear_active();
    return LzssHashTreePromotionError::none;
}

void initialize_lzss_hash_tree_promotion_state(
    const std::size_t bucket_count,
    const std::uint64_t candidate_threshold,
    LzssHashTreePromotionState& state) noexcept {
    LzssHashTreePromotionState initialized{};
    initialized.bucket_count_ = bucket_count;
    initialized.candidate_threshold_ = candidate_threshold;
    initialized.initialized_ = true;
    initialized.state_valid_ = true;
    state = initialized;
}

} // namespace marc::dictionary::internal

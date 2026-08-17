#ifndef MARC_DICTIONARY_LZSS_HASH_TREE_PROMOTION_HPP
#define MARC_DICTIONARY_LZSS_HASH_TREE_PROMOTION_HPP

#include <cstddef>
#include <cstdint>
#include <limits>

namespace marc::dictionary::internal {

inline constexpr std::size_t lzss_hash_tree_no_promotion_bucket =
    std::numeric_limits<std::size_t>::max();

enum class LzssHashTreePromotionPhase : std::uint8_t {
    idle,
    pending,
    building,
};

enum class LzssHashTreePromotionError : std::uint8_t {
    none,
    invalid_bucket,
    invalid_transition,
};

struct LzssHashTreePromotionRecordResult {
    bool pending{};
    LzssHashTreePromotionError error{LzssHashTreePromotionError::none};
};

struct LzssHashTreePromotionBeginResult {
    bool required{};
    std::size_t bucket{lzss_hash_tree_no_promotion_bucket};
    std::uint64_t trigger_candidate_count{};
    LzssHashTreePromotionError error{LzssHashTreePromotionError::none};
};

class LzssHashTreePromotionState {
public:
    LzssHashTreePromotionState() noexcept = default;

    [[nodiscard]] bool initialized() const noexcept { return initialized_; }
    [[nodiscard]] bool state_valid() const noexcept { return state_valid_; }
    [[nodiscard]] std::size_t bucket_count() const noexcept {
        return bucket_count_;
    }
    [[nodiscard]] std::uint64_t candidate_threshold() const noexcept {
        return candidate_threshold_;
    }
    [[nodiscard]] LzssHashTreePromotionPhase phase() const noexcept {
        return phase_;
    }
    [[nodiscard]] std::size_t active_bucket() const noexcept {
        return active_bucket_;
    }
    [[nodiscard]] std::uint64_t trigger_candidate_count() const noexcept {
        return trigger_candidate_count_;
    }
    [[nodiscard]] LzssHashTreePromotionError last_error() const noexcept {
        return last_error_;
    }

    [[nodiscard]] LzssHashTreePromotionRecordResult
    record_completed_chain_query(
        std::size_t bucket, std::uint64_t candidate_count) noexcept;
    [[nodiscard]] LzssHashTreePromotionBeginResult begin_advance() noexcept;
    [[nodiscard]] LzssHashTreePromotionError commit(
        std::size_t bucket) noexcept;

private:
    friend void initialize_lzss_hash_tree_promotion_state(
        std::size_t, std::uint64_t,
        LzssHashTreePromotionState&) noexcept;

    void mark_error(LzssHashTreePromotionError error) noexcept;
    void clear_active() noexcept;

    std::size_t bucket_count_{};
    std::uint64_t candidate_threshold_{};
    LzssHashTreePromotionPhase phase_{LzssHashTreePromotionPhase::idle};
    std::size_t active_bucket_{lzss_hash_tree_no_promotion_bucket};
    std::uint64_t trigger_candidate_count_{};
    LzssHashTreePromotionError last_error_{
        LzssHashTreePromotionError::none};
    bool initialized_{};
    bool state_valid_{};
};

void initialize_lzss_hash_tree_promotion_state(
    std::size_t bucket_count, std::uint64_t candidate_threshold,
    LzssHashTreePromotionState& state) noexcept;

} // namespace marc::dictionary::internal

#endif

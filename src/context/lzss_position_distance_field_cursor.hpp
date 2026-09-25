#ifndef MARC_CONTEXT_LZSS_POSITION_DISTANCE_FIELD_CURSOR_HPP
#define MARC_CONTEXT_LZSS_POSITION_DISTANCE_FIELD_CURSOR_HPP

#include "context/lzss_position_distance_context_layout.hpp"

#include <cstdint>

namespace marc::context::internal {

enum class LzssPositionDistanceField : std::uint8_t {
    symbol, uniform_length_extra, adaptive_distance_extra,
};

struct LzssPositionDistanceRequest {
    ModeledOperation shape{}; // value is supplied only after decoding
    LzssPositionDistanceField field{LzssPositionDistanceField::symbol};
};

// Field grammar only: frame counts, history and output bounds are separate.
class LzssPositionDistanceFieldCursor {
public:
    void reset() noexcept { *this = LzssPositionDistanceFieldCursor{}; }

    [[nodiscard]] LzssPositionDistanceRequest next() const noexcept {
        switch (phase_) {
        case Phase::kind: return symbol(previous_kind_, 2);
        case Phase::literal: return symbol(has_literal_ ?
            static_cast<std::uint16_t>(4 + (last_literal_ >> 5)) : 3, 256);
        case Phase::length: return symbol(static_cast<std::uint16_t>(12 + previous_kind_), 9);
        case Phase::distance: return symbol(static_cast<std::uint16_t>(15 + length_class_), 17);
        case Phase::length_extra:
            return {{ModeledOperationKind::bypass_bits,0,0,0,length_width()},
                    LzssPositionDistanceField::uniform_length_extra};
        case Phase::distance_extra:
            return {{ModeledOperationKind::bypass_bits,0,0,0,distance_class_},
                    LzssPositionDistanceField::adaptive_distance_extra};
        }
        return {}; // all states are private and exhaustively handled above
    }

    [[nodiscard]] LzssFieldContextError accept(const ModeledOperation& op) noexcept {
        const auto expected = next().shape;
        if (op.kind != expected.kind) return LzssFieldContextError::unexpected_operation_kind;
        if (op.kind == ModeledOperationKind::symbol) {
            const auto error = validate_lzss_position_distance_symbol(op);
            if (error != LzssFieldContextError::none) return error;
            if (op.context_id != expected.context_id) return LzssFieldContextError::unexpected_context;
        } else {
            if (op.context_id != 0 || op.alphabet_size != 0)
                return LzssFieldContextError::nonzero_unused_field;
            if (op.bit_count != expected.bit_count)
                return LzssFieldContextError::invalid_bypass_width;
            if ((op.value >> op.bit_count) != 0) return LzssFieldContextError::invalid_symbol;
            if ((phase_ == Phase::length_extra && length_class_ == 7 && op.value == 127)
                || (phase_ == Phase::distance_extra && distance_class_ == 16 && op.value != 0))
                return LzssFieldContextError::invalid_token;
        }
        switch (phase_) {
        case Phase::kind: phase_ = op.value == 0 ? Phase::literal : Phase::length; break;
        case Phase::literal:
            last_literal_ = static_cast<std::uint8_t>(op.value);
            has_literal_ = true;
            previous_kind_ = 1;
            phase_ = Phase::kind;
            break;
        case Phase::length:
            length_class_ = static_cast<std::uint8_t>(op.value);
            phase_ = length_width() == 0 ? Phase::distance : Phase::length_extra;
            break;
        case Phase::length_extra: phase_ = Phase::distance; break;
        case Phase::distance:
            distance_class_ = static_cast<std::uint8_t>(op.value);
            if (distance_class_ != 0) phase_ = Phase::distance_extra;
            else complete_match();
            break;
        case Phase::distance_extra: complete_match(); break;
        }
        return LzssFieldContextError::none;
    }

    [[nodiscard]] LzssFieldContextError finish() const noexcept {
        return phase_ == Phase::kind ? LzssFieldContextError::none
                                     : LzssFieldContextError::truncated_token;
    }

private:
    enum class Phase : std::uint8_t { kind, literal, length, length_extra, distance, distance_extra };
    [[nodiscard]] static LzssPositionDistanceRequest symbol(std::uint16_t id, std::uint16_t alphabet) noexcept {
        return {{ModeledOperationKind::symbol,id,alphabet,0,0}, LzssPositionDistanceField::symbol};
    }
    [[nodiscard]] std::uint8_t length_width() const noexcept {
        return length_class_ == 8 ? 1 : length_class_;
    }
    void complete_match() noexcept { previous_kind_ = 2; phase_ = Phase::kind; }
    Phase phase_{Phase::kind};
    std::uint16_t previous_kind_{};
    std::uint8_t last_literal_{};
    std::uint8_t length_class_{};
    std::uint8_t distance_class_{};
    bool has_literal_{};
};

} // namespace marc::context::internal
#endif

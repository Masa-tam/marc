#ifndef MARC_CONTEXT_LZSS_POSITION_DISTANCE_32M_FIELD_CURSOR_HPP
#define MARC_CONTEXT_LZSS_POSITION_DISTANCE_32M_FIELD_CURSOR_HPP
#include "context/lzss_field_context.hpp"
#include "context/lzss_position_distance_32m_context_layout.hpp"
namespace marc::context::internal {
enum class LzssPositionDistance32mField : std::uint8_t {
  symbol,
  uniform_length_extra,
  adaptive_distance_extra
};
struct LzssPositionDistance32mRequest {
  ModeledOperation shape{};
  LzssPositionDistance32mField field{LzssPositionDistance32mField::symbol};
};
inline LzssPositionDistance32mRequest lzss_position_distance_32m_next(
    const LzssPositionDistance32mFieldState &s) noexcept {
  using Phase = LzssPositionDistance32mFieldState::Phase;
  using Field = LzssPositionDistance32mField;
  const auto symbol =
      [](std::uint16_t id,
         std::uint16_t alphabet) -> LzssPositionDistance32mRequest {
    return {{ModeledOperationKind::symbol, id, alphabet, 0, 0}, Field::symbol};
  };
  switch (s.phase) {
  case Phase::kind:
    return symbol(s.previous_kind, 2);
  case Phase::literal:
    return symbol(s.has_literal
                      ? static_cast<std::uint16_t>(4 + (s.last_literal >> 5))
                      : 3,
                  256);
  case Phase::length:
    return symbol(static_cast<std::uint16_t>(12 + s.previous_kind), 9);
  case Phase::distance:
    return symbol(static_cast<std::uint16_t>(15 + s.length_class), 26);
  case Phase::length_extra:
    return {
        {ModeledOperationKind::bypass_bits, 0, 0, 0,
         static_cast<std::uint8_t>(s.length_class == 8 ? 1 : s.length_class)},
        Field::uniform_length_extra};
  case Phase::distance_extra:
    return {{ModeledOperationKind::bypass_bits, 0, 0, 0, s.distance_class},
            Field::adaptive_distance_extra};
  }
  return {};
}
// Transactional grammar update. Counts, history and raw bounds belong to later
// layers.
inline LzssFieldContextError
lzss_position_distance_32m_accept(LzssPositionDistance32mFieldState &s,
                                  const ModeledOperation &op) noexcept {
  const auto expected = lzss_position_distance_32m_next(s).shape;
  if (op.kind != expected.kind)
    return LzssFieldContextError::unexpected_operation_kind;
  if (op.context_id != expected.context_id)
    return LzssFieldContextError::unexpected_context;
  if (op.alphabet_size != expected.alphabet_size)
    return LzssFieldContextError::unexpected_alphabet;
  if (op.bit_count != expected.bit_count)
    return LzssFieldContextError::invalid_bypass_width;
  if (op.kind == ModeledOperationKind::symbol) {
    if (op.value >= op.alphabet_size)
      return LzssFieldContextError::invalid_symbol;
  } else {
    if ((op.value >> op.bit_count) != 0)
      return LzssFieldContextError::invalid_symbol;
    using Phase = LzssPositionDistance32mFieldState::Phase;
    if ((s.phase == Phase::length_extra && s.length_class == 7 &&
         op.value == 127) ||
        (s.phase == Phase::distance_extra && s.distance_class == 25 &&
         op.value != 0))
      return LzssFieldContextError::invalid_token;
  }
  using Phase = LzssPositionDistance32mFieldState::Phase;
  switch (s.phase) {
  case Phase::kind:
    s.phase = op.value == 0 ? Phase::literal : Phase::length;
    break;
  case Phase::literal:
    s.last_literal = static_cast<std::uint8_t>(op.value);
    s.has_literal = true;
    s.previous_kind = 1;
    s.phase = Phase::kind;
    break;
  case Phase::length:
    s.length_class = static_cast<std::uint8_t>(op.value);
    s.phase = op.value == 0 ? Phase::distance : Phase::length_extra;
    break;
  case Phase::length_extra:
    s.phase = Phase::distance;
    break;
  case Phase::distance:
    s.distance_class = static_cast<std::uint8_t>(op.value);
    if (op.value != 0) {
      s.phase = Phase::distance_extra;
      break;
    }
    [[fallthrough]];
  case Phase::distance_extra:
    s.previous_kind = 2;
    s.phase = Phase::kind;
    break;
  }
  return LzssFieldContextError::none;
}
inline LzssFieldContextError lzss_position_distance_32m_finish(
    const LzssPositionDistance32mFieldState &s) noexcept {
  return s.phase == LzssPositionDistance32mFieldState::Phase::kind
             ? LzssFieldContextError::none
             : LzssFieldContextError::truncated_token;
}
} // namespace marc::context::internal
#endif

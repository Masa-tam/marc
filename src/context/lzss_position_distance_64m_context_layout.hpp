#ifndef MARC_CONTEXT_LZSS_POSITION_DISTANCE_64M_CONTEXT_LAYOUT_HPP
#define MARC_CONTEXT_LZSS_POSITION_DISTANCE_64M_CONTEXT_LAYOUT_HPP
#include <array>
#include <cstddef>
#include <cstdint>
namespace marc::context::internal {
// Private reserved 2/14 + 1/15 + 3/2; not public parser admission.
inline constexpr std::uint16_t lzss_position_distance_64m_context_count = 50;
inline constexpr std::size_t lzss_position_distance_64m_frequency_entries =
    2632;
inline constexpr auto lzss_position_distance_64m_alphabets = [] {
  std::array<std::uint16_t, 50> a{};
  for (std::size_t i = 0; i < a.size(); ++i)
    a[i] = i < 3 || i >= 24 ? 2 : i < 12 ? 256 : i < 15 ? 9 : 27;
  return a;
}();
inline constexpr auto lzss_position_distance_64m_offsets = [] {
  std::array<std::size_t, 51> o{};
  for (std::size_t i = 0; i < 50; ++i)
    o[i + 1] = o[i] + lzss_position_distance_64m_alphabets[i];
  return o;
}();
static_assert(lzss_position_distance_64m_offsets[24] == 2580);
static_assert(lzss_position_distance_64m_offsets.back() == 2632);
// Plain bounded grammar storage; private field transitions are defined
// separately.
struct LzssPositionDistance64mFieldState {
  enum class Phase : std::uint8_t {
    kind,
    literal,
    length,
    length_extra,
    distance,
    distance_extra
  };
  Phase phase{Phase::kind};
  std::uint8_t previous_kind{}, last_literal{}, length_class{},
      distance_class{};
  bool has_literal{};
};
} // namespace marc::context::internal
#endif

#ifndef MARC_CONTEXT_LZSS_POSITION_DISTANCE_32M_CONTEXT_LAYOUT_HPP
#define MARC_CONTEXT_LZSS_POSITION_DISTANCE_32M_CONTEXT_LAYOUT_HPP
#include <array>
#include <cstddef>
#include <cstdint>
namespace marc::context::internal {
// Private reserved 2/13 + 1/14 + 3/2; not public parser admission.
inline constexpr std::uint16_t lzss_position_distance_32m_context_count = 49;
inline constexpr std::size_t lzss_position_distance_32m_frequency_entries =
    2621;
inline constexpr auto lzss_position_distance_32m_alphabets = [] {
  std::array<std::uint16_t, 49> a{};
  for (std::size_t i = 0; i < a.size(); ++i)
    a[i] = i < 3 || i >= 24 ? 2 : i < 12 ? 256 : i < 15 ? 9 : 26;
  return a;
}();
inline constexpr auto lzss_position_distance_32m_offsets = [] {
  std::array<std::size_t, 50> o{};
  for (std::size_t i = 0; i < 49; ++i)
    o[i + 1] = o[i] + lzss_position_distance_32m_alphabets[i];
  return o;
}();
static_assert(lzss_position_distance_32m_offsets[24] == 2571);
static_assert(lzss_position_distance_32m_offsets.back() == 2621);
// Plain bounded grammar storage; private field transitions are defined
// separately.
struct LzssPositionDistance32mFieldState {
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

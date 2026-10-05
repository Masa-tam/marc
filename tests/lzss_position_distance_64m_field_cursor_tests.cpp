#include "context/lzss_position_distance_64m_field_cursor.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace marc::context::internal;
using State = LzssPositionDistance64mFieldState;
using Error = LzssFieldContextError;
int main() {
  const auto require = [](bool value) {
    if (!value)
      std::abort();
  };
  require(lzss_position_distance_64m_context_count == 50);
  require(lzss_position_distance_64m_frequency_entries == 2632);
  require(lzss_position_distance_64m_offsets[24] == 2580);
  require(lzss_position_distance_64m_offsets.back() == 2632);
  std::size_t checks{};
  for (std::uint8_t previous = 0; previous < 3; ++previous) {
    State s{};
    s.previous_kind = previous;
    auto op = lzss_position_distance_64m_next(s).shape;
    require(op.context_id == previous && op.alphabet_size == 2);
    op.value = 0;
    require(lzss_position_distance_64m_accept(s, op) == Error::none);
    require(lzss_position_distance_64m_next(s).shape.context_id == 3);
    ++checks;
  }
  for (unsigned byte = 0; byte < 256; ++byte) {
    State s{};
    auto op = lzss_position_distance_64m_next(s).shape;
    require(lzss_position_distance_64m_accept(s, op) == Error::none);
    op = lzss_position_distance_64m_next(s).shape;
    op.value = byte;
    require(lzss_position_distance_64m_accept(s, op) == Error::none);
    require(s.last_literal == byte && s.has_literal && s.previous_kind == 1);
    op = lzss_position_distance_64m_next(s).shape;
    require(lzss_position_distance_64m_accept(s, op) == Error::none);
    require(lzss_position_distance_64m_next(s).shape.context_id ==
            4 + (byte >> 5));
    ++checks;
  }
  for (std::uint8_t length_class = 0; length_class < 9; ++length_class) {
    for (std::uint8_t distance_class = 0; distance_class < 27;
         ++distance_class) {
      State s{};
      s.previous_kind = 2;
      s.has_literal = true;
      s.last_literal = 255;
      auto op = lzss_position_distance_64m_next(s).shape;
      op.value = 1;
      require(lzss_position_distance_64m_accept(s, op) == Error::none);
      op = lzss_position_distance_64m_next(s).shape;
      require(op.context_id == 14 && op.alphabet_size == 9);
      op.value = length_class;
      require(lzss_position_distance_64m_accept(s, op) == Error::none);
      if (length_class) {
        op = lzss_position_distance_64m_next(s).shape;
        require(op.bit_count == (length_class == 8 ? 1 : length_class));
        op.value = 0;
        require(lzss_position_distance_64m_accept(s, op) == Error::none);
      }
      op = lzss_position_distance_64m_next(s).shape;
      require(op.context_id == 15 + length_class && op.alphabet_size == 27);
      op.value = distance_class;
      require(lzss_position_distance_64m_accept(s, op) == Error::none);
      if (distance_class) {
        op = lzss_position_distance_64m_next(s).shape;
        require(op.bit_count == distance_class);
        if (distance_class == 26) {
          const auto before = s;
          op.value = 1;
          require(lzss_position_distance_64m_accept(s, op) ==
                  Error::invalid_token);
          require(std::memcmp(&before, &s, sizeof(s)) == 0);
        }
        op.value = 0;
        require(lzss_position_distance_64m_accept(s, op) == Error::none);
      }
      require(lzss_position_distance_64m_finish(s) == Error::none);
      require(s.last_literal == 255 && s.has_literal && s.previous_kind == 2);
      ++checks;
    }
  }
  State s{};
  auto op = lzss_position_distance_64m_next(s).shape;
  for (unsigned invalid = 0; invalid < 4; ++invalid) {
    auto bad = op;
    if (invalid == 0)
      ++bad.context_id;
    if (invalid == 1)
      ++bad.alphabet_size;
    if (invalid == 2)
      bad.value = 2;
    if (invalid == 3)
      ++bad.bit_count;
    const auto before = s;
    require(lzss_position_distance_64m_accept(s, bad) != Error::none);
    require(std::memcmp(&before, &s, sizeof(s)) == 0);
    ++checks;
  }
  s.phase = State::Phase::length_extra;
  s.length_class = 7;
  op = lzss_position_distance_64m_next(s).shape;
  op.value = 127;
  const auto before = s;
  require(lzss_position_distance_64m_accept(s, op) == Error::invalid_token);
  require(std::memcmp(&before, &s, sizeof(s)) == 0);
  require(lzss_position_distance_64m_finish(s) == Error::truncated_token);
  std::printf(
      "PASS %zu cursor recipes and invalid-transition invariants; model only\n",
      checks + 1);
}

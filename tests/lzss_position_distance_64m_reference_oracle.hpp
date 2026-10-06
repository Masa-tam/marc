#pragma once
#include "dictionary/lzss_typed_token.hpp"
#include <algorithm>
#include <vector>
namespace reference_oracle {
using namespace marc::dictionary::internal;
inline bool equal(const LzssTypedToken &a, const LzssTypedToken &b) {
  return a.kind == b.kind && a.literal == b.literal &&
         a.distance == b.distance && a.length == b.length;
}
// Independent selection by descending candidate phrase length, then nearest
// source position. This does not invoke a production finder or token parser.
inline std::vector<LzssTypedToken> parse(std::span<const std::byte> raw,
                                         const LzssParameters &p) {
  std::vector<LzssTypedToken> out;
  std::size_t pos = 0;
  while (pos < raw.size()) {
    std::size_t chosen = 0, length = 0;
    for (std::size_t n =
             std::min<std::size_t>(p.max_match_length, raw.size() - pos);
         n >= 5 && !chosen; --n) {
      const auto start = pos > p.window_size ? pos - p.window_size : 0;
      for (std::size_t source = pos; source > start;) {
        --source;
        if (std::equal(raw.begin() + pos, raw.begin() + pos + n,
                       raw.begin() + source)) {
          chosen = pos - source;
          length = n;
          break;
        }
      }
    }
    out.push_back(
        chosen ? LzssTypedToken{LzssTypedTokenKind::match, 0,
                                static_cast<std::uint32_t>(chosen),
                                static_cast<std::uint32_t>(length)}
               : LzssTypedToken{LzssTypedTokenKind::literal,
                                std::to_integer<std::uint8_t>(raw[pos]), 0, 0});
    pos += chosen ? length : 1;
  }
  return out;
}
inline std::vector<std::byte> reconstruct(std::span<const LzssTypedToken> t) {
  std::vector<std::byte> raw;
  for (auto token : t) {
    if (token.kind == LzssTypedTokenKind::literal)
      raw.push_back(std::byte{token.literal});
    else
      for (unsigned i = 0; i < token.length; ++i)
        raw.push_back(raw[raw.size() - token.distance]);
  }
  return raw;
}
} // namespace reference_oracle

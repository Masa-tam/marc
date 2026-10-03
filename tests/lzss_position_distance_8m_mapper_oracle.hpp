#pragma once
// Independently authored field equations; no production cursor/mapper calls.
#include "context/lzss_field_context.hpp"
#include <vector>
namespace mapper_oracle {
using namespace marc::context::internal;
using namespace marc::dictionary::internal;
inline std::vector<ModeledOperation>
fields(std::span<const LzssTypedToken> tokens) {
  std::vector<ModeledOperation> out;
  unsigned previous = 0, last = 0;
  auto symbol = [&](unsigned c, unsigned a, unsigned v) {
    out.push_back({ModeledOperationKind::symbol, static_cast<std::uint16_t>(c),
                   static_cast<std::uint16_t>(a), v, 0});
  };
  auto bits = [&](unsigned value, unsigned width) {
    out.push_back({ModeledOperationKind::bypass_bits, 0, 0, value,
                   static_cast<std::uint8_t>(width)});
  };
  for (auto t : tokens) {
    const bool literal = t.kind == LzssTypedTokenKind::literal;
    symbol(previous, 2, literal ? 0 : 1);
    if (literal) {
      symbol(previous ? 4 + (last / 32) : 3, 256, t.literal);
      last = t.literal;
      previous = 1;
      continue;
    }
    unsigned lc = 8, le = t.length - 3;
    if (t.length >= 5) {
      lc = 0;
      while ((1u << (lc + 1)) <= t.length - 4)
        ++lc;
      le = t.length - 4 - (1u << lc);
    }
    symbol(12 + previous, 9, lc);
    if (lc)
      bits(le, lc == 8 ? 1 : lc);
    unsigned dc = 0;
    while ((1u << (dc + 1)) <= t.distance)
      ++dc;
    symbol(15 + lc, 24, dc);
    if (dc)
      bits(t.distance - (1u << dc), dc);
    previous = 2;
  }
  return out;
}
inline LzssFieldContextValidationContext
counts(std::span<const LzssTypedToken> tokens) {
  const auto ops = fields(tokens);
  LzssFieldContextValidationContext c{};
  c.declared_token_count = static_cast<std::uint32_t>(tokens.size());
  c.declared_event_count = static_cast<std::uint32_t>(ops.size());
  for (auto op : ops)
    c.declared_decision_count +=
        op.kind == ModeledOperationKind::symbol ? 1 : op.bit_count;
  for (auto t : tokens)
    c.declared_raw_size += t.kind == LzssTypedTokenKind::literal ? 1 : t.length;
  return c;
}
inline bool equal(const ModeledOperation &a, const ModeledOperation &b) {
  return a.kind == b.kind && a.context_id == b.context_id &&
         a.alphabet_size == b.alphabet_size && a.value == b.value &&
         a.bit_count == b.bit_count;
}
inline LzssTypedToken literal(unsigned b = 65) {
  return {LzssTypedTokenKind::literal, static_cast<std::uint8_t>(b), 0, 0};
}
inline LzssTypedToken match(unsigned d, unsigned n) {
  return {LzssTypedTokenKind::match, 0, d, n};
}
} // namespace mapper_oracle

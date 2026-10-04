#include "context/lzss_position_distance_16m_mapper.hpp"
#include "entropy/lzss_position_distance_16m_range_encoder.hpp"
#include "entropy/lzss_position_distance_16m_token_range_encoder.hpp"
#include <algorithm>
#include <bit>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace marc;
using namespace context::internal;
using namespace entropy::internal;
using Token = dictionary::internal::LzssTypedToken;
using Kind = dictionary::internal::LzssTypedTokenKind;
void require(bool b) {
  if (!b)
    std::abort();
}
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (size < 2 || size > 128)
    return 0;
  std::vector<Token> tokens{{Kind::literal, data[1], 0, 0}};
  uint32_t f = 1, e = 2, d = 2;
  for (size_t i = 2; i < size; ++i) {
    auto b = data[i];
    if (b & 1) {
      auto len = 3u + (b % 256u);
      auto distance = 1u + (b % f);
      tokens.push_back({Kind::match, 0, distance, len});
      f += len;
      auto lc = len < 5 ? 8u : std::bit_width(len - 4) - 1u;
      auto dc = std::bit_width(distance) - 1u;
      e += 3 + (lc != 0) + (dc != 0);
      d += 3 + (lc == 8 ? 1 : lc) + dc;
    } else {
      tokens.push_back({Kind::literal, b, 0, 0});
      ++f;
      e += 2;
      d += 2;
    }
  }
  dictionary::internal::LzssParameters p{16777216, 3, 258, 0};
  core::DecoderLimits limits{};
  limits.max_block_size = 16777216;
  LzssFieldContextValidationContext c{static_cast<uint32_t>(tokens.size()), e,
                                      d, f, 0};
  const auto clean = query_lzss_position_distance_16m_token_range_encode(
      tokens, p, c, limits, 0, 0);
  require(clean.details.error ==
          LzssPositionDistance16mTokenRangeError::payload_output_too_small);
  std::vector<std::byte> out(clean.details.payload_size + 3, std::byte{0xa5}),
      scratch(out.size());
  const auto mode = data[0] % 12;
  switch (mode) {
  case 1:
    tokens.back().kind = static_cast<Kind>(99);
    break;
  case 2:
    tokens.back().length = 259;
    break;
  case 3:
    tokens.back().distance = 16777217;
    break;
  case 4:
    ++c.declared_event_count;
    break;
  case 5:
    ++c.declared_decision_count;
    break;
  case 6:
    ++c.declared_raw_size;
    break;
  case 7:
    p.flags = 1;
    break;
  case 8:
    limits.max_internal_buffered_bytes = 1;
    break;
  case 9:
    out.resize(clean.details.payload_size - 1);
    break;
  case 10:
    scratch.resize(clean.details.payload_size - 1);
    break;
  default:
    break;
  }
  ContextualDynamicRangeDescriptor desc{123, 456, 7};
  const auto saved = desc;
  const auto original = out;
  const auto r = encode_lzss_position_distance_16m_token_range(
      tokens, p, c, limits, out, scratch, desc);
  if (r.details.error != LzssPositionDistance16mTokenRangeError::none) {
    require(!r.bytes_committed && out == original &&
            std::memcmp(&desc, &saved, sizeof(desc)) == 0);
    return 0;
  }
  require(r.bytes_committed == clean.details.payload_size &&
          desc.decision_count == d && desc.context_count == 48);
  require(std::all_of(out.begin() + r.bytes_committed, out.end(),
                      [](auto b) { return b == std::byte{0xa5}; }));
  std::vector<ModeledOperation> ops(e), os(e);
  LzssFieldContextResult md{};
  const auto m =
      map_lzss_position_distance_16m_tokens(tokens, p, c, limits, ops, os, md);
  require(m.details.error == LzssFieldContextError::none);
  std::vector<std::byte> old(r.bytes_committed), priv(old.size());
  ContextualDynamicRangeDescriptor reference{};
  const auto re = encode_lzss_position_distance_16m_range_operations(
      ops, limits, old, priv, reference);
  require(re.details.error == ContextualDynamicRangeEncodeError::none &&
          std::equal(old.begin(), old.end(), out.begin()));
  std::vector<Token> decoded(tokens.size()), ds(tokens.size());
  const auto dr = decode_lzss_position_distance_16m_tokens(
      desc, std::span(out).first(r.bytes_committed), p, c, limits, decoded, ds);
  require(dr.error == LzssContextualRangeDecodeError::none);
  for (size_t i = 0; i < tokens.size(); ++i)
    require(decoded[i].kind == tokens[i].kind &&
            decoded[i].literal == tokens[i].literal &&
            decoded[i].distance == tokens[i].distance &&
            decoded[i].length == tokens[i].length);
  const auto repeat = out;
  require(encode_lzss_position_distance_16m_token_range(tokens, p, c, limits,
                                                       out, scratch, desc)
                  .details.error ==
              LzssPositionDistance16mTokenRangeError::none &&
          out == repeat);
  return 0;
}

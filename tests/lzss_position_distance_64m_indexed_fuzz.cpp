#include "context/lzss_position_distance_64m_mapper.hpp"
#include "dictionary/lzss_position_distance_64m_indexed.hpp"
#include "entropy/lzss_position_distance_64m_range_encoder.hpp"
#include "lzss_position_distance_64m_mapper_oracle.hpp"
#include "lzss_position_distance_64m_reference_oracle.hpp"
#include <array>
#include <cstdlib>
#include <cstring>
namespace {
using namespace marc;
using namespace dictionary::internal;
void require(bool b) {
  if (!b)
    std::abort();
}
} // namespace
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data,
                                      std::size_t size) {
  if (size < 18)
    return 0;
  auto u16 = [&](std::size_t n) {
    return unsigned(data[n]) | (unsigned(data[n + 1]) << 8);
  };
  const auto window = std::uint32_t(data[4]) | (std::uint32_t(data[5]) << 8) |
                      (std::uint32_t(data[6]) << 16) |
                      (std::uint32_t(data[7]) << 24);
  const LzssParameters p{window, data[14], u16(8), data[15]};
  core::DecoderLimits l{};
  l.max_lz_distance = 67108864;
  l.max_block_size = 128;
  l.max_frame_size = 128;
  l.max_internal_buffered_bytes = 67108864;
  const auto raw = std::as_bytes(
      std::span(data + 18, std::min<std::size_t>(size - 18, 128)));
  const auto oc = u16(0) % 129, sc = u16(2) % 129, retained = u16(10),
             committed = u16(12);
  constexpr LzssTypedToken guard{LzssTypedTokenKind::literal, 0xa5, 77, 88};
  std::vector<LzssTypedToken> out(oc, guard), scratch(sc, guard),
      out2(oc, guard), scratch2(sc, guard);
  const auto original =
      std::vector<std::byte>(std::as_bytes(std::span(out)).begin(),
                             std::as_bytes(std::span(out)).end());
  std::vector<std::uint32_t> ws(1048576 + u16(16) % 129, 123),
      ws2(ws.size(), 123);
  LzssPositionDistance64mParseMetadata meta{777, 888}, meta2{777, 888};
  std::array<std::byte, sizeof(meta)> before{};
  std::memcpy(before.data(), &meta, sizeof(meta));
  auto r = tokenize_lzss_position_distance_64m_indexed(
      raw, p, l, out, scratch, ws, meta, retained, committed);
  auto r2 = tokenize_lzss_position_distance_64m_indexed(
      raw, p, l, out2, scratch2, ws2, meta2, retained, committed);
  require(r.error == r2.error && r.tokens_committed == r2.tokens_committed &&
          r.details.token_count == r2.details.token_count &&
          r.details.raw_size == r2.details.raw_size);
  if (r.error != LzssPositionDistance64mParseError::none) {
    require(r.tokens_committed == 0 &&
            std::memcmp(&meta, before.data(), sizeof(meta)) == 0);
    require(meta2.token_count == 777 && meta2.raw_size == 888 &&
            std::ranges::equal(std::as_bytes(std::span(out)), original));
    for (auto t : out2)
      require(reference_oracle::equal(t, guard));
    return 0;
  }
  auto expected = reference_oracle::parse(raw, p);
  std::vector<LzssTypedToken> reference(raw.size()), rs(raw.size());
  LzssPositionDistance64mParseMetadata rm{};
  auto rr = tokenize_lzss_position_distance_64m_reference(
      raw, p, l, reference, rs, rm, retained, committed);
  require(rr.error == LzssPositionDistance64mParseError::none &&
          rr.tokens_committed == r.tokens_committed);
  for (std::size_t i = 0; i < r.tokens_committed; ++i)
    require(reference_oracle::equal(out[i], reference[i]));
  require(std::ranges::equal(ws, ws2));
  for (std::size_t i = 1048576 + raw.size(); i < ws.size(); ++i)
    require(ws[i] == 123);
  require(expected.size() == r.tokens_committed &&
          meta.token_count == meta2.token_count &&
          meta.raw_size == meta2.raw_size);
  for (std::size_t i = 0; i < out.size(); ++i)
    require(reference_oracle::equal(out[i], out2[i]) &&
            reference_oracle::equal(out[i],
                                    i < expected.size() ? expected[i] : guard));
  for (std::size_t i = 0; i < scratch.size(); ++i)
    require(reference_oracle::equal(scratch[i],
                                    i < expected.size() ? expected[i] : guard));
  auto tokens = std::span(out).first(r.tokens_committed);
  require(std::ranges::equal(reference_oracle::reconstruct(tokens), raw));
  auto c = mapper_oracle::counts(tokens);
  std::vector<context::internal::ModeledOperation> ops(c.declared_event_count),
      os(ops.size());
  context::internal::LzssFieldContextResult metadata{};
  auto mapped = context::internal::map_lzss_position_distance_64m_tokens(
      tokens, p, c, l, ops, os, metadata);
  require(mapped.details.error ==
          context::internal::LzssFieldContextError::none);
  std::array<std::byte, 2048> payload{}, ps{};
  entropy::internal::ContextualDynamicRangeDescriptor desc{};
  auto enc =
      entropy::internal::encode_lzss_position_distance_64m_range_operations(
          ops, l, payload, ps, desc);
  require(enc.details.error ==
          entropy::internal::ContextualDynamicRangeEncodeError::none);
  std::vector<LzssTypedToken> decoded(expected.size()), ds(expected.size());
  auto dec = context::internal::decode_lzss_position_distance_64m_tokens(
      desc, std::span(payload).first(enc.bytes_committed), p, c, l, decoded,
      ds);
  require(dec.error == context::internal::LzssContextualRangeDecodeError::none);
  for (std::size_t i = 0; i < expected.size(); ++i)
    require(reference_oracle::equal(decoded[i], expected[i]));
  require(std::ranges::equal(reference_oracle::reconstruct(decoded), raw));
  return 0;
}

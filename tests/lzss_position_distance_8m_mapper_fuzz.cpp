#include "context/lzss_position_distance_8m_mapper.hpp"
#include "entropy/lzss_position_distance_8m_range_encoder.hpp"
#include "lzss_position_distance_8m_mapper_oracle.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
namespace {
using namespace marc;
using namespace context::internal;
using namespace dictionary::internal;
void require(bool b) {
  if (!b)
    std::abort();
}
bool equal_result(const LzssFieldContextResult &a,
                  const LzssFieldContextResult &b) {
  return a.operation_count == b.operation_count &&
         a.operation_index == b.operation_index &&
         a.token_count == b.token_count && a.token_index == b.token_index &&
         a.decision_count == b.decision_count && a.raw_size == b.raw_size &&
         a.token_error == b.token_error && a.error == b.error;
}
} // namespace
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data,
                                      std::size_t size) {
  if (size < 16)
    return 0;
  auto u16 = [&](std::size_t n) {
    return unsigned(data[n]) | (unsigned(data[n + 1]) << 8);
  };
  auto u32 = [&](std::size_t n) {
    return std::uint32_t(data[n]) | (std::uint32_t(data[n + 1]) << 8) |
           (std::uint32_t(data[n + 2]) << 16) |
           (std::uint32_t(data[n + 3]) << 24);
  };
  const auto oc = u16(0) % 321, sc = u16(2) % 321;
  LzssFieldContextValidationContext c{u16(4), u16(6), u16(8), u16(10), 0};
  const LzssParameters p{u32(12), 3, 258, 0};
  core::DecoderLimits l{};
  l.max_block_size = 512;
  l.max_frame_size = 512;
  l.max_internal_buffered_bytes = 65536;
  l.max_compressed_payload_size = 65536;
  std::vector<LzssTypedToken> tokens;
  for (std::size_t n = 16; n + 10 <= size && tokens.size() < 64; n += 10)
    tokens.push_back({static_cast<LzssTypedTokenKind>(data[n]), data[n + 1],
                      u32(n + 2), u32(n + 6)});
  constexpr ModeledOperation guard{ModeledOperationKind::bypass_bits, 77, 88,
                                   99, 11};
  std::vector<ModeledOperation> out(oc, guard), scratch(sc, guard),
      out2(oc, guard), scratch2(sc, guard);
  const auto original_out =
      std::vector<std::byte>(std::as_bytes(std::span(out)).begin(),
                             std::as_bytes(std::span(out)).end());
  LzssFieldContextResult meta{}, meta2{};
  std::memset(&meta, 0xcc, sizeof(meta));
  std::memset(&meta2, 0xcc, sizeof(meta2));
  std::array<std::byte, sizeof(meta)> original_meta{};
  std::memcpy(original_meta.data(), &meta, sizeof(meta));
  auto r =
      map_lzss_position_distance_8m_tokens(tokens, p, c, l, out, scratch, meta);
  auto r2 = map_lzss_position_distance_8m_tokens(tokens, p, c, l, out2,
                                                 scratch2, meta2);
  require(equal_result(r.details, r2.details) &&
          r.operations_committed == r2.operations_committed);
  if (r.details.error != LzssFieldContextError::none) {
    require(r.operations_committed == 0 &&
            std::memcmp(&meta, original_meta.data(), sizeof(meta)) == 0);
    require(std::ranges::equal(std::as_bytes(std::span(out)), original_out));
    require(std::memcmp(&meta2, original_meta.data(), sizeof(meta2)) == 0);
    for (auto op : out2)
      require(mapper_oracle::equal(op, guard));
    return 0;
  }
  require(equal_result(meta, meta2) &&
          r.operations_committed == c.declared_event_count);
  const auto expected = mapper_oracle::fields(tokens);
  require(expected.size() == r.operations_committed);
  for (std::size_t i = 0; i < out.size(); ++i) {
    require(mapper_oracle::equal(out[i], out2[i]));
    require(mapper_oracle::equal(out[i],
                                 i < expected.size() ? expected[i] : guard));
  }
  for (std::size_t i = 0; i < scratch.size(); ++i)
    require(mapper_oracle::equal(scratch[i],
                                 i < expected.size() ? expected[i] : guard));
  std::array<std::byte, 2048> payload{}, byte_scratch{};
  entropy::internal::ContextualDynamicRangeDescriptor desc{};
  const auto enc =
      entropy::internal::encode_lzss_position_distance_8m_range_operations(
          std::span(out).first(expected.size()), l, payload, byte_scratch,
          desc);
  require(enc.details.error ==
          entropy::internal::ContextualDynamicRangeEncodeError::none);
  std::vector<LzssTypedToken> decoded(tokens.size()),
      token_scratch(tokens.size());
  const auto dec = decode_lzss_position_distance_8m_tokens(
      desc, std::span(payload).first(enc.bytes_committed), p, c, l, decoded,
      token_scratch);
  require(dec.error == LzssContextualRangeDecodeError::none);
  for (std::size_t i = 0; i < tokens.size(); ++i)
    require(decoded[i].kind == tokens[i].kind &&
            decoded[i].literal == tokens[i].literal &&
            decoded[i].distance == tokens[i].distance &&
            decoded[i].length == tokens[i].length);
  return 0;
}

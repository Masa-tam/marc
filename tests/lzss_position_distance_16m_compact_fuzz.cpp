#include "dictionary/lzss_position_distance_16m_compact.hpp"
#include "lzss_position_distance_16m_reference_oracle.hpp"
#include <cstdlib>

namespace {
void require(bool b) {
  if (!b)
    std::abort();
}
} // namespace
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data,
                                      std::size_t size) {
  using namespace marc;
  using namespace dictionary::internal;
  if (size < 4 || size > 132)
    return 0;
  const auto raw = std::as_bytes(std::span(data + 4, size - 4));
  LzssParameters p{16777216, 3, 258, 0};
  p.window_size = 1 + (data[0] % 128);
  p.max_match_length = 3 + (data[1] % 32);
  core::DecoderLimits l{};
  l.max_block_size = 16777216;
  std::vector<std::uint32_t> index(
      lzss_position_distance_16m_index_heads + raw.size(), 73);
  const auto oracle = reference_oracle::parse(raw, p);
  std::size_t exact = 0;
  for (auto t : oracle)
    exact += t.kind == LzssTypedTokenKind::literal ? 2 : 9;
  std::vector<std::byte> storage(2 * raw.size() + 3, std::byte{0xa5});
  const auto q = query_lzss_position_distance_16m_compact(
      raw, p, l, storage.size(), index);
  require(raw.empty()
              ? q.error == LzssPositionDistance16mParseError::invalid_parameters
              : q.error == LzssPositionDistance16mParseError::none);
  if (!raw.empty()) {
    require(q.byte_count == exact && q.details.token_count == oracle.size());
    auto r = write_lzss_position_distance_16m_compact_private(raw, p, l,
                                                              storage, index);
    require(r.error == LzssPositionDistance16mParseError::none &&
            r.byte_count == exact);
    LzssPositionDistance16mCompactReader reader(
        std::span<const std::byte>(storage).first(exact));
    for (auto t : oracle) {
      LzssTypedToken got{};
      require(reader.read(got) && reference_oracle::equal(t, got));
    }
    require(reader.empty());
    for (std::size_t i = exact; i < storage.size(); ++i)
      require(storage[i] == std::byte{0xa5});
    l.max_block_size = raw.size();
    l.max_internal_buffered_bytes = q.aggregate_bytes - 1;
    std::fill(storage.begin(), storage.end(), std::byte{0xa5});
    std::fill(index.begin(), index.end(), 73);
    r = write_lzss_position_distance_16m_compact_private(raw, p, l, storage,
                                                         index);
    require(r.error == LzssPositionDistance16mParseError::limit_exceeded);
    for (auto b : storage)
      require(b == std::byte{0xa5});
    for (auto x : index)
      require(x == 73);
  }
  // Independently exercise arbitrary serialized syntax and atomic refusal.
  LzssPositionDistance16mCompactReader reader(raw);
  while (!reader.empty()) {
    const auto pos = reader.position();
    const LzssTypedToken guard{LzssTypedTokenKind::literal, 7, 8, 9};
    auto t = guard;
    const unsigned tag = std::to_integer<unsigned>(raw[pos]);
    const std::size_t n = tag == 0 ? 2 : 9;
    const bool valid = tag <= 1 && raw.size() - pos >= n;
    require(reader.read(t) == valid);
    if (!valid) {
      require(reader.position() == pos && reference_oracle::equal(t, guard));
      break;
    }
    require(reader.position() == pos + n);
    if (tag == 0)
      require(t.kind == LzssTypedTokenKind::literal &&
              t.literal == std::to_integer<unsigned>(raw[pos + 1]) &&
              !t.distance && !t.length);
    else {
      std::uint32_t d = 0, len = 0;
      for (unsigned i = 0; i < 4; ++i) {
        d |= std::to_integer<std::uint32_t>(raw[pos + 1 + i]) << (8 * i);
        len |= std::to_integer<std::uint32_t>(raw[pos + 5 + i]) << (8 * i);
      }
      require(t.kind == LzssTypedTokenKind::match && t.distance == d &&
              t.length == len && !t.literal);
    }
  }
  return 0;
}

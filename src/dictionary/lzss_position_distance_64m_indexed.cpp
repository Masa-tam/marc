#include "dictionary/lzss_position_distance_64m_indexed.hpp"
#include "context/lzss_position_distance_64m_tokens.hpp"
#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"
#include <algorithm>
#include <array>
#include <limits>
namespace marc::dictionary::internal {
namespace {
using E = LzssPositionDistance64mParseError;
constexpr auto nil = std::numeric_limits<std::uint32_t>::max();
struct Region {
  const void *data;
  std::size_t bytes;
};
struct Working {
  LzssPositionDistance64mParseMetadata details{};
  LzssTypedToken token{};
  std::span<std::uint32_t> heads{}, links{};
  std::size_t position{}, source{}, length{}, best_distance{}, best_length{},
      maximum{}, next{};
  std::uint32_t key{}, bucket{};
};
constexpr std::size_t working_bytes =
    sizeof(Working) + 2 * sizeof(LzssPositionDistance64mParsePlan) +
    2 * sizeof(LzssPositionDistance64mParseResult) +
    sizeof(std::array<Region, 7>);
std::uint32_t key_at(std::span<const std::byte> raw, std::size_t pos) noexcept {
  return (std::to_integer<std::uint32_t>(raw[pos]) << 16) |
         (std::to_integer<std::uint32_t>(raw[pos + 1]) << 8) |
         std::to_integer<std::uint32_t>(raw[pos + 2]);
}
std::uint32_t bucket(std::uint32_t key) noexcept {
  return (key * UINT32_C(0x9e3779b1)) >> 12;
}
template <std::size_t N>
E aliases(const std::array<Region, N> &regions) noexcept {
  for (std::size_t i = 0; i < N; ++i)
    for (std::size_t j = i + 1; j < N; ++j) {
      const auto o = core::check_buffer_overlap(
          regions[i].data, regions[i].bytes, regions[j].data, regions[j].bytes);
      if (o != core::BufferOverlap::disjoint)
        return o == core::BufferOverlap::arithmetic_overflow
                   ? E::arithmetic_overflow
                   : E::overlapping_buffers;
    }
  return E::none;
}
LzssPositionDistance64mParseResult
run(std::span<const std::byte> raw, const LzssParameters &p,
    std::span<LzssTypedToken> scratch,
    std::span<std::uint32_t> workspace) noexcept {
  Working w{};
  w.heads = workspace.first(lzss_position_distance_64m_index_heads);
  w.links =
      workspace.subspan(lzss_position_distance_64m_index_heads, raw.size());
  std::ranges::fill(w.heads, nil);
  std::ranges::fill(w.links, nil);
  while (w.position < raw.size()) {
    w.maximum =
        std::min<std::size_t>(p.max_match_length, raw.size() - w.position);
    w.best_length = 0;
    w.best_distance = 0;
    if (w.maximum >= 5) {
      w.key = key_at(raw, w.position);
      w.bucket = bucket(w.key);
      w.source = w.heads[w.bucket];
      while (w.source != nil) {
        // Links created by this call are strictly decreasing raw positions.
        if (w.source >= w.position || w.source + 2 >= raw.size())
          return {w.details, 0, E::invalid_parameters};
        const auto distance = w.position - w.source;
        if (distance > p.window_size)
          break;
        // Chains are nearest first. An equal match cannot change the winner;
        // only a candidate extending the current best length can improve it.
        // best_length < maximum here (a maximum match already breaks), so
        // both tested bytes are in the supplied raw extent, even for overlap.
        const bool can_extend =
            w.best_length < 5 ||
            raw[w.source + w.best_length] == raw[w.position + w.best_length];
        if (can_extend && key_at(raw, w.source) == w.key) {
          w.length = 3;
          while (w.length < w.maximum &&
                 raw[w.position + w.length] == raw[w.source + w.length])
            ++w.length;
          if (w.length > w.best_length) {
            w.best_length = w.length;
            w.best_distance = distance;
          }
          if (w.best_length == w.maximum)
            break;
        }
        const auto link = w.links[w.source];
        if (link != nil && link >= w.source)
          return {w.details, 0, E::invalid_parameters};
        w.source = link;
      }
    }
    const bool use = w.best_length >= 5;
    w.token = use ? LzssTypedToken{LzssTypedTokenKind::match, 0,
                                   static_cast<std::uint32_t>(w.best_distance),
                                   static_cast<std::uint32_t>(w.best_length)}
                  : LzssTypedToken{
                        LzssTypedTokenKind::literal,
                        std::to_integer<std::uint8_t>(raw[w.position]), 0, 0};
    if (!scratch.empty()) {
      if (w.details.token_count >= scratch.size())
        return {w.details, 0, E::output_too_small};
      scratch[w.details.token_count] = w.token;
    }
    ++w.details.token_count;
    w.next = w.position + (use ? w.best_length : 1);
    // Insert EVERY consumed position, including positions inside a match.
    for (; w.position < w.next; ++w.position)
      if (raw.size() - w.position >= 3) {
        w.bucket = bucket(key_at(raw, w.position));
        w.links[w.position] = w.heads[w.bucket];
        w.heads[w.bucket] = static_cast<std::uint32_t>(w.position);
      }
  }
  w.details.raw_size = w.position;
  return {w.details, 0, E::none};
}
} // namespace
std::size_t lzss_position_distance_64m_indexed_working_bytes() noexcept {
  return working_bytes;
}
LzssPositionDistance64mParsePlan query_lzss_position_distance_64m_indexed(
    std::span<const std::byte> raw, const LzssParameters &p,
    const core::DecoderLimits &l, std::size_t oc, std::size_t sc,
    std::span<std::uint32_t> workspace, std::size_t retained,
    std::uint64_t committed) noexcept {
  LzssPositionDistance64mParsePlan q{};
  q.working_state_bytes = working_bytes;
  const auto pe =
      context::internal::validate_lzss_position_distance_64m_parameters(p, l);
  if (pe != LzssTypedTokenError::none) {
    q.error = pe == LzssTypedTokenError::limit_exceeded ? E::limit_exceeded
                                                        : E::invalid_parameters;
    return q;
  }
  if (raw.empty() || raw.size() > 67108864) {
    q.error = E::invalid_parameters;
    return q;
  }
  std::size_t ob{}, sb{}, wb{}, required{}, aggregate = raw.size();
  std::uint64_t total{};
  if (!core::checked_add(committed, static_cast<std::uint64_t>(raw.size()),
                         total) ||
      !core::checked_multiply(oc, sizeof(LzssTypedToken), ob) ||
      !core::checked_multiply(sc, sizeof(LzssTypedToken), sb) ||
      !core::checked_multiply(workspace.size(), sizeof(std::uint32_t), wb) ||
      !core::checked_add(raw.size(), lzss_position_distance_64m_index_heads,
                         required)) {
    q.error = E::arithmetic_overflow;
    return q;
  }
  for (auto n : {ob, sb, wb, working_bytes, retained})
    if (!core::checked_add(aggregate, n, aggregate)) {
      q.error = E::arithmetic_overflow;
      return q;
    }
  q.aggregate_bytes = aggregate;
  if (raw.size() > l.max_frame_size || raw.size() > l.max_block_size ||
      total > l.max_total_output_size ||
      aggregate > l.max_internal_buffered_bytes) {
    q.error = E::limit_exceeded;
    return q;
  }
  if (workspace.size() < required) {
    q.error = E::output_too_small;
    return q;
  }
  q.error =
      aliases(std::array{Region{raw.data(), raw.size()}, Region{&p, sizeof(p)},
                         Region{&l, sizeof(l)}, Region{workspace.data(), wb}});
  if (q.error != E::none)
    return q;
  const auto counted = run(raw, p, {}, workspace);
  q.details = counted.details;
  q.error = counted.error;
  if (q.error == E::none &&
      (oc < q.details.token_count || sc < q.details.token_count))
    q.error = E::output_too_small;
  return q;
}
LzssPositionDistance64mParseResult tokenize_lzss_position_distance_64m_indexed(
    std::span<const std::byte> raw, const LzssParameters &p,
    const core::DecoderLimits &l, std::span<LzssTypedToken> out,
    std::span<LzssTypedToken> scratch, std::span<std::uint32_t> workspace,
    LzssPositionDistance64mParseMetadata &meta, std::size_t retained,
    std::uint64_t committed) noexcept {
  LzssPositionDistance64mParseResult r{};
  std::size_t ob{}, sb{}, wb{};
  if (!core::checked_multiply(out.size(), sizeof(LzssTypedToken), ob) ||
      !core::checked_multiply(scratch.size(), sizeof(LzssTypedToken), sb) ||
      !core::checked_multiply(workspace.size(), sizeof(std::uint32_t), wb)) {
    r.error = E::arithmetic_overflow;
    return r;
  }
  r.error = aliases(std::array{
      Region{raw.data(), raw.size()}, Region{&p, sizeof(p)},
      Region{&l, sizeof(l)}, Region{out.data(), ob}, Region{scratch.data(), sb},
      Region{workspace.data(), wb}, Region{&meta, sizeof(meta)}});
  if (r.error != E::none)
    return r;
  const auto q = query_lzss_position_distance_64m_indexed(
      raw, p, l, out.size(), scratch.size(), workspace, retained, committed);
  r.details = q.details;
  r.error = q.error;
  if (r.error != E::none)
    return r;
  r = run(raw, p, scratch, workspace);
  if (r.error != E::none)
    return r;
  if (r.details.token_count != q.details.token_count ||
      r.details.raw_size != q.details.raw_size) {
    r.error = E::invalid_parameters;
    return r;
  }
  std::copy_n(scratch.begin(), r.details.token_count, out.begin());
  meta = r.details;
  r.tokens_committed = r.details.token_count;
  return r;
}
} // namespace marc::dictionary::internal

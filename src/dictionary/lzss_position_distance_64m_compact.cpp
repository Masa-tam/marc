#include "dictionary/lzss_position_distance_64m_compact.hpp"
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
  std::size_t byte_count{};
  std::span<std::uint32_t> heads{}, links{};
  std::size_t position{}, source{}, length{}, best_distance{}, best_length{},
      maximum{}, next{};
  std::uint32_t key{}, bucket{};
};
constexpr std::size_t working_bytes =
    sizeof(Working) + 2 * sizeof(LzssPositionDistance64mCompactPlan) +
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
LzssPositionDistance64mCompactPlan run(std::span<const std::byte> raw,
                                       const LzssParameters &p,
                                       std::span<std::byte> scratch,
                                       std::span<std::uint32_t> workspace,
                                       bool write) noexcept {
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
          return {w.details, w.byte_count, 0, 0, E::invalid_parameters};
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
          return {w.details, w.byte_count, 0, 0, E::invalid_parameters};
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
    const auto bytes = use ? 9u : 2u;
    if (write) {
      if (w.byte_count > scratch.size() ||
          scratch.size() - w.byte_count < bytes)
        return {w.details, w.byte_count, 0, 0, E::output_too_small};
      scratch[w.byte_count] = use ? std::byte{1} : std::byte{0};
      if (use) {
        if (!core::store_le(scratch, w.byte_count + 1, w.token.distance) ||
            !core::store_le(scratch, w.byte_count + 5, w.token.length))
          return {w.details, w.byte_count, 0, 0, E::arithmetic_overflow};
      } else
        scratch[w.byte_count + 1] = std::byte{w.token.literal};
    }
    w.byte_count += bytes;
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
  return {w.details, w.byte_count, 0, 0, E::none};
}
} // namespace
std::size_t lzss_position_distance_64m_compact_working_bytes() noexcept {
  return working_bytes;
}
namespace {
LzssPositionDistance64mCompactPlan
admit(std::span<const std::byte> raw, const LzssParameters &p,
      const core::DecoderLimits &l, std::size_t capacity,
      std::span<std::uint32_t> workspace, std::size_t retained,
      std::uint64_t committed) noexcept {
  LzssPositionDistance64mCompactPlan q{};
  q.working_state_bytes = working_bytes;
  const auto pe =
      context::internal::validate_lzss_position_distance_64m_parameters(p, l);
  if (pe != LzssTypedTokenError::none) {
    q.error = pe == LzssTypedTokenError::limit_exceeded ? E::limit_exceeded
                                                        : E::invalid_parameters;
    return q;
  }
  if (core::validate_limits(l) != core::LimitError::none) {
    q.error = E::limit_exceeded;
    return q;
  }
  if (raw.empty() || raw.size() > 67108864) {
    q.error = E::invalid_parameters;
    return q;
  }
  std::size_t wb{}, required{};
  std::uint64_t total{};
  if (!core::checked_multiply(workspace.size(), sizeof(std::uint32_t), wb) ||
      !core::checked_add(raw.size(), lzss_position_distance_64m_index_heads,
                         required) ||
      !core::checked_add(committed, static_cast<std::uint64_t>(raw.size()),
                         total)) {
    q.error = E::arithmetic_overflow;
    return q;
  }
  q.aggregate_bytes = raw.size();
  for (auto n : {wb, capacity, working_bytes, retained})
    if (!core::checked_add(q.aggregate_bytes, n, q.aggregate_bytes)) {
      q.error = E::arithmetic_overflow;
      return q;
    }
  if (raw.size() > l.max_frame_size || raw.size() > l.max_block_size ||
      total > l.max_total_output_size ||
      q.aggregate_bytes > l.max_internal_buffered_bytes) {
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
  return q;
}
} // namespace
LzssPositionDistance64mCompactPlan query_lzss_position_distance_64m_compact(
    std::span<const std::byte> raw, const LzssParameters &p,
    const core::DecoderLimits &l, std::size_t capacity,
    std::span<std::uint32_t> workspace, std::size_t retained,
    std::uint64_t committed) noexcept {
  auto q = admit(raw, p, l, capacity, workspace, retained, committed);
  if (q.error != E::none)
    return q;
  const auto counted = run(raw, p, {}, workspace, false);
  q.details = counted.details;
  q.byte_count = counted.byte_count;
  q.error = counted.error;
  if (q.error == E::none && capacity < q.byte_count)
    q.error = E::output_too_small;
  return q;
}
LzssPositionDistance64mCompactPlan
write_lzss_position_distance_64m_compact_private(
    std::span<const std::byte> raw, const LzssParameters &p,
    const core::DecoderLimits &l, std::span<std::byte> bytes,
    std::span<std::uint32_t> workspace, std::size_t retained,
    std::uint64_t committed) noexcept {
  std::size_t wb{};
  LzssPositionDistance64mCompactPlan q{};
  if (!core::checked_multiply(workspace.size(), sizeof(std::uint32_t), wb)) {
    q.error = E::arithmetic_overflow;
    return q;
  }
  q.error =
      aliases(std::array{Region{raw.data(), raw.size()}, Region{&p, sizeof(p)},
                         Region{&l, sizeof(l)}, Region{workspace.data(), wb},
                         Region{bytes.data(), bytes.size()}});
  if (q.error != E::none)
    return q;
  q = admit(raw, p, l, bytes.size(), workspace, retained, committed);
  if (q.error != E::none)
    return q;
  const auto written = run(raw, p, bytes, workspace, true);
  q.details = written.details;
  q.byte_count = written.byte_count;
  q.error = written.error;
  return q;
}
} // namespace marc::dictionary::internal

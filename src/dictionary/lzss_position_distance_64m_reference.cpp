#include "dictionary/lzss_position_distance_64m_reference.hpp"
#include "context/lzss_position_distance_64m_tokens.hpp"
#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"
#include <algorithm>
#include <array>
namespace marc::dictionary::internal {
namespace {
using E = LzssPositionDistance64mParseError;
struct Region {
  const void *data;
  std::size_t bytes;
};
struct Working {
  LzssPositionDistance64mParseMetadata details{};
  LzssTypedToken token{};
  std::size_t position{}, distance{}, length{}, best_distance{}, best_length{},
      maximum{}, history{};
};
constexpr std::size_t working_bytes =
    sizeof(Working) + 2 * sizeof(LzssPositionDistance64mParsePlan) +
    2 * sizeof(LzssPositionDistance64mParseResult) +
    sizeof(std::array<Region, 6>);
LzssPositionDistance64mParseResult
run(std::span<const std::byte> input, const LzssParameters &p,
    std::span<LzssTypedToken> scratch) noexcept {
  Working w{};
  while (w.position < input.size()) {
    w.maximum =
        std::min<std::size_t>(p.max_match_length, input.size() - w.position);
    w.history = std::min<std::size_t>(p.window_size, w.position);
    w.best_length = 0;
    w.best_distance = 0;
    if (w.maximum >= 5)
      for (w.distance = 1; w.distance <= w.history; ++w.distance) {
        w.length = 0;
        // Source may overlap target: raw input supplies the corresponding
        // bytes.
        while (w.length < w.maximum &&
               input[w.position + w.length] ==
                   input[w.position - w.distance + w.length])
          ++w.length;
        if (w.length > w.best_length) {
          w.best_length = w.length;
          w.best_distance = w.distance;
        }
        // Ascending distances already proved the nearest possible maximum
        // match.
        if (w.best_length == w.maximum)
          break;
      }
    const bool use = w.best_length >= 5;
    w.token = use ? LzssTypedToken{LzssTypedTokenKind::match, 0,
                                   static_cast<std::uint32_t>(w.best_distance),
                                   static_cast<std::uint32_t>(w.best_length)}
                  : LzssTypedToken{
                        LzssTypedTokenKind::literal,
                        std::to_integer<std::uint8_t>(input[w.position]), 0, 0};
    if (!scratch.empty()) {
      if (w.details.token_count >= scratch.size())
        return {w.details, 0, E::output_too_small};
      scratch[w.details.token_count] = w.token;
    }
    ++w.details.token_count;
    w.position += use ? w.best_length : 1;
  }
  w.details.raw_size = w.position;
  return {w.details, 0, E::none};
}
} // namespace
std::size_t lzss_position_distance_64m_parse_working_bytes() noexcept {
  return working_bytes;
}
LzssPositionDistance64mParsePlan query_lzss_position_distance_64m_reference(
    std::span<const std::byte> input, const LzssParameters &p,
    const core::DecoderLimits &l, std::size_t oc, std::size_t sc,
    std::size_t retained, std::uint64_t committed) noexcept {
  LzssPositionDistance64mParsePlan q{};
  q.working_state_bytes = working_bytes;
  const auto pe =
      context::internal::validate_lzss_position_distance_64m_parameters(p, l);
  if (pe != LzssTypedTokenError::none) {
    q.error = pe == LzssTypedTokenError::limit_exceeded ? E::limit_exceeded
                                                        : E::invalid_parameters;
    return q;
  }
  if (input.empty() || input.size() > 67108864) {
    q.error = E::invalid_parameters;
    return q;
  }
  std::uint64_t total{};
  std::size_t ob{}, sb{}, aggregate = input.size();
  if (!core::checked_add(committed, static_cast<std::uint64_t>(input.size()),
                         total) ||
      !core::checked_multiply(oc, sizeof(LzssTypedToken), ob) ||
      !core::checked_multiply(sc, sizeof(LzssTypedToken), sb)) {
    q.error = E::arithmetic_overflow;
    return q;
  }
  for (auto n : {ob, sb, working_bytes, retained})
    if (!core::checked_add(aggregate, n, aggregate)) {
      q.error = E::arithmetic_overflow;
      return q;
    }
  q.aggregate_bytes = aggregate;
  if (input.size() > l.max_frame_size || input.size() > l.max_block_size ||
      total > l.max_total_output_size ||
      aggregate > l.max_internal_buffered_bytes) {
    q.error = E::limit_exceeded;
    return q;
  }
  const auto counted = run(input, p, {});
  q.details = counted.details;
  q.error = counted.error;
  if (q.error == E::none &&
      (oc < q.details.token_count || sc < q.details.token_count))
    q.error = E::output_too_small;
  return q;
}
LzssPositionDistance64mParseResult
tokenize_lzss_position_distance_64m_reference(
    std::span<const std::byte> input, const LzssParameters &p,
    const core::DecoderLimits &l, std::span<LzssTypedToken> out,
    std::span<LzssTypedToken> scratch,
    LzssPositionDistance64mParseMetadata &meta, std::size_t retained,
    std::uint64_t committed) noexcept {
  LzssPositionDistance64mParseResult r{};
  std::size_t ob{}, sb{};
  if (!core::checked_multiply(out.size(), sizeof(LzssTypedToken), ob) ||
      !core::checked_multiply(scratch.size(), sizeof(LzssTypedToken), sb)) {
    r.error = E::arithmetic_overflow;
    return r;
  }
  const std::array regions{Region{input.data(), input.size()},
                           Region{&p, sizeof(p)},
                           Region{&l, sizeof(l)},
                           Region{out.data(), ob},
                           Region{scratch.data(), sb},
                           Region{&meta, sizeof(meta)}};
  for (std::size_t i = 0; i < regions.size(); ++i)
    for (std::size_t j = i + 1; j < regions.size(); ++j) {
      const auto o = core::check_buffer_overlap(
          regions[i].data, regions[i].bytes, regions[j].data, regions[j].bytes);
      if (o != core::BufferOverlap::disjoint) {
        r.error = o == core::BufferOverlap::arithmetic_overflow
                      ? E::arithmetic_overflow
                      : E::overlapping_buffers;
        return r;
      }
    }
  const auto q = query_lzss_position_distance_64m_reference(
      input, p, l, out.size(), scratch.size(), retained, committed);
  r.details = q.details;
  r.error = q.error;
  if (r.error != E::none)
    return r;
  r = run(input, p, scratch);
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

#include "lzss_position_distance_8m_max_fault_seam.hpp"
#include "core/checked_math.hpp"
#include <algorithm>
#include <exception>
#include <type_traits>
namespace marc::test::max8m {
namespace {
thread_local Late *late{};
thread_local Observer *observer{};
} // namespace
LateScope::LateScope(Late &c) noexcept : previous_(late) { late = &c; }
LateScope::~LateScope() { late = previous_; }
ObserveScope::ObserveScope(Observer &c) noexcept : previous_(observer) {
  observer = &c;
}
ObserveScope::~ObserveScope() { observer = previous_; }
Late *controller() noexcept { return late; }
Observer *allocation_observer() noexcept { return observer; }
std::size_t working_bytes() noexcept {
  return sizeof(Late) + 2 * sizeof(void *) + sizeof(LateScope) +
         sizeof(ObserveScope) + 128 * sizeof(std::size_t) + 4096 +
         entropy::internal::
             lzss_position_distance_8m_token_range_working_bytes() +
         frame::internal::
             lzss_position_distance_8m_prefix_serialize_working_bytes() +
         sizeof(entropy::internal::LzssPositionDistance8mRangeState) +
         4 * sizeof(frame::internal::TypedContextFrameLayout) +
         2 * sizeof(frame::internal::TypedContextFrameValidationContext) +
         2 * sizeof(entropy::internal::LzssPositionDistance8mTokenRangeResult) +
         2 * sizeof(frame::internal::LzssPositionDistance8mSerializeResult) +
         2 * sizeof(frame::internal::LzssPositionDistance8mFrameRequirements);
}
void Observer::reserve(std::size_t t, std::size_t b) {
  snapshots[0].tokens.resize(t);
  snapshots[1].tokens.resize(t);
  for (unsigned j = 2; j < 5; ++j)
    snapshots[j].bytes.resize(b);
}
std::size_t Observer::snapshot_bytes() const {
  std::size_t total{};
  for (const auto &s : snapshots) {
    std::size_t n{};
    if (!core::checked_multiply(s.tokens.capacity(), sizeof(Token), n) ||
        !core::checked_add(total, n, total) ||
        !core::checked_add(total, s.bytes.capacity(), total))
      std::terminate();
  }
  return total;
}
void Observer::capture() noexcept {
  before_live = live;
  before_deleted = deleted;
  unsigned ti{}, bi = 2;
  for (const auto &r : records)
    if (r.p && r.id > base) {
      if (snapshot_count >= 5) {
        valid = false;
        return;
      }
      const auto index = r.kind == 1 ? ti++ : bi++;
      if (index >= 5 || r.kind == 3) {
        valid = false;
        return;
      }
      auto &s = snapshots[index];
      s.record = r;
      ++snapshot_count;
      if (r.kind == 1) {
        if (r.count > s.tokens.size()) {
          valid = false;
          return;
        }
        std::copy_n(static_cast<const Token *>(r.p), r.count, s.tokens.begin());
      } else {
        if (r.bytes > s.bytes.size()) {
          valid = false;
          return;
        }
        std::copy_n(static_cast<const std::byte *>(r.p), r.bytes,
                    s.bytes.begin());
      }
    }
}
void Observer::arrived() noexcept {
  if (calls + 1 == snapshot_at)
    capture();
  ++calls;
}
void Observer::obtained(void *p, std::size_t n, std::size_t b,
                        unsigned k) noexcept {
  if (!p) {
    valid = false;
    return;
  }
  auto slot = std::find_if(records.begin(), records.end(),
                           [](auto &r) { return !r.p; });
  if (slot == records.end() || !core::checked_add(live, b, live)) {
    valid = false;
    std::terminate();
  }
  *slot = {p, n, b, calls, k};
  peak = std::max(peak, live);
  if (calls >= snapshot_at && calls < snapshot_at + 5 &&
      !core::checked_add(candidate_bytes, b, candidate_bytes))
    std::terminate();
}
void Observer::before_release(void *p, std::size_t n, unsigned k) noexcept {
  if (!p)
    return;
  auto r = std::find_if(records.begin(), records.end(),
                        [&](auto &r) { return r.p == p; });
  if (r == records.end() || r->count != n || r->kind != k) {
    valid = false;
    std::terminate();
  }
  if (fault && k == 2 && r->id == snapshot_at + 4)
    pristine = pristine && std::ranges::all_of(
                               std::span(static_cast<const std::byte *>(p), n),
                               [](auto b) { return b == sentinel; });
  if (fault && k == 2 && r->id == snapshot_at + 3)
    dirty = std::ranges::any_of(std::span(static_cast<const std::byte *>(p), n),
                                [](auto b) { return b != sentinel; });
}
void Observer::released(void *p) noexcept {
  if (!p)
    return;
  auto r = std::find_if(records.begin(), records.end(),
                        [&](auto &r) { return r.p == p; });
  if (r == records.end() || live < r->bytes)
    std::terminate();
  live -= r->bytes;
  *r = {};
  ++deleted;
}
bool Observer::preserved() const noexcept {
  if (!valid || live != before_live || deleted != before_deleted + 5 ||
      !pristine || !dirty)
    return false;
  for (const auto &s : snapshots)
    if (s.record.p) {
      const auto &r = s.record;
      if (!std::ranges::any_of(records, [&](auto &x) {
            return x.p == r.p && x.id == r.id && x.count == r.count &&
                   x.kind == r.kind;
          }))
        return false;
      if (r.kind == 1) {
        const auto *p = static_cast<const Token *>(r.p);
        for (std::size_t i = 0; i < r.count; ++i) {
          auto &a = p[i];
          auto &b = s.tokens[i];
          if (a.kind != b.kind || a.literal != b.literal ||
              a.distance != b.distance || a.length != b.length)
            return false;
        }
      } else if (!std::equal(s.bytes.begin(), s.bytes.begin() + r.bytes,
                             static_cast<const std::byte *>(r.p)))
        return false;
    }
  return true;
}
bool Observer::zero() const noexcept {
  return valid && !live && deleted == calls &&
         std::ranges::all_of(records, [](auto &r) { return !r.p; });
}
bool selected(Late *c, std::uint64_t prior, std::size_t raw) noexcept {
  return c && c->prior == prior && c->raw == raw;
}
bool context(Late &c,
             const frame::internal::TypedContextFrameValidationContext &x,
             std::size_t raw) noexcept {
  c.valid = c.valid && x.expected_sequence == c.sequence &&
            x.stream.frame_size == c.frame &&
            x.output_already_committed == c.prior &&
            c.prior <= x.stream.original_size &&
            raw == std::min<std::uint64_t>(x.stream.frame_size,
                                           x.stream.original_size - c.prior);
  return c.valid;
}
bool inject(Late &c, unsigned mode) noexcept {
  if (c.mode != mode)
    return false;
  if (c.injections++)
    c.valid = false;
  return true;
}
} // namespace marc::test::max8m
namespace marc::entropy::internal {
LzssPositionDistance8mTokenRangeResult
max_test_encode_lzss_position_distance_8m_token_range(
    std::span<const dictionary::internal::LzssTypedToken> tokens,
    const dictionary::internal::LzssParameters &p,
    const context::internal::LzssFieldContextValidationContext &counts,
    const core::DecoderLimits &l, std::span<std::byte> output,
    std::span<std::byte> scratch, ContextualDynamicRangeDescriptor &d,
    std::size_t retained) noexcept {
  auto r = encode_lzss_position_distance_8m_token_range(
      tokens, p, counts, l, output, scratch, d, retained);
  auto *c = test::max8m::controller();
  if (!test::max8m::selected(
          c, counts.output_already_committed,
          static_cast<std::size_t>(counts.declared_raw_size)))
    return r;
  ++c->range;
  c->range_bytes = r.bytes_committed;
  c->payload = d.payload_size;
  c->valid = c->valid &&
             r.details.error == LzssPositionDistance8mTokenRangeError::none &&
             r.bytes_committed > 0 && r.bytes_committed == d.payload_size;
  if (test::max8m::inject(*c, 1))
    r.details.error = LzssPositionDistance8mTokenRangeError::internal_error;
  return r;
}
} // namespace marc::entropy::internal
namespace marc::frame::internal {
LzssPositionDistance8mSerializeResult
max_test_serialize_lzss_position_distance_8m_frame_prefix(
    const TypedContextFrameLayout &layout,
    const TypedContextFrameValidationContext &x, std::span<std::byte> out,
    std::size_t &written, std::size_t retained) noexcept {
  auto r = serialize_lzss_position_distance_8m_frame_prefix(layout, x, out,
                                                            written, retained);
  auto *c = test::max8m::controller();
  if (!test::max8m::selected(c, x.output_already_committed,
                             layout.header.uncompressed_size))
    return r;
  ++c->prefix;
  c->prefix_bytes = written;
  test::max8m::context(*c, x, layout.header.uncompressed_size);
  c->valid = c->valid &&
             r.error == LzssPositionDistance8mSerializeError::none &&
             written == 80 && r.bytes_committed == 80;
  return r;
}
LzssPositionDistance8mPreflightError
max_test_preflight_lzss_position_distance_8m_frame_prefix(
    std::span<const std::byte> input,
    const TypedContextFrameValidationContext &x,
    TypedContextFrameLayout &layout, LzssPositionDistance8mFrameRequirements &q,
    std::size_t retained) noexcept {
  auto r = preflight_lzss_position_distance_8m_frame_prefix(input, x, layout, q,
                                                            retained);
  auto *c = test::max8m::controller();
  if (!c || x.output_already_committed != c->prior)
    return r;
  if (x.output_already_committed > x.stream.original_size) {
    c->valid = false;
    return r;
  }
  const auto raw = static_cast<std::size_t>(std::min<std::uint64_t>(
      x.stream.frame_size,
      x.stream.original_size - x.output_already_committed));
  if (!test::max8m::selected(c, x.output_already_committed, raw))
    return r;
  ++c->reparse;
  test::max8m::context(*c, x, raw);
  c->parsed_sequence = layout.header.sequence;
  c->valid = c->valid && r == LzssPositionDistance8mPreflightError::none &&
             layout.header.sequence == c->sequence;
  if (test::max8m::inject(*c, 6) &&
      !core::checked_add(layout.header.sequence, std::uint64_t{1},
                         layout.header.sequence))
    c->valid = false;
  return r;
}
namespace {
template <class T>
auto max_obtain(LzssPositionDistance8mExactStreamAllocator &a,
                std::size_t n) noexcept {
  auto *c = test::max8m::allocation_observer();
  if (c)
    c->arrived();
  auto b = [&] {
    if constexpr (std::is_same_v<T, std::byte>)
      return owning8m_bytes(a, n);
    else if constexpr (std::is_same_v<T, std::uint32_t>)
      return owning8m_indices(a, n);
    else
      return owning8m_tokens(a, n);
  }();
  if (c && b.data) {
    std::size_t bytes{};
    if (!core::checked_multiply(b.capacity, sizeof(T), bytes))
      std::terminate();
    if constexpr (std::is_same_v<T, std::byte>)
      std::fill_n(b.data, b.capacity, test::max8m::sentinel);
    c->obtained(b.data, b.capacity, bytes,
                std::is_same_v<T, std::byte>       ? 2
                : std::is_same_v<T, std::uint32_t> ? 3
                                                   : 1);
  }
  return b;
}
template <class T>
void max_drop(LzssPositionDistance8mExactStreamAllocator &a,
              LzssPositionDistance8mOwnedBlock<T> &b) noexcept {
  auto *c = test::max8m::allocation_observer();
  auto *p = b.data;
  if (c)
    c->before_release(p, b.capacity,
                      std::is_same_v<T, std::byte>       ? 2
                      : std::is_same_v<T, std::uint32_t> ? 3
                                                         : 1);
  if constexpr (std::is_same_v<T, std::byte>)
    owning8m_release_bytes(a, b);
  else if constexpr (std::is_same_v<T, std::uint32_t>)
    owning8m_release_indices(a, b);
  else
    owning8m_release_tokens(a, b);
  if (c) {
    c->valid = c->valid && !b.data && !b.capacity;
    c->released(p);
  }
}
} // namespace
LzssPositionDistance8mOwnedTokens
max_test_owning8m_tokens(LzssPositionDistance8mExactStreamAllocator &a,
                         std::size_t n) noexcept {
  return max_obtain<dictionary::internal::LzssTypedToken>(a, n);
}
LzssPositionDistance8mOwnedBytes
max_test_owning8m_bytes(LzssPositionDistance8mExactStreamAllocator &a,
                        std::size_t n) noexcept {
  return max_obtain<std::byte>(a, n);
}
LzssPositionDistance8mOwnedIndex
max_test_owning8m_indices(LzssPositionDistance8mExactStreamAllocator &a,
                          std::size_t n) noexcept {
  return max_obtain<std::uint32_t>(a, n);
}
void max_test_owning8m_release_tokens(
    LzssPositionDistance8mExactStreamAllocator &a,
    LzssPositionDistance8mOwnedTokens &b) noexcept {
  max_drop(a, b);
}
void max_test_owning8m_release_bytes(
    LzssPositionDistance8mExactStreamAllocator &a,
    LzssPositionDistance8mOwnedBytes &b) noexcept {
  max_drop(a, b);
}
void max_test_owning8m_release_indices(
    LzssPositionDistance8mExactStreamAllocator &a,
    LzssPositionDistance8mOwnedIndex &b) noexcept {
  max_drop(a, b);
}
} // namespace marc::frame::internal

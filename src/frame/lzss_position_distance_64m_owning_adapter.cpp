#include "frame/lzss_position_distance_64m_owning_adapter.hpp"
#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"
#include <algorithm>
#include <exception>
#include <limits>
#include <type_traits>
namespace marc::frame::internal {
namespace {
using Adapter = LzssPositionDistance64mOwningAdapter;
using Code = core::ErrorCode;
bool add(std::size_t &n, std::size_t x) noexcept {
  return core::checked_add(n, x, n);
}
bool disjoint(const void *a, std::size_t n, const void *b,
              std::size_t m) noexcept {
  return core::check_buffer_overlap(a, n, b, m) ==
         core::BufferOverlap::disjoint;
}
template <class T>
constexpr unsigned kind = std::is_same_v<T, std::byte>       ? 1
                          : std::is_same_v<T, std::uint32_t> ? 2
                                                             : 3;
template <class T>
auto allocate(LzssPositionDistance64mExactStreamAllocator &a,
              std::size_t n) noexcept {
  if constexpr (std::is_same_v<T, std::byte>)
    return owning64m_bytes(a, n);
  else {
    static_assert(std::is_same_v<T, std::uint32_t>);
    return owning64m_indices(a, n);
  }
}
template <class T>
void delegate_release(LzssPositionDistance64mExactStreamAllocator &a,
                      LzssPositionDistance64mOwnedBlock<T> &b) noexcept {
  if constexpr (std::is_same_v<T, std::byte>)
    owning64m_release_bytes(a, b);
  else {
    static_assert(std::is_same_v<T, std::uint32_t>);
    owning64m_release_indices(a, b);
  }
}
} // namespace
std::size_t Adapter::Allocator::working() noexcept {
  // Includes delegate's 128-byte working contract, wrapper controls and scans.
  return 128 + 4 * sizeof(Receipt) + 8 * sizeof(void *) +
         16 * sizeof(std::size_t);
}
LzssPositionDistance64mAllocatorControls
Adapter::Allocator::controls() const noexcept {
  return {this, sizeof(*this), working()};
}
template <class T>
LzssPositionDistance64mOwnedBlock<T>
Adapter::Allocator::obtain(std::size_t n) noexcept {
  if (!core::checked_add(state.calls, std::size_t{1}, state.calls))
    return {};
  std::size_t bytes{}, total = reserve;
  auto slot = std::find_if(receipts.begin(), receipts.end(),
                           [](const auto &r) { return !r.data; });
  if (!n || slot == receipts.end() ||
      !core::checked_multiply(n, sizeof(T), bytes) || !add(total, state.live) ||
      !add(total, bytes) || total > limit)
    return {};
  auto b = allocate<T>(exact, n);
  if (!b.data && !b.capacity)
    return {};
  // Exact delegate owns all successful returns. In tests malformed returns
  // must also be independently owned, so rejection can destroy them safely.
  bool valid =
      b.data && b.capacity == n && disjoint(b.data, bytes, this, sizeof(*this));
  for (const auto &r : receipts)
    valid = valid && disjoint(b.data, bytes, r.data, r.bytes);
  if (!valid) {
    delegate_release<T>(exact, b);
    return {};
  }
  *slot = {b.data, n, bytes, kind<T>};
  state.live += bytes;
  state.peak = std::max(state.peak, state.live);
  ++state.blocks;
  return b;
}
template <class T>
void Adapter::Allocator::drop(
    LzssPositionDistance64mOwnedBlock<T> &b) noexcept {
  if (!b.data && !b.capacity)
    return;
  auto slot = std::find_if(receipts.begin(), receipts.end(),
                           [&](const auto &r) { return r.data == b.data; });
  if (slot == receipts.end() || slot->count != b.capacity ||
      slot->kind != kind<T>)
    std::terminate();
  const auto bytes = slot->bytes;
  delegate_release<T>(exact,
                      b); // Real delegate deletion precedes receipt clearing.
  if (b.data || b.capacity)
    std::terminate();
  *slot = {};
  state.live -= bytes;
  --state.blocks;
  ++state.released;
}
Adapter::Allocator::~Allocator() {
  if (state.live || state.blocks)
    std::terminate();
}
LzssPositionDistance64mOwnedBytes
Adapter::Allocator::bytes(std::size_t n) noexcept {
  return obtain<std::byte>(n);
}
LzssPositionDistance64mOwnedIndex
Adapter::Allocator::indices(std::size_t n) noexcept {
  return obtain<std::uint32_t>(n);
}
void Adapter::Allocator::release(
    LzssPositionDistance64mOwnedBytes &b) noexcept {
  drop(b);
}
void Adapter::Allocator::release(
    LzssPositionDistance64mOwnedIndex &b) noexcept {
  drop(b);
}
std::size_t Adapter::controls_bytes() noexcept {
  return sizeof(LzssPositionDistance64mOwningConfig) +
         sizeof(LzssPositionDistance64mOwningRequirements) +
         4 * sizeof(core::ProcessResult) + sizeof(core::DecoderLimits) +
         sizeof(TypedContextStreamHeader) + 16 * sizeof(std::size_t) +
         8 * sizeof(void *) + 4 * sizeof(std::span<std::byte>);
}
LzssPositionDistance64mOwningRequirements
Adapter::query(const LzssPositionDistance64mOwningConfig &c) noexcept {
  LzssPositionDistance64mOwningRequirements r{};
  auto refuse = [&](Code e) {
    r.error = e;
    return r;
  };
  if (core::validate_limits(c.limits) != core::LimitError::none)
    return refuse(Code::invalid_argument);
  if (c.limits.max_internal_buffered_bytes >
      std::numeric_limits<std::size_t>::max())
    return refuse(Code::limit_exceeded);
  auto e =
      validate_lzss_position_distance_64m_stream_semantics(c.stream, c.limits);
  if (e != LzssPositionDistance64mPreflightError::none)
    return refuse(e == LzssPositionDistance64mPreflightError::limit_exceeded
                      ? Code::limit_exceeded
                      : Code::invalid_argument);
  const auto w = LzssPositionDistance64mOwnedStreamEncoder::working_bytes();
  // The unchanged coordinator sums its object and controls with the maximum
  // sequential helper charge. No transfer or discount of that grant here.
  if (w < LzssPositionDistance64mCompactOwner::working_bytes())
    return refuse(Code::internal_error);
  r.external_charge = sizeof(Adapter);
  for (auto n :
       {controls_bytes(), c.external, c.input_capacity, c.output_capacity})
    if (!add(r.external_charge, n))
      return refuse(Code::limit_exceeded);
  r.fixed_bytes = r.external_charge;
  for (auto n : {w, sizeof(Allocator), Allocator::working(), c.input_capacity,
                 c.output_capacity})
    if (!add(r.fixed_bytes, n))
      return refuse(Code::limit_exceeded);
  r.raw_bytes = static_cast<std::size_t>(
      std::min<std::uint64_t>(c.stream.frame_size, c.stream.original_size));
  std::size_t index_bytes{};
  if (r.raw_bytes &&
      (!core::checked_add(std::size_t{1048576}, r.raw_bytes, r.index_entries) ||
       !core::checked_multiply(r.index_entries, sizeof(std::uint32_t),
                               index_bytes)))
    return refuse(Code::limit_exceeded);
  r.initial_bytes = r.fixed_bytes;
  if (!add(r.initial_bytes, r.raw_bytes) ||
      !add(r.initial_bytes, index_bytes) ||
      r.initial_bytes > c.limits.max_internal_buffered_bytes)
    return refuse(Code::limit_exceeded);
  return r;
}
Adapter::LzssPositionDistance64mOwningAdapter(
    const LzssPositionDistance64mOwningConfig &c) noexcept
    : input_capacity_(c.input_capacity), output_capacity_(c.output_capacity) {
  const auto r = query(c);
  if (r.error != Code::none) {
    fail(r.error);
    return;
  }
  allocator_.reserve = r.fixed_bytes;
  allocator_.limit =
      static_cast<std::size_t>(c.limits.max_internal_buffered_bytes);
  // Query proves all initial prospective requests before either callback.
  coordinator_.emplace(c.stream, c.limits, allocator_, r.external_charge);
}
Adapter::~LzssPositionDistance64mOwningAdapter() {
  coordinator_.reset();
  if (allocator_.state.live || allocator_.state.blocks)
    std::terminate();
}
Adapter::Ledger Adapter::ledger() const noexcept { return allocator_.state; }
core::ProcessResult Adapter::fail(Code e) noexcept {
  ended_ = true;
  terminal_ = {0, 0, core::StreamStatus::error, {e, accepted_, 0}};
  return terminal_;
}
core::ProcessResult Adapter::process(std::span<const std::byte> in,
                                     std::span<std::byte> out,
                                     std::uint32_t flags) noexcept {
  if (ended_)
    return {0, 0, terminal_.status, terminal_.error};
  if (in.size() > input_capacity_ || out.size() > output_capacity_ ||
      !disjoint(in.data(), in.size(), out.data(), out.size()) ||
      !disjoint(in.data(), in.size(), this, sizeof(*this)) ||
      !disjoint(out.data(), out.size(), this, sizeof(*this)))
    return fail(Code::invalid_argument);
  auto r = coordinator_->process(in, out, flags);
  if (!core::checked_add(
          accepted_, static_cast<std::uint64_t>(r.input_consumed), accepted_))
    std::terminate();
  if (r.status == core::StreamStatus::error ||
      r.status == core::StreamStatus::end_of_stream) {
    ended_ = true;
    terminal_ = r;
  }
  return r;
}
} // namespace marc::frame::internal

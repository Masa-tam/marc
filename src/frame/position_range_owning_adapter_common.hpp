#ifndef MARC_FRAME_POSITION_RANGE_OWNING_ADAPTER_COMMON_HPP
#define MARC_FRAME_POSITION_RANGE_OWNING_ADAPTER_COMMON_HPP
#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"
#include "frame/typed_context_format.hpp"
#include <algorithm>
#include <exception>
#include <limits>
#include <span>
#include <type_traits>
namespace marc::frame::internal {
// Stateless receipt/admission mechanics; concrete adapter and allocator own state.
struct PositionRangeOwningAdapterMechanics {
  using Code = core::ErrorCode;
  template <class Owner> using AdapterAllocator = typename Owner::Allocator;
  template <class T>
  static constexpr unsigned kind = std::is_same_v<T, std::byte>       ? 1
                                   : std::is_same_v<T, std::uint32_t> ? 2
                                                                      : 3;
  static bool add(std::size_t &n, std::size_t x) noexcept {
    return core::checked_add(n, x, n);
  }
  static bool disjoint(const void *a, std::size_t n, const void *b,
                       std::size_t m) noexcept {
    return core::check_buffer_overlap(a, n, b, m) ==
           core::BufferOverlap::disjoint;
  }
  template <class A> static std::size_t allocator_working() noexcept {
    using Receipt = typename A::Receipt;
    // Includes delegate's 128-byte working contract, wrapper controls and scans.
    return 128 + 4 * sizeof(Receipt) + 8 * sizeof(void *) +
           16 * sizeof(std::size_t);
  }
  template <class Profile, class A>
  static typename Profile::AllocatorControls
  allocator_controls(const A &self) noexcept {
    return {&self, sizeof(self), self.working()};
  }
  template <class Profile, class T, class A>
  static typename Profile::template OwnedBlock<T>
  obtain(A &self, std::size_t n) noexcept {
    if (!core::checked_add(self.state.calls, std::size_t{1}, self.state.calls))
      return {};
    std::size_t bytes{}, total = self.reserve;
    auto slot = std::find_if(self.receipts.begin(), self.receipts.end(),
                             [](const auto &r) { return !r.data; });
    if (!n || slot == self.receipts.end() ||
        !core::checked_multiply(n, sizeof(T), bytes) ||
        !add(total, self.state.live) || !add(total, bytes) ||
        total > self.limit)
      return {};
    auto b = Profile::template allocate_block<T>(self.exact, n);
    if (!b.data && !b.capacity)
      return {};
    // Exact delegate owns all successful returns. In tests malformed returns
    // must also be independently owned, so rejection can destroy them safely.
    bool valid = b.data && b.capacity == n &&
                 disjoint(b.data, bytes, &self, sizeof(self));
    for (const auto &r : self.receipts)
      valid = valid && disjoint(b.data, bytes, r.data, r.bytes);
    if (!valid) {
      Profile::template release_block<T>(self.exact, b);
      return {};
    }
    *slot = {b.data, n, bytes, kind<T>};
    self.state.live += bytes;
    self.state.peak = std::max(self.state.peak, self.state.live);
    ++self.state.blocks;
    return b;
  }
  template <class Profile, class T, class A>
  static void drop(A &self,
                   typename Profile::template OwnedBlock<T> &b) noexcept {
    if (!b.data && !b.capacity)
      return;
    auto slot = std::find_if(self.receipts.begin(), self.receipts.end(),
                             [&](const auto &r) { return r.data == b.data; });
    if (slot == self.receipts.end() || slot->count != b.capacity ||
        slot->kind != kind<T>)
      std::terminate();
    const auto bytes = slot->bytes;
    Profile::template release_block<T>(
        self.exact,
        b); // Real delegate deletion precedes receipt clearing.
    if (b.data || b.capacity)
      std::terminate();
    *slot = {};
    self.state.live -= bytes;
    --self.state.blocks;
    ++self.state.released;
  }
  template <class Profile> static std::size_t controls_bytes() noexcept {
    return sizeof(typename Profile::OwningConfig) +
           sizeof(typename Profile::OwningRequirements) +
           4 * sizeof(core::ProcessResult) + sizeof(core::DecoderLimits) +
           sizeof(TypedContextStreamHeader) + 16 * sizeof(std::size_t) +
           8 * sizeof(void *) + 4 * sizeof(std::span<std::byte>);
  }
  template <class Profile, class Owner>
  static typename Profile::OwningRequirements
  query(const typename Profile::OwningConfig &c) noexcept {
    using Allocator = AdapterAllocator<Owner>;
    typename Profile::OwningRequirements r{};
    auto refuse = [&](Code e) {
      r.error = e;
      return r;
    };
    if (core::validate_limits(c.limits) != core::LimitError::none)
      return refuse(Code::invalid_argument);
    if (c.limits.max_internal_buffered_bytes >
        std::numeric_limits<std::size_t>::max())
      return refuse(Code::limit_exceeded);
    auto e = Profile::validate_stream(c.stream, c.limits);
    if (e != Profile::PreflightError::none)
      return refuse(e == Profile::PreflightError::limit_exceeded
                        ? Code::limit_exceeded
                        : Code::invalid_argument);
    const auto w = Profile::OwnedStreamEncoder::working_bytes();
    // The unchanged coordinator sums its object and controls with the maximum
    // sequential helper charge. No transfer or discount of that grant here.
    if (w < Profile::CompactOwner::working_bytes())
      return refuse(Code::internal_error);
    r.external_charge = sizeof(Owner);
    for (auto n : {Owner::controls_bytes(), c.external, c.input_capacity,
                   c.output_capacity})
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
        (!core::checked_add(std::size_t{1048576}, r.raw_bytes,
                            r.index_entries) ||
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
  template <class Profile, class Owner>
  static void initialize(Owner &self,
                         const typename Profile::OwningConfig &c) noexcept {
    const auto r = Owner::query(c);
    if (r.error != Code::none) {
      self.fail(r.error);
      return;
    }
    self.allocator_.reserve = r.fixed_bytes;
    self.allocator_.limit =
        static_cast<std::size_t>(c.limits.max_internal_buffered_bytes);
    // Query proves all initial prospective requests before either callback.
    self.coordinator_.emplace(c.stream, c.limits, self.allocator_,
                              r.external_charge);
  }
  template <class Owner> static void destroy(Owner &self) noexcept {
    self.coordinator_.reset();
    if (self.allocator_.state.live || self.allocator_.state.blocks)
      std::terminate();
  }
  template <class Owner>
  static core::ProcessResult fail(Owner &self, core::ErrorCode e) noexcept {
    self.ended_ = true;
    self.terminal_ = {0, 0, core::StreamStatus::error, {e, self.accepted_, 0}};
    return self.terminal_;
  }
  template <class Owner>
  static core::ProcessResult process(Owner &self, std::span<const std::byte> in,
                                     std::span<std::byte> out,
                                     std::uint32_t flags) noexcept {
    if (self.ended_)
      return {0, 0, self.terminal_.status, self.terminal_.error};
    if (in.size() > self.input_capacity_ ||
        out.size() > self.output_capacity_ ||
        !disjoint(in.data(), in.size(), out.data(), out.size()) ||
        !disjoint(in.data(), in.size(), &self, sizeof(self)) ||
        !disjoint(out.data(), out.size(), &self, sizeof(self)))
      return self.fail(Code::invalid_argument);
    auto r = self.coordinator_->process(in, out, flags);
    if (!core::checked_add(self.accepted_,
                           static_cast<std::uint64_t>(r.input_consumed),
                           self.accepted_))
      std::terminate();
    if (r.status == core::StreamStatus::error ||
        r.status == core::StreamStatus::end_of_stream) {
      self.ended_ = true;
      self.terminal_ = r;
    }
    return r;
  }
};
} // namespace marc::frame::internal
#endif

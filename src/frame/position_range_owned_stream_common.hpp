#ifndef MARC_FRAME_POSITION_RANGE_OWNED_STREAM_COMMON_HPP
#define MARC_FRAME_POSITION_RANGE_OWNED_STREAM_COMMON_HPP
#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"
#include "frame/typed_context_format.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <exception>
#include <limits>
#include <span>
namespace marc::frame::internal {
// Stateless mechanics for the three matching large-window Range coordinators.
struct PositionRangeOwnedStreamMechanics {
  using Code = core::ErrorCode;
  static bool add(std::size_t &n, std::size_t x) noexcept {
    return core::checked_add(n, x, n);
  }
  static bool subtract(std::size_t &n, std::size_t x) noexcept {
    if (x > n)
      return false;
    n -= x;
    return true;
  }
  template <class Profile, class Bridge>
  static typename Profile::AllocatorControls
  bridge_controls(const Bridge &self) noexcept {
    auto c = self.source.controls();
    std::size_t working = Profile::callback_bytes;
    if (!add(working, c.bytes) || !add(working, c.working_bytes))
      working = std::numeric_limits<std::size_t>::max();
    return {&self, sizeof(self), working};
  }
  template <class Bridge>
  static void bridge_remember(Bridge &self, const void *p,
                              std::size_t n) noexcept {
    if (!p)
      return;
    for (auto &r : self.regions)
      if (!r.data) {
        r = {p, n};
        return;
      }
    // Contract admits at most one current and four candidate blocks.
    std::terminate();
  }
  template <class Bridge>
  static void bridge_forget(Bridge &self, const void *p) noexcept {
    if (!p)
      return;
    for (auto &r : self.regions)
      if (r.data == p) {
        r = {};
        return;
      }
    std::terminate();
  }
  template <class Profile, class Bridge>
  static typename Profile::OwnedBytes bridge_bytes(Bridge &self,
                                                   std::size_t n) noexcept {
    auto b = self.source.bytes(n);
    self.remember(b.data, b.capacity);
    return b;
  }
  template <class Profile, class Bridge>
  static void bridge_release(Bridge &self,
                             typename Profile::OwnedBytes &b) noexcept {
    auto p = b.data;
    self.source.release(b);
    self.forget(p);
  }
  template <class Bridge>
  static bool bridge_bytes_live(const Bridge &self, std::size_t &out) noexcept {
    out = 0;
    for (const auto &r : self.regions)
      if (!add(out, r.bytes))
        return false;
    return true;
  }
  template <class Profile, class Owner>
  static std::size_t working_bytes() noexcept {
    using CompactOwner = typename Profile::CompactOwner;
    return sizeof(Owner) + Profile::call_bytes + Profile::callback_bytes +
           std::max(CompactOwner::working_bytes() - sizeof(CompactOwner),
                    Profile::serialize_working_bytes());
  }
  template <class Owner>
  static bool budget(const Owner &self, std::size_t in, std::size_t out,
                     std::size_t &total) noexcept {
    if (!self.bridge_.bytes_live(total))
      return false;
    for (auto n : {Owner::working_bytes(), self.allocator_controls_.bytes,
                   self.allocator_controls_.working_bytes, self.external_,
                   self.raw_.capacity, self.index_bytes_, in, out})
      if (!add(total, n))
        return false;
    return total <= self.limits_.max_internal_buffered_bytes;
  }
  template <class Owner>
  static bool disjoint(const Owner &self, std::span<const std::byte> in,
                       std::span<std::byte> out) noexcept {
    using Region = typename Owner::Region;
    const std::array regions{
        Region{in.data(), in.size()},
        Region{out.data(), out.size()},
        Region{&self, sizeof(self)},
        Region{self.allocator_controls_.data, self.allocator_controls_.bytes},
        Region{self.raw_.data, self.raw_.capacity},
        Region{self.index_.data, self.index_bytes_}};
    auto disjoint = [](Region a, Region b) {
      return core::check_buffer_overlap(a.data, a.bytes, b.data, b.bytes) ==
             core::BufferOverlap::disjoint;
    };
    for (std::size_t i = 0; i < regions.size(); ++i) {
      for (std::size_t j = i + 1; j < regions.size(); ++j)
        if (!disjoint(regions[i], regions[j]))
          return false;
      for (const auto &r : self.bridge_.regions)
        if (!disjoint(regions[i], r))
          return false;
    }
    return true;
  }
  template <class Profile, class Owner>
  static void initialize(Owner &self, TypedContextStreamHeader s,
                         typename Profile::StreamAllocator &a) noexcept {
    auto reject = [&](Code code) { static_cast<void>(self.fail(code, 0)); };
    if (!self.allocator_controls_.data || !self.allocator_controls_.bytes ||
        core::validate_limits(self.limits_) != core::LimitError::none ||
        !self.disjoint({}, {})) {
      reject(Code::invalid_argument);
      return;
    }
    const auto raw = static_cast<std::size_t>(
        std::min<std::uint64_t>(s.frame_size, s.original_size));
    std::size_t index{}, total = Owner::working_bytes();
    if ((raw &&
         (!core::checked_add(raw, std::size_t{Profile::index_heads}, index) ||
          !core::checked_multiply(index, sizeof(std::uint32_t),
                                  self.index_bytes_))) ||
        !add(total, raw) || !add(total, self.index_bytes_) ||
        !add(total, self.allocator_controls_.bytes) ||
        !add(total, self.allocator_controls_.working_bytes) ||
        !add(total, self.external_) ||
        total > self.limits_.max_internal_buffered_bytes) {
      reject(Code::limit_exceeded);
      return;
    }
    const auto local = self.header_.size() + Profile::serialize_working_bytes();
    if (local > total) {
      reject(Code::internal_error);
      return;
    }
    const auto h =
        Profile::serialize_header(self.stream_, self.limits_, self.header_,
                                  self.header_written_, total - local);
    if (h.error != Profile::SerializeError::none) {
      reject(h.error == Profile::SerializeError::limit_exceeded
                 ? Code::limit_exceeded
                 : Code::invalid_argument);
      return;
    }
    // Both prospective full requests were admitted before either callback.
    if (raw) {
      self.raw_ = a.bytes(raw);
      if (!self.raw_.data || self.raw_.capacity != raw) {
        reject(!self.raw_.data && !self.raw_.capacity ? Code::out_of_memory
                                                      : Code::limit_exceeded);
        return;
      }
      self.index_ = a.indices(index);
      if (!self.index_.data || self.index_.capacity != index) {
        reject(!self.index_.data && !self.index_.capacity
                   ? Code::out_of_memory
                   : Code::limit_exceeded);
        return;
      }
    }
    if (!self.disjoint({}, {}))
      reject(Code::invalid_argument);
  }
  template <class Owner> static void destroy(Owner &self) noexcept {
    self.bridge_.source.release(self.index_);
    self.bridge_.source.release(self.raw_);
  }
  template <class Owner>
  static core::ProcessResult fail(Owner &self, core::ErrorCode code,
                                  std::uint64_t position, std::size_t used,
                                  std::size_t made) noexcept {
    using State = typename Owner::State;
    self.state_ = State::error;
    self.error_ = {code, position, 0};
    return {used, made, core::StreamStatus::error, self.error_};
  }
  template <class Profile, class Owner>
  static core::ProcessResult
  process(Owner &self, std::span<const std::byte> input,
          std::span<std::byte> output, std::uint32_t flags) noexcept {
    using CompactOwner = typename Profile::CompactOwner;
    using State = typename Owner::State;
    using Bridge = typename Owner::Bridge;
    using S = core::StreamStatus;
    if (self.state_ == State::error)
      return {0, 0, S::error, self.error_};
    if (self.state_ == State::ended)
      return {0, 0, S::end_of_stream, {}};
    constexpr auto end = core::flag_value(core::ProcessFlags::end_input);
    constexpr auto allowed = end | core::flag_value(core::ProcessFlags::flush);
    if (flags & ~allowed)
      return self.fail(Code::unsupported, self.accepted_);
    auto controls = self.bridge_.source.controls();
    if (controls.data != self.allocator_controls_.data ||
        controls.bytes != self.allocator_controls_.bytes ||
        controls.working_bytes != self.allocator_controls_.working_bytes ||
        !self.disjoint(input, output))
      return self.fail(Code::invalid_argument, self.accepted_);
    std::size_t total{};
    if (!self.budget(input.size(), output.size(), total))
      return self.fail(Code::limit_exceeded, self.accepted_);
    if (self.end_seen_ && !input.empty())
      return self.fail(Code::malformed_stream, self.accepted_);
    std::size_t used{}, made{};
    const bool final = (flags & end) != 0;
    while (true) {
      if (final && used == input.size())
        self.end_seen_ = true;
      if (self.state_ == State::header_drain ||
          self.state_ == State::frame_drain) {
        const auto pending = self.state_ == State::header_drain
                                 ? std::span<const std::byte>(self.header_)
                                       .first(self.header_written_)
                                 : self.owner_.publication();
        const auto n =
            std::min(pending.size() - self.drained_, output.size() - made);
        if (n)
          std::memcpy(output.data() + made, pending.data() + self.drained_, n);
        made += n;
        self.drained_ += n;
        if (self.drained_ != pending.size())
          return {used, made, S::need_output, {}};
        if (self.state_ == State::frame_drain)
          self.owner_.acknowledge_drained();
        self.drained_ = self.collected_ = 0;
        self.state_ = self.validated_ == self.stream_.original_size
                          ? State::awaiting_end
                          : State::collecting;
        continue;
      }
      if (self.state_ == State::awaiting_end) {
        if (used != input.size())
          return self.fail(Code::malformed_stream, self.accepted_, used, made);
        if (self.end_seen_) {
          self.state_ = State::ended;
          return {used, made, S::end_of_stream, {}};
        }
        return {used, made, used || made ? S::progress : S::need_input, {}};
      }
      const auto needed = static_cast<std::size_t>(std::min<std::uint64_t>(
          self.stream_.frame_size,
          self.stream_.original_size - self.validated_));
      const auto n = std::min(needed - self.collected_, input.size() - used);
      std::uint64_t next{};
      if (!core::checked_add(self.accepted_, static_cast<std::uint64_t>(n),
                             next))
        return self.fail(Code::limit_exceeded, self.accepted_, used, made);
      if (n)
        std::memcpy(self.raw_.data + self.collected_, input.data() + used, n);
      used += n;
      self.collected_ += n;
      self.accepted_ = next;
      if (final && used == input.size())
        self.end_seen_ = true;
      if (self.collected_ != needed) {
        if (self.end_seen_)
          return self.fail(Code::malformed_stream, self.accepted_, used, made);
        return {used, made, used || made ? S::progress : S::need_input, {}};
      }
      std::uint64_t next_raw{}, next_seq{};
      if (!core::checked_add(self.validated_,
                             static_cast<std::uint64_t>(needed), next_raw) ||
          !core::checked_add(self.sequence_, std::uint64_t{1}, next_seq))
        return self.fail(Code::limit_exceeded, self.validated_, used, made);
      // Owner charges its full persistent object, bridge, underlying allocator,
      // old/new generations, index and supplied raw prefix. Remove those controls
      // from &self reservation, then add the raw tail and complete call views.
      std::size_t extra = Owner::working_bytes();
      if (!subtract(extra, CompactOwner::working_bytes()) ||
          !subtract(extra, sizeof(Bridge)) ||
          !subtract(extra, Profile::callback_bytes) ||
          needed > self.raw_.capacity || !add(extra, self.external_) ||
          !add(extra, self.raw_.capacity - needed) ||
          !add(extra, input.size()) || !add(extra, output.size()))
        return self.fail(Code::limit_exceeded, self.validated_, used, made);
      const TypedContextFrameValidationContext context{
          self.stream_, self.limits_, self.sequence_, self.validated_};
      const auto r = self.owner_.encode(self.raw_.view().first(needed), context,
                                        self.index_.view(), extra);
      if (r.error != Code::none)
        return self.fail(r.error, self.validated_, used, made);
      self.validated_ = next_raw;
      self.sequence_ = next_seq;
      self.state_ = State::frame_drain;
      self.drained_ = 0;
    }
  }
};
} // namespace marc::frame::internal
#endif

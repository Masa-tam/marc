#ifndef MARC_FRAME_LZSS_POSITION_RANS_STREAM_DECODER_COMMON_HPP
#define MARC_FRAME_LZSS_POSITION_RANS_STREAM_DECODER_COMMON_HPP
#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"
#include "core/limits.hpp"
#include "core/status.hpp"
#include "dictionary/lzss_typed_token.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <span>
namespace marc::frame::internal {
// Stateless friend: concrete decoders retain all state and borrowed lifetimes.
struct PositionRansStreamMechanics {
  template <class Profile>
  static core::ErrorCode
  category(typename Profile::FormatError error) noexcept {
    using E = typename Profile::FormatError;
    if (error == E::limit_exceeded || error == E::arithmetic_overflow)
      return core::ErrorCode::limit_exceeded;
    if (error == E::unsupported_identity)
      return core::ErrorCode::unsupported;
    return core::ErrorCode::malformed_stream;
  }
  template <class Owner>
  static bool disjoint(const Owner &self, std::span<const std::byte> input,
                       std::span<std::byte> output) noexcept {
    struct Region {
      const void *data;
      std::size_t size;
    };
    const std::array regions{
        Region{input.data(), input.size()},
        Region{output.data(), output.size()},
        Region{self.serialized_.data(), self.serialized_.size()},
        Region{self.tokens_.data(), self.token_bytes_},
        Region{self.raw_.data(), self.raw_.size()},
        Region{&self, sizeof(self)}};
    for (std::size_t i = 0; i < regions.size(); ++i)
      for (std::size_t j = i + 1; j < regions.size(); ++j)
        if (core::check_buffer_overlap(regions[i].data, regions[i].size,
                                       regions[j].data, regions[j].size) !=
            core::BufferOverlap::disjoint)
          return false;
    return true;
  }
  template <class Profile, class Owner>
  static void initialize(Owner &self,
                         std::size_t additional_owner_bytes) noexcept {
    using State = typename Owner::State;
    if (core::validate_limits(self.limits_) != core::LimitError::none ||
        !core::checked_multiply(self.tokens_.size(),
                                sizeof(dictionary::internal::LzssTypedToken),
                                self.token_bytes_) ||
        !self.disjoint({}, {})) {
      self.state_ = State::error;
      self.error_ = {core::ErrorCode::invalid_argument, 0, 0};
      return;
    }
    std::size_t aggregate{};
    if (!core::checked_add(sizeof(self), additional_owner_bytes, aggregate) ||
        !core::checked_add(aggregate, self.serialized_.size(), aggregate) ||
        !core::checked_add(aggregate, self.raw_.size(), aggregate) ||
        !core::checked_add(aggregate, self.token_bytes_, aggregate) ||
        !core::checked_add(aggregate, Profile::fixed_working_bytes,
                           aggregate) ||
        aggregate > self.limits_.max_internal_buffered_bytes) {
      self.state_ = State::error;
      self.error_ = {core::ErrorCode::limit_exceeded, 0, 0};
    }
  }
  template <class Owner>
  static core::ProcessResult fail(Owner &self, core::ErrorCode code,
                                  std::uint64_t position, std::size_t consumed,
                                  std::size_t produced) noexcept {
    using State = typename Owner::State;
    self.state_ = State::error;
    self.error_ = {code, position, 0};
    return {consumed, produced, core::StreamStatus::error, self.error_};
  }
  template <class Profile, class Owner>
  static core::ProcessResult
  process(Owner &self, std::span<const std::byte> input,
          std::span<std::byte> output, std::uint32_t flags) noexcept {
    using State = typename Owner::State;
    using Status = core::StreamStatus;
    using Code = core::ErrorCode;
    if (self.state_ == State::error)
      return {0, 0, Status::error, self.error_};
    if (self.state_ == State::ended)
      return {0, 0, Status::end_of_stream, {}};
    constexpr auto end = core::flag_value(core::ProcessFlags::end_input);
    constexpr auto allowed = end | core::flag_value(core::ProcessFlags::flush);
    if (flags & ~allowed)
      return self.fail(Code::unsupported, self.position_, 0, 0);
    if (!self.disjoint(input, output))
      return self.fail(Code::invalid_argument, self.position_, 0, 0);
    if (self.end_seen_ && !input.empty())
      return self.fail(Code::malformed_stream, self.position_, 0, 0);
    const bool final = (flags & end) != 0;
    std::size_t consumed{}, produced{};
    while (true) {
      if (final && consumed == input.size())
        self.end_seen_ = true;
      if (self.state_ == State::draining) {
        const auto count =
            std::min(self.layout_.requirements.raw_size - self.drained_,
                     output.size() - produced);
        if (count)
          std::memcpy(output.data() + produced,
                      self.raw_.data() + self.drained_, count);
        produced += count;
        self.drained_ += count;
        if (self.drained_ != self.layout_.requirements.raw_size)
          return {consumed, produced, Status::need_output, {}};
        self.collected_ = 0;
        self.frame_start_ = self.position_;
        self.state_ = self.raw_validated_ == self.stream_.original_size
                          ? State::awaiting_end
                          : State::frame_header;
        continue;
      }
      if (self.state_ == State::awaiting_end) {
        if (consumed != input.size())
          return self.fail(Code::malformed_stream, self.position_, consumed,
                           produced);
        if (self.end_seen_) {
          self.state_ = State::ended;
          return {consumed, produced, Status::end_of_stream, {}};
        }
        return {consumed,
                produced,
                consumed || produced ? Status::progress : Status::need_input,
                {}};
      }
      if (self.state_ == State::frame_header && self.serialized_.size() < 64)
        return self.fail(Code::limit_exceeded, self.frame_start_, consumed,
                         produced);
      const auto destination =
          self.state_ == State::stream_header
              ? std::span<std::byte>(self.header_)
          : self.state_ == State::frame_header ? self.serialized_.first(64)
          : self.state_ == State::descriptor
              ? self.serialized_.first(self.layout_.requirements.prefix_size)
              : self.serialized_.first(
                    self.layout_.requirements.serialized_size);
      const auto count = std::min(destination.size() - self.collected_,
                                  input.size() - consumed);
      std::uint64_t next{};
      if (!core::checked_add(self.position_, static_cast<std::uint64_t>(count),
                             next))
        return self.fail(Code::limit_exceeded, self.position_, consumed,
                         produced);
      if (count)
        std::memcpy(destination.data() + self.collected_,
                    input.data() + consumed, count);
      consumed += count;
      self.collected_ += count;
      self.position_ = next;
      if (final && consumed == input.size())
        self.end_seen_ = true;
      if (self.collected_ != destination.size()) {
        if (self.end_seen_)
          return self.fail(Code::malformed_stream, self.position_, consumed,
                           produced);
        return {consumed,
                produced,
                consumed || produced ? Status::progress : Status::need_input,
                {}};
      }
      if (self.state_ == State::stream_header) {
        std::size_t parsed{};
        const auto error = Profile::parse_stream(self.header_, self.limits_,
                                                 self.stream_, parsed);
        if (error != Profile::FormatError::none)
          return self.fail(category<Profile>(error), 0, consumed, produced);
        self.collected_ = 0;
        self.frame_start_ = self.position_;
        self.state_ = self.stream_.original_size == 0 ? State::awaiting_end
                                                      : State::frame_header;
      } else if (self.state_ == State::frame_header) {
        const auto error = Profile::parse_frame_header(
            self.serialized_.first(64),
            {self.stream_, self.limits_, self.sequence_, self.raw_validated_},
            self.layout_);
        if (error != Profile::FormatError::none)
          return self.fail(category<Profile>(error), self.frame_start_,
                           consumed, produced);
        if (self.layout_.requirements.serialized_size >
                self.serialized_.size() ||
            self.layout_.requirements.token_count > self.tokens_.size() ||
            self.layout_.requirements.raw_size > self.raw_.size())
          return self.fail(Code::limit_exceeded, self.frame_start_, consumed,
                           produced);
        self.state_ = State::descriptor;
      } else if (self.state_ == State::descriptor) {
        const auto error = Profile::preflight_prefix(
            self.serialized_.first(self.layout_.requirements.prefix_size),
            {self.stream_, self.limits_, self.sequence_, self.raw_validated_},
            self.layout_);
        if (error != Profile::FormatError::none)
          return self.fail(category<Profile>(error), self.frame_start_ + 64,
                           consumed, produced);
        self.state_ = State::payload;
      } else {
        const auto decoded = Profile::decode_frame(
            self.serialized_.first(self.layout_.requirements.serialized_size),
            {self.stream_, self.limits_, self.sequence_, self.raw_validated_},
            self.tokens_, self.raw_);
        if (decoded.error != Profile::FrameError::none)
          return self.fail(Code::malformed_stream,
                           self.frame_start_ +
                               self.layout_.requirements.prefix_size,
                           consumed, produced);
        self.raw_validated_ += self.layout_.requirements.raw_size;
        ++self.sequence_;
        self.drained_ = 0;
        self.state_ = State::draining;
      }
    }
  }
};
} // namespace marc::frame::internal
#endif

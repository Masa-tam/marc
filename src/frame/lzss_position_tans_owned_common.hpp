#ifndef MARC_FRAME_LZSS_POSITION_TANS_OWNED_COMMON_HPP
#define MARC_FRAME_LZSS_POSITION_TANS_OWNED_COMMON_HPP
#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"
#include "core/limits.hpp"
#include "core/status.hpp"
#include "dictionary/lzss_short_length_escape_candidate.hpp"
#include "dictionary/lzss_typed_token.hpp"
#include "entropy/tans_tables.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <new>
#include <span>
namespace marc::frame::internal {
// Stateless family mechanics; concrete owners retain all data and admission.
struct PositionTansOwnedMechanics {
  using Code = core::ErrorCode;
  using Token = dictionary::internal::LzssTypedToken;
  template <class Profile>
  static Code format_error(typename Profile::FormatError e) noexcept {
    return e == Profile::FormatError::limit_exceeded ||
                   e == Profile::FormatError::arithmetic_overflow
               ? Code::limit_exceeded
               : Code::invalid_argument;
  }
  template <class Profile>
  static Code requirements(const typename Profile::StreamHeader &stream,
                           const core::DecoderLimits &limits, bool encode,
                           std::size_t owner,
                           typename Profile::Workspace &result) noexcept {
    if (const auto e = Profile::validate_stream(stream, limits);
        e != Profile::FormatError::none)
      return format_error<Profile>(e);
    typename Profile::Workspace r{};
    r.raw_bytes = stream.frame_size;
    r.token_count = stream.frame_size;
    r.owner_bytes = owner;
    r.fixed_working_bytes =
        131072 + sizeof(typename Profile::Descriptor) +
        sizeof(typename Profile::TokenContext) +
        sizeof(dictionary::internal::LzssTypedFrameValidationContext);
    std::size_t bits{};
    if (!core::checked_multiply(r.raw_bytes, Profile::payload_bits, bits) ||
        !core::checked_add(bits, std::size_t{7}, bits) ||
        !core::checked_add(bits / 8, std::size_t{2}, r.payload_bytes) ||
        !core::checked_add(r.payload_bytes, Profile::serialized_overhead,
                           r.serialized_bytes) ||
        !core::checked_multiply(r.token_count, sizeof(Token), r.token_bytes) ||
        !core::checked_multiply(
            Profile::table_entries,
            encode ? sizeof(std::uint16_t)
                   : sizeof(entropy::internal::TansDecodeEntry),
            r.table_bytes))
      return Code::limit_exceeded;
    if (encode) {
      r.private_serialized_bytes = r.serialized_bytes;
      const auto f =
          Profile::finder_workspace(r.raw_bytes, stream.dictionary, limits);
      if (f.error != dictionary::internal::LzssShortPrefixError::none)
        return Code::limit_exceeded;
      r.finder_bytes = f.workspace_size;
    } else
      r.payload_bytes = 0; // Payload is retained inside serialized_ on decode.
    for (const auto bytes :
         {r.raw_bytes, r.token_bytes, r.serialized_bytes, r.finder_bytes,
          r.table_bytes, r.payload_bytes, r.private_serialized_bytes})
      if (!core::checked_add(r.owned_buffer_bytes, bytes, r.owned_buffer_bytes))
        return Code::limit_exceeded;
    if (!core::checked_add(r.owner_bytes, r.owned_buffer_bytes,
                           r.aggregate_bytes) ||
        !core::checked_add(r.aggregate_bytes, r.fixed_working_bytes,
                           r.aggregate_bytes))
      return Code::limit_exceeded;
    // Shared header admission also charges a full decode bank. Expose this
    // conservative nonallocated floor separately from actual owned buffers.
    r.metadata_floor_bytes = 131072;
    for (const auto bytes :
         {r.raw_bytes, r.token_bytes, r.serialized_bytes,
          Profile::table_entries * sizeof(entropy::internal::TansDecodeEntry)})
      if (!core::checked_add(r.metadata_floor_bytes, bytes,
                             r.metadata_floor_bytes))
        return Code::limit_exceeded;
    r.aggregate_bytes = std::max(r.aggregate_bytes, r.metadata_floor_bytes);
    if (r.aggregate_bytes > limits.max_internal_buffered_bytes)
      return Code::limit_exceeded;
    result = r;
    return Code::none;
  }
  template <class Profile, class Owner>
  static Code encoder_requirements(const typename Profile::StreamHeader &s,
                                   const core::DecoderLimits &l,
                                   typename Profile::Workspace &r) noexcept {
    return PositionTansOwnedMechanics::requirements<Profile>(s, l, true,
                                                             sizeof(Owner), r);
  }
  template <class Profile, class Owner>
  static std::unique_ptr<Owner>
  create_encoder(const typename Profile::StreamHeader &stream,
                 const core::DecoderLimits &limits, Code &error) noexcept {
    const auto s = stream;
    const auto l = limits;
    typename Profile::Workspace r{};
    error = Owner::requirements(s, l, r);
    if (error != Code::none)
      return {};
    std::unique_ptr<Owner> p(new (std::nothrow) Owner);
    if (!p) {
      error = Code::out_of_memory;
      return {};
    }
    p->stream_ = s;
    p->limits_ = l;
    p->workspace_ = r;
    p->raw_.reset(new (std::nothrow) std::byte[r.raw_bytes]);
    if (!p->raw_) {
      error = Code::out_of_memory;
      return {};
    }
    p->serialized_.reset(new (std::nothrow) std::byte[r.serialized_bytes]);
    if (!p->serialized_) {
      error = Code::out_of_memory;
      return {};
    }
    p->tokens_.reset(new (std::nothrow) Token[r.token_count]);
    if (!p->tokens_) {
      error = Code::out_of_memory;
      return {};
    }
    static_assert(alignof(std::uint32_t) <= alignof(std::max_align_t));
    if (r.finder_bytes)
      p->finder_.reset(new (std::nothrow) std::byte[r.finder_bytes]);
    if (!p->raw_ || !p->serialized_ || !p->tokens_ ||
        (r.finder_bytes && !p->finder_)) {
      error = Code::out_of_memory;
      return {};
    }
    p->tables_.reset(new (std::nothrow) std::uint16_t[Profile::table_entries]);
    if (!p->tables_) {
      error = Code::out_of_memory;
      return {};
    }
    p->payload_.reset(new (std::nothrow) std::byte[r.payload_bytes]);
    if (!p->payload_) {
      error = Code::out_of_memory;
      return {};
    }
    p->private_serialized_.reset(new (std::nothrow)
                                     std::byte[r.private_serialized_bytes]);
    if (!p->private_serialized_) {
      error = Code::out_of_memory;
      return {};
    }
    if (const auto e = Profile::serialize_stream(s, l, p->header_);
        e != Profile::FormatError::none) {
      error = format_error<Profile>(e);
      return {};
    }
    error = Code::none;
    return p;
  }
  template <class Owner>
  static bool encoder_disjoint(const Owner &self,
                               std::span<const std::byte> input,
                               std::span<std::byte> output) noexcept {
    struct Region {
      const void *data;
      std::size_t bytes;
    };
    const std::array regions{
        Region{&self, sizeof(self)},
        Region{input.data(), input.size()},
        Region{output.data(), output.size()},
        Region{self.raw_.get(), self.workspace_.raw_bytes},
        Region{self.serialized_.get(), self.workspace_.serialized_bytes},
        Region{self.tokens_.get(), self.workspace_.token_bytes},
        Region{self.finder_.get(), self.workspace_.finder_bytes},
        Region{self.tables_.get(), self.workspace_.table_bytes},
        Region{self.payload_.get(), self.workspace_.payload_bytes},
        Region{self.private_serialized_.get(),
               self.workspace_.private_serialized_bytes}};
    for (std::size_t i = 0; i < regions.size(); ++i)
      for (std::size_t j = i + 1; j < regions.size(); ++j)
        if (core::check_buffer_overlap(regions[i].data, regions[i].bytes,
                                       regions[j].data, regions[j].bytes) !=
            core::BufferOverlap::disjoint)
          return false;
    return true;
  }
  template <class Owner>
  static core::ProcessResult fail_encoder(Owner &self, Code code,
                                          std::size_t consumed,
                                          std::size_t produced) noexcept {
    using State = typename Owner::State;
    self.state_ = State::error;
    self.error_ = {code, self.received_, 0};
    return {consumed, produced, core::StreamStatus::error, self.error_};
  }
  template <class Profile, class Owner>
  static core::ProcessResult
  encode(Owner &self, std::span<const std::byte> input,
         std::span<std::byte> output, std::uint32_t flags) noexcept {
    using State = typename Owner::State;
    using Status = core::StreamStatus;
    if (self.state_ == State::error)
      return {0, 0, Status::error, self.error_};
    if (self.state_ == State::ended)
      return {0, 0, Status::end_of_stream, {}};
    constexpr auto end = core::flag_value(core::ProcessFlags::end_input);
    constexpr auto allowed = end | core::flag_value(core::ProcessFlags::flush);
    if (flags & ~allowed)
      return self.fail(Code::unsupported);
    if (!self.disjoint(input, output))
      return self.fail(Code::invalid_argument);
    const bool final = (flags & end) != 0;
    const auto remaining = self.stream_.original_size - self.received_;
    if (input.size() > remaining || (final && input.size() != remaining) ||
        (self.end_seen_ && !input.empty()))
      return self.fail(Code::invalid_argument);
    std::size_t consumed{}, produced{};
    while (true) {
      if (final && consumed == input.size())
        self.end_seen_ = true;
      if (self.state_ == State::header || self.state_ == State::draining) {
        const auto source = self.state_ == State::header
                                ? std::span<const std::byte>(self.header_)
                                : std::span<const std::byte>(
                                      self.serialized_.get(), self.pending_);
        const auto count =
            std::min(self.pending_ - self.drained_, output.size() - produced);
        if (count)
          std::memcpy(output.data() + produced, source.data() + self.drained_,
                      count);
        produced += count;
        self.drained_ += count;
        if (self.drained_ != self.pending_)
          return {consumed, produced, Status::need_output, {}};
        self.collected_ = 0;
        self.drained_ = 0;
        self.pending_ = 0;
        self.state_ = self.committed_ == self.stream_.original_size
                          ? State::awaiting_end
                          : State::collecting;
        continue;
      }
      if (self.state_ == State::awaiting_end) {
        if (self.end_seen_) {
          self.state_ = State::ended;
          return {consumed, produced, Status::end_of_stream, {}};
        }
        return {consumed,
                produced,
                consumed || produced ? Status::progress : Status::need_input,
                {}};
      }
      const auto target = static_cast<std::size_t>(std::min<std::uint64_t>(
          self.stream_.frame_size,
          self.stream_.original_size - self.committed_));
      const auto count =
          std::min(target - self.collected_, input.size() - consumed);
      if (count)
        std::memcpy(self.raw_.get() + self.collected_, input.data() + consumed,
                    count);
      consumed += count;
      self.received_ += count;
      self.collected_ += count;
      if (self.collected_ != target)
        return {consumed,
                produced,
                consumed || produced ? Status::progress : Status::need_input,
                {}};
      const auto candidate = Profile::tokenize(
          {self.raw_.get(), target}, self.stream_.dictionary, self.limits_, 5,
          {self.tokens_.get(), self.workspace_.token_count},
          {self.finder_.get(), self.workspace_.finder_bytes});
      if (candidate.error !=
          dictionary::internal::LzssShortMatchCandidateError::none)
        return self.fail(Code::internal_error, consumed, produced);
      const auto encoded = Profile::encode_frame(
          {self.tokens_.get(), candidate.token_count},
          {self.stream_, self.limits_, self.sequence_, self.committed_},
          {self.tables_.get(), Profile::table_entries},
          {self.payload_.get(), self.workspace_.payload_bytes},
          {self.private_serialized_.get(),
           self.workspace_.private_serialized_bytes},
          {self.serialized_.get(), self.workspace_.serialized_bytes});
      if (encoded.error != Profile::FrameError::none) {
        const bool limited =
            encoded.error == Profile::FrameError::limit_exceeded ||
            encoded.error == Profile::FrameError::arithmetic_overflow ||
            encoded.format_error == Profile::FormatError::limit_exceeded ||
            encoded.format_error == Profile::FormatError::arithmetic_overflow ||
            encoded.token_result.error == Profile::TokenError::limit_exceeded;
        return self.fail(limited ? Code::limit_exceeded : Code::internal_error,
                         consumed, produced);
      }
      self.committed_ += target;
      ++self.sequence_;
      self.pending_ = encoded.serialized_size;
      self.state_ = State::draining;
    }
  }
  template <class Profile, class Owner>
  static Code decoder_requirements(std::uint32_t capacity,
                                   const core::DecoderLimits &l,
                                   typename Profile::Workspace &r) noexcept {
    return PositionTansOwnedMechanics::requirements<Profile>(
        typename Profile::StreamHeader{capacity, 0}, l, false, sizeof(Owner),
        r);
  }
  template <class Profile, class Owner>
  static std::unique_ptr<Owner>
  create_decoder(std::uint32_t capacity, const core::DecoderLimits &limits,
                 Code &error) noexcept {
    auto l = limits;
    typename Profile::Workspace r{};
    error = Owner::requirements(capacity, l, r);
    if (error != Code::none)
      return {};
    l.max_frame_size = std::min<std::uint64_t>(l.max_frame_size, capacity);
    std::unique_ptr<Owner> p(new (std::nothrow) Owner);
    if (!p) {
      error = Code::out_of_memory;
      return {};
    }
    p->raw_.reset(new (std::nothrow) std::byte[r.raw_bytes]);
    if (!p->raw_) {
      error = Code::out_of_memory;
      return {};
    }
    p->serialized_.reset(new (std::nothrow) std::byte[r.serialized_bytes]);
    if (!p->serialized_) {
      error = Code::out_of_memory;
      return {};
    }
    p->tokens_.reset(new (std::nothrow) Token[r.token_count]);
    if (!p->raw_ || !p->serialized_ || !p->tokens_) {
      error = Code::out_of_memory;
      return {};
    }
    p->tables_.reset(
        new (std::nothrow)
            entropy::internal::TansDecodeEntry[Profile::table_entries]);
    if (!p->tables_) {
      error = Code::out_of_memory;
      return {};
    }
    p->decoder_.emplace(l, std::span(p->serialized_.get(), r.serialized_bytes),
                        std::span(p->tokens_.get(), r.token_count),
                        std::span(p->raw_.get(), r.raw_bytes),
                        std::span(p->tables_.get(), Profile::table_entries),
                        sizeof(Owner) -
                            sizeof(typename Profile::StreamDecoder));
    const auto initial = p->decoder_->process({}, {}, 0);
    if (initial.status == core::StreamStatus::error) {
      error = initial.error.code;
      return {};
    }
    error = Code::none;
    return p;
  }
  template <class Owner>
  static core::ProcessResult
  decode(Owner &self, std::span<const std::byte> input,
         std::span<std::byte> output, std::uint32_t flags) noexcept {
    if (self.error_.code != Code::none)
      return {0, 0, core::StreamStatus::error, self.error_};
    if (self.ended_)
      return {0, 0, core::StreamStatus::end_of_stream, {}};
    if (core::check_buffer_overlap(&self, sizeof(self), input.data(),
                                   input.size()) !=
            core::BufferOverlap::disjoint ||
        core::check_buffer_overlap(&self, sizeof(self), output.data(),
                                   output.size()) !=
            core::BufferOverlap::disjoint) {
      self.error_ = {Code::invalid_argument, 0, 0};
      return {0, 0, core::StreamStatus::error, self.error_};
    }
    const auto r = self.decoder_->process(input, output, flags);
    if (r.status == core::StreamStatus::error)
      self.error_ = r.error;
    self.ended_ = r.status == core::StreamStatus::end_of_stream;
    return r;
  }
};
} // namespace marc::frame::internal
#endif

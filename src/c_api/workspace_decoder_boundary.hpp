#ifndef MARC_C_API_WORKSPACE_DECODER_BOUNDARY_HPP
#define MARC_C_API_WORKSPACE_DECODER_BOUNDARY_HPP

#include "c_api/buffer_validation.hpp"
#include "c_api/transform_handle.hpp"
#include "core/checked_math.hpp"
#include "core/status.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include "core/limits.hpp"

#include <memory>
#include <optional>
#include <type_traits>

namespace marc::c_api::detail {

struct WorkspaceRegion {
  const void *data;
  std::size_t bytes;
};
inline bool disjoint(WorkspaceRegion a, WorkspaceRegion b) noexcept {
  return disjoint_buffers(a.data, a.bytes, b.data, b.bytes);
}

template <std::size_t N>
inline bool separate(const std::array<WorkspaceRegion, N> &r) noexcept {
  for (std::size_t i = 0; i < N; ++i)
    for (std::size_t j = i + 1; j < N; ++j)
      if (!disjoint(r[i], r[j]))
        return false;
  return true;
}
template <class Buffers>
inline std::array<WorkspaceRegion, 5> regions(const Buffers &b) noexcept {
  return {{{b.serialized.data, b.serialized.size},
           {b.tokens.data, b.tokens.size},
           {b.token_scratch.data, b.token_scratch.size},
           {b.raw.data, b.raw.size},
           {b.raw_scratch.data, b.raw_scratch.size}}};
}

template <class Decoder, class Buffers, class Token>
class TypedWorkspaceDecoderBoundary final : public core::Transform {
public:
  TypedWorkspaceDecoderBoundary(const Buffers &b, std::size_t ic, std::size_t oc) noexcept
      : b_(b), ic_(ic), oc_(oc) {}
  ~TypedWorkspaceDecoderBoundary() override {
    decoder_.reset();
    if (live_) {
      std::destroy_n(reinterpret_cast<Token *>(b_.tokens.data),
                     b_.tokens.size / sizeof(Token));
      std::destroy_n(reinterpret_cast<Token *>(b_.token_scratch.data),
                     b_.token_scratch.size / sizeof(Token));
    }
  }
  void start(const core::DecoderLimits &l, std::size_t extra,
             marc_transform *h) noexcept {
    static_assert(std::is_nothrow_default_constructible_v<Token> &&
                  std::is_nothrow_destructible_v<Token>);
    std::uninitialized_value_construct_n(reinterpret_cast<Token *>(b_.tokens.data),
                                         b_.tokens.size / sizeof(Token));
    std::uninitialized_value_construct_n(
        reinterpret_cast<Token *>(b_.token_scratch.data),
        b_.token_scratch.size / sizeof(Token));
    live_ = true;
    handle_ = h;
    decoder_.emplace(
        l,
        std::span(reinterpret_cast<std::byte *>(b_.serialized.data),
                  b_.serialized.size),
        std::span(reinterpret_cast<Token *>(b_.tokens.data),
                  b_.tokens.size / sizeof(Token)),
        std::span(reinterpret_cast<Token *>(b_.token_scratch.data),
                  b_.token_scratch.size / sizeof(Token)),
        std::span(reinterpret_cast<std::byte *>(b_.raw.data), b_.raw.size),
        std::span(reinterpret_cast<std::byte *>(b_.raw_scratch.data),
                  b_.raw_scratch.size),
        extra);
  }
  core::ProcessResult ready() noexcept { return decoder_->process({}, {}, 0); }
  core::ProcessResult process(std::span<const std::byte> in,
                              std::span<std::byte> out,
                              std::uint32_t flags) noexcept override {
    if (terminal_)
      return {0, 0, last_.status, last_.error};
    auto w = regions(b_);
    const std::array<WorkspaceRegion, 9> r{{w[0],
                                   w[1],
                                   w[2],
                                   w[3],
                                   w[4],
                                   {this, sizeof(*this)},
                                   {handle_, sizeof(*handle_)},
                                   {in.data(), in.size()},
                                   {out.data(), out.size()}}};
    const auto e = in.size() > ic_ || out.size() > oc_
                       ? core::ErrorCode::limit_exceeded
                       : (!separate(r) ? core::ErrorCode::invalid_argument
                                       : core::ErrorCode::none);
    if (e != core::ErrorCode::none) {
      terminal_ = true;
      last_ = {0, 0, core::StreamStatus::error, {e, accepted_, 0}};
      return last_;
    }
    last_ = decoder_->process(in, out, flags);
    if (!core::checked_add(accepted_,
                           static_cast<std::uint64_t>(last_.input_consumed),
                           accepted_)) {
      last_.status = core::StreamStatus::error;
      last_.error = {core::ErrorCode::internal_error, accepted_, 0};
    }
    terminal_ = last_.status == core::StreamStatus::error ||
                last_.status == core::StreamStatus::end_of_stream;
    return last_;
  }

private:
  Buffers b_{};
  std::size_t ic_{}, oc_{};
  std::optional<Decoder> decoder_;
  marc_transform *handle_{};
  std::uint64_t accepted_{};
  core::ProcessResult last_{};
  bool live_{}, terminal_{};
};

template <class Decoder, class Buffers>
class PackedWorkspaceDecoderBoundary final : public core::Transform {
public:
  PackedWorkspaceDecoderBoundary(const Buffers &b, std::size_t ic, std::size_t oc) noexcept
      : b_(b), ic_(ic), oc_(oc) {}
  ~PackedWorkspaceDecoderBoundary() override = default;
  void start(const core::DecoderLimits &l, std::size_t extra,
             marc_transform *h) noexcept {
    handle_ = h;
    decoder_.emplace(
        l,
        std::span(reinterpret_cast<std::byte *>(b_.serialized.data),
                  b_.serialized.size),
        std::span(reinterpret_cast<std::byte *>(b_.tokens.data),
                  b_.tokens.size),
        std::span(reinterpret_cast<std::byte *>(b_.token_scratch.data),
                  b_.token_scratch.size),
        std::span(reinterpret_cast<std::byte *>(b_.raw.data), b_.raw.size),
        std::span(reinterpret_cast<std::byte *>(b_.raw_scratch.data),
                  b_.raw_scratch.size),
        extra);
  }
  core::ProcessResult ready() noexcept { return decoder_->process({}, {}, 0); }
  core::ProcessResult process(std::span<const std::byte> in,
                              std::span<std::byte> out,
                              std::uint32_t flags) noexcept override {
    if (terminal_)
      return {0, 0, last_.status, last_.error};
    auto w = regions(b_);
    const std::array<WorkspaceRegion, 9> r{{w[0],
                                   w[1],
                                   w[2],
                                   w[3],
                                   w[4],
                                   {this, sizeof(*this)},
                                   {handle_, sizeof(*handle_)},
                                   {in.data(), in.size()},
                                   {out.data(), out.size()}}};
    const auto e = in.size() > ic_ || out.size() > oc_
                       ? core::ErrorCode::limit_exceeded
                       : (!separate(r) ? core::ErrorCode::invalid_argument
                                       : core::ErrorCode::none);
    if (e != core::ErrorCode::none) {
      terminal_ = true;
      last_ = {0, 0, core::StreamStatus::error, {e, accepted_, 0}};
      return last_;
    }
    last_ = decoder_->process(in, out, flags);
    if (!core::checked_add(accepted_,
                           static_cast<std::uint64_t>(last_.input_consumed),
                           accepted_)) {
      last_.status = core::StreamStatus::error;
      last_.error = {core::ErrorCode::internal_error, accepted_, 0};
    }
    terminal_ = last_.status == core::StreamStatus::error ||
                last_.status == core::StreamStatus::end_of_stream;
    return last_;
  }

private:
  Buffers b_{};
  std::size_t ic_{}, oc_{};
  std::optional<Decoder> decoder_;
  marc_transform *handle_{};
  std::uint64_t accepted_{};
  core::ProcessResult last_{};
  bool terminal_{};
};

} // namespace marc::c_api::detail

#endif

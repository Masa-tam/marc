#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"
#include "frame/lzss_position_distance_32m_stream_decoder.hpp"
#include "marc/marc.h"
#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <type_traits>

struct marc_transform {
  marc::core::Transform *implementation;
};
namespace {
using namespace marc;
using namespace frame::internal;
using Config = marc_lzss_position_distance_dynamic_range_32m_decoder_config;
using Requirements =
    marc_lzss_position_distance_dynamic_range_32m_decoder_requirements;
using Buffers = marc_lzss_position_distance_dynamic_range_32m_decoder_buffers;
struct Region {
  const void *data;
  std::size_t bytes;
};
bool disjoint(Region a, Region b) noexcept {
  return core::check_buffer_overlap(a.data, a.bytes, b.data, b.bytes) ==
         core::BufferOverlap::disjoint;
}
template <std::size_t N>
bool separate(const std::array<Region, N> &r) noexcept {
  for (std::size_t i = 0; i < N; ++i)
    for (std::size_t j = i + 1; j < N; ++j)
      if (!disjoint(r[i], r[j]))
        return false;
  return true;
}
std::array<Region, 5> regions(const Buffers &b) noexcept {
  return {{{b.serialized.data, b.serialized.size},
           {b.tokens.data, b.tokens.size},
           {b.token_scratch.data, b.token_scratch.size},
           {b.raw.data, b.raw.size},
           {b.raw_scratch.data, b.raw_scratch.size}}};
}
class Boundary final : public core::Transform {
public:
  Boundary(const Buffers &b, std::size_t ic, std::size_t oc) noexcept
      : b_(b), ic_(ic), oc_(oc) {}
  ~Boundary() override = default;
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
    const std::array<Region, 9> r{{w[0],
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
  std::optional<LzssPositionDistance32mStreamDecoder> decoder_;
  marc_transform *handle_{};
  std::uint64_t accepted_{};
  core::ProcessResult last_{};
  bool terminal_{};
};
constexpr std::size_t public_controls =
    sizeof(Config) + sizeof(Requirements) + sizeof(Buffers) +
    2 * sizeof(core::DecoderLimits) +
    2 * sizeof(LzssPositionDistance32mStreamRequirements) +
    4 * sizeof(core::ProcessResult) + 32 * sizeof(Region) +
    24 * sizeof(std::size_t) + 8 * sizeof(void *);
marc_status status(core::ErrorCode e) noexcept {
  if (e == core::ErrorCode::none)
    return MARC_STATUS_OK;
  if (e == core::ErrorCode::limit_exceeded)
    return MARC_STATUS_LIMIT_EXCEEDED;
  if (e == core::ErrorCode::out_of_memory)
    return MARC_STATUS_OUT_OF_MEMORY;
  if (e == core::ErrorCode::invalid_argument)
    return MARC_STATUS_INVALID_ARGUMENT;
  return MARC_STATUS_INTERNAL_ERROR;
}
marc_status prepare(const Config *c, core::DecoderLimits &l, std::size_t &extra,
                    Requirements &q) noexcept {
  if (!c || c->struct_size != sizeof(*c) ||
      c->abi_version != MARC_ABI_VERSION || c->reserved || c->reserved2)
    return MARC_STATUS_INVALID_ARGUMENT;
  l = {};
  l.max_total_output_size = c->max_total_output_size;
  l.max_frame_size = c->max_frame_size;
  l.max_block_size = c->max_block_size;
  l.max_compressed_payload_size = c->max_compressed_payload_size;
  l.max_internal_buffered_bytes = c->max_internal_buffered_bytes;
  l.max_lz_distance = c->max_lz_distance;
  l.max_lz_match_length = c->max_lz_match_length;
  l.max_entropy_table_entries = c->max_entropy_table_entries;
  l.max_range_model_total = c->max_range_model_total;
  l.max_expansion_ratio = c->max_expansion_ratio;
  l.expansion_slack = c->expansion_slack;
  l.max_dictionary_serialized_size = 1;
  l.max_dictionary_entries = 1;
  l.max_huffman_code_length = 1;
  l.max_blocks_per_frame = 1;
  if (core::validate_limits(l) != core::LimitError::none)
    return MARC_STATUS_INVALID_ARGUMENT;
  if (l.max_entropy_table_entries < 2621 || l.max_range_model_total < 32768 ||
      l.max_lz_match_length < 3)
    return MARC_STATUS_LIMIT_EXCEEDED;
  auto maximum = std::numeric_limits<std::size_t>::max();
  if (c->external_retained_bytes > maximum ||
      c->input_capacity_bytes > maximum || c->output_capacity_bytes > maximum)
    return MARC_STATUS_LIMIT_EXCEEDED;
  extra = static_cast<std::size_t>(c->external_retained_bytes);
  for (auto n : {sizeof(Boundary), sizeof(marc_transform), public_controls,
                 static_cast<std::size_t>(c->input_capacity_bytes),
                 static_cast<std::size_t>(c->output_capacity_bytes)})
    if (!core::checked_add(extra, n, extra))
      return MARC_STATUS_LIMIT_EXCEEDED;
  const auto f =
      std::min({c->max_frame_size, c->max_block_size, std::uint64_t{33554432}});
  std::uint64_t p{}, s{}, tb{};
  if (!core::checked_multiply(f, std::uint64_t{20}, p) ||
      !core::checked_add(p, std::uint64_t{5}, p))
    return MARC_STATUS_LIMIT_EXCEEDED;
  p = std::min(p, c->max_compressed_payload_size);
  if (!core::checked_add(p, std::uint64_t{80}, s) ||
      !core::checked_multiply(f, std::uint64_t{3}, tb) || s > maximum ||
      tb > maximum || f > maximum)
    return MARC_STATUS_LIMIT_EXCEEDED;
  auto r = query_lzss_position_distance_32m_stream_workspace(
      l, static_cast<std::size_t>(s), static_cast<std::size_t>(tb),
      static_cast<std::size_t>(tb), static_cast<std::size_t>(f),
      static_cast<std::size_t>(f), extra);
  if (r.error != core::ErrorCode::none)
    return status(r.error);
  q = {sizeof(q),
       MARC_ABI_VERSION,
       MARC_LZSS_POSITION_DISTANCE_32M_CAPACITY_ONLY,
       0,
       s,
       tb,
       tb,
       f,
       f,
       1,
       tb,
       r.aggregate_bytes};
  return MARC_STATUS_OK;
}
} // namespace
marc_status marc_lzss_position_distance_dynamic_range_32m_decoder_config_init(
    Config *c) noexcept {
  if (!c)
    return MARC_STATUS_INVALID_ARGUMENT;
  Config v{};
  v.struct_size = sizeof(v);
  v.abi_version = MARC_ABI_VERSION;
  v.max_frame_size = v.max_block_size = v.max_lz_distance = 33554432;
  v.max_lz_match_length = 258;
  v.max_range_model_total = 32768;
  *c = v;
  return MARC_STATUS_OK;
}
marc_status
marc_lzss_position_distance_dynamic_range_32m_decoder_workspace_requirements(
    const Config *c, Requirements *out) noexcept {
  if (!c || !out || !disjoint({c, sizeof(*c)}, {out, sizeof(*out)}) ||
      out->struct_size != sizeof(*out) ||
      out->abi_version != MARC_ABI_VERSION || out->reserved)
    return MARC_STATUS_INVALID_ARGUMENT;
  core::DecoderLimits l{};
  std::size_t extra{};
  Requirements q{};
  auto s = prepare(c, l, extra, q);
  if (s == MARC_STATUS_OK)
    *out = q;
  return s;
}
marc_status marc_lzss_position_distance_dynamic_range_32m_create_decoder(
    const Config *c, const Buffers *b, marc_transform **out) noexcept {
  if (!c || !b || !out)
    return MARC_STATUS_INVALID_ARGUMENT;
  auto w = regions(*b);
  const std::array<Region, 8> r{{{c, sizeof(*c)},
                                 {b, sizeof(*b)},
                                 {out, sizeof(*out)},
                                 w[0],
                                 w[1],
                                 w[2],
                                 w[3],
                                 w[4]}};
  if (!separate(r))
    return MARC_STATUS_INVALID_ARGUMENT;
  *out = nullptr;
  if (b->struct_size != sizeof(*b) || b->abi_version != MARC_ABI_VERSION ||
      b->reserved || b->reserved2)
    return MARC_STATUS_INVALID_ARGUMENT;
  core::DecoderLimits l{};
  std::size_t extra{};
  Requirements q{};
  auto s = prepare(c, l, extra, q);
  if (s != MARC_STATUS_OK)
    return s;
  const std::array<std::uint64_t, 5> minima{q.serialized_bytes, q.token_bytes,
                                            q.token_scratch_bytes, q.raw_bytes,
                                            q.raw_scratch_bytes};
  for (std::size_t i = 0; i < 5; ++i)
    if (!w[i].data || w[i].bytes < minima[i])
      return MARC_STATUS_INVALID_ARGUMENT;
  auto actual = query_lzss_position_distance_32m_stream_workspace(
      l, b->serialized.size, b->tokens.size, b->token_scratch.size, b->raw.size,
      b->raw_scratch.size, extra);
  if (actual.error != core::ErrorCode::none)
    return status(actual.error);
  auto guard = std::unique_ptr<Boundary>(new (std::nothrow) Boundary(
      *b, static_cast<std::size_t>(c->input_capacity_bytes),
      static_cast<std::size_t>(c->output_capacity_bytes)));
  if (!guard)
    return MARC_STATUS_OUT_OF_MEMORY;
  auto h = std::unique_ptr<marc_transform>(new (std::nothrow)
                                               marc_transform{guard.get()});
  if (!h)
    return MARC_STATUS_OUT_OF_MEMORY;
  for (auto v : r)
    if (!disjoint(v, {guard.get(), sizeof(Boundary)}) ||
        !disjoint(v, {h.get(), sizeof(marc_transform)}))
      return MARC_STATUS_INVALID_ARGUMENT;
  if (!disjoint({guard.get(), sizeof(Boundary)},
                {h.get(), sizeof(marc_transform)}))
    return MARC_STATUS_INVALID_ARGUMENT;
  guard->start(l, extra, h.get());
  auto ready = guard->ready();
  if (ready.status == core::StreamStatus::error)
    return status(ready.error.code);
  if (ready.status != core::StreamStatus::need_input || ready.input_consumed ||
      ready.output_produced)
    return MARC_STATUS_INTERNAL_ERROR;
  guard.release();
  *out = h.release();
  return MARC_STATUS_OK;
}

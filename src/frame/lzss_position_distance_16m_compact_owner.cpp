#include "frame/lzss_position_distance_16m_compact_owner.hpp"
#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"
#include <algorithm>
#include <array>
#include <utility>
namespace marc::frame::internal {
namespace {
using Code = core::ErrorCode;
struct Region {
  const void *data;
  std::size_t bytes;
};
struct Candidate {
  LzssPositionDistance16mCompactAllocator &allocator;
  LzssPositionDistance16mOwnedBytes compact{}, frame{}, payload{},
      publication{};
  ~Candidate() {
    allocator.release(publication);
    allocator.release(payload);
    allocator.release(frame);
    allocator.release(compact);
  }
};
struct Working {
  LzssPositionDistance16mCompactFramePlan plan{};
  entropy::internal::LzssPositionDistance16mTokenRangeResult range{};
  entropy::internal::ContextualDynamicRangeDescriptor descriptor{};
  LzssPositionDistance16mSerializeResult prefix{};
  TypedContextFrameLayout layout{}, parsed{};
  LzssPositionDistance16mFrameRequirements requirements{};
  LzssPositionDistance16mAllocatorControls controls{};
  LzssPositionDistance16mCompactFrameWorkspace workspace{};
  std::size_t index_bytes{}, base{}, total{}, local{}, retained{}, written{};
};
bool add(std::size_t &n, std::size_t x) noexcept {
  return core::checked_add(n, x, n);
}
bool remainder(Working &w, std::initializer_list<std::size_t> sizes) noexcept {
  w.local = 0;
  for (auto n : sizes)
    if (!add(w.local, n))
      return false;
  if (w.local > w.total)
    return false;
  w.retained = w.total - w.local;
  return true;
}
template <std::size_t N>
bool disjoint(const std::array<Region, N> &regions) noexcept {
  for (std::size_t i = 0; i < N; ++i)
    for (std::size_t j = i + 1; j < N; ++j)
      if (core::check_buffer_overlap(regions[i].data, regions[i].bytes,
                                     regions[j].data, regions[j].bytes) !=
          core::BufferOverlap::disjoint)
        return false;
  return true;
}
} // namespace
std::size_t LzssPositionDistance16mCompactOwner::working_bytes() noexcept {
  return sizeof(LzssPositionDistance16mCompactOwner) + sizeof(Candidate) +
         sizeof(Working) + 2 * sizeof(LzssPositionDistance16mOwnerResult) +
         2 * sizeof(std::array<Region, 12>) + 40 * sizeof(std::size_t) +
         std::max({lzss_position_distance_16m_compact_frame_working_bytes(),
                   entropy::internal::
                       lzss_position_distance_16m_compact_range_working_bytes(),
                   lzss_position_distance_16m_compact_prefix_working_bytes()});
}
LzssPositionDistance16mCompactOwner::LzssPositionDistance16mCompactOwner(
    LzssPositionDistance16mCompactAllocator &a) noexcept
    : allocator_(a) {}
LzssPositionDistance16mCompactOwner::~LzssPositionDistance16mCompactOwner() {
  allocator_.release(publication_);
}
LzssPositionDistance16mOwnerResult LzssPositionDistance16mCompactOwner::encode(
    std::span<const std::byte> raw, const TypedContextFrameValidationContext &c,
    std::span<std::uint32_t> index, std::size_t extra) noexcept {
  LzssPositionDistance16mOwnerResult r{};
  Working w{};
  Candidate g{allocator_};
  w.controls = allocator_.controls();
  auto error = [&](Code e) {
    r.error = e;
    r.aggregate_bytes = w.total;
    return r;
  };
  if (!w.controls.data || !w.controls.bytes ||
      core::validate_limits(c.limits) != core::LimitError::none)
    return error(Code::invalid_argument);
  if (!core::checked_multiply(index.size(), sizeof(std::uint32_t),
                              w.index_bytes))
    return error(Code::limit_exceeded);
  auto regions = [&]() {
    return std::array{Region{this, sizeof(*this)},
                      Region{w.controls.data, w.controls.bytes},
                      Region{raw.data(), raw.size()},
                      Region{index.data(), w.index_bytes},
                      Region{&c, sizeof(c)},
                      Region{&c.stream, sizeof(c.stream)},
                      Region{&c.limits, sizeof(c.limits)},
                      Region{publication_.data, publication_.capacity},
                      Region{g.compact.data, g.compact.capacity},
                      Region{g.frame.data, g.frame.capacity},
                      Region{g.payload.data, g.payload.capacity},
                      Region{g.publication.data, g.publication.capacity}};
  };
  if (!disjoint(regions()))
    return error(Code::invalid_argument);
  const auto stream_error =
      validate_lzss_position_distance_16m_stream_semantics(c.stream, c.limits);
  if (stream_error != LzssPositionDistance16mPreflightError::none)
    return error(stream_error ==
                         LzssPositionDistance16mPreflightError::limit_exceeded
                     ? Code::limit_exceeded
                     : Code::invalid_argument);
  if (raw.size() > c.limits.max_block_size)
    return error(Code::limit_exceeded);
  if (raw.empty() || raw.size() > 16777216 ||
      c.output_already_committed % c.stream.frame_size ||
      c.expected_sequence != c.output_already_committed / c.stream.frame_size ||
      c.output_already_committed >= c.stream.original_size ||
      raw.size() != std::min<std::uint64_t>(c.stream.frame_size,
                                            c.stream.original_size -
                                                c.output_already_committed))
    return error(Code::invalid_argument);
  w.base = working_bytes();
  for (auto n : {raw.size(), w.index_bytes, w.controls.bytes,
                 w.controls.working_bytes, publication_.capacity, extra})
    if (!add(w.base, n))
      return error(Code::limit_exceeded);
  auto budget = [&](std::size_t request) {
    w.total = w.base;
    for (auto n : {g.compact.capacity, g.frame.capacity, g.payload.capacity,
                   g.publication.capacity, request})
      if (!add(w.total, n))
        return false;
    return w.total <= c.limits.max_internal_buffered_bytes;
  };
  auto allocate = [&](LzssPositionDistance16mOwnedBytes &b,
                      std::size_t n) -> Code {
    if (!budget(n))
      return Code::limit_exceeded;
    b = allocator_.bytes(n);
    const auto controls = allocator_.controls();
    if (controls.data != w.controls.data ||
        controls.bytes != w.controls.bytes ||
        controls.working_bytes != w.controls.working_bytes)
      return Code::invalid_argument;
    if (!b.data && !b.capacity)
      return Code::out_of_memory;
    if (!b.data || b.capacity != n)
      return Code::limit_exceeded;
    if (!budget(0) || !disjoint(regions()))
      return Code::invalid_argument;
    return Code::none;
  };
  std::size_t capacity{};
  if (!core::checked_multiply(raw.size(), std::size_t{2}, capacity))
    return error(Code::limit_exceeded);
  auto e = allocate(g.compact, capacity);
  if (e != Code::none)
    return error(e);
  w.workspace = {g.compact.view(), index, {}, {}};
  if (!remainder(w, {raw.size(), g.compact.capacity, w.index_bytes,
                     lzss_position_distance_16m_compact_frame_working_bytes()}))
    return error(Code::internal_error);
  w.plan = query_lzss_position_distance_16m_compact_frame_encode(
      raw, c, w.workspace, 0, w.retained);
  if (w.plan.error !=
          LzssPositionDistance16mCompactFrameError::storage_too_small ||
      w.plan.bytes_required < 85 || w.plan.compact_bytes > g.compact.capacity ||
      w.plan.aggregate_bytes != w.total)
    return error(
        w.plan.error ==
                    LzssPositionDistance16mCompactFrameError::limit_exceeded ||
                w.plan.range_plan.details.error ==
                    entropy::internal::LzssPositionDistance16mTokenRangeError::
                        limit_exceeded
            ? Code::limit_exceeded
            : Code::invalid_argument);
  // Admission for all three exact requests precedes the first payload request.
  std::size_t requests = w.plan.bytes_required;
  if (!add(requests, w.plan.bytes_required) ||
      !add(requests, w.plan.bytes_required - 80) || !budget(requests))
    return error(Code::limit_exceeded);
  e = allocate(g.frame, w.plan.bytes_required);
  if (e != Code::none)
    return error(e);
  e = allocate(g.payload, w.plan.bytes_required - 80);
  if (e != Code::none)
    return error(e);
  e = allocate(g.publication, w.plan.bytes_required);
  if (e != Code::none)
    return error(e);
  if (!budget(0) ||
      !remainder(
          w, {w.plan.compact_bytes, g.frame.capacity - 80, g.payload.capacity,
              entropy::internal::
                  lzss_position_distance_16m_compact_range_working_bytes()}))
    return error(Code::limit_exceeded);
  w.range = entropy::internal::encode_lzss_position_distance_16m_compact_range(
      g.compact.view().first(w.plan.compact_bytes), c.stream.dictionary,
      w.plan.counts, c.limits, g.frame.view().subspan(80), g.payload.view(),
      w.descriptor, w.retained);
  if (w.range.details.error !=
          entropy::internal::LzssPositionDistance16mTokenRangeError::none ||
      w.range.bytes_committed != w.plan.bytes_required - 80 ||
      w.range.details.token_count != w.plan.token_count ||
      w.range.details.raw_size != raw.size() ||
      w.range.details.operation_count != w.plan.counts.declared_event_count ||
      w.range.details.decision_count != w.plan.counts.declared_decision_count ||
      w.descriptor.payload_size != w.range.bytes_committed ||
      w.descriptor.decision_count != w.range.details.decision_count ||
      w.descriptor.context_count != 48)
    return error(Code::internal_error);
  auto &h = w.layout.header;
  h.sequence = c.expected_sequence;
  h.uncompressed_size = static_cast<std::uint32_t>(raw.size());
  h.token_count = w.plan.counts.declared_token_count;
  h.event_count = w.plan.counts.declared_event_count;
  h.decision_count = w.plan.counts.declared_decision_count;
  h.payload_size = w.descriptor.payload_size;
  h.descriptor_size = 16;
  w.layout.descriptor = w.descriptor;
  w.layout.serialized_size = w.plan.bytes_required;
  if (!remainder(w,
                 {80, g.compact.capacity,
                  lzss_position_distance_16m_compact_prefix_working_bytes()}))
    return error(Code::internal_error);
  w.prefix = serialize_lzss_position_distance_16m_compact_prefix(
      w.layout, c, g.frame.view().first(80), w.written, g.compact.capacity,
      w.retained);
  if (w.prefix.error != LzssPositionDistance16mSerializeError::none)
    return error(w.prefix.error ==
                         LzssPositionDistance16mSerializeError::limit_exceeded
                     ? Code::limit_exceeded
                     : Code::invalid_argument);
  if (!remainder(
          w, {g.frame.capacity, g.compact.capacity, raw.size(),
              sizeof(entropy::internal::LzssPositionDistance16mRangeState)}))
    return error(Code::internal_error);
  if (preflight_lzss_position_distance_16m_compact_prefix(
          g.frame.view(), c, w.parsed, w.requirements, g.compact.capacity,
          w.retained) != LzssPositionDistance16mPreflightError::none ||
      w.requirements.aggregate_working_bytes != w.total || w.written != 80 ||
      w.parsed.serialized_size != w.plan.bytes_required)
    return error(Code::internal_error);
  std::copy_n(g.frame.data, w.plan.bytes_required, g.publication.data);
  std::swap(publication_, g.publication);
  layout_ = w.parsed;
  written_ = w.plan.bytes_required;
  pending_ = true;
  r.aggregate_bytes = w.total;
  r.bytes_validated = written_;
  return r;
}
} // namespace marc::frame::internal

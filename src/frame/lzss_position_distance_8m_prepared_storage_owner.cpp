#include "frame/lzss_position_distance_8m_prepared_storage_owner.hpp"
#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <new>
#include <type_traits>
#include <utility>
namespace marc::frame::internal {
namespace {
using Code = core::ErrorCode;
using Caps = LzssPositionDistance8mGenerationCapacities;
struct Region {
  const void *data;
  std::size_t bytes;
};
struct Working {
  LzssPositionDistance8mStorageLedger ledger{};
  LzssPositionDistance8mStorageAdmission admission{};
  LzssPositionDistance8mTokenStorageDemand tokens{};
  LzssPositionDistance8mFrameStorageDemand frame{};
  LzssPositionDistance8mTokenFrameWorkspace workspace{};
  LzssPositionDistance8mTokenFrameResult encoded{};
  TypedContextFrameLayout layout{};
  std::size_t written{}, local{}, retained{};
};
bool generation_bytes(const Caps &c, std::size_t &out) noexcept {
  std::size_t a{}, b{};
  out = 0;
  if (!core::checked_multiply(
          c.tokens, sizeof(dictionary::internal::LzssTypedToken), a) ||
      !core::checked_multiply(c.token_scratch,
                              sizeof(dictionary::internal::LzssTypedToken), b))
    return false;
  for (auto n : {a, b, c.frame_bytes, c.payload_bytes, c.publication_bytes})
    if (!core::checked_add(out, n, out))
      return false;
  return true;
}
Code translated(LzssPositionDistance8mStorageError e) noexcept {
  if (e == LzssPositionDistance8mStorageError::none)
    return Code::none;
  if (e == LzssPositionDistance8mStorageError::overlapping_buffers ||
      e == LzssPositionDistance8mStorageError::invalid_stream ||
      e == LzssPositionDistance8mStorageError::invalid_position)
    return Code::invalid_argument;
  return Code::limit_exceeded;
}
} // namespace
// No caller-visible factory or transferable plan: only this owner's encode
// member can issue preparation after unchanged frame demand succeeds.
class LzssPositionDistance8mPreparedStorageOwner::Prepared final {
  friend class LzssPositionDistance8mPreparedStorageOwner;
  Prepared(std::span<const std::byte> r,
           const TypedContextFrameValidationContext &c, std::span<uint32_t> i,
           const Generation &g,
           const LzssPositionDistance8mFrameStorageDemand &d) noexcept
      : raw(r), index(i), stream(c.stream), limits(c.limits),
        sequence(c.expected_sequence), prior(c.output_already_committed),
        generation(&g), tokens(g.tokens), scratch(g.scratch), demand(d) {}
  Prepared(const Prepared &) = delete;
  Prepared &operator=(const Prepared &) = delete;
  Prepared(Prepared &&) = delete;
  Prepared &operator=(Prepared &&) = delete;
  std::span<const std::byte> raw;
  std::span<uint32_t> index;
  TypedContextStreamHeader stream;
  core::DecoderLimits limits;
  std::uint64_t sequence, prior;
  const Generation *generation;
  LzssPositionDistance8mOwnedTokens tokens, scratch;
  LzssPositionDistance8mFrameStorageDemand demand;
  bool consumed{};
};
namespace {
struct FinishWorking {
  entropy::internal::LzssPositionDistance8mTokenRangeResult range{};
  entropy::internal::ContextualDynamicRangeDescriptor descriptor{};
  LzssPositionDistance8mSerializeResult prefix{};
  TypedContextFrameLayout parsed{};
  LzssPositionDistance8mFrameRequirements requirements{};
  std::size_t token_bytes{}, local{}, retained{}, prefix_written{};
};
bool same_layout(const TypedContextFrameLayout &a,
                 const TypedContextFrameLayout &b) noexcept {
  return a.header.sequence == b.header.sequence &&
         a.header.uncompressed_size == b.header.uncompressed_size &&
         a.header.token_count == b.header.token_count &&
         a.header.event_count == b.header.event_count &&
         a.header.decision_count == b.header.decision_count &&
         a.header.payload_size == b.header.payload_size &&
         a.header.descriptor_size == b.header.descriptor_size &&
         a.descriptor.decision_count == b.descriptor.decision_count &&
         a.descriptor.payload_size == b.descriptor.payload_size &&
         a.descriptor.context_count == b.descriptor.context_count &&
         a.serialized_size == b.serialized_size;
}
} // namespace
std::size_t LzssPositionDistance8mPreparedStorageOwner::
    continuation_working_bytes() noexcept {
  return sizeof(FinishWorking) +
         2 * sizeof(LzssPositionDistance8mTokenFrameResult) +
         sizeof(TypedContextFrameValidationContext) +
         sizeof(std::array<Region, 28>) + 32 * sizeof(std::size_t) +
         std::max(
             {entropy::internal::
                  lzss_position_distance_8m_token_range_working_bytes(),
              lzss_position_distance_8m_prefix_serialize_working_bytes(),
              sizeof(entropy::internal::LzssPositionDistance8mRangeState)});
}
LzssPositionDistance8mTokenFrameResult
LzssPositionDistance8mPreparedStorageOwner::continue_frame(
    Prepared &p, Generation &g, std::size_t total,
    TypedContextFrameLayout &layout, std::size_t &written) noexcept {
  using E = LzssPositionDistance8mTokenFrameError;
  LzssPositionDistance8mTokenFrameResult result{};
  result.aggregate_bytes = total;
  result.working_state_bytes = continuation_working_bytes();
  if (p.consumed || p.generation != &g || p.tokens.data != g.tokens.data ||
      p.tokens.capacity != g.tokens.capacity ||
      p.scratch.data != g.scratch.data ||
      p.scratch.capacity != g.scratch.capacity) {
    result.error = E::inconsistent_counts;
    return result;
  }
  p.consumed = true;
  const auto &d = p.demand;
  const auto t = d.counts.declared_token_count;
  if (total > p.limits.max_internal_buffered_bytes || t != g.tokens.capacity ||
      t != g.scratch.capacity || d.counts.declared_raw_size != p.raw.size() ||
      d.counts.output_already_committed != p.prior ||
      g.frame.capacity != d.frame_bytes ||
      g.payload.capacity != d.payload_bytes ||
      g.publication.capacity != d.publication_bytes || d.frame_bytes < 80 ||
      d.frame_bytes - 80 != d.payload_bytes ||
      d.publication_bytes != d.frame_bytes) {
    result.error = E::inconsistent_counts;
    return result;
  }
  FinishWorking w{};
  const TypedContextFrameValidationContext c{p.stream, p.limits, p.sequence,
                                             p.prior};
  if (validate_lzss_position_distance_8m_stream_semantics(p.stream, p.limits) !=
      LzssPositionDistance8mPreflightError::none) {
    result.error = E::invalid_stream;
    return result;
  }
  auto remainder = [&](std::initializer_list<std::size_t> locals) noexcept {
    w.local = 0;
    for (auto n : locals)
      if (!core::checked_add(w.local, n, w.local))
        return false;
    if (w.local > total)
      return false;
    w.retained = total - w.local;
    return true;
  };
  if (!core::checked_multiply(static_cast<std::size_t>(t),
                              sizeof(dictionary::internal::LzssTypedToken),
                              w.token_bytes) ||
      !remainder({w.token_bytes, d.payload_bytes, g.payload.capacity,
                  entropy::internal::
                      lzss_position_distance_8m_token_range_working_bytes()})) {
    result.error = E::arithmetic_overflow;
    return result;
  }
  // Retain range count/write, token/parameter/policy validation and finish.
  // This stage never invokes the dictionary parser or token-frame query.
  w.range = entropy::internal::encode_lzss_position_distance_8m_token_range(
      std::span<const dictionary::internal::LzssTypedToken>(g.tokens.view()),
      p.stream.dictionary, d.counts, p.limits, g.frame.view().subspan(80),
      g.payload.view(), w.descriptor, w.retained);
  if (w.range.details.error !=
      entropy::internal::LzssPositionDistance8mTokenRangeError::none) {
    result.error = E::range_error;
    return result;
  }
  if (w.range.bytes_committed != d.payload_bytes ||
      w.range.details.token_count != t ||
      w.range.details.raw_size != p.raw.size() ||
      w.range.details.operation_count != d.counts.declared_event_count ||
      w.range.details.decision_count != d.counts.declared_decision_count ||
      w.descriptor.payload_size != d.layout.descriptor.payload_size ||
      w.descriptor.decision_count != d.layout.descriptor.decision_count ||
      w.descriptor.context_count != d.layout.descriptor.context_count) {
    result.error = E::inconsistent_counts;
    return result;
  }
  if (!remainder(
          {80, lzss_position_distance_8m_prefix_serialize_working_bytes()})) {
    result.error = E::arithmetic_overflow;
    return result;
  }
  w.prefix = serialize_lzss_position_distance_8m_frame_prefix(
      d.layout, c, g.frame.view().first(80), w.prefix_written, w.retained);
  if (w.prefix.error != LzssPositionDistance8mSerializeError::none) {
    result.error = E::prefix_error;
    return result;
  }
  if (!remainder(
          {d.frame_bytes, w.token_bytes, p.raw.size(),
           sizeof(entropy::internal::LzssPositionDistance8mRangeState)})) {
    result.error = E::arithmetic_overflow;
    return result;
  }
  if (preflight_lzss_position_distance_8m_frame_prefix(
          g.frame.view(), c, w.parsed, w.requirements, w.retained) !=
      LzssPositionDistance8mPreflightError::none) {
    result.error = E::prefix_error;
    return result;
  }
  if (w.prefix_written != 80 ||
      w.requirements.aggregate_working_bytes != total ||
      !same_layout(w.parsed, d.layout)) {
    result.error = E::inconsistent_counts;
    return result;
  }
  std::copy_n(g.frame.data, d.frame_bytes, g.publication.data);
  layout = w.parsed;
  written = d.frame_bytes;
  result.bytes_committed = d.frame_bytes;
  return result;
}
LzssPositionDistance8mPreparedStorageOwner::Generation::~Generation() {
  allocator->release(publication);
  allocator->release(payload);
  allocator->release(frame);
  allocator->release(scratch);
  allocator->release(tokens);
}
Caps LzssPositionDistance8mPreparedStorageOwner::Generation::capacities()
    const noexcept {
  return {tokens.capacity, scratch.capacity, frame.capacity, payload.capacity,
          publication.capacity};
}
void LzssPositionDistance8mPreparedStorageOwner::Generation::swap(
    Generation &other) noexcept {
  std::swap(tokens, other.tokens);
  std::swap(scratch, other.scratch);
  std::swap(frame, other.frame);
  std::swap(payload, other.payload);
  std::swap(publication, other.publication);
}
LzssPositionDistance8mPreparedStorageOwner::
    LzssPositionDistance8mPreparedStorageOwner(
        LzssPositionDistance8mBlockAllocator &a) noexcept
    : allocator_(a), current_{&a} {}
LzssPositionDistance8mPreparedStorageOwner::
    ~LzssPositionDistance8mPreparedStorageOwner() = default;
std::span<const std::byte>
LzssPositionDistance8mPreparedStorageOwner::publication() const noexcept {
  return current_.publication.view().first(written_);
}
std::size_t
LzssPositionDistance8mPreparedStorageOwner::working_bytes() noexcept {
  return sizeof(LzssPositionDistance8mPreparedStorageOwner) +
         sizeof(Generation) + sizeof(Working) +
         2 * sizeof(LzssPositionDistance8mOwnerResult) +
         sizeof(std::array<Region, 32>) + sizeof(Prepared) +
         sizeof(TypedContextFrameValidationContext) +
         sizeof(TypedContextStreamHeader) + sizeof(core::DecoderLimits) +
         2 * sizeof(Caps) + sizeof(LzssPositionDistance8mAllocatorControls) +
         32 * sizeof(std::size_t) +
         std::max({lzss_position_distance_8m_storage_demand_working_bytes(),
                   lzss_position_distance_8m_storage_admission_working_bytes(),
                   continuation_working_bytes()});
}
LzssPositionDistance8mOwnerResult
LzssPositionDistance8mPreparedStorageOwner::encode(
    std::span<const std::byte> raw, const TypedContextFrameValidationContext &c,
    std::span<uint32_t> index, std::size_t external) noexcept {
  if (pending_)
    return {Code::invalid_argument};
  Working w{};
  Generation candidate{&allocator_};
  const auto controls = allocator_.controls();
  std::size_t ib{}, oldbytes{}, base{};
  if (!controls.data || !controls.bytes)
    return {Code::invalid_argument};
  if (!core::checked_multiply(index.size(), sizeof(uint32_t), ib) ||
      !generation_bytes(current_.capacities(), oldbytes))
    return {Code::limit_exceeded};
  const std::array regions{
      Region{raw.data(), raw.size()},
      Region{index.data(), ib},
      Region{this, sizeof(*this)},
      Region{&c, sizeof(c)},
      Region{&c.stream, sizeof(c.stream)},
      Region{&c.limits, sizeof(c.limits)},
      Region{controls.data, controls.bytes},
      Region{current_.tokens.data,
             current_.tokens.capacity *
                 sizeof(dictionary::internal::LzssTypedToken)},
      Region{current_.scratch.data,
             current_.scratch.capacity *
                 sizeof(dictionary::internal::LzssTypedToken)},
      Region{current_.frame.data, current_.frame.capacity},
      Region{current_.payload.data, current_.payload.capacity},
      Region{current_.publication.data, current_.publication.capacity}};
  for (std::size_t i = 0; i < regions.size(); ++i)
    for (std::size_t j = i + 1; j < regions.size(); ++j)
      if (core::check_buffer_overlap(regions[i].data, regions[i].bytes,
                                     regions[j].data, regions[j].bytes) !=
          core::BufferOverlap::disjoint)
        return {Code::invalid_argument};
  if (!core::checked_add(external, controls.bytes, base) ||
      !core::checked_add(base, controls.working_bytes, base) ||
      !core::checked_add(base, oldbytes, base) ||
      !core::checked_add(
          base,
          working_bytes() -
              lzss_position_distance_8m_storage_demand_working_bytes(),
          base))
    return {Code::limit_exceeded};
  auto e = prepare_lzss_position_distance_8m_token_storage(raw, c, index,
                                                           w.tokens, base);
  if (e != LzssPositionDistance8mStorageError::none)
    return {translated(e)};
  w.ledger.raw_bytes = raw.size();
  w.ledger.index_entries = index.size();
  w.ledger.old = current_.capacities();
  w.ledger.external_bytes = external;
  if (!core::checked_add(external, controls.bytes, w.ledger.external_bytes) ||
      !core::checked_add(w.ledger.external_bytes, controls.working_bytes,
                         w.ledger.external_bytes))
    return {Code::limit_exceeded};
  w.ledger.persistent_controls =
      working_bytes() -
      lzss_position_distance_8m_storage_admission_working_bytes();
  auto obtain = [&](auto &block, std::size_t count,
                    Caps request) noexcept -> Code {
    w.ledger.partial = candidate.capacities();
    w.ledger.request = request;
    auto status = admit_lzss_position_distance_8m_storage(c.limits, w.ledger,
                                                          w.admission);
    if (status != LzssPositionDistance8mStorageError::none)
      return translated(status);
    if constexpr (std::is_same_v<std::remove_reference_t<decltype(block)>,
                                 LzssPositionDistance8mOwnedTokens>)
      block = allocator_.tokens(count);
    else
      block = allocator_.bytes(count);
    if (!block.data)
      return block.capacity ? Code::internal_error : Code::out_of_memory;
    if (block.capacity != count)
      return Code::limit_exceeded;
    Caps actual = request;
    status = reconcile_lzss_position_distance_8m_storage(c.limits, w.ledger,
                                                         actual, w.admission);
    return translated(status);
  };
  auto code =
      obtain(candidate.tokens, w.tokens.tokens, {w.tokens.tokens, 0, 0, 0, 0});
  if (code != Code::none)
    return {code};
  code = obtain(candidate.scratch, w.tokens.token_scratch,
                {0, w.tokens.token_scratch, 0, 0, 0});
  if (code != Code::none)
    return {code};
  std::size_t pairbytes{};
  if (!generation_bytes(candidate.capacities(), pairbytes) ||
      !core::checked_add(base, pairbytes, w.retained))
    return {Code::limit_exceeded};
  // The pair is passed as local full views, not retained twice.
  e = prepare_lzss_position_distance_8m_frame_storage(
      raw, c, candidate.tokens.view(), candidate.scratch.view(), index, w.frame,
      base);
  if (e != LzssPositionDistance8mStorageError::none)
    return {translated(e)};
  if (w.frame.counts.declared_token_count != candidate.tokens.capacity ||
      w.frame.counts.declared_token_count != candidate.scratch.capacity)
    return {Code::internal_error};
  Prepared prepared(raw, c, index, candidate, w.frame);
  code = obtain(candidate.frame, w.frame.frame_bytes,
                {0, 0, w.frame.frame_bytes, 0, 0});
  if (code != Code::none)
    return {code};
  code = obtain(candidate.payload, w.frame.payload_bytes,
                {0, 0, 0, w.frame.payload_bytes, 0});
  if (code != Code::none)
    return {code};
  code = obtain(candidate.publication, w.frame.publication_bytes,
                {0, 0, 0, 0, w.frame.publication_bytes});
  if (code != Code::none)
    return {code};
  w.ledger.partial = candidate.capacities();
  w.ledger.request = {};
  e = admit_lzss_position_distance_8m_storage(c.limits, w.ledger, w.admission);
  if (e != LzssPositionDistance8mStorageError::none)
    return {translated(e)};
  const auto now = allocator_.controls();
  if (now.data != controls.data || now.bytes != controls.bytes ||
      now.working_bytes != controls.working_bytes)
    return {Code::invalid_argument};
  const std::array complete{
      Region{raw.data(), raw.size()},
      Region{index.data(), ib},
      Region{this, sizeof(*this)},
      Region{&c, sizeof(c)},
      Region{&c.stream, sizeof(c.stream)},
      Region{&c.limits, sizeof(c.limits)},
      Region{controls.data, controls.bytes},
      Region{&prepared, sizeof(prepared)},
      Region{current_.tokens.data,
             current_.tokens.capacity *
                 sizeof(dictionary::internal::LzssTypedToken)},
      Region{current_.scratch.data,
             current_.scratch.capacity *
                 sizeof(dictionary::internal::LzssTypedToken)},
      Region{current_.frame.data, current_.frame.capacity},
      Region{current_.payload.data, current_.payload.capacity},
      Region{current_.publication.data, current_.publication.capacity},
      Region{candidate.tokens.data,
             candidate.tokens.capacity *
                 sizeof(dictionary::internal::LzssTypedToken)},
      Region{candidate.scratch.data,
             candidate.scratch.capacity *
                 sizeof(dictionary::internal::LzssTypedToken)},
      Region{candidate.frame.data, candidate.frame.capacity},
      Region{candidate.payload.data, candidate.payload.capacity},
      Region{candidate.publication.data, candidate.publication.capacity}};
  for (std::size_t i = 0; i < complete.size(); ++i)
    for (std::size_t j = i + 1; j < complete.size(); ++j)
      if (core::check_buffer_overlap(complete[i].data, complete[i].bytes,
                                     complete[j].data, complete[j].bytes) !=
          core::BufferOverlap::disjoint)
        return {Code::invalid_argument};
  w.encoded = continue_frame(prepared, candidate, w.admission.aggregate_bytes,
                             w.layout, w.written);
  if (w.encoded.error != LzssPositionDistance8mTokenFrameError::none)
    return {Code::limit_exceeded};
  if (w.encoded.aggregate_bytes != w.admission.aggregate_bytes ||
      w.written != w.frame.frame_bytes || w.layout.serialized_size != w.written)
    return {Code::internal_error};
  current_.swap(candidate);
  layout_ = w.layout;
  written_ = w.written;
  pending_ = true;
  return {Code::none, w.admission.aggregate_bytes, written_};
}
} // namespace marc::frame::internal

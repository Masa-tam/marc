#include "frame/lzss_position_distance_8m_storage_owner.hpp"
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
template <class T>
LzssPositionDistance8mOwnedBlock<T> allocate(std::size_t n) noexcept {
  if (!n || n > std::numeric_limits<std::size_t>::max() / sizeof(T))
    return {};
  try {
    auto p = new (std::nothrow) T[n]{};
    return {p, p ? n : 0};
  } catch (const std::bad_alloc &) {
    return {};
  }
}
template <class T>
void destroy(LzssPositionDistance8mOwnedBlock<T> &b) noexcept {
  delete[] b.data;
  b = {};
}
} // namespace
LzssPositionDistance8mAllocatorControls
LzssPositionDistance8mExactAllocator::controls() const noexcept {
  return {this, sizeof(*this), 128};
}
LzssPositionDistance8mOwnedTokens
LzssPositionDistance8mExactAllocator::tokens(std::size_t n) noexcept {
  return allocate<dictionary::internal::LzssTypedToken>(n);
}
LzssPositionDistance8mOwnedBytes
LzssPositionDistance8mExactAllocator::bytes(std::size_t n) noexcept {
  return allocate<std::byte>(n);
}
void LzssPositionDistance8mExactAllocator::release(
    LzssPositionDistance8mOwnedTokens &b) noexcept {
  destroy(b);
}
void LzssPositionDistance8mExactAllocator::release(
    LzssPositionDistance8mOwnedBytes &b) noexcept {
  destroy(b);
}
LzssPositionDistance8mStorageOwner::Generation::~Generation() {
  allocator->release(publication);
  allocator->release(payload);
  allocator->release(frame);
  allocator->release(scratch);
  allocator->release(tokens);
}
Caps LzssPositionDistance8mStorageOwner::Generation::capacities()
    const noexcept {
  return {tokens.capacity, scratch.capacity, frame.capacity, payload.capacity,
          publication.capacity};
}
void LzssPositionDistance8mStorageOwner::Generation::swap(
    Generation &other) noexcept {
  std::swap(tokens, other.tokens);
  std::swap(scratch, other.scratch);
  std::swap(frame, other.frame);
  std::swap(payload, other.payload);
  std::swap(publication, other.publication);
}
LzssPositionDistance8mStorageOwner::LzssPositionDistance8mStorageOwner(
    LzssPositionDistance8mBlockAllocator &a) noexcept
    : allocator_(a), current_{&a} {}
LzssPositionDistance8mStorageOwner::~LzssPositionDistance8mStorageOwner() =
    default;
std::span<const std::byte>
LzssPositionDistance8mStorageOwner::publication() const noexcept {
  return current_.publication.view().first(written_);
}
std::size_t LzssPositionDistance8mStorageOwner::working_bytes() noexcept {
  return sizeof(LzssPositionDistance8mStorageOwner) + sizeof(Generation) +
         sizeof(Working) + 2 * sizeof(LzssPositionDistance8mOwnerResult) +
         sizeof(std::array<Region, 17>) +
         sizeof(TypedContextFrameValidationContext) +
         sizeof(TypedContextStreamHeader) + sizeof(core::DecoderLimits) +
         2 * sizeof(Caps) + sizeof(LzssPositionDistance8mAllocatorControls) +
         32 * sizeof(std::size_t) +
         std::max({lzss_position_distance_8m_storage_demand_working_bytes(),
                   lzss_position_distance_8m_storage_admission_working_bytes(),
                   lzss_position_distance_8m_token_frame_working_bytes()});
}
LzssPositionDistance8mOwnerResult LzssPositionDistance8mStorageOwner::encode(
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
  w.workspace = {candidate.tokens.view(), candidate.scratch.view(), index,
                 candidate.frame.view(), candidate.payload.view()};
  if (!generation_bytes(candidate.capacities(), pairbytes))
    return {Code::limit_exceeded};
  w.local = raw.size();
  for (auto n :
       {ib, pairbytes, lzss_position_distance_8m_token_frame_working_bytes()})
    if (!core::checked_add(w.local, n, w.local))
      return {Code::limit_exceeded};
  if (w.local > w.admission.aggregate_bytes)
    return {Code::internal_error};
  w.encoded = encode_lzss_position_distance_8m_token_frame(
      raw, c, w.workspace, candidate.publication.view(), w.layout, w.written,
      w.admission.aggregate_bytes - w.local);
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

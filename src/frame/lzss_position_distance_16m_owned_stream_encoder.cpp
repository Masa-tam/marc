#include "frame/lzss_position_distance_16m_owned_stream_encoder.hpp"
#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"
#include "frame/position_range_owned_stream_common.hpp"
#include <algorithm>
#include <cstring>
#include <exception>
#include <limits>
#include <new>
namespace marc::frame::internal {
namespace {
using Code = core::ErrorCode;
using Encoder = LzssPositionDistance16mOwnedStreamEncoder;
constexpr std::size_t callback_working = 128;
constexpr std::size_t call_controls =
    sizeof(TypedContextFrameValidationContext) +
    sizeof(TypedContextStreamHeader) + sizeof(core::DecoderLimits) +
    sizeof(LzssPositionDistance16mOwnerResult) +
    sizeof(LzssPositionDistance16mSerializeResult) +
    sizeof(core::ProcessResult) +
    sizeof(LzssPositionDistance16mAllocatorControls) +
    16 * (sizeof(void *) + sizeof(std::size_t)) +
    4 * sizeof(std::span<std::byte>) + 32 * sizeof(std::size_t);
struct Profile {
  using CompactOwner = LzssPositionDistance16mCompactOwner;
  using SerializeError = LzssPositionDistance16mSerializeError;
  using StreamAllocator = LzssPositionDistance16mStreamAllocator;
  using AllocatorControls = LzssPositionDistance16mAllocatorControls;
  using OwnedBytes = LzssPositionDistance16mOwnedBytes;
  static constexpr auto call_bytes = call_controls;
  static constexpr auto callback_bytes = callback_working;
  static constexpr auto serialize_working_bytes =
      lzss_position_distance_16m_stream_serialize_working_bytes;
  static constexpr auto serialize_header =
      serialize_lzss_position_distance_16m_stream_header;
  static constexpr auto index_heads =
      dictionary::internal::lzss_position_distance_16m_index_heads;
};
using Mechanics = PositionRangeOwnedStreamMechanics;
template <class T>
LzssPositionDistance16mOwnedBlock<T> allocate_exact(std::size_t n) noexcept {
  if (!n || n > std::numeric_limits<std::size_t>::max() / sizeof(T))
    return {};
  try {
    auto p = new (std::nothrow) T[n]{};
    return {p, p ? n : 0};
  } catch (const std::bad_alloc &) {
    return {};
  }
}
} // namespace
LzssPositionDistance16mAllocatorControls
LzssPositionDistance16mExactStreamAllocator::controls() const noexcept {
  return {this, sizeof(*this), 128};
}
LzssPositionDistance16mOwnedBytes
LzssPositionDistance16mExactStreamAllocator::bytes(std::size_t n) noexcept {
  return allocate_exact<std::byte>(n);
}
LzssPositionDistance16mOwnedIndex
LzssPositionDistance16mExactStreamAllocator::indices(std::size_t n) noexcept {
  return allocate_exact<std::uint32_t>(n);
}
void LzssPositionDistance16mExactStreamAllocator::release(
    LzssPositionDistance16mOwnedBytes &b) noexcept {
  delete[] b.data;
  b = {};
}
void LzssPositionDistance16mExactStreamAllocator::release(
    LzssPositionDistance16mOwnedIndex &b) noexcept {
  delete[] b.data;
  b = {};
}
LzssPositionDistance16mAllocatorControls
Encoder::Bridge::controls() const noexcept {
  return Mechanics::bridge_controls<Profile>(*this);
}
void Encoder::Bridge::remember(const void *p, std::size_t n) noexcept {
  Mechanics::bridge_remember(*this, p, n);
}
void Encoder::Bridge::forget(const void *p) noexcept {
  Mechanics::bridge_forget(*this, p);
}
LzssPositionDistance16mOwnedBytes
Encoder::Bridge::bytes(std::size_t n) noexcept {
  return Mechanics::bridge_bytes<Profile>(*this, n);
}
void Encoder::Bridge::release(LzssPositionDistance16mOwnedBytes &b) noexcept {
  Mechanics::bridge_release<Profile>(*this, b);
}
bool Encoder::Bridge::bytes_live(std::size_t &out) const noexcept {
  return Mechanics::bridge_bytes_live(*this, out);
}
std::size_t Encoder::working_bytes() noexcept {
  return Mechanics::working_bytes<Profile, Encoder>();
}
bool Encoder::budget(std::size_t in, std::size_t out,
                     std::size_t &total) const noexcept {
  return Mechanics::budget(*this, in, out, total);
}
bool Encoder::disjoint(std::span<const std::byte> in,
                       std::span<std::byte> out) const noexcept {
  return Mechanics::disjoint(*this, in, out);
}
Encoder::LzssPositionDistance16mOwnedStreamEncoder(
    TypedContextStreamHeader s, core::DecoderLimits l,
    LzssPositionDistance16mStreamAllocator &a, std::size_t extra) noexcept
    : stream_(s), limits_(l), bridge_(a), owner_(bridge_),
      allocator_controls_(a.controls()), external_(extra) {
  Mechanics::initialize<Profile>(*this, s, a);
}
Encoder::~LzssPositionDistance16mOwnedStreamEncoder() {
  Mechanics::destroy(*this);
}
core::ProcessResult Encoder::fail(Code code, std::uint64_t position,
                                  std::size_t used, std::size_t made) noexcept {
  return Mechanics::fail(*this, code, position, used, made);
}
core::ProcessResult Encoder::process(std::span<const std::byte> input,
                                     std::span<std::byte> output,
                                     std::uint32_t flags) noexcept {
  return Mechanics::process<Profile>(*this, input, output, flags);
}
} // namespace marc::frame::internal

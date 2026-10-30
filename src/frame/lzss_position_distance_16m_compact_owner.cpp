#include "frame/lzss_position_distance_16m_compact_owner.hpp"
#include "frame/position_range_compact_owner_common.hpp"
namespace marc::frame::internal {
namespace {
struct Profile {
  using AllocatorControls = LzssPositionDistance16mAllocatorControls;
  using CompactAllocator = LzssPositionDistance16mCompactAllocator;
  using CompactFrameError = LzssPositionDistance16mCompactFrameError;
  using CompactFramePlan = LzssPositionDistance16mCompactFramePlan;
  using CompactFrameWorkspace = LzssPositionDistance16mCompactFrameWorkspace;
  using CompactOwner = LzssPositionDistance16mCompactOwner;
  using FrameRequirements = LzssPositionDistance16mFrameRequirements;
  using OwnedBytes = LzssPositionDistance16mOwnedBytes;
  using OwnerResult = LzssPositionDistance16mOwnerResult;
  using PreflightError = LzssPositionDistance16mPreflightError;
  using RangeState = entropy::internal::LzssPositionDistance16mRangeState;
  using SerializeError = LzssPositionDistance16mSerializeError;
  using SerializeResult = LzssPositionDistance16mSerializeResult;
  using TokenRangeError =
      entropy::internal::LzssPositionDistance16mTokenRangeError;
  using TokenRangeResult =
      entropy::internal::LzssPositionDistance16mTokenRangeResult;
  static constexpr auto context_count = 48;
  static constexpr auto max_frame = 16777216;
  static constexpr auto compact_frame_working_bytes =
      lzss_position_distance_16m_compact_frame_working_bytes;
  static constexpr auto compact_prefix_working_bytes =
      lzss_position_distance_16m_compact_prefix_working_bytes;
  static constexpr auto compact_range_working_bytes =
      entropy::internal::lzss_position_distance_16m_compact_range_working_bytes;
  static constexpr auto encode_compact_range =
      entropy::internal::encode_lzss_position_distance_16m_compact_range;
  static constexpr auto preflight_compact_prefix =
      preflight_lzss_position_distance_16m_compact_prefix;
  static constexpr auto query_compact_frame_encode =
      query_lzss_position_distance_16m_compact_frame_encode;
  static constexpr auto serialize_compact_prefix =
      serialize_lzss_position_distance_16m_compact_prefix;
  static constexpr auto validate_stream_semantics =
      validate_lzss_position_distance_16m_stream_semantics;
};
using Mechanics = PositionRangeCompactOwnerMechanics<Profile>;
} // namespace
std::size_t LzssPositionDistance16mCompactOwner::working_bytes() noexcept {
  return Mechanics::working_bytes();
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
  return Mechanics::encode(*this, raw, c, index, extra);
}
} // namespace marc::frame::internal

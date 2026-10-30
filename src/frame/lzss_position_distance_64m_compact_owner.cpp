#include "frame/lzss_position_distance_64m_compact_owner.hpp"
#include "frame/position_range_compact_owner_common.hpp"
namespace marc::frame::internal {
namespace {
struct Profile {
  using AllocatorControls = LzssPositionDistance64mAllocatorControls;
  using CompactAllocator = LzssPositionDistance64mCompactAllocator;
  using CompactFrameError = LzssPositionDistance64mCompactFrameError;
  using CompactFramePlan = LzssPositionDistance64mCompactFramePlan;
  using CompactFrameWorkspace = LzssPositionDistance64mCompactFrameWorkspace;
  using CompactOwner = LzssPositionDistance64mCompactOwner;
  using FrameRequirements = LzssPositionDistance64mFrameRequirements;
  using OwnedBytes = LzssPositionDistance64mOwnedBytes;
  using OwnerResult = LzssPositionDistance64mOwnerResult;
  using PreflightError = LzssPositionDistance64mPreflightError;
  using RangeState = entropy::internal::LzssPositionDistance64mRangeState;
  using SerializeError = LzssPositionDistance64mSerializeError;
  using SerializeResult = LzssPositionDistance64mSerializeResult;
  using TokenRangeError =
      entropy::internal::LzssPositionDistance64mTokenRangeError;
  using TokenRangeResult =
      entropy::internal::LzssPositionDistance64mTokenRangeResult;
  static constexpr auto context_count = 50;
  static constexpr auto max_frame = 67108864;
  static constexpr auto compact_frame_working_bytes =
      lzss_position_distance_64m_compact_frame_encode_working_bytes;
  static constexpr auto compact_prefix_working_bytes =
      lzss_position_distance_64m_compact_prefix_working_bytes;
  static constexpr auto compact_range_working_bytes =
      entropy::internal::lzss_position_distance_64m_compact_range_working_bytes;
  static constexpr auto encode_compact_range =
      entropy::internal::encode_lzss_position_distance_64m_compact_range;
  static constexpr auto preflight_compact_prefix =
      preflight_lzss_position_distance_64m_compact_encode_prefix;
  static constexpr auto query_compact_frame_encode =
      query_lzss_position_distance_64m_compact_frame_encode;
  static constexpr auto serialize_compact_prefix =
      serialize_lzss_position_distance_64m_compact_prefix;
  static constexpr auto validate_stream_semantics =
      validate_lzss_position_distance_64m_stream_semantics;
};
using Mechanics = PositionRangeCompactOwnerMechanics<Profile>;
} // namespace
std::size_t LzssPositionDistance64mCompactOwner::working_bytes() noexcept {
  return Mechanics::working_bytes();
}
LzssPositionDistance64mCompactOwner::LzssPositionDistance64mCompactOwner(
    LzssPositionDistance64mCompactAllocator &a) noexcept
    : allocator_(a) {}
LzssPositionDistance64mCompactOwner::~LzssPositionDistance64mCompactOwner() {
  allocator_.release(publication_);
}
LzssPositionDistance64mOwnerResult LzssPositionDistance64mCompactOwner::encode(
    std::span<const std::byte> raw, const TypedContextFrameValidationContext &c,
    std::span<std::uint32_t> index, std::size_t extra) noexcept {
  return Mechanics::encode(*this, raw, c, index, extra);
}
} // namespace marc::frame::internal

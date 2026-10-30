#include "frame/lzss_position_distance_32m_compact_owner.hpp"
#include "frame/position_range_compact_owner_common.hpp"
namespace marc::frame::internal {
namespace {
struct Profile {
  using AllocatorControls = LzssPositionDistance32mAllocatorControls;
  using CompactAllocator = LzssPositionDistance32mCompactAllocator;
  using CompactFrameError = LzssPositionDistance32mCompactFrameError;
  using CompactFramePlan = LzssPositionDistance32mCompactFramePlan;
  using CompactFrameWorkspace = LzssPositionDistance32mCompactFrameWorkspace;
  using CompactOwner = LzssPositionDistance32mCompactOwner;
  using FrameRequirements = LzssPositionDistance32mFrameRequirements;
  using OwnedBytes = LzssPositionDistance32mOwnedBytes;
  using OwnerResult = LzssPositionDistance32mOwnerResult;
  using PreflightError = LzssPositionDistance32mPreflightError;
  using RangeState = entropy::internal::LzssPositionDistance32mRangeState;
  using SerializeError = LzssPositionDistance32mSerializeError;
  using SerializeResult = LzssPositionDistance32mSerializeResult;
  using TokenRangeError =
      entropy::internal::LzssPositionDistance32mTokenRangeError;
  using TokenRangeResult =
      entropy::internal::LzssPositionDistance32mTokenRangeResult;
  static constexpr auto context_count = 49;
  static constexpr auto max_frame = 33554432;
  static constexpr auto compact_frame_working_bytes =
      lzss_position_distance_32m_compact_frame_encode_working_bytes;
  static constexpr auto compact_prefix_working_bytes =
      lzss_position_distance_32m_compact_prefix_working_bytes;
  static constexpr auto compact_range_working_bytes =
      entropy::internal::lzss_position_distance_32m_compact_range_working_bytes;
  static constexpr auto encode_compact_range =
      entropy::internal::encode_lzss_position_distance_32m_compact_range;
  static constexpr auto preflight_compact_prefix =
      preflight_lzss_position_distance_32m_compact_encode_prefix;
  static constexpr auto query_compact_frame_encode =
      query_lzss_position_distance_32m_compact_frame_encode;
  static constexpr auto serialize_compact_prefix =
      serialize_lzss_position_distance_32m_compact_prefix;
  static constexpr auto validate_stream_semantics =
      validate_lzss_position_distance_32m_stream_semantics;
};
using Mechanics = PositionRangeCompactOwnerMechanics<Profile>;
} // namespace
std::size_t LzssPositionDistance32mCompactOwner::working_bytes() noexcept {
  return Mechanics::working_bytes();
}
LzssPositionDistance32mCompactOwner::LzssPositionDistance32mCompactOwner(
    LzssPositionDistance32mCompactAllocator &a) noexcept
    : allocator_(a) {}
LzssPositionDistance32mCompactOwner::~LzssPositionDistance32mCompactOwner() {
  allocator_.release(publication_);
}
LzssPositionDistance32mOwnerResult LzssPositionDistance32mCompactOwner::encode(
    std::span<const std::byte> raw, const TypedContextFrameValidationContext &c,
    std::span<std::uint32_t> index, std::size_t extra) noexcept {
  return Mechanics::encode(*this, raw, c, index, extra);
}
} // namespace marc::frame::internal

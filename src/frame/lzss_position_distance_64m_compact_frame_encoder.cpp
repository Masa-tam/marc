#include "frame/lzss_position_distance_64m_compact_frame_encoder.hpp"
#include "frame/position_range_compact_frame_encoder_common.hpp"
namespace marc::frame::internal {
namespace {
struct Profile {
  using CompactFrameError = LzssPositionDistance64mCompactFrameError;
  using CompactFramePlan = LzssPositionDistance64mCompactFramePlan;
  using CompactFrameResult = LzssPositionDistance64mCompactFrameResult;
  using CompactFrameWorkspace = LzssPositionDistance64mCompactFrameWorkspace;
  using CompactPlan = dictionary::internal::LzssPositionDistance64mCompactPlan;
  using CompactReader =
      dictionary::internal::LzssPositionDistance64mCompactReader;
  using FrameRequirements = LzssPositionDistance64mFrameRequirements;
  using ParseError = dictionary::internal::LzssPositionDistance64mParseError;
  using ParseMetadata =
      dictionary::internal::LzssPositionDistance64mParseMetadata;
  using PreflightError = LzssPositionDistance64mPreflightError;
  using RangeState = entropy::internal::LzssPositionDistance64mRangeState;
  using SerializeError = LzssPositionDistance64mSerializeError;
  using SerializePlan = LzssPositionDistance64mSerializePlan;
  using SerializeResult = LzssPositionDistance64mSerializeResult;
  using TokenCheck = context::internal::LzssPositionDistance64mTokenCheck;
  using TokenRangeError =
      entropy::internal::LzssPositionDistance64mTokenRangeError;
  using TokenRangeResult =
      entropy::internal::LzssPositionDistance64mTokenRangeResult;
  static constexpr auto context_count = 50;
  static constexpr auto compact_prefix_working_bytes =
      lzss_position_distance_64m_compact_prefix_working_bytes;
  static constexpr auto compact_range_working_bytes =
      entropy::internal::lzss_position_distance_64m_compact_range_working_bytes;
  static constexpr auto compact_working_bytes =
      dictionary::internal::lzss_position_distance_64m_compact_working_bytes;
  static constexpr auto encode_compact_range =
      entropy::internal::encode_lzss_position_distance_64m_compact_range;
  static constexpr auto preflight_compact_prefix =
      preflight_lzss_position_distance_64m_compact_encode_prefix;
  static constexpr auto query_compact_prefix_serialize =
      query_lzss_position_distance_64m_compact_prefix_serialize;
  static constexpr auto query_compact_range_encode =
      entropy::internal::query_lzss_position_distance_64m_compact_range_encode;
  static constexpr auto serialize_compact_prefix =
      serialize_lzss_position_distance_64m_compact_prefix;
  static constexpr auto validate_stream_semantics =
      validate_lzss_position_distance_64m_stream_semantics;
  static constexpr auto validate_token =
      context::internal::validate_lzss_position_distance_64m_token;
  static constexpr auto write_compact_private =
      dictionary::internal::write_lzss_position_distance_64m_compact_private;
};
using Mechanics = PositionRangeCompactFrameMechanics<Profile>;
} // namespace
std::size_t
lzss_position_distance_64m_compact_frame_encode_working_bytes() noexcept {
  return Mechanics::working_bytes();
}
LzssPositionDistance64mCompactFramePlan
query_lzss_position_distance_64m_compact_frame_encode(
    std::span<const std::byte> raw,
    const TypedContextFrameValidationContext &context,
    const LzssPositionDistance64mCompactFrameWorkspace &b,
    std::size_t output_capacity, std::size_t retained) noexcept {
  return Mechanics::query(raw, context, b, output_capacity, retained);
}
LzssPositionDistance64mCompactFrameResult
encode_lzss_position_distance_64m_compact_frame(
    std::span<const std::byte> raw,
    const TypedContextFrameValidationContext &context,
    const LzssPositionDistance64mCompactFrameWorkspace &b,
    std::span<std::byte> output, TypedContextFrameLayout &layout,
    std::size_t &written, std::size_t retained) noexcept {
  return Mechanics::encode(raw, context, b, output, layout, written, retained);
}
} // namespace marc::frame::internal

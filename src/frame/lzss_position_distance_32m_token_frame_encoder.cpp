#include "frame/lzss_position_distance_32m_token_frame_encoder.hpp"
#include "frame/position_range_token_frame_encoder_common.hpp"
namespace marc::frame::internal {
namespace {
struct Profile {
  using FrameRequirements = LzssPositionDistance32mFrameRequirements;
  using ParseError = dictionary::internal::LzssPositionDistance32mParseError;
  using ParseMetadata =
      dictionary::internal::LzssPositionDistance32mParseMetadata;
  using ParseResult = dictionary::internal::LzssPositionDistance32mParseResult;
  using PreflightError = LzssPositionDistance32mPreflightError;
  using RangeState = entropy::internal::LzssPositionDistance32mRangeState;
  using SerializeError = LzssPositionDistance32mSerializeError;
  using SerializePlan = LzssPositionDistance32mSerializePlan;
  using SerializeResult = LzssPositionDistance32mSerializeResult;
  using TokenCheck = context::internal::LzssPositionDistance32mTokenCheck;
  using TokenFrameError = LzssPositionDistance32mTokenFrameError;
  using TokenFramePlan = LzssPositionDistance32mTokenFramePlan;
  using TokenFrameResult = LzssPositionDistance32mTokenFrameResult;
  using TokenFrameWorkspace = LzssPositionDistance32mTokenFrameWorkspace;
  using TokenRangeError =
      entropy::internal::LzssPositionDistance32mTokenRangeError;
  using TokenRangeResult =
      entropy::internal::LzssPositionDistance32mTokenRangeResult;
  static constexpr auto context_count = 49;
  static constexpr auto encode_token_range =
      entropy::internal::encode_lzss_position_distance_32m_token_range;
  static constexpr auto indexed_working_bytes =
      dictionary::internal::lzss_position_distance_32m_indexed_working_bytes;
  static constexpr auto prefix_serialize_working_bytes =
      lzss_position_distance_32m_prefix_serialize_working_bytes;
  static constexpr auto preflight_frame_prefix =
      preflight_lzss_position_distance_32m_frame_prefix;
  static constexpr auto query_prefix_serialize =
      query_lzss_position_distance_32m_prefix_serialize;
  static constexpr auto query_token_range_encode =
      entropy::internal::query_lzss_position_distance_32m_token_range_encode;
  static constexpr auto serialize_frame_prefix =
      serialize_lzss_position_distance_32m_frame_prefix;
  static constexpr auto token_range_working_bytes =
      entropy::internal::lzss_position_distance_32m_token_range_working_bytes;
  static constexpr auto tokenize_indexed =
      dictionary::internal::tokenize_lzss_position_distance_32m_indexed;
  static constexpr auto validate_stream_semantics =
      validate_lzss_position_distance_32m_stream_semantics;
  static constexpr auto validate_token =
      context::internal::validate_lzss_position_distance_32m_token;
};
using Mechanics = PositionRangeTokenFrameMechanics<Profile>;
} // namespace
std::size_t lzss_position_distance_32m_token_frame_working_bytes() noexcept {
  return Mechanics::working_bytes();
}
LzssPositionDistance32mTokenFramePlan
query_lzss_position_distance_32m_token_frame_encode(
    std::span<const std::byte> raw,
    const TypedContextFrameValidationContext &context,
    const LzssPositionDistance32mTokenFrameWorkspace &b,
    std::size_t output_capacity, std::size_t retained) noexcept {
  return Mechanics::query(raw, context, b, output_capacity, retained);
}
LzssPositionDistance32mTokenFrameResult
encode_lzss_position_distance_32m_token_frame(
    std::span<const std::byte> raw,
    const TypedContextFrameValidationContext &context,
    const LzssPositionDistance32mTokenFrameWorkspace &b,
    std::span<std::byte> output, TypedContextFrameLayout &layout,
    std::size_t &written, std::size_t retained) noexcept {
  return Mechanics::encode(raw, context, b, output, layout, written, retained);
}
} // namespace marc::frame::internal

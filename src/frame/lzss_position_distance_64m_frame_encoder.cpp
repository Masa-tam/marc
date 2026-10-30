#include "frame/lzss_position_distance_64m_frame_encoder.hpp"
#include "frame/position_range_frame_encoder_common.hpp"
namespace marc::frame::internal {
namespace {
struct Profile {
  using FrameEncodeError = LzssPositionDistance64mFrameEncodeError;
  using ParseResult = dictionary::internal::LzssPositionDistance64mParseResult;
  using ParseMetadata =
      dictionary::internal::LzssPositionDistance64mParseMetadata;
  using MapResult = context::internal::LzssPositionDistance64mMapResult;
  using RangeEncodePlan =
      entropy::internal::LzssPositionDistance64mRangeEncodePlan;
  using RangeEncodeResult =
      entropy::internal::LzssPositionDistance64mRangeEncodeResult;
  using SerializeResult = LzssPositionDistance64mSerializeResult;
  using FrameRequirements = LzssPositionDistance64mFrameRequirements;
  using FrameEncodeResult = LzssPositionDistance64mFrameEncodeResult;
  using FrameEncodeWorkspace = LzssPositionDistance64mFrameEncodeWorkspace;
  using PreflightError = LzssPositionDistance64mPreflightError;
  using ParseError = dictionary::internal::LzssPositionDistance64mParseError;
  using SerializeError = LzssPositionDistance64mSerializeError;
  using RangeState = entropy::internal::LzssPositionDistance64mRangeState;
  static constexpr auto query_range_encode =
      entropy::internal::query_lzss_position_distance_64m_range_encode;
  static constexpr auto encode_range_operations =
      entropy::internal::encode_lzss_position_distance_64m_range_operations;
  static constexpr auto map_tokens =
      context::internal::map_lzss_position_distance_64m_tokens;
  static constexpr auto validate_stream_semantics =
      validate_lzss_position_distance_64m_stream_semantics;
  static constexpr auto prefix_serialize_working_bytes =
      lzss_position_distance_64m_prefix_serialize_working_bytes;
  static constexpr auto indexed_working_bytes =
      dictionary::internal::lzss_position_distance_64m_indexed_working_bytes;
  static constexpr auto range_encode_working_bytes =
      entropy::internal::lzss_position_distance_64m_range_encode_working_bytes;
  static constexpr auto serialize_frame_prefix =
      serialize_lzss_position_distance_64m_frame_prefix;
  static auto
  preflight_frame_prefix(std::span<const std::byte> input,
                         const TypedContextFrameValidationContext &c,
                         TypedContextFrameLayout &layout, FrameRequirements &r,
                         std::size_t retained = 0) noexcept {
    return preflight_lzss_position_distance_64m_frame_prefix(input, c, layout,
                                                             r, retained);
  }
  static constexpr auto map_working_bytes =
      context::internal::lzss_position_distance_64m_map_working_bytes;
  static constexpr auto tokenize_indexed =
      dictionary::internal::tokenize_lzss_position_distance_64m_indexed;
};
using Mechanics = PositionRangeFrameEncodeMechanics<Profile>;
} // namespace
std::size_t lzss_position_distance_64m_frame_encode_working_bytes() noexcept {
  return Mechanics::working_bytes();
}
LzssPositionDistance64mFrameEncodeResult
encode_lzss_position_distance_64m_frame(
    std::span<const std::byte> raw,
    const TypedContextFrameValidationContext &context,
    const LzssPositionDistance64mFrameEncodeWorkspace &b,
    std::span<std::byte> output, TypedContextFrameLayout &layout,
    std::size_t &written, std::size_t retained) noexcept {
  return Mechanics::encode(raw, context, b, output, layout, written, retained);
}
} // namespace marc::frame::internal

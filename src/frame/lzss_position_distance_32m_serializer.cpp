#include "frame/lzss_position_distance_32m_serializer.hpp"
#include "frame/position_range_serializer_common.hpp"
namespace marc::frame::internal {
namespace {
struct Profile {
  using SerializeError = LzssPositionDistance32mSerializeError;
  using PreflightError = LzssPositionDistance32mPreflightError;
  using FrameRequirements = LzssPositionDistance32mFrameRequirements;
  using SerializePlan = LzssPositionDistance32mSerializePlan;
  using SerializeResult = LzssPositionDistance32mSerializeResult;
  static constexpr auto parse_stream_header =
      parse_lzss_position_distance_32m_stream_header;
  static auto
  preflight_frame_prefix(std::span<const std::byte> input,
                         const TypedContextFrameValidationContext &c,
                         TypedContextFrameLayout &layout, FrameRequirements &r,
                         std::size_t retained = 0) noexcept {
    return preflight_lzss_position_distance_32m_frame_prefix(input, c, layout,
                                                             r, retained);
  }
  static constexpr auto validate_stream_semantics =
      validate_lzss_position_distance_32m_stream_semantics;
};
using Mechanics = PositionRangeSerializeMechanics<Profile>;
} // namespace
std::size_t
lzss_position_distance_32m_stream_serialize_working_bytes() noexcept {
  return Mechanics::stream_working_bytes();
}
std::size_t
lzss_position_distance_32m_prefix_serialize_working_bytes() noexcept {
  return Mechanics::prefix_working_bytes();
}
LzssPositionDistance32mSerializePlan
query_lzss_position_distance_32m_stream_serialize(
    const TypedContextStreamHeader &s, const core::DecoderLimits &l,
    std::size_t capacity, std::size_t retained) noexcept {
  return Mechanics::query_stream(s, l, capacity, retained);
}
LzssPositionDistance32mSerializePlan
query_lzss_position_distance_32m_prefix_serialize(
    const TypedContextFrameLayout &layout,
    const TypedContextFrameValidationContext &c, std::size_t capacity,
    std::size_t retained) noexcept {
  return Mechanics::query_prefix(layout, c, capacity, retained);
}
LzssPositionDistance32mSerializeResult
serialize_lzss_position_distance_32m_stream_header(
    const TypedContextStreamHeader &s, const core::DecoderLimits &l,
    std::span<std::byte> out, std::size_t &written,
    std::size_t retained) noexcept {
  return Mechanics::serialize_stream(s, l, out, written, retained);
}
LzssPositionDistance32mSerializeResult
serialize_lzss_position_distance_32m_frame_prefix(
    const TypedContextFrameLayout &layout,
    const TypedContextFrameValidationContext &c, std::span<std::byte> out,
    std::size_t &written, std::size_t retained) noexcept {
  return Mechanics::serialize_prefix(layout, c, out, written, retained);
}
} // namespace marc::frame::internal

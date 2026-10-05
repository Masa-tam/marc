#ifndef MARC_FRAME_LZSS_POSITION_DISTANCE_32M_PREFLIGHT_HPP
#define MARC_FRAME_LZSS_POSITION_DISTANCE_32M_PREFLIGHT_HPP
#include "entropy/lzss_position_distance_32m_range_state.hpp"
#include "frame/typed_context_format.hpp"
namespace marc::frame::internal {
enum class LzssPositionDistance32mPreflightError : std::uint8_t {
  none,
  invalid_stream,
  unexpected_sequence,
  unexpected_frame_size,
  contradictory_counts,
  invalid_descriptor,
  unsupported_feature,
  limit_exceeded,
  arithmetic_overflow,
  truncated_stream_header,
  truncated_frame_header,
  truncated_descriptor,
  invalid_magic,
  unsupported_version,
  invalid_header_size,
  nonzero_reserved,
  unsupported_format,
  overlapping_output,
};
struct LzssPositionDistance32mFrameRequirements {
  std::size_t serialized_frame_bytes{}, token_count{}, raw_frame_bytes{},
      aggregate_working_bytes{};
  bool
  operator==(const LzssPositionDistance32mFrameRequirements &) const = default;
};
[[nodiscard]] LzssPositionDistance32mPreflightError
validate_lzss_position_distance_32m_stream_semantics(
    const TypedContextStreamHeader &, const core::DecoderLimits &) noexcept;
// Private parser. Output and consumed count remain unchanged on any failure.
[[nodiscard]] LzssPositionDistance32mPreflightError
parse_lzss_position_distance_32m_stream_header(std::span<const std::byte>,
                                               const core::DecoderLimits &,
                                               TypedContextStreamHeader &,
                                               std::size_t &) noexcept;
// Requires only the 80-byte prefix. Success never proves payload validity.
// Charges actual model storage and caller-specified separately retained bytes.
// Layout/requirements remain unchanged on failure, including metadata aliasing.
[[nodiscard]] LzssPositionDistance32mPreflightError
preflight_lzss_position_distance_32m_frame_prefix(
    std::span<const std::byte>, const TypedContextFrameValidationContext &,
    TypedContextFrameLayout &, LzssPositionDistance32mFrameRequirements &,
    std::size_t retained_state_bytes = 0) noexcept;
// Same wire validation, using the compact record bound min(3R,9T) for the
// prefix's staging-storage floor. Complete caller capacities are charged by
// the compact frame/stream queries before decoding or publication.
[[nodiscard]] LzssPositionDistance32mPreflightError
preflight_lzss_position_distance_32m_compact_frame_prefix(
    std::span<const std::byte>, const TypedContextFrameValidationContext &,
    TypedContextFrameLayout &, LzssPositionDistance32mFrameRequirements &,
    std::size_t retained_state_bytes = 0) noexcept;
} // namespace marc::frame::internal
#endif

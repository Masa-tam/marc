#include "frame/lzss_position_distance_32m_frame_decoder.hpp"
#include "frame/position_range_frame_decoder_common.hpp"
namespace marc::frame::internal {
namespace {
struct Profile {
  using FrameDecodeError = LzssPositionDistance32mFrameDecodeError;
  using FrameDecodePlan = LzssPositionDistance32mFrameDecodePlan;
  using PreflightError = LzssPositionDistance32mPreflightError;
  using FrameDecodeResult = LzssPositionDistance32mFrameDecodeResult;
  using FrameRequirements = LzssPositionDistance32mFrameRequirements;
  static auto
  preflight_frame_prefix(std::span<const std::byte> input,
                         const TypedContextFrameValidationContext &c,
                         TypedContextFrameLayout &layout, FrameRequirements &r,
                         std::size_t retained = 0) noexcept {
    return preflight_lzss_position_distance_32m_frame_prefix(input, c, layout,
                                                             r, retained);
  }
  static constexpr auto decode_tokens =
      context::internal::decode_lzss_position_distance_32m_tokens;
  static constexpr auto query_tokens =
      context::internal::query_lzss_position_distance_32m_tokens;
  static constexpr auto validate_token =
      context::internal::validate_lzss_position_distance_32m_token;
};
using Mechanics = PositionRangeFrameDecodeMechanics<Profile>;
} // namespace
LzssPositionDistance32mFrameDecodePlan
query_lzss_position_distance_32m_frame_decode(
    std::span<const std::byte> input,
    const TypedContextFrameValidationContext &c, std::size_t token_capacity,
    std::size_t token_scratch_capacity, std::size_t raw_capacity,
    std::size_t raw_scratch_capacity, std::size_t retained) noexcept {
  return Mechanics::query(input, c, token_capacity, token_scratch_capacity,
                          raw_capacity, raw_scratch_capacity, retained);
}
LzssPositionDistance32mFrameDecodeResult
decode_lzss_position_distance_32m_frame(
    std::span<const std::byte> input,
    const TypedContextFrameValidationContext &c,
    std::span<dictionary::internal::LzssTypedToken> tokens,
    std::span<dictionary::internal::LzssTypedToken> token_scratch,
    std::span<std::byte> raw_output, std::span<std::byte> raw_scratch,
    TypedContextFrameLayout &layout_output, std::size_t retained) noexcept {
  return Mechanics::decode(input, c, tokens, token_scratch, raw_output,
                           raw_scratch, layout_output, retained);
}
} // namespace marc::frame::internal

#ifndef MARC_FRAME_LZSS_POSITION_DISTANCE_8M_STORAGE_ADAPTER_HPP
#define MARC_FRAME_LZSS_POSITION_DISTANCE_8M_STORAGE_ADAPTER_HPP
#include "frame/lzss_position_distance_8m_token_frame_encoder.hpp"
namespace marc::frame::internal {
enum class LzssPositionDistance8mStorageError : std::uint8_t {
  none,
  overlapping_buffers,
  arithmetic_overflow,
  limit_exceeded,
  invalid_stream,
  invalid_position,
  parser_error,
  count_error,
  prefix_error,
  overcapacity
};
struct LzssPositionDistance8mGenerationCapacities {
  std::size_t tokens{}, token_scratch{}, frame_bytes{}, payload_bytes{},
      publication_bytes{};
};
struct LzssPositionDistance8mStorageLedger {
  std::size_t raw_bytes{}, index_entries{};
  LzssPositionDistance8mGenerationCapacities old{}, partial{}, request{};
  std::size_t persistent_controls{}, call_controls{}, input_bytes{},
      output_bytes{}, external_bytes{}, helper_bytes{};
};
struct LzssPositionDistance8mStorageAdmission {
  std::size_t aggregate_bytes{}, request_bytes{}, working_bytes{};
};
struct LzssPositionDistance8mTokenStorageDemand {
  std::size_t tokens{}, token_scratch{}, aggregate_bytes{}, admitted_bytes{};
};
struct LzssPositionDistance8mFrameStorageDemand {
  std::size_t frame_bytes{}, payload_bytes{}, publication_bytes{},
      aggregate_bytes{}, admitted_bytes{};
  context::internal::LzssFieldContextValidationContext counts{};
  TypedContextFrameLayout layout{};
};
[[nodiscard]] std::size_t
lzss_position_distance_8m_storage_admission_working_bytes() noexcept;
[[nodiscard]] std::size_t
lzss_position_distance_8m_storage_demand_working_bytes() noexcept;
// Numeric old + partial candidate + next request, with full call
// views/controls. No allocation or release; ANY error preserves the whole
// result object.
[[nodiscard]] LzssPositionDistance8mStorageError
admit_lzss_position_distance_8m_storage(
    const core::DecoderLimits &, const LzssPositionDistance8mStorageLedger &,
    LzssPositionDistance8mStorageAdmission &) noexcept;
// Re-admit the declared request, reject ANY actual extent above its bound, then
// reconcile actual extents. Old/partial owners stay charged; no death is
// inferred.
[[nodiscard]] LzssPositionDistance8mStorageError
reconcile_lzss_position_distance_8m_storage(
    const core::DecoderLimits &, const LzssPositionDistance8mStorageLedger &,
    const LzssPositionDistance8mGenerationCapacities &actual,
    LzssPositionDistance8mStorageAdmission &) noexcept;
// Successful demand authorizes only the numerically admitted NEW token pair.
// Index is private/discardable. Whole output metadata unchanged on any error.
[[nodiscard]] LzssPositionDistance8mStorageError
prepare_lzss_position_distance_8m_token_storage(
    std::span<const std::byte>, const TypedContextFrameValidationContext &,
    std::span<std::uint32_t> index, LzssPositionDistance8mTokenStorageDemand &,
    std::size_t retained_owner_bytes = 0) noexcept;
// Token pair/index already owned and charged. Select NEW
// frame/payload/publication extents only after exact
// count/descriptor/prefix/hard-policy checks. Private tokens/index discardable;
// success does NOT authorize payload publication. Existing frame
// owners/replacements and caller views belong in retained bytes.
[[nodiscard]] LzssPositionDistance8mStorageError
prepare_lzss_position_distance_8m_frame_storage(
    std::span<const std::byte>, const TypedContextFrameValidationContext &,
    std::span<dictionary::internal::LzssTypedToken> tokens,
    std::span<dictionary::internal::LzssTypedToken> token_scratch,
    std::span<std::uint32_t> index, LzssPositionDistance8mFrameStorageDemand &,
    std::size_t retained_owner_bytes = 0) noexcept;
} // namespace marc::frame::internal
#endif

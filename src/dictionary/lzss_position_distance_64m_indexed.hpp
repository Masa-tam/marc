#ifndef MARC_DICTIONARY_LZSS_POSITION_DISTANCE_64M_INDEXED_HPP
#define MARC_DICTIONARY_LZSS_POSITION_DISTANCE_64M_INDEXED_HPP
#include "dictionary/lzss_position_distance_64m_reference.hpp"
namespace marc::dictionary::internal {
inline constexpr std::size_t lzss_position_distance_64m_index_heads = 1048576;
[[nodiscard]] std::size_t
lzss_position_distance_64m_indexed_working_bytes() noexcept;
// Same finite longest/nearest/minimum-five policy as the exhaustive reference.
// Workspace holds live uint32 objects: 1048576 heads + one link per raw byte.
// FULL workspace capacity is charged. Query resets/writes private workspace;
// it returns exact counts, never caller tokens/metadata. Workspace discardable.
[[nodiscard]] LzssPositionDistance64mParsePlan
query_lzss_position_distance_64m_indexed(
    std::span<const std::byte>, const LzssParameters &,
    const core::DecoderLimits &, std::size_t output_capacity,
    std::size_t scratch_capacity, std::span<std::uint32_t> private_workspace,
    std::size_t retained_owner_bytes = 0,
    std::uint64_t raw_already_committed = 0) noexcept;
// Full regions/config/metadata disjoint; borrowed raw/config stable.
// ANY failure preserves whole caller tokens AND metadata, committed0.
// Private token scratch/workspace discardable. No allocation/public admission.
[[nodiscard]] LzssPositionDistance64mParseResult
tokenize_lzss_position_distance_64m_indexed(
    std::span<const std::byte>, const LzssParameters &,
    const core::DecoderLimits &, std::span<LzssTypedToken> output,
    std::span<LzssTypedToken> private_scratch,
    std::span<std::uint32_t> private_workspace,
    LzssPositionDistance64mParseMetadata &,
    std::size_t retained_owner_bytes = 0,
    std::uint64_t raw_already_committed = 0) noexcept;
} // namespace marc::dictionary::internal
#endif

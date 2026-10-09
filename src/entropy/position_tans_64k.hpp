#ifndef MARC_ENTROPY_POSITION_TANS_64K_HPP
#define MARC_ENTROPY_POSITION_TANS_64K_HPP
#include "core/limits.hpp"
#include "entropy/tans_tables.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {
inline constexpr std::size_t position_tans_64k_contexts = 48;
inline constexpr std::size_t position_tans_64k_frequencies = 4570;
inline constexpr std::size_t position_tans_64k_descriptor_capacity = 9122;
inline constexpr std::size_t position_tans_64k_table_entries = 49 * 4096;
inline constexpr auto position_tans_64k_alphabets = [] {
    std::array<std::uint16_t, 48> a{};
    for (std::size_t c=0; c<48; ++c)
        a[c] = c<3 ? 2 : c<20 ? 256 : c<23 ? 9 : c<32 ? 17 : 2;
    return a;
}();
inline constexpr auto position_tans_64k_offsets = [] {
    std::array<std::size_t, 49> o{};
    for (std::size_t c=0; c<48; ++c) o[c+1]=o[c]+position_tans_64k_alphabets[c];
    return o;
}();
static_assert(position_tans_64k_offsets.back()==4570);
struct PositionTans64kDescriptor {
    std::uint32_t decision_count{};
    std::uint32_t payload_size{2};
    std::uint8_t final_valid_bits{};
    std::array<std::uint16_t, 4570> frequencies{};
    bool operator==(const PositionTans64kDescriptor&) const = default;
};
enum class PositionTans64kError : std::uint8_t {
    none, truncated, invalid_metadata, invalid_payload_size, invalid_frequencies,
    invalid_mode, noncanonical, trailing_data, limit_exceeded, output_too_small,
    invalid_context, inactive_context, invalid_symbol, arithmetic_overflow,
    invalid_table, invalid_state, nonzero_padding, count_mismatch,
    unused_context, invalid_terminal_state, trailing_bits, overlapping_buffers,
    not_started, already_finished
};
// Descriptor/size destinations commit only on success, including aliasing.
[[nodiscard]] PositionTans64kError validate_position_tans_64k_descriptor(
    const PositionTans64kDescriptor&, const core::DecoderLimits&, std::size_t&) noexcept;
[[nodiscard]] PositionTans64kError serialize_position_tans_64k_descriptor(
    const PositionTans64kDescriptor&, const core::DecoderLimits&,
    std::span<std::byte>, std::size_t&) noexcept;
[[nodiscard]] PositionTans64kError parse_position_tans_64k_descriptor(
    std::span<const std::byte>, std::uint32_t expected_decisions,
    std::uint32_t expected_payload_size, const core::DecoderLimits&,
    PositionTans64kDescriptor&) noexcept;
class PositionTans64kModelBuilder {
public:
    // -1 is a uniform length-residual bit; ordinary IDs are 0..47.
    [[nodiscard]] PositionTans64kError add(int context, std::uint32_t symbol) noexcept;
    [[nodiscard]] PositionTans64kError finish(PositionTans64kDescriptor&) const noexcept;
private:
    std::array<std::uint32_t, 4570> counts_{};
    std::array<std::uint32_t, 48> totals_{};
    std::uint32_t decisions_{};
};
// Table spans are private scratch, not ordinary committed destinations.
[[nodiscard]] PositionTans64kError build_position_tans_64k_encode_tables(
    const PositionTans64kDescriptor&, const core::DecoderLimits&,
    std::span<std::uint16_t>) noexcept;
class PositionTans64kReverseWriter {
public:
    // Empty output measures. Nonempty output is private scratch and must be
    // discarded on failure; input/table/output overlap is rejected before writes.
    PositionTans64kReverseWriter(const PositionTans64kDescriptor&,
        const core::DecoderLimits&, std::span<const std::uint16_t>,
        std::span<std::byte>) noexcept;
    [[nodiscard]] PositionTans64kError add(int context, std::uint32_t symbol) noexcept;
    [[nodiscard]] PositionTans64kError finish(std::size_t&, std::uint8_t&) noexcept;
private:
    PositionTans64kDescriptor descriptor_{};
    std::span<const std::uint16_t> tables_{};
    std::span<std::byte> output_{};
    std::array<bool, 48> used_{};
    std::uint32_t state_{4096}, decisions_{};
    std::size_t bits_{}, cursor_{};
    PositionTans64kError error_{};
    bool finished_{};
};
class PositionTans64kDecoder {
public:
    [[nodiscard]] PositionTans64kError begin(const PositionTans64kDescriptor&,
        std::span<const std::byte>, const core::DecoderLimits&,
        std::span<TansDecodeEntry> private_tables) noexcept;
    // Each symbol destination commits only on success. Tokens/raw frames must
    // remain private until finish succeeds and dictionary validation completes.
    [[nodiscard]] PositionTans64kError read(int context, std::uint32_t&) noexcept;
    [[nodiscard]] PositionTans64kError finish() noexcept;
private:
    PositionTans64kDescriptor descriptor_{};
    std::span<const std::byte> payload_{};
    std::span<const TansDecodeEntry> tables_{};
    std::array<bool, 48> used_{};
    std::uint32_t state_{4096}, decisions_{};
    std::size_t bits_{}, cursor_{};
    PositionTans64kError error_{PositionTans64kError::not_started};
    bool finished_{};
};
} // namespace marc::entropy::internal
#endif

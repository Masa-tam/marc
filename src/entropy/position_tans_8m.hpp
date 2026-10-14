#ifndef MARC_ENTROPY_POSITION_TANS_8M_HPP
#define MARC_ENTROPY_POSITION_TANS_8M_HPP
#include "core/limits.hpp"
#include "entropy/tans_tables.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {
inline constexpr std::size_t position_tans_8m_contexts = 55;
inline constexpr std::size_t position_tans_8m_frequencies = 4647;
inline constexpr std::size_t position_tans_8m_descriptor_capacity = 9270;
inline constexpr std::size_t position_tans_8m_table_entries = 56 * 4096;
inline constexpr auto position_tans_8m_alphabets = [] {
    std::array<std::uint16_t, 55> a{};
    for (std::size_t c=0; c<55; ++c)
        a[c] = c<3 ? 2 : c<20 ? 256 : c<23 ? 9 : c<32 ? 24 : 2;
    return a;
}();
inline constexpr auto position_tans_8m_offsets = [] {
    std::array<std::size_t, 56> o{};
    for (std::size_t c=0; c<55; ++c) o[c+1]=o[c]+position_tans_8m_alphabets[c];
    return o;
}();
static_assert(position_tans_8m_offsets.back()==4647);
struct PositionTans8mDescriptor {
    std::uint32_t decision_count{};
    std::uint32_t payload_size{2};
    std::uint8_t final_valid_bits{};
    std::array<std::uint16_t, 4647> frequencies{};
    bool operator==(const PositionTans8mDescriptor&) const = default;
};
enum class PositionTans8mError : std::uint8_t {
    none, truncated, invalid_metadata, invalid_payload_size, invalid_frequencies,
    invalid_mode, noncanonical, trailing_data, limit_exceeded, output_too_small,
    invalid_context, inactive_context, invalid_symbol, arithmetic_overflow,
    invalid_table, invalid_state, nonzero_padding, count_mismatch,
    unused_context, invalid_terminal_state, trailing_bits, overlapping_buffers,
    not_started, already_finished
};
// Descriptor/size destinations commit only on success, including aliasing.
[[nodiscard]] PositionTans8mError validate_position_tans_8m_descriptor(
    const PositionTans8mDescriptor&, const core::DecoderLimits&, std::size_t&) noexcept;
[[nodiscard]] PositionTans8mError serialize_position_tans_8m_descriptor(
    const PositionTans8mDescriptor&, const core::DecoderLimits&,
    std::span<std::byte>, std::size_t&) noexcept;
[[nodiscard]] PositionTans8mError parse_position_tans_8m_descriptor(
    std::span<const std::byte>, std::uint32_t expected_decisions,
    std::uint32_t expected_payload_size, const core::DecoderLimits&,
    PositionTans8mDescriptor&) noexcept;
class PositionTans8mModelBuilder {
public:
    // -1 is a uniform length-residual bit; ordinary IDs are 0..54.
    [[nodiscard]] PositionTans8mError add(int context, std::uint32_t symbol) noexcept;
    [[nodiscard]] PositionTans8mError finish(PositionTans8mDescriptor&) const noexcept;
private:
    std::array<std::uint32_t, 4647> counts_{};
    std::array<std::uint32_t, 55> totals_{};
    std::uint32_t decisions_{};
};
// Table spans are private scratch, not ordinary committed destinations.
[[nodiscard]] PositionTans8mError build_position_tans_8m_encode_tables(
    const PositionTans8mDescriptor&, const core::DecoderLimits&,
    std::span<std::uint16_t>) noexcept;
class PositionTans8mReverseWriter {
public:
    // Empty output measures. Nonempty output is private scratch and must be
    // discarded on failure; input/table/output overlap is rejected before writes.
    PositionTans8mReverseWriter(const PositionTans8mDescriptor&,
        const core::DecoderLimits&, std::span<const std::uint16_t>,
        std::span<std::byte>) noexcept;
    [[nodiscard]] PositionTans8mError add(int context, std::uint32_t symbol) noexcept;
    [[nodiscard]] PositionTans8mError finish(std::size_t&, std::uint8_t&) noexcept;
private:
    PositionTans8mDescriptor descriptor_{};
    std::span<const std::uint16_t> tables_{};
    std::span<std::byte> output_{};
    std::array<bool, 55> used_{};
    std::uint32_t state_{4096}, decisions_{};
    std::size_t bits_{}, cursor_{};
    PositionTans8mError error_{};
    bool finished_{};
};
class PositionTans8mDecoder {
public:
    [[nodiscard]] PositionTans8mError begin(const PositionTans8mDescriptor&,
        std::span<const std::byte>, const core::DecoderLimits&,
        std::span<TansDecodeEntry> private_tables) noexcept;
    // Each symbol destination commits only on success. Tokens/raw frames must
    // remain private until finish succeeds and dictionary validation completes.
    [[nodiscard]] PositionTans8mError read(int context, std::uint32_t&) noexcept;
    [[nodiscard]] PositionTans8mError finish() noexcept;
private:
    PositionTans8mDescriptor descriptor_{};
    std::span<const std::byte> payload_{};
    std::span<const TansDecodeEntry> tables_{};
    std::array<bool, 55> used_{};
    std::uint32_t state_{4096}, decisions_{};
    std::size_t bits_{}, cursor_{};
    PositionTans8mError error_{PositionTans8mError::not_started};
    bool finished_{};
};
} // namespace marc::entropy::internal
#endif





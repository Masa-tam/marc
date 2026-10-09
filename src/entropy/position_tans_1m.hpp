#ifndef MARC_ENTROPY_POSITION_TANS_1M_HPP
#define MARC_ENTROPY_POSITION_TANS_1M_HPP
#include "core/limits.hpp"
#include "entropy/tans_tables.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {
inline constexpr std::size_t position_tans_1m_contexts = 52;
inline constexpr std::size_t position_tans_1m_frequencies = 4614;
inline constexpr std::size_t position_tans_1m_descriptor_capacity = 9207;
inline constexpr std::size_t position_tans_1m_table_entries = 53 * 4096;
inline constexpr auto position_tans_1m_alphabets = [] {
    std::array<std::uint16_t, 52> a{};
    for (std::size_t c=0; c<52; ++c)
        a[c] = c<3 ? 2 : c<20 ? 256 : c<23 ? 9 : c<32 ? 21 : 2;
    return a;
}();
inline constexpr auto position_tans_1m_offsets = [] {
    std::array<std::size_t, 53> o{};
    for (std::size_t c=0; c<52; ++c) o[c+1]=o[c]+position_tans_1m_alphabets[c];
    return o;
}();
static_assert(position_tans_1m_offsets.back()==4614);
struct PositionTans1mDescriptor {
    std::uint32_t decision_count{};
    std::uint32_t payload_size{2};
    std::uint8_t final_valid_bits{};
    std::array<std::uint16_t, 4614> frequencies{};
    bool operator==(const PositionTans1mDescriptor&) const = default;
};
enum class PositionTans1mError : std::uint8_t {
    none, truncated, invalid_metadata, invalid_payload_size, invalid_frequencies,
    invalid_mode, noncanonical, trailing_data, limit_exceeded, output_too_small,
    invalid_context, inactive_context, invalid_symbol, arithmetic_overflow,
    invalid_table, invalid_state, nonzero_padding, count_mismatch,
    unused_context, invalid_terminal_state, trailing_bits, overlapping_buffers,
    not_started, already_finished
};
// Descriptor/size destinations commit only on success, including aliasing.
[[nodiscard]] PositionTans1mError validate_position_tans_1m_descriptor(
    const PositionTans1mDescriptor&, const core::DecoderLimits&, std::size_t&) noexcept;
[[nodiscard]] PositionTans1mError serialize_position_tans_1m_descriptor(
    const PositionTans1mDescriptor&, const core::DecoderLimits&,
    std::span<std::byte>, std::size_t&) noexcept;
[[nodiscard]] PositionTans1mError parse_position_tans_1m_descriptor(
    std::span<const std::byte>, std::uint32_t expected_decisions,
    std::uint32_t expected_payload_size, const core::DecoderLimits&,
    PositionTans1mDescriptor&) noexcept;
class PositionTans1mModelBuilder {
public:
    // -1 is a uniform length-residual bit; ordinary IDs are 0..51.
    [[nodiscard]] PositionTans1mError add(int context, std::uint32_t symbol) noexcept;
    [[nodiscard]] PositionTans1mError finish(PositionTans1mDescriptor&) const noexcept;
private:
    std::array<std::uint32_t, 4614> counts_{};
    std::array<std::uint32_t, 52> totals_{};
    std::uint32_t decisions_{};
};
// Table spans are private scratch, not ordinary committed destinations.
[[nodiscard]] PositionTans1mError build_position_tans_1m_encode_tables(
    const PositionTans1mDescriptor&, const core::DecoderLimits&,
    std::span<std::uint16_t>) noexcept;
class PositionTans1mReverseWriter {
public:
    // Empty output measures. Nonempty output is private scratch and must be
    // discarded on failure; input/table/output overlap is rejected before writes.
    PositionTans1mReverseWriter(const PositionTans1mDescriptor&,
        const core::DecoderLimits&, std::span<const std::uint16_t>,
        std::span<std::byte>) noexcept;
    [[nodiscard]] PositionTans1mError add(int context, std::uint32_t symbol) noexcept;
    [[nodiscard]] PositionTans1mError finish(std::size_t&, std::uint8_t&) noexcept;
private:
    PositionTans1mDescriptor descriptor_{};
    std::span<const std::uint16_t> tables_{};
    std::span<std::byte> output_{};
    std::array<bool, 52> used_{};
    std::uint32_t state_{4096}, decisions_{};
    std::size_t bits_{}, cursor_{};
    PositionTans1mError error_{};
    bool finished_{};
};
class PositionTans1mDecoder {
public:
    [[nodiscard]] PositionTans1mError begin(const PositionTans1mDescriptor&,
        std::span<const std::byte>, const core::DecoderLimits&,
        std::span<TansDecodeEntry> private_tables) noexcept;
    // Each symbol destination commits only on success. Tokens/raw frames must
    // remain private until finish succeeds and dictionary validation completes.
    [[nodiscard]] PositionTans1mError read(int context, std::uint32_t&) noexcept;
    [[nodiscard]] PositionTans1mError finish() noexcept;
private:
    PositionTans1mDescriptor descriptor_{};
    std::span<const std::byte> payload_{};
    std::span<const TansDecodeEntry> tables_{};
    std::array<bool, 52> used_{};
    std::uint32_t state_{4096}, decisions_{};
    std::size_t bits_{}, cursor_{};
    PositionTans1mError error_{PositionTans1mError::not_started};
    bool finished_{};
};
} // namespace marc::entropy::internal
#endif


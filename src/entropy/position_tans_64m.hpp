#ifndef MARC_ENTROPY_POSITION_TANS_64M_HPP
#define MARC_ENTROPY_POSITION_TANS_64M_HPP
#include "core/limits.hpp"
#include "entropy/tans_tables.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {
inline constexpr std::size_t position_tans_64m_contexts = 58;
inline constexpr std::size_t position_tans_64m_frequencies = 4680;
inline constexpr std::size_t position_tans_64m_descriptor_capacity = 9334;
inline constexpr std::size_t position_tans_64m_table_entries = 59 * 4096;
inline constexpr auto position_tans_64m_alphabets = [] {
    std::array<std::uint16_t, 58> a{};
    for (std::size_t c=0; c<58; ++c)
        a[c] = c<3 ? 2 : c<20 ? 256 : c<23 ? 9 : c<32 ? 27 : 2;
    return a;
}();
inline constexpr auto position_tans_64m_offsets = [] {
    std::array<std::size_t, 59> o{};
    for (std::size_t c=0; c<58; ++c) o[c+1]=o[c]+position_tans_64m_alphabets[c];
    return o;
}();
static_assert(position_tans_64m_offsets.back()==4680);
struct PositionTans64mDescriptor {
    std::uint32_t decision_count{};
    std::uint32_t payload_size{2};
    std::uint8_t final_valid_bits{};
    std::array<std::uint16_t, 4680> frequencies{};
    bool operator==(const PositionTans64mDescriptor&) const = default;
};
enum class PositionTans64mError : std::uint8_t {
    none, truncated, invalid_metadata, invalid_payload_size, invalid_frequencies,
    invalid_mode, noncanonical, trailing_data, limit_exceeded, output_too_small,
    invalid_context, inactive_context, invalid_symbol, arithmetic_overflow,
    invalid_table, invalid_state, nonzero_padding, count_mismatch,
    unused_context, invalid_terminal_state, trailing_bits, overlapping_buffers,
    not_started, already_finished
};
// Descriptor/size destinations commit only on success, including aliasing.
[[nodiscard]] PositionTans64mError validate_position_tans_64m_descriptor(
    const PositionTans64mDescriptor&, const core::DecoderLimits&, std::size_t&) noexcept;
[[nodiscard]] PositionTans64mError serialize_position_tans_64m_descriptor(
    const PositionTans64mDescriptor&, const core::DecoderLimits&,
    std::span<std::byte>, std::size_t&) noexcept;
[[nodiscard]] PositionTans64mError parse_position_tans_64m_descriptor(
    std::span<const std::byte>, std::uint32_t expected_decisions,
    std::uint32_t expected_payload_size, const core::DecoderLimits&,
    PositionTans64mDescriptor&) noexcept;
class PositionTans64mModelBuilder {
public:
    // -1 is a uniform length-residual bit; ordinary IDs are 0..57.
    [[nodiscard]] PositionTans64mError add(int context, std::uint32_t symbol) noexcept;
    [[nodiscard]] PositionTans64mError finish(PositionTans64mDescriptor&) const noexcept;
private:
    std::array<std::uint32_t, 4680> counts_{};
    std::array<std::uint32_t, 58> totals_{};
    std::uint32_t decisions_{};
};
// Table spans are private scratch, not ordinary committed destinations.
[[nodiscard]] PositionTans64mError build_position_tans_64m_encode_tables(
    const PositionTans64mDescriptor&, const core::DecoderLimits&,
    std::span<std::uint16_t>) noexcept;
class PositionTans64mReverseWriter {
public:
    // Empty output measures. Nonempty output is private scratch and must be
    // discarded on failure; input/table/output overlap is rejected before writes.
    PositionTans64mReverseWriter(const PositionTans64mDescriptor&,
        const core::DecoderLimits&, std::span<const std::uint16_t>,
        std::span<std::byte>) noexcept;
    [[nodiscard]] PositionTans64mError add(int context, std::uint32_t symbol) noexcept;
    [[nodiscard]] PositionTans64mError finish(std::size_t&, std::uint8_t&) noexcept;
private:
    PositionTans64mDescriptor descriptor_{};
    std::span<const std::uint16_t> tables_{};
    std::span<std::byte> output_{};
    std::array<bool, 58> used_{};
    std::uint32_t state_{4096}, decisions_{};
    std::size_t bits_{}, cursor_{};
    PositionTans64mError error_{};
    bool finished_{};
};
class PositionTans64mDecoder {
public:
    [[nodiscard]] PositionTans64mError begin(const PositionTans64mDescriptor&,
        std::span<const std::byte>, const core::DecoderLimits&,
        std::span<TansDecodeEntry> private_tables) noexcept;
    // Each symbol destination commits only on success. Tokens/raw frames must
    // remain private until finish succeeds and dictionary validation completes.
    [[nodiscard]] PositionTans64mError read(int context, std::uint32_t&) noexcept;
    [[nodiscard]] PositionTans64mError finish() noexcept;
private:
    PositionTans64mDescriptor descriptor_{};
    std::span<const std::byte> payload_{};
    std::span<const TansDecodeEntry> tables_{};
    std::array<bool, 58> used_{};
    std::uint32_t state_{4096}, decisions_{};
    std::size_t bits_{}, cursor_{};
    PositionTans64mError error_{PositionTans64mError::not_started};
    bool finished_{};
};
} // namespace marc::entropy::internal
#endif

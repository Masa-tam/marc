#ifndef MARC_ENTROPY_CONTEXTUAL_TANS_ENCODE_CORE_HPP
#define MARC_ENTROPY_CONTEXTUAL_TANS_ENCODE_CORE_HPP

#include "entropy/contextual_tans_format.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

inline constexpr std::size_t contextual_tans_encode_table_entries =
    static_cast<std::size_t>(contextual_tans_decode_table_entries);

enum class ContextualTansEncodeError : std::uint8_t {
    none,
    unsupported_context_variant,
    empty_operations,
    invalid_operation_kind,
    invalid_context,
    invalid_alphabet,
    invalid_symbol,
    invalid_bypass_width,
    nonzero_unused_field,
    table_output_too_small,
    payload_output_too_small,
    overlapping_buffers,
    limit_exceeded,
    normalization_error,
    table_error,
    arithmetic_overflow,
    internal_error,
};

class ContextualTansModelBuilder {
public:
    explicit ContextualTansModelBuilder(
        context::internal::LzssFieldContextVariant variant =
            context::internal::LzssFieldContextVariant::field_context_64k)
        noexcept;

    [[nodiscard]] ContextualTansEncodeError add_symbol(
        std::uint16_t context_id,
        std::uint16_t alphabet,
        std::uint32_t value) noexcept;

    [[nodiscard]] ContextualTansEncodeError add_bypass(
        std::uint8_t bit_count,
        std::uint32_t value) noexcept;

    [[nodiscard]] ContextualTansEncodeError finish(
        ContextualTansDescriptor& descriptor) const noexcept;

    [[nodiscard]] std::uint32_t decision_count() const noexcept {
        return decision_count_;
    }

private:
    context::internal::LzssFieldContextLayout layout_{};
    std::array<std::uint32_t, contextual_tans_frequency_capacity> counts_{};
    std::array<std::uint32_t, contextual_tans_context_count> totals_{};
    std::uint32_t decision_count_{};
};

[[nodiscard]] ContextualTansEncodeError build_contextual_tans_encode_tables(
    const ContextualTansDescriptor& descriptor,
    const core::DecoderLimits& limits,
    std::span<std::uint16_t> output,
    context::internal::LzssFieldContextVariant variant =
        context::internal::LzssFieldContextVariant::field_context_64k) noexcept;

class ContextualTansReverseWriter {
public:
    ContextualTansReverseWriter(
        const ContextualTansDescriptor& descriptor,
        std::span<const std::uint16_t> encode_tables,
        std::span<std::byte> output,
        context::internal::LzssFieldContextVariant variant =
            context::internal::LzssFieldContextVariant::field_context_64k)
        noexcept;

    [[nodiscard]] ContextualTansEncodeError encode_symbol(
        std::uint16_t context_id,
        std::uint16_t alphabet,
        std::uint32_t value) noexcept;

    [[nodiscard]] ContextualTansEncodeError encode_bypass(
        std::uint8_t bit_count,
        std::uint32_t value) noexcept;

    [[nodiscard]] ContextualTansEncodeError finish(
        std::size_t& payload_size,
        std::uint8_t& final_valid_bits) noexcept;

private:
    [[nodiscard]] ContextualTansEncodeError encode_transition(
        std::size_t table_index,
        std::uint16_t cumulative,
        std::uint16_t frequency) noexcept;

    const ContextualTansDescriptor& descriptor_;
    context::internal::LzssFieldContextLayout layout_{};
    std::span<const std::uint16_t> encode_tables_{};
    std::span<std::byte> output_{};
    std::uint32_t state_{contextual_tans_total_frequency};
    std::size_t bit_count_{};
    std::size_t cursor_{};
    ContextualTansEncodeError error_{ContextualTansEncodeError::none};
    bool finished_{};
};

} // namespace marc::entropy::internal

#endif

#ifndef MARC_ENTROPY_CONTEXTUAL_RANS_ENCODE_CORE_HPP
#define MARC_ENTROPY_CONTEXTUAL_RANS_ENCODE_CORE_HPP

#include "entropy/contextual_rans_format.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

enum class ContextualRansEncodeError : std::uint8_t {
    none,
    unsupported_context_variant,
    empty_operations,
    invalid_operation_kind,
    invalid_context,
    invalid_alphabet,
    invalid_symbol,
    invalid_bypass_width,
    nonzero_unused_field,
    payload_output_too_small,
    overlapping_buffers,
    limit_exceeded,
    normalization_error,
    arithmetic_overflow,
    internal_error,
};

class ContextualRansModelBuilder {
public:
    explicit ContextualRansModelBuilder(
        context::internal::LzssFieldContextVariant variant =
            context::internal::LzssFieldContextVariant::
                field_context_64k) noexcept;

    [[nodiscard]] ContextualRansEncodeError add_symbol(
        std::uint16_t context_id, std::uint16_t alphabet,
        std::uint32_t value) noexcept;

    [[nodiscard]] ContextualRansEncodeError add_bypass(
        std::uint8_t bit_count, std::uint32_t value) noexcept;

    [[nodiscard]] ContextualRansEncodeError finish(
        ContextualRansDescriptor& descriptor) const noexcept;

    [[nodiscard]] std::uint32_t decision_count() const noexcept {
        return decision_count_;
    }

private:
    context::internal::LzssFieldContextLayout layout_{};
    std::array<std::uint32_t, contextual_rans_frequency_capacity> counts_{};
    std::array<std::uint32_t, contextual_rans_context_count> totals_{};
    std::uint32_t decision_count_{};
};

class ContextualRansReverseWriter {
public:
    ContextualRansReverseWriter(
        const ContextualRansDescriptor& descriptor,
        std::span<std::byte> output,
        context::internal::LzssFieldContextVariant variant =
            context::internal::LzssFieldContextVariant::
                field_context_64k) noexcept;

    [[nodiscard]] ContextualRansEncodeError encode_symbol(
        std::uint16_t context_id, std::uint16_t alphabet,
        std::uint32_t value) noexcept;

    [[nodiscard]] ContextualRansEncodeError encode_bypass(
        std::uint8_t bit_count, std::uint32_t value) noexcept;

    [[nodiscard]] ContextualRansEncodeError finish(
        std::size_t& payload_size) noexcept;

private:
    [[nodiscard]] ContextualRansEncodeError encode_range(
        std::uint16_t cumulative, std::uint16_t frequency) noexcept;

    const ContextualRansDescriptor& descriptor_;
    context::internal::LzssFieldContextLayout layout_{};
    std::span<std::byte> output_{};
    std::uint64_t state_{rans_lower_bound};
    std::size_t cursor_{};
    std::size_t renormalization_bytes_{};
    ContextualRansEncodeError error_{ContextualRansEncodeError::none};
    bool finished_{};
};

} // namespace marc::entropy::internal

#endif

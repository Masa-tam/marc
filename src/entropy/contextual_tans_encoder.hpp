#ifndef MARC_ENTROPY_CONTEXTUAL_TANS_ENCODER_HPP
#define MARC_ENTROPY_CONTEXTUAL_TANS_ENCODER_HPP

#include "context/lzss_field_context.hpp"
#include "entropy/contextual_tans_encode_core.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

struct ContextualTansEncodeResult {
    std::size_t operation_count{};
    std::size_t operation_index{};
    std::uint32_t decision_count{};
    std::size_t payload_size{};
    std::size_t required_table_entries{
        contextual_tans_encode_table_entries};
    ContextualTansEncodeError error{ContextualTansEncodeError::none};
};

[[nodiscard]] ContextualTansEncodeResult plan_contextual_tans_operations(
    std::span<const context::internal::ModeledOperation> operations,
    const core::DecoderLimits& limits,
    std::span<std::uint16_t> private_encode_tables,
    ContextualTansDescriptor& descriptor,
    context::internal::LzssFieldContextVariant variant =
        context::internal::LzssFieldContextVariant::field_context_64k) noexcept;

[[nodiscard]] ContextualTansEncodeResult encode_contextual_tans_operations(
    std::span<const context::internal::ModeledOperation> operations,
    const core::DecoderLimits& limits,
    std::span<std::uint16_t> private_encode_tables,
    std::span<std::byte> payload_output,
    ContextualTansDescriptor& descriptor,
    context::internal::LzssFieldContextVariant variant =
        context::internal::LzssFieldContextVariant::field_context_64k) noexcept;

} // namespace marc::entropy::internal

#endif

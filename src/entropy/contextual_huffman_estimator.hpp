#ifndef MARC_ENTROPY_CONTEXTUAL_HUFFMAN_ESTIMATOR_HPP
#define MARC_ENTROPY_CONTEXTUAL_HUFFMAN_ESTIMATOR_HPP

#include "context/lzss_field_context.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::entropy::internal {

struct ContextualHuffmanEstimate {
    std::uint64_t descriptor_bytes{};
    std::uint64_t symbol_bits{};
    std::uint64_t bypass_bits{};
    std::uint64_t payload_bytes{};
    std::uint64_t total_bytes{};
    std::uint16_t active_tables{};
    std::uint16_t stored_models{};
};

struct ContextualHuffmanEstimates {
    ContextualHuffmanEstimate field_tables{};
    ContextualHuffmanEstimate contextual_tables{};
    ContextualHuffmanEstimate shared_contextual_tables{};
};

enum class ContextualHuffmanEstimateError : std::uint8_t {
    none,
    invalid_operation,
    frequency_overflow,
    huffman_build_error,
    arithmetic_overflow,
};

struct ContextualHuffmanEstimateResult {
    ContextualHuffmanEstimates estimates{};
    std::size_t operation_index{};
    ContextualHuffmanEstimateError error{
        ContextualHuffmanEstimateError::none};
};

// Estimates a provisional Format 2 descriptor. This does not reserve a
// bitstream representation and deliberately emits no bytes.
[[nodiscard]] ContextualHuffmanEstimateResult
estimate_contextual_huffman_cost(
    std::span<const context::internal::ModeledOperation> operations) noexcept;

} // namespace marc::entropy::internal

#endif

#include "entropy/rans_encoder.hpp"

#include "core/checked_math.hpp"
#include "core/endian.hpp"
#include "entropy/rans_normalizer.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace marc::entropy::internal {
namespace {

struct EncodePassResult {
    std::size_t payload_size{};
    RansEncodeError error{RansEncodeError::none};
};

[[nodiscard]] std::array<std::uint32_t, 256> cumulative_for(
    const std::array<std::uint16_t, 256>& frequencies) noexcept {
    std::array<std::uint32_t, 256> cumulative{};
    std::uint32_t sum{};
    for (std::size_t symbol = 0; symbol < frequencies.size(); ++symbol) {
        cumulative[symbol] = sum;
        sum += frequencies[symbol];
    }
    return cumulative;
}

[[nodiscard]] EncodePassResult encode_pass(
    const std::span<const std::byte> input,
    const std::array<std::uint16_t, 256>& frequencies,
    const std::span<std::byte> output) noexcept {
    const auto cumulative = cumulative_for(frequencies);
    std::uint64_t state = rans_lower_bound;
    std::size_t renormalization_bytes{};
    std::size_t cursor = output.size();
    for (std::size_t reverse = input.size(); reverse != 0; --reverse) {
        if (state < rans_lower_bound
            || state >= rans_lower_bound * UINT64_C(256)) {
            return {0, RansEncodeError::internal_error};
        }
        const auto symbol = std::to_integer<std::uint8_t>(input[reverse - 1]);
        const auto frequency = frequencies[symbol];
        if (frequency == 0) return {0, RansEncodeError::internal_error};
        const auto maximum =
            ((rans_lower_bound >> rans_table_log) << 8) * frequency;
        while (state >= maximum) {
            if (renormalization_bytes
                == std::numeric_limits<std::size_t>::max()) {
                return {0, RansEncodeError::arithmetic_overflow};
            }
            if (!output.empty()) {
                if (cursor <= rans_min_payload_size) {
                    return {0, RansEncodeError::internal_error};
                }
                output[--cursor] = static_cast<std::byte>(state & 0xffU);
            }
            ++renormalization_bytes;
            state >>= 8;
        }
        std::uint64_t quotient_part{};
        if (!core::checked_multiply(
                state / frequency,
                static_cast<std::uint64_t>(rans_total_frequency),
                quotient_part)
            || !core::checked_add(
                quotient_part, state % frequency, state)
            || !core::checked_add(
                state, static_cast<std::uint64_t>(cumulative[symbol]),
                state)) {
            return {0, RansEncodeError::arithmetic_overflow};
        }
    }
    if (state < rans_lower_bound
        || state >= rans_lower_bound * UINT64_C(256)) {
        return {0, RansEncodeError::internal_error};
    }
    std::size_t payload_size{};
    if (!core::checked_add(
            static_cast<std::size_t>(rans_min_payload_size),
            renormalization_bytes, payload_size)) {
        return {0, RansEncodeError::arithmetic_overflow};
    }
    if (!output.empty()) {
        if (output.size() != payload_size || cursor != rans_min_payload_size
            || !core::store_le(output, 0, state)) {
            return {payload_size, RansEncodeError::internal_error};
        }
    }
    return {payload_size, RansEncodeError::none};
}

} // namespace

RansEncodeResult plan_rans_block(
    const std::span<const std::byte> input,
    const core::DecoderLimits& limits,
    RansDescriptor& descriptor) noexcept {
    if (input.empty()) return {0, RansEncodeError::empty_input};
    if (input.size() > rans_max_block_size
        || input.size() > std::numeric_limits<std::uint32_t>::max()) {
        return {0, RansEncodeError::block_too_large};
    }
    std::array<std::uint16_t, 256> frequencies{};
    const auto normalized = normalize_rans_frequencies(input, limits, frequencies);
    if (normalized != RansNormalizationError::none) {
        return {0, normalized == RansNormalizationError::limit_exceeded
                       ? RansEncodeError::limit_exceeded
                       : RansEncodeError::normalization_error};
    }
    const auto pass = encode_pass(input, frequencies, {});
    if (pass.error != RansEncodeError::none) {
        return {pass.payload_size, pass.error};
    }
    if (pass.payload_size > std::numeric_limits<std::uint32_t>::max()) {
        return {pass.payload_size, RansEncodeError::arithmetic_overflow};
    }
    std::uint64_t buffered{};
    if (!core::checked_add(
            static_cast<std::uint64_t>(rans_descriptor_size),
            static_cast<std::uint64_t>(pass.payload_size), buffered)) {
        return {pass.payload_size, RansEncodeError::arithmetic_overflow};
    }
    if (pass.payload_size > limits.max_compressed_payload_size
        || buffered > limits.max_internal_buffered_bytes) {
        return {pass.payload_size, RansEncodeError::limit_exceeded};
    }
    RansDescriptor planned{};
    planned.symbol_count = static_cast<std::uint32_t>(input.size());
    planned.payload_size = static_cast<std::uint32_t>(pass.payload_size);
    planned.frequencies = frequencies;
    if (validate_rans_descriptor(
            planned, planned.symbol_count, planned.payload_size, limits)
        != RansFormatError::none) {
        return {pass.payload_size, RansEncodeError::internal_error};
    }
    descriptor = planned;
    return {pass.payload_size, RansEncodeError::none};
}

RansEncodeResult encode_rans_block(
    const std::span<const std::byte> input,
    const core::DecoderLimits& limits,
    const std::span<std::byte> payload_output,
    RansDescriptor& descriptor) noexcept {
    RansDescriptor planned{};
    const auto plan = plan_rans_block(input, limits, planned);
    if (plan.error != RansEncodeError::none) return plan;
    if (payload_output.size() < plan.payload_size) {
        return {plan.payload_size, RansEncodeError::payload_output_too_small};
    }
    const auto pass = encode_pass(
        input, planned.frequencies, payload_output.first(plan.payload_size));
    if (pass.error != RansEncodeError::none
        || pass.payload_size != plan.payload_size) {
        return {plan.payload_size, RansEncodeError::internal_error};
    }
    descriptor = planned;
    return plan;
}

} // namespace marc::entropy::internal

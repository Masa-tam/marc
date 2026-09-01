#include "entropy/contextual_dynamic_range_encoder.hpp"

#include "core/checked_math.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace marc::entropy::internal {
namespace {

inline constexpr std::uint32_t normalization_threshold = UINT32_C(1) << 24;

class RangeEncoder {
public:
    explicit RangeEncoder(const std::span<std::byte> output) noexcept
        : output_(output) {}

    [[nodiscard]] bool encode(
        const std::uint32_t cumulative,
        const std::uint16_t frequency,
        const std::uint32_t total) noexcept {
        if (total == 0 || frequency == 0 || cumulative >= total
            || static_cast<std::uint64_t>(cumulative) + frequency > total
            || range_ < normalization_threshold) {
            return false;
        }
        range_ /= total;
        if (range_ == 0) return false;
        low_ += static_cast<std::uint64_t>(cumulative) * range_;
        range_ *= frequency;
        while (range_ < normalization_threshold) {
            range_ <<= 8;
            if (!shift_low()) return false;
        }
        return true;
    }

    [[nodiscard]] bool finish() noexcept {
        for (int index = 0; index < 5; ++index) {
            if (!shift_low()) return false;
        }
        return true;
    }

    [[nodiscard]] std::size_t size() const noexcept { return output_size_; }

private:
    [[nodiscard]] bool emit(const std::uint8_t value) noexcept {
        if (output_size_ == std::numeric_limits<std::size_t>::max()) {
            return false;
        }
        if (!output_.empty()) {
            if (output_size_ >= output_.size()) return false;
            output_[output_size_] = static_cast<std::byte>(value);
        }
        ++output_size_;
        return true;
    }

    [[nodiscard]] bool shift_low() noexcept {
        const auto low32 = static_cast<std::uint32_t>(low_);
        const auto carry = static_cast<std::uint32_t>(low_ >> 32);
        if (carry > 1) return false;
        if (low32 < UINT32_C(0xff000000) || carry != 0) {
            if (!emit(static_cast<std::uint8_t>(cache_ + carry))) return false;
            const auto delayed =
                static_cast<std::uint8_t>(UINT32_C(0xff) + carry);
            for (std::size_t index = 1; index < pending_; ++index) {
                if (!emit(delayed)) return false;
            }
            cache_ = static_cast<std::uint8_t>(low32 >> 24);
            pending_ = 0;
        }
        if (pending_ == std::numeric_limits<std::size_t>::max()) return false;
        ++pending_;
        low_ = static_cast<std::uint32_t>(low32 << 8);
        return true;
    }

    std::span<std::byte> output_{};
    std::uint64_t low_{};
    std::uint32_t range_{UINT32_MAX};
    std::uint8_t cache_{};
    std::size_t pending_{1};
    std::size_t output_size_{};
};

struct ModelState {
    std::array<std::uint16_t,
               marc::context::internal::
                   lzss_field_context_frequency_entries_v5>
        frequencies{};
    std::array<std::uint32_t,
               marc::context::internal::lzss_field_context_count> totals{};

    explicit ModelState(
        const context::internal::LzssFieldContextLayout& layout) noexcept
        : layout_(layout) {
        frequencies.fill(1);
        for (std::size_t index = 0; index < totals.size(); ++index) {
            totals[index] = (*layout_.alphabets)[index];
        }
    }

    void update(const std::uint16_t context, const std::uint32_t symbol)
        noexcept {
        const auto offset = (*layout_.offsets)[context];
        ++frequencies[offset + symbol];
        auto& total = totals[context];
        ++total;
        if (total != contextual_dynamic_range_model_total_limit) return;
        total = 0;
        const auto alphabet = (*layout_.alphabets)[context];
        for (std::size_t index = 0; index < alphabet; ++index) {
            auto& frequency = frequencies[offset + index];
            frequency = static_cast<std::uint16_t>(
                (static_cast<std::uint32_t>(frequency) + 1U) / 2U);
            total += frequency;
        }
    }

    context::internal::LzssFieldContextLayout layout_{};
};

[[nodiscard]] ContextualDynamicRangeEncodeResult fail(
    ContextualDynamicRangeEncodeResult result,
    const ContextualDynamicRangeEncodeError error) noexcept {
    result.error = error;
    return result;
}

[[nodiscard]] bool add_decisions(
    const std::uint32_t increment,
    ContextualDynamicRangeEncodeResult& result) noexcept {
    std::uint32_t updated{};
    if (!core::checked_add(result.decision_count, increment, updated)) {
        result.error = ContextualDynamicRangeEncodeError::arithmetic_overflow;
        return false;
    }
    result.decision_count = updated;
    return true;
}

[[nodiscard]] ContextualDynamicRangeEncodeResult run_encoder(
    const std::span<const context::internal::ModeledOperation> operations,
    const std::span<std::byte> output,
    const context::internal::LzssFieldContextLayout& layout) noexcept {
    ContextualDynamicRangeEncodeResult result{};
    ModelState models{layout};
    RangeEncoder encoder(output);
    for (const auto& operation : operations) {
        result.operation_index = result.operation_count;
        const auto kind = static_cast<std::uint8_t>(operation.kind);
        if (kind > static_cast<std::uint8_t>(
                       context::internal::ModeledOperationKind::bypass_bits)) {
            return fail(result,
                        ContextualDynamicRangeEncodeError::
                            invalid_operation_kind);
        }
        if (operation.kind == context::internal::ModeledOperationKind::symbol) {
            if (operation.context_id
                >= marc::context::internal::lzss_field_context_count) {
                return fail(result,
                            ContextualDynamicRangeEncodeError::invalid_context);
            }
            if (operation.alphabet_size
                != (*layout.alphabets)[operation.context_id]) {
                return fail(
                    result, ContextualDynamicRangeEncodeError::invalid_alphabet);
            }
            if (operation.value >= operation.alphabet_size) {
                return fail(result,
                            ContextualDynamicRangeEncodeError::invalid_symbol);
            }
            if (operation.bit_count != 0) {
                return fail(
                    result,
                    ContextualDynamicRangeEncodeError::nonzero_unused_field);
            }
            const auto offset = (*layout.offsets)[operation.context_id];
            std::uint32_t cumulative{};
            for (std::uint32_t symbol = 0; symbol < operation.value; ++symbol) {
                cumulative += models.frequencies[offset + symbol];
            }
            if (!encoder.encode(
                    cumulative,
                    models.frequencies[offset + operation.value],
                    models.totals[operation.context_id])) {
                return fail(result,
                            ContextualDynamicRangeEncodeError::internal_error);
            }
            models.update(operation.context_id, operation.value);
            if (!add_decisions(1, result)) return result;
        } else {
            if (operation.context_id != 0 || operation.alphabet_size != 0) {
                return fail(
                    result,
                    ContextualDynamicRangeEncodeError::nonzero_unused_field);
            }
            if (operation.bit_count == 0
                || operation.bit_count > layout.maximum_bypass_bits) {
                return fail(
                    result,
                    ContextualDynamicRangeEncodeError::invalid_bypass_width);
            }
            if ((operation.value >> operation.bit_count) != 0) {
                return fail(
                    result,
                    ContextualDynamicRangeEncodeError::nonzero_unused_field);
            }
            for (std::uint8_t bit = 0; bit < operation.bit_count; ++bit) {
                const auto value = (operation.value >> bit) & 1U;
                if (!encoder.encode(value, 1, 2)) {
                    return fail(
                        result,
                        ContextualDynamicRangeEncodeError::internal_error);
                }
            }
            if (!add_decisions(operation.bit_count, result)) return result;
        }
        ++result.operation_count;
    }
    result.operation_index = result.operation_count;
    if (!encoder.finish()) {
        return fail(result, ContextualDynamicRangeEncodeError::internal_error);
    }
    result.payload_size = encoder.size();
    return result;
}

enum class OverlapCheck : std::uint8_t {
    disjoint,
    overlap,
    arithmetic_overflow,
};

[[nodiscard]] OverlapCheck operation_payload_overlap(
    const std::span<const context::internal::ModeledOperation> operations,
    const std::span<std::byte> payload) noexcept {
    if (operations.empty() || payload.empty()) return OverlapCheck::disjoint;
    std::size_t operation_bytes{};
    if (!core::checked_multiply(
            operations.size(), sizeof(context::internal::ModeledOperation),
            operation_bytes)) {
        return OverlapCheck::arithmetic_overflow;
    }
    const auto operation_begin =
        reinterpret_cast<std::uintptr_t>(operations.data());
    const auto payload_begin = reinterpret_cast<std::uintptr_t>(payload.data());
    std::uintptr_t operation_end{};
    std::uintptr_t payload_end{};
    if (!core::checked_add(
            operation_begin, static_cast<std::uintptr_t>(operation_bytes),
            operation_end)
        || !core::checked_add(
            payload_begin, static_cast<std::uintptr_t>(payload.size()),
            payload_end)) {
        return OverlapCheck::arithmetic_overflow;
    }
    return operation_begin < payload_end && payload_begin < operation_end
        ? OverlapCheck::overlap
        : OverlapCheck::disjoint;
}

} // namespace

ContextualDynamicRangeEncodeResult plan_contextual_dynamic_range_operations(
    const std::span<const context::internal::ModeledOperation> operations,
    const core::DecoderLimits& limits,
    ContextualDynamicRangeDescriptor& descriptor,
    const context::internal::LzssFieldContextVariant variant) noexcept {
    if (operations.empty()) {
        return {0, 0, 0, 0,
                ContextualDynamicRangeEncodeError::empty_operations};
    }
    const auto selected = context::internal::get_lzss_field_context_layout(
        variant);
    if (selected.error
            != context::internal::LzssFieldContextLayoutError::none
        || core::validate_limits(limits) != core::LimitError::none
        || selected.layout.frequency_entries
               > limits.max_entropy_table_entries
        || contextual_dynamic_range_model_total_limit
               > limits.max_range_model_total) {
        return {0, 0, 0, 0,
                ContextualDynamicRangeEncodeError::limit_exceeded};
    }
    std::size_t operation_bytes{};
    if (!core::checked_multiply(
            operations.size(), sizeof(context::internal::ModeledOperation),
            operation_bytes)) {
        return {0, 0, 0, 0,
                ContextualDynamicRangeEncodeError::arithmetic_overflow};
    }
    if (operation_bytes > limits.max_internal_buffered_bytes) {
        return {0, 0, 0, 0,
                ContextualDynamicRangeEncodeError::limit_exceeded};
    }

    const auto result = run_encoder(operations, {}, selected.layout);
    if (result.error != ContextualDynamicRangeEncodeError::none) return result;
    if (result.payload_size > std::numeric_limits<std::uint32_t>::max()) {
        return fail(result,
                    ContextualDynamicRangeEncodeError::arithmetic_overflow);
    }
    if (result.payload_size > limits.max_compressed_payload_size
        || result.payload_size > limits.max_internal_buffered_bytes) {
        return fail(result, ContextualDynamicRangeEncodeError::limit_exceeded);
    }
    descriptor = {
        result.decision_count,
        static_cast<std::uint32_t>(result.payload_size),
        marc::context::internal::lzss_field_context_count,
    };
    return result;
}

ContextualDynamicRangeEncodeResult encode_contextual_dynamic_range_operations(
    const std::span<const context::internal::ModeledOperation> operations,
    const core::DecoderLimits& limits,
    const std::span<std::byte> payload_output,
    ContextualDynamicRangeDescriptor& descriptor,
    const context::internal::LzssFieldContextVariant variant) noexcept {
    ContextualDynamicRangeDescriptor planned{};
    const auto plan =
        plan_contextual_dynamic_range_operations(
            operations, limits, planned, variant);
    if (plan.error != ContextualDynamicRangeEncodeError::none) return plan;
    if (payload_output.size() < plan.payload_size) {
        return fail(
            plan, ContextualDynamicRangeEncodeError::payload_output_too_small);
    }
    const auto output = payload_output.first(plan.payload_size);
    const auto overlap = operation_payload_overlap(operations, output);
    if (overlap == OverlapCheck::arithmetic_overflow) {
        return fail(plan,
                    ContextualDynamicRangeEncodeError::arithmetic_overflow);
    }
    if (overlap == OverlapCheck::overlap) {
        return fail(plan,
                    ContextualDynamicRangeEncodeError::overlapping_buffers);
    }
    const auto selected = context::internal::get_lzss_field_context_layout(
        variant);
    if (selected.error
        != context::internal::LzssFieldContextLayoutError::none) {
        return fail(plan,
                    ContextualDynamicRangeEncodeError::internal_error);
    }
    const auto encoded = run_encoder(operations, output, selected.layout);
    if (encoded.error != ContextualDynamicRangeEncodeError::none
        || encoded.operation_count != plan.operation_count
        || encoded.decision_count != plan.decision_count
        || encoded.payload_size != plan.payload_size) {
        return fail(plan, ContextualDynamicRangeEncodeError::internal_error);
    }
    descriptor = planned;
    return encoded;
}

} // namespace marc::entropy::internal

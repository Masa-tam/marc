// Private 4 MiB operation core; no public/frame admission.
#include "entropy/lzss_position_distance_4m_scratch_range_encoder.hpp"

#include "context/lzss_position_distance_4m_context_layout.hpp"
#include "core/checked_math.hpp"
#include "core/buffer_overlap.hpp"
#include "context/lzss_position_distance_4m_field_cursor.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace marc::entropy::internal {
namespace {

inline constexpr std::uint32_t normalization_threshold = UINT32_C(1) << 24;

class RangeWriter {
public:
    explicit RangeWriter(const std::span<std::byte> output) noexcept
        : output_(output) {}

    [[nodiscard]] bool encode(const std::uint32_t cumulative,
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

    [[nodiscard]] std::size_t size() const noexcept { return size_; }

private:
    [[nodiscard]] bool emit(const std::uint8_t value) noexcept {
        if (size_ == std::numeric_limits<std::size_t>::max()) return false;
        if (!output_.empty()) {
            if (size_ >= output_.size()) return false;
            output_[size_] = static_cast<std::byte>(value);
        }
        ++size_;
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
    std::size_t size_{};
};

struct Models {
    std::array<std::uint16_t,
               context::internal::lzss_position_distance_4m_frequency_entries>
        frequencies{};
    std::array<std::uint32_t,
               context::internal::lzss_position_distance_4m_context_count> totals{};

    Models() noexcept {
        frequencies.fill(1);
        for (std::size_t index = 0; index < totals.size(); ++index) {
            totals[index] = context::internal::lzss_position_distance_4m_alphabets[index];
        }
    }

    void update(const std::uint16_t context_id,
                const std::uint32_t symbol) noexcept {
        const auto offset = context::internal::lzss_position_distance_4m_offsets[
            context_id];
        ++frequencies[offset + symbol];
        auto& total = totals[context_id];
        ++total;
        if (total != contextual_dynamic_range_model_total_limit) return;
        total = 0;
        const auto alphabet = context::internal::lzss_position_distance_4m_alphabets[
            context_id];
        for (std::size_t index = 0; index < alphabet; ++index) {
            auto& frequency = frequencies[offset + index];
            frequency = static_cast<std::uint16_t>(
                (static_cast<std::uint32_t>(frequency) + 1U) / 2U);
            total += frequency;
        }
    }
};

[[nodiscard]] ContextualDynamicRangeEncodeResult fail(
    ContextualDynamicRangeEncodeResult result,
    const ContextualDynamicRangeEncodeError error) noexcept {
    result.error = error;
    return result;
}

[[nodiscard]] bool add_decisions(const std::uint32_t count,
                                 ContextualDynamicRangeEncodeResult& result)
    noexcept {
    std::uint32_t updated{};
    if (!core::checked_add(result.decision_count, count, updated)) {
        result.error = ContextualDynamicRangeEncodeError::arithmetic_overflow;
        return false;
    }
    result.decision_count = updated;
    return true;
}

[[nodiscard]] ContextualDynamicRangeEncodeResult run(
    const std::span<const context::internal::ModeledOperation> operations,
    const std::span<std::byte> output) noexcept {
    ContextualDynamicRangeEncodeResult result{};
    Models models{};
    RangeWriter writer(output);
    context::internal::LzssPositionDistance4mFieldCursor cursor;
    for (const auto& operation : operations) {
        result.operation_index = result.operation_count;
        const auto field = cursor.next().field;
        // Grammar validation precedes model indexing and bit shifts.
        if (cursor.accept(operation) != context::internal::LzssFieldContextError::none) {
            return fail(result, ContextualDynamicRangeEncodeError::invalid_symbol);
        }
        if (operation.kind == context::internal::ModeledOperationKind::symbol) {
            // Successful cursor acceptance proves these storage predicates.
            // Retain the explicit checks in the reference implementation.
            if (operation.context_id
                >= context::internal::lzss_position_distance_4m_context_count) {
                return fail(result,
                            ContextualDynamicRangeEncodeError::invalid_context);
            }
            if (operation.alphabet_size
                != context::internal::lzss_position_distance_4m_alphabets[
                    operation.context_id]) {
                return fail(result,
                            ContextualDynamicRangeEncodeError::invalid_alphabet);
            }
            if (operation.value >= operation.alphabet_size) {
                return fail(result,
                            ContextualDynamicRangeEncodeError::invalid_symbol);
            }
            if (operation.bit_count != 0) {
                return fail(result, ContextualDynamicRangeEncodeError::
                                        nonzero_unused_field);
            }
            const auto offset = context::internal::lzss_position_distance_4m_offsets[
                operation.context_id];
            std::uint32_t cumulative{};
            for (std::uint32_t symbol = 0; symbol < operation.value;
                 ++symbol) {
                cumulative += models.frequencies[offset + symbol];
            }
            if (!writer.encode(
                    cumulative,
                    models.frequencies[offset + operation.value],
                    models.totals[operation.context_id])) {
                return fail(result,
                            ContextualDynamicRangeEncodeError::internal_error);
            }
            models.update(operation.context_id, operation.value);
            if (!add_decisions(1, result)) return result;
        } else if (operation.kind
                   == context::internal::ModeledOperationKind::bypass_bits) {
            // The accepted phase fixes a width in 1..22 and bounds its value.
            if (operation.context_id != 0 || operation.alphabet_size != 0) {
                return fail(result, ContextualDynamicRangeEncodeError::
                                        nonzero_unused_field);
            }
            if (operation.bit_count == 0 || operation.bit_count > 22) {
                return fail(result, ContextualDynamicRangeEncodeError::
                                        invalid_bypass_width);
            }
            if ((operation.value >> operation.bit_count) != 0) {
                return fail(result, ContextualDynamicRangeEncodeError::
                                        nonzero_unused_field);
            }
            for (std::uint8_t bit = 0; bit < operation.bit_count; ++bit) {
                const auto value = (operation.value >> bit) & 1U;
                const bool adaptive = field == context::internal::LzssPositionDistance4mField::adaptive_distance_extra;
                const auto id = static_cast<std::uint16_t>(24 + bit);
                const auto offset = context::internal::lzss_position_distance_4m_offsets[id];
                const auto cumulative = adaptive && value != 0 ? models.frequencies[offset] : value;
                const auto frequency = adaptive ? models.frequencies[offset + value] : 1;
                const auto total = adaptive ? models.totals[id] : 2;
                if (!writer.encode(adaptive && value == 0 ? 0 : cumulative,
                                   static_cast<std::uint16_t>(frequency), total)) {
                    return fail(result, ContextualDynamicRangeEncodeError::
                                            internal_error);
                }
                if (adaptive) models.update(id, value);
            }
            if (!add_decisions(operation.bit_count, result)) return result;
        } else {
            return fail(result, ContextualDynamicRangeEncodeError::
                                    invalid_operation_kind);
        }
        ++result.operation_count;
    }
    result.operation_index = result.operation_count;
    if (cursor.finish() != context::internal::LzssFieldContextError::none) {
        return fail(result, ContextualDynamicRangeEncodeError::invalid_symbol);
    }
    if (!writer.finish()) {
        return fail(result, ContextualDynamicRangeEncodeError::internal_error);
    }
    result.payload_size = writer.size();
    return result;
}

enum class OverlapCheck : std::uint8_t {
    disjoint,
    overlap,
    arithmetic_overflow,
};

[[nodiscard]] OverlapCheck overlaps(
    const std::span<const context::internal::ModeledOperation> operations,
    const std::span<std::byte> payload) noexcept {
    if (operations.empty() || payload.empty()) return OverlapCheck::disjoint;
    std::size_t operation_bytes{};
    if (!core::checked_multiply(operations.size(),
                                sizeof(context::internal::ModeledOperation),
                                operation_bytes)) {
        return OverlapCheck::arithmetic_overflow;
    }
    const auto operation_begin =
        reinterpret_cast<std::uintptr_t>(operations.data());
    const auto payload_begin =
        reinterpret_cast<std::uintptr_t>(payload.data());
    std::uintptr_t operation_end{};
    std::uintptr_t payload_end{};
    if (!core::checked_add(operation_begin,
                           static_cast<std::uintptr_t>(operation_bytes),
                           operation_end)
        || !core::checked_add(payload_begin,
                              static_cast<std::uintptr_t>(payload.size()),
                              payload_end)) {
        return OverlapCheck::arithmetic_overflow;
    }
    return operation_begin < payload_end && payload_begin < operation_end
        ? OverlapCheck::overlap
        : OverlapCheck::disjoint;
}

} // namespace

ContextualDynamicRangeEncodeResult encode_lzss_position_distance_4m_range_operations_scratch(
    const std::span<const context::internal::ModeledOperation> operations,
    const core::DecoderLimits& limits, const std::span<std::byte> output,
    ContextualDynamicRangeDescriptor& descriptor) noexcept {
    std::size_t checked_operation_bytes{};
    if (!core::checked_multiply(operations.size(), sizeof(context::internal::ModeledOperation), checked_operation_bytes))
        return fail({}, ContextualDynamicRangeEncodeError::arithmetic_overflow);
    struct Region { const void* data; std::size_t size; };
    const Region desc{&descriptor, sizeof(descriptor)}, config{&limits, sizeof(limits)};
    for (const auto pair : std::array<std::array<Region,2>,4>{{
        {{desc, {operations.data(), checked_operation_bytes}}}, {{desc, {output.data(), output.size()}}},
        {{desc, config}}, {{config, {output.data(), output.size()}}}}}) {
        const auto overlap=core::check_buffer_overlap(pair[0].data,pair[0].size,pair[1].data,pair[1].size);
        if(overlap!=core::BufferOverlap::disjoint)
            return fail({},overlap==core::BufferOverlap::arithmetic_overflow
                ? ContextualDynamicRangeEncodeError::arithmetic_overflow : ContextualDynamicRangeEncodeError::overlapping_buffers);
    }
    const auto fallback = [&] {
        return encode_lzss_position_distance_4m_range_operations(operations, limits, output, descriptor);
    };
    if (operations.empty() || core::validate_limits(limits) != core::LimitError::none
        || context::internal::lzss_position_distance_4m_frequency_entries > limits.max_entropy_table_entries
        || contextual_dynamic_range_model_total_limit > limits.max_range_model_total) {
        return fallback();
    }
    std::uint32_t decisions{};
    for (const auto& operation : operations) {
        std::uint32_t count{};
        if (operation.kind == context::internal::ModeledOperationKind::symbol) {
            count = 1;
        } else if (operation.kind == context::internal::ModeledOperationKind::bypass_bits
                   && operation.bit_count > 0 && operation.bit_count <= 22) {
            count = operation.bit_count;
        } else {
            return fallback();
        }
        if (!core::checked_add(decisions, count, decisions)) return fallback();
    }
    // At most two renormalizations per decision, then five final shifts.
    // Deferred carry emission cannot exceed the total number of shifts.
    std::size_t bound{}, operation_bytes{}, aggregate{};
    if (!core::checked_multiply(static_cast<std::size_t>(decisions), std::size_t{2}, bound)
        || !core::checked_add(bound, std::size_t{5}, bound)
        || bound > UINT32_MAX || bound > output.size()
        || bound > limits.max_compressed_payload_size
        || !core::checked_multiply(operations.size(), sizeof(context::internal::ModeledOperation), operation_bytes)
        || !core::checked_add(operation_bytes, lzss_position_distance_4m_range_encoder_state_bytes(), aggregate)
        || !core::checked_add(aggregate, bound, aggregate)
        || aggregate > limits.max_internal_buffered_bytes
        || overlaps(operations, output.first(bound)) != OverlapCheck::disjoint) {
        return fallback();
    }
    const auto result = run(operations, output.first(bound));
    if (result.error != ContextualDynamicRangeEncodeError::none) return result;
    descriptor = {result.decision_count, static_cast<std::uint32_t>(result.payload_size),
                  context::internal::lzss_position_distance_4m_context_count};
    return result;
}

} // namespace marc::entropy::internal

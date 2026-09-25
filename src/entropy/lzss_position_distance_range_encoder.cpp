#include "entropy/lzss_position_distance_range_encoder.hpp"

#include "context/lzss_position_distance_context_layout.hpp"
#include "core/checked_math.hpp"
#include "context/lzss_position_distance_field_cursor.hpp"

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
               context::internal::lzss_position_distance_frequency_entries>
        frequencies{};
    std::array<std::uint32_t,
               context::internal::lzss_position_distance_context_count> totals{};

    Models() noexcept {
        frequencies.fill(1);
        for (std::size_t index = 0; index < totals.size(); ++index) {
            totals[index] = context::internal::lzss_position_distance_alphabets[index];
        }
    }

    void update(const std::uint16_t context_id,
                const std::uint32_t symbol) noexcept {
        const auto offset = context::internal::lzss_position_distance_offsets[
            context_id];
        ++frequencies[offset + symbol];
        auto& total = totals[context_id];
        ++total;
        if (total != contextual_dynamic_range_model_total_limit) return;
        total = 0;
        const auto alphabet = context::internal::lzss_position_distance_alphabets[
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

template<bool Reference>
[[nodiscard]] ContextualDynamicRangeEncodeResult run(
    const std::span<const context::internal::ModeledOperation> operations,
    const std::span<std::byte> output) noexcept {
    ContextualDynamicRangeEncodeResult result{};
    Models models{};
    RangeWriter writer(output);
    context::internal::LzssPositionDistanceFieldCursor cursor;
    for (const auto& operation : operations) {
        result.operation_index = result.operation_count;
        const auto field = cursor.next().field;
        // Grammar validation precedes model indexing and bit shifts.
        if (cursor.accept(operation) != context::internal::LzssFieldContextError::none) {
            return fail(result, ContextualDynamicRangeEncodeError::invalid_symbol);
        }
        if (operation.kind == context::internal::ModeledOperationKind::symbol) {
            if (operation.context_id
                >= context::internal::lzss_position_distance_context_count) {
                return fail(result,
                            ContextualDynamicRangeEncodeError::invalid_context);
            }
            if (operation.alphabet_size
                != context::internal::lzss_position_distance_alphabets[
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
            const auto offset = context::internal::lzss_position_distance_offsets[
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
            if (operation.context_id != 0 || operation.alphabet_size != 0) {
                return fail(result, ContextualDynamicRangeEncodeError::
                                        nonzero_unused_field);
            }
            if (operation.bit_count == 0 || operation.bit_count > 16) {
                return fail(result, ContextualDynamicRangeEncodeError::
                                        invalid_bypass_width);
            }
            if ((operation.value >> operation.bit_count) != 0) {
                return fail(result, ContextualDynamicRangeEncodeError::
                                        nonzero_unused_field);
            }
            if (!Reference && field == context::internal::LzssPositionDistanceField::adaptive_distance_extra) {
                for (std::uint8_t bit = 0; bit < operation.bit_count; ++bit) {
                    const auto value = (operation.value >> bit) & 1U;
                    const auto offset = context::internal::lzss_position_distance_offsets[24 + bit];
                    auto& zero = models.frequencies[offset];
                    auto& one = models.frequencies[offset + 1];
                    auto& total = models.totals[24 + bit];
                    if (zero == 0 || one == 0
                        || total != static_cast<std::uint32_t>(zero) + one
                        || total >= contextual_dynamic_range_model_total_limit
                        || !writer.encode(value == 0 ? 0U : zero,
                                          value == 0 ? zero : one, total)) {
                        return fail(result, ContextualDynamicRangeEncodeError::internal_error);
                    }
                    // Commit adaptation only after interval coding succeeds.
                    ++models.frequencies[offset + value];
                    if (++total == contextual_dynamic_range_model_total_limit) {
                        zero = static_cast<std::uint16_t>((static_cast<std::uint32_t>(zero) + 1U) / 2U);
                        one = static_cast<std::uint16_t>((static_cast<std::uint32_t>(one) + 1U) / 2U);
                        total = static_cast<std::uint32_t>(zero) + one;
                    }
                }
            } else for (std::uint8_t bit = 0; bit < operation.bit_count; ++bit) {
                const auto value = (operation.value >> bit) & 1U;
                const bool adaptive = field == context::internal::LzssPositionDistanceField::adaptive_distance_extra;
                const auto id = static_cast<std::uint16_t>(24 + bit);
                const auto offset = context::internal::lzss_position_distance_offsets[id];
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

std::size_t lzss_position_distance_range_encoder_state_bytes() noexcept {
    return sizeof(Models) + sizeof(RangeWriter)
        + sizeof(context::internal::LzssPositionDistanceFieldCursor);
}

namespace {
template<bool Reference>
ContextualDynamicRangeEncodeResult plan_operations(
    const std::span<const context::internal::ModeledOperation> operations,
    const core::DecoderLimits& limits,
    ContextualDynamicRangeDescriptor& descriptor) noexcept {
    if (operations.empty()) {
        return {0, 0, 0, 0,
                ContextualDynamicRangeEncodeError::empty_operations};
    }
    if (core::validate_limits(limits) != core::LimitError::none
        || context::internal::lzss_position_distance_frequency_entries
               > limits.max_entropy_table_entries
        || contextual_dynamic_range_model_total_limit
               > limits.max_range_model_total) {
        return {0, 0, 0, 0,
                ContextualDynamicRangeEncodeError::limit_exceeded};
    }
    std::size_t operation_bytes{};
    if (!core::checked_multiply(operations.size(),
                                sizeof(context::internal::ModeledOperation),
                                operation_bytes)) {
        return {0, 0, 0, 0,
                ContextualDynamicRangeEncodeError::arithmetic_overflow};
    }
    std::size_t working_bytes{};
    if (!core::checked_add(operation_bytes,
            lzss_position_distance_range_encoder_state_bytes(), working_bytes)) {
        return {0, 0, 0, 0, ContextualDynamicRangeEncodeError::arithmetic_overflow};
    }
    if (working_bytes > limits.max_internal_buffered_bytes) {
        return {0, 0, 0, 0,
                ContextualDynamicRangeEncodeError::limit_exceeded};
    }

    const auto result = run<Reference>(operations, {});
    if (result.error != ContextualDynamicRangeEncodeError::none) return result;
    if (result.payload_size > std::numeric_limits<std::uint32_t>::max()) {
        return fail(result,
                    ContextualDynamicRangeEncodeError::arithmetic_overflow);
    }
    std::size_t aggregate{};
    if (!core::checked_add(working_bytes, result.payload_size, aggregate)) {
        return fail(result, ContextualDynamicRangeEncodeError::arithmetic_overflow);
    }
    if (result.payload_size > limits.max_compressed_payload_size
        || aggregate > limits.max_internal_buffered_bytes) {
        return fail(result, ContextualDynamicRangeEncodeError::limit_exceeded);
    }
    descriptor = {result.decision_count,
                  static_cast<std::uint32_t>(result.payload_size),
                  context::internal::lzss_position_distance_context_count};
    return result;
}

template<bool Reference>
ContextualDynamicRangeEncodeResult encode_operations(
    const std::span<const context::internal::ModeledOperation> operations,
    const core::DecoderLimits& limits,
    const std::span<std::byte> payload_output,
    ContextualDynamicRangeDescriptor& descriptor) noexcept {
    ContextualDynamicRangeDescriptor planned{};
    const auto plan = plan_operations<Reference>(
        operations, limits, planned);
    if (plan.error != ContextualDynamicRangeEncodeError::none) return plan;
    if (payload_output.size() < plan.payload_size) {
        return fail(plan,
                    ContextualDynamicRangeEncodeError::payload_output_too_small);
    }
    const auto output = payload_output.first(plan.payload_size);
    const auto overlap = overlaps(operations, output);
    if (overlap == OverlapCheck::arithmetic_overflow) {
        return fail(plan,
                    ContextualDynamicRangeEncodeError::arithmetic_overflow);
    }
    if (overlap == OverlapCheck::overlap) {
        return fail(plan,
                    ContextualDynamicRangeEncodeError::overlapping_buffers);
    }
    const auto encoded = run<Reference>(operations, output);
    if (encoded.error != ContextualDynamicRangeEncodeError::none
        || encoded.operation_count != plan.operation_count
        || encoded.decision_count != plan.decision_count
        || encoded.payload_size != plan.payload_size) {
        return fail(plan, ContextualDynamicRangeEncodeError::internal_error);
    }
    descriptor = planned;
    return encoded;
}

} // namespace

ContextualDynamicRangeEncodeResult plan_lzss_position_distance_range_operations(
    const std::span<const context::internal::ModeledOperation> operations,
    const core::DecoderLimits& limits,
    ContextualDynamicRangeDescriptor& descriptor) noexcept {
    return plan_operations<false>(operations, limits, descriptor);
}

ContextualDynamicRangeEncodeResult encode_lzss_position_distance_range_operations(
    const std::span<const context::internal::ModeledOperation> operations,
    const core::DecoderLimits& limits, const std::span<std::byte> output,
    ContextualDynamicRangeDescriptor& descriptor) noexcept {
    return encode_operations<false>(operations, limits, output, descriptor);
}

ContextualDynamicRangeEncodeResult PreparedLzssPositionDistanceEncode::prepare(
    const std::span<const context::internal::ModeledOperation> operations,
    const core::DecoderLimits& limits,
    ContextualDynamicRangeDescriptor& descriptor) noexcept {
    ready_ = false;
    operations_ = {};
    plan_ = {};
    descriptor_ = {};
    ContextualDynamicRangeDescriptor planned{};
    const auto result = plan_operations<false>(operations, limits, planned);
    if (result.error != ContextualDynamicRangeEncodeError::none) return result;
    operations_ = operations;
    plan_ = result;
    descriptor_ = planned;
    ready_ = true;
    descriptor = planned;
    return result;
}

ContextualDynamicRangeEncodeResult PreparedLzssPositionDistanceEncode::write(
    const std::span<std::byte> output,
    ContextualDynamicRangeDescriptor& descriptor) noexcept {
    if (!ready_) return fail({}, ContextualDynamicRangeEncodeError::internal_error);
    ready_ = false;
    const auto operations = operations_;
    operations_ = {};
    if (output.size() < plan_.payload_size)
        return fail(plan_, ContextualDynamicRangeEncodeError::payload_output_too_small);
    const auto payload = output.first(plan_.payload_size);
    const auto overlap = overlaps(operations, payload);
    if (overlap == OverlapCheck::arithmetic_overflow)
        return fail(plan_, ContextualDynamicRangeEncodeError::arithmetic_overflow);
    if (overlap == OverlapCheck::overlap)
        return fail(plan_, ContextualDynamicRangeEncodeError::overlapping_buffers);
    // Exactly one checked writing run; no count-only planning here.
    const auto encoded = run<false>(operations, payload);
    if (encoded.error != ContextualDynamicRangeEncodeError::none
        || encoded.operation_count != plan_.operation_count
        || encoded.operation_index != plan_.operation_index
        || encoded.decision_count != plan_.decision_count
        || encoded.payload_size != plan_.payload_size
        || descriptor_.decision_count != encoded.decision_count
        || descriptor_.payload_size != encoded.payload_size
        || descriptor_.context_count != context::internal::lzss_position_distance_context_count)
        return fail(plan_, ContextualDynamicRangeEncodeError::internal_error);
    descriptor = descriptor_;
    return encoded;
}

ContextualDynamicRangeEncodeResult plan_lzss_position_distance_range_operations_reference(
    const std::span<const context::internal::ModeledOperation> operations,
    const core::DecoderLimits& limits,
    ContextualDynamicRangeDescriptor& descriptor) noexcept {
    return plan_operations<true>(operations, limits, descriptor);
}

ContextualDynamicRangeEncodeResult encode_lzss_position_distance_range_operations_reference(
    const std::span<const context::internal::ModeledOperation> operations,
    const core::DecoderLimits& limits, const std::span<std::byte> output,
    ContextualDynamicRangeDescriptor& descriptor) noexcept {
    return encode_operations<true>(operations, limits, output, descriptor);
}

} // namespace marc::entropy::internal

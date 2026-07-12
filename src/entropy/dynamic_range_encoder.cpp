#include "entropy/dynamic_range_encoder.hpp"

#include "entropy/dynamic_range_model.hpp"

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

    [[nodiscard]] bool encode(const std::uint32_t cumulative,
                              const std::uint16_t frequency,
                              const std::uint32_t total) noexcept {
        if (total == 0 || range_ < normalization_threshold) return false;
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
            const auto delayed = static_cast<std::uint8_t>(UINT32_C(0xff) + carry);
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

[[nodiscard]] DynamicRangeEncodeResult run_encoder(
    const std::span<const std::byte> input,
    const std::span<std::byte> output) noexcept {
    DynamicRangeModel model;
    RangeEncoder encoder(output);
    for (const auto byte : input) {
        const auto symbol = std::to_integer<std::uint8_t>(byte);
        if (!encoder.encode(model.cumulative(symbol), model.frequency(symbol),
                            model.total())) {
            return {encoder.size(), DynamicRangeEncodeError::internal_error};
        }
        model.update(symbol);
    }
    if (!encoder.finish()) {
        return {encoder.size(), DynamicRangeEncodeError::internal_error};
    }
    return {encoder.size(), DynamicRangeEncodeError::none};
}

} // namespace

DynamicRangeEncodeResult plan_dynamic_range_frame(
    const std::span<const std::byte> input,
    const core::DecoderLimits& limits,
    DynamicRangeDescriptor& descriptor) noexcept {
    if (input.empty()) return {0, DynamicRangeEncodeError::empty_input};
    if (input.size() > dynamic_range_max_frame_size
        || input.size() > std::numeric_limits<std::uint32_t>::max()) {
        return {0, DynamicRangeEncodeError::frame_too_large};
    }
    if (input.size() > limits.max_frame_size
        || limits.max_range_model_total < dynamic_range_model_total_limit) {
        return {0, DynamicRangeEncodeError::limit_exceeded};
    }
    const auto result = run_encoder(input, {});
    if (result.error != DynamicRangeEncodeError::none) return result;
    if (result.payload_size > std::numeric_limits<std::uint32_t>::max()) {
        return {result.payload_size, DynamicRangeEncodeError::arithmetic_overflow};
    }
    if (result.payload_size > limits.max_compressed_payload_size
        || result.payload_size > limits.max_internal_buffered_bytes) {
        return {result.payload_size, DynamicRangeEncodeError::limit_exceeded};
    }
    DynamicRangeDescriptor planned{
        static_cast<std::uint32_t>(input.size()),
        static_cast<std::uint32_t>(result.payload_size), 0};
    if (validate_dynamic_range_descriptor(
            planned, planned.symbol_count, planned.payload_size, limits)
        != DynamicRangeFormatError::none) {
        return {result.payload_size, DynamicRangeEncodeError::internal_error};
    }
    descriptor = planned;
    return result;
}

DynamicRangeEncodeResult encode_dynamic_range_frame(
    const std::span<const std::byte> input,
    const core::DecoderLimits& limits,
    const std::span<std::byte> payload_output,
    DynamicRangeDescriptor& descriptor) noexcept {
    DynamicRangeDescriptor planned{};
    const auto plan = plan_dynamic_range_frame(input, limits, planned);
    if (plan.error != DynamicRangeEncodeError::none) return plan;
    if (payload_output.size() < plan.payload_size) {
        return {plan.payload_size,
                DynamicRangeEncodeError::payload_output_too_small};
    }
    const auto result = run_encoder(input, payload_output.first(plan.payload_size));
    if (result.error != DynamicRangeEncodeError::none
        || result.payload_size != plan.payload_size) {
        return {plan.payload_size, DynamicRangeEncodeError::internal_error};
    }
    descriptor = planned;
    return result;
}

} // namespace marc::entropy::internal

#include "entropy/dynamic_range_decoder.hpp"

#include "entropy/dynamic_range_model.hpp"

#include <cstddef>
#include <cstdint>

namespace marc::entropy::internal {
namespace {

inline constexpr std::uint32_t normalization_threshold = UINT32_C(1) << 24;

struct DecodePassResult {
    std::size_t payload_consumed{};
    DynamicRangeDecodeError error{DynamicRangeDecodeError::none};
};

[[nodiscard]] bool read_byte(const std::span<const std::byte> payload,
                             std::size_t& offset,
                             std::uint32_t& value) noexcept {
    if (offset >= payload.size()) return false;
    value = std::to_integer<std::uint8_t>(payload[offset]);
    ++offset;
    return true;
}

[[nodiscard]] DecodePassResult decode_pass(
    const DynamicRangeDescriptor& descriptor,
    const std::span<const std::byte> payload,
    const std::span<std::byte> output) noexcept {
    std::size_t offset{};
    std::uint32_t code{};
    for (int index = 0; index < 5; ++index) {
        std::uint32_t byte{};
        if (!read_byte(payload, offset, byte)) {
            return {offset, DynamicRangeDecodeError::truncated_payload};
        }
        code = static_cast<std::uint32_t>((code << 8) | byte);
    }

    DynamicRangeModel model;
    std::uint32_t range{UINT32_MAX};
    for (std::uint32_t produced = 0; produced < descriptor.symbol_count;
         ++produced) {
        if (range < normalization_threshold || model.total() == 0) {
            return {offset, DynamicRangeDecodeError::invalid_interval};
        }
        const auto unit = range / model.total();
        if (unit == 0) {
            return {offset, DynamicRangeDecodeError::invalid_interval};
        }
        const auto scaled = code / unit;
        std::uint8_t symbol{};
        std::uint32_t cumulative{};
        std::uint16_t frequency{};
        if (!model.find_symbol(scaled, symbol, cumulative, frequency)) {
            return {offset, DynamicRangeDecodeError::invalid_interval};
        }
        code -= cumulative * unit;
        range = unit * frequency;
        while (range < normalization_threshold) {
            std::uint32_t byte{};
            if (!read_byte(payload, offset, byte)) {
                return {offset, DynamicRangeDecodeError::truncated_payload};
            }
            range <<= 8;
            code = static_cast<std::uint32_t>((code << 8) | byte);
        }
        if (!output.empty()) output[produced] = static_cast<std::byte>(symbol);
        model.update(symbol);
    }
    if (offset != payload.size()) {
        return {offset, DynamicRangeDecodeError::trailing_payload};
    }
    if (!model.validate()) {
        return {offset, DynamicRangeDecodeError::invalid_model};
    }
    return {offset, DynamicRangeDecodeError::none};
}

[[nodiscard]] DynamicRangeDecodeResult preflight(
    const DynamicRangeDescriptor& descriptor,
    const std::span<const std::byte> payload,
    const core::DecoderLimits& limits) noexcept {
    if (validate_dynamic_range_descriptor(
            descriptor, descriptor.symbol_count, descriptor.payload_size,
            limits) != DynamicRangeFormatError::none) {
        return {descriptor.symbol_count, 0,
                DynamicRangeDecodeError::invalid_descriptor};
    }
    core::FrameBounds bounds{};
    bounds.uncompressed_size = descriptor.symbol_count;
    bounds.dictionary_serialized_size = descriptor.symbol_count;
    bounds.compressed_payload_size = descriptor.payload_size;
    bounds.range_model_total = dynamic_range_model_total_limit;
    bounds.payload_buffered_bytes = descriptor.payload_size;
    bounds.block_count = 1;
    if (core::validate_frame_bounds(limits, bounds, 0)
        != core::LimitError::none) {
        return {descriptor.symbol_count, 0,
                DynamicRangeDecodeError::invalid_descriptor};
    }
    if (payload.size() != descriptor.payload_size) {
        return {descriptor.symbol_count, 0,
                DynamicRangeDecodeError::payload_size_mismatch};
    }
    return {descriptor.symbol_count, 0, DynamicRangeDecodeError::none};
}

} // namespace

DynamicRangeDecodeResult validate_dynamic_range_frame(
    const DynamicRangeDescriptor& descriptor,
    const std::span<const std::byte> payload,
    const core::DecoderLimits& limits) noexcept {
    const auto checked = preflight(descriptor, payload, limits);
    if (checked.error != DynamicRangeDecodeError::none) return checked;
    const auto validation = decode_pass(descriptor, payload, {});
    return {descriptor.symbol_count, validation.payload_consumed,
            validation.error};
}

DynamicRangeDecodeResult decode_dynamic_range_frame(
    const DynamicRangeDescriptor& descriptor,
    const std::span<const std::byte> payload,
    const core::DecoderLimits& limits,
    const std::span<std::byte> output) noexcept {
    const auto checked = preflight(descriptor, payload, limits);
    if (checked.error != DynamicRangeDecodeError::none) return checked;
    if (output.size() < descriptor.symbol_count) {
        return {descriptor.symbol_count, 0,
                DynamicRangeDecodeError::output_too_small};
    }
    const auto validation = decode_pass(descriptor, payload, {});
    if (validation.error != DynamicRangeDecodeError::none) {
        return {descriptor.symbol_count, validation.payload_consumed,
                validation.error};
    }
    const auto decoded = decode_pass(
        descriptor, payload, output.first(descriptor.symbol_count));
    if (decoded.error != DynamicRangeDecodeError::none
        || decoded.payload_consumed != validation.payload_consumed) {
        return {descriptor.symbol_count, decoded.payload_consumed,
                DynamicRangeDecodeError::internal_error};
    }
    return {descriptor.symbol_count, decoded.payload_consumed,
            DynamicRangeDecodeError::none};
}

} // namespace marc::entropy::internal

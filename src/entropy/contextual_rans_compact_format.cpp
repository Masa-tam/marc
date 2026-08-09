#include "entropy/contextual_rans_compact_format.hpp"

#include "core/checked_math.hpp"
#include "core/endian.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace marc::entropy::internal {
namespace {

enum class RecordMode : std::uint8_t {
    dense = 0,
    sparse = 1,
};

struct ModelAnalysis {
    ContextualRansCompactFormatError error{
        ContextualRansCompactFormatError::none};
    std::uint32_t active_mask{};
    std::size_t serialized_size{contextual_rans_compact_prefix_size};
};

[[nodiscard]] std::size_t dense_record_size(
    const std::uint16_t alphabet) noexcept {
    return 1 + 2 * (static_cast<std::size_t>(alphabet) - 1);
}

[[nodiscard]] std::size_t sparse_record_size(
    const std::size_t nonzero_count) noexcept {
    return 3 * nonzero_count;
}

[[nodiscard]] bool sparse_is_canonical(
    const std::uint16_t alphabet,
    const std::size_t nonzero_count) noexcept {
    return sparse_record_size(nonzero_count) < dense_record_size(alphabet);
}

[[nodiscard]] ContextualRansCompactFormatError validate_fields(
    const ContextualRansDescriptor& descriptor,
    const std::uint32_t expected_decision_count,
    const std::uint32_t expected_payload_size) noexcept {
    if (descriptor.decision_count == 0) {
        return ContextualRansCompactFormatError::invalid_decision_count;
    }
    if (descriptor.payload_size < rans_min_payload_size) {
        return ContextualRansCompactFormatError::invalid_payload_size;
    }
    std::uint64_t maximum_payload{};
    if (!core::checked_multiply(
            static_cast<std::uint64_t>(descriptor.decision_count),
            UINT64_C(2), maximum_payload)
        || !core::checked_add(
            maximum_payload,
            static_cast<std::uint64_t>(rans_min_payload_size),
            maximum_payload)) {
        return ContextualRansCompactFormatError::arithmetic_overflow;
    }
    if (descriptor.payload_size > maximum_payload) {
        return ContextualRansCompactFormatError::invalid_payload_size;
    }
    if (descriptor.table_log != contextual_rans_table_log) {
        return ContextualRansCompactFormatError::invalid_table_log;
    }
    if (descriptor.flags != 0) {
        return ContextualRansCompactFormatError::unknown_flags;
    }
    if (descriptor.context_count != contextual_rans_context_count) {
        return ContextualRansCompactFormatError::invalid_context_count;
    }
    if (descriptor.frequency_entry_count
        != contextual_rans_frequency_entries) {
        return ContextualRansCompactFormatError::
            invalid_frequency_entry_count;
    }
    if (descriptor.decision_count != expected_decision_count
        || descriptor.payload_size != expected_payload_size) {
        return ContextualRansCompactFormatError::contradictory_size;
    }
    return ContextualRansCompactFormatError::none;
}

[[nodiscard]] ModelAnalysis analyze_models(
    const ContextualRansDescriptor& descriptor) noexcept {
    ModelAnalysis analysis{};
    for (std::size_t context_id = 0;
         context_id < contextual_rans_context_count; ++context_id) {
        const auto begin =
            context::internal::lzss_field_context_offsets[context_id];
        const auto end =
            context::internal::lzss_field_context_offsets[context_id + 1];
        std::uint32_t sum{};
        std::size_t nonzero_count{};
        for (auto index = begin; index < end; ++index) {
            const auto frequency = descriptor.frequencies[index];
            sum += frequency;
            if (frequency != 0) ++nonzero_count;
        }
        if (sum == 0) continue;
        if (sum != contextual_rans_total_frequency) {
            analysis.error =
                ContextualRansCompactFormatError::invalid_frequency_table;
            return analysis;
        }
        analysis.active_mask |= UINT32_C(1) << context_id;
        const auto alphabet =
            context::internal::lzss_field_context_alphabets[context_id];
        const auto record_size = sparse_is_canonical(alphabet, nonzero_count)
            ? sparse_record_size(nonzero_count)
            : dense_record_size(alphabet);
        if (!core::checked_add(
                analysis.serialized_size, record_size,
                analysis.serialized_size)) {
            analysis.error =
                ContextualRansCompactFormatError::arithmetic_overflow;
            return analysis;
        }
    }
    if (analysis.active_mask == 0) {
        analysis.error =
            ContextualRansCompactFormatError::invalid_active_context_mask;
    } else if (analysis.serialized_size
               > contextual_rans_compact_max_descriptor_size) {
        analysis.error =
            ContextualRansCompactFormatError::invalid_descriptor_size;
    }
    return analysis;
}

[[nodiscard]] ContextualRansCompactFormatError validate_limits(
    const ContextualRansDescriptor& descriptor,
    const std::size_t serialized_size,
    const core::DecoderLimits& limits) noexcept {
    std::uint64_t buffered{};
    if (!core::checked_add(
            static_cast<std::uint64_t>(serialized_size),
            static_cast<std::uint64_t>(descriptor.payload_size), buffered)) {
        return ContextualRansCompactFormatError::arithmetic_overflow;
    }
    if (core::validate_limits(limits) != core::LimitError::none
        || descriptor.decision_count > limits.max_block_size
        || descriptor.payload_size > limits.max_compressed_payload_size
        || contextual_rans_decode_table_entries
            > limits.max_entropy_table_entries
        || buffered > limits.max_internal_buffered_bytes) {
        return ContextualRansCompactFormatError::limit_exceeded;
    }
    return ContextualRansCompactFormatError::none;
}

[[nodiscard]] bool read_byte(
    const std::span<const std::byte> input,
    std::size_t& cursor,
    std::uint8_t& value) noexcept {
    if (cursor >= input.size()) return false;
    value = std::to_integer<std::uint8_t>(input[cursor++]);
    return true;
}

[[nodiscard]] bool read_u16(
    const std::span<const std::byte> input,
    std::size_t& cursor,
    std::uint16_t& value) noexcept {
    if (!core::load_le(input, cursor, value)) return false;
    cursor += sizeof(value);
    return true;
}

} // namespace

ContextualRansCompactFormatError
validate_contextual_rans_compact_descriptor(
    const ContextualRansDescriptor& descriptor,
    const std::uint32_t expected_decision_count,
    const std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits,
    std::size_t& serialized_size) noexcept {
    const auto field_error = validate_fields(
        descriptor, expected_decision_count, expected_payload_size);
    if (field_error != ContextualRansCompactFormatError::none) {
        return field_error;
    }
    const auto analysis = analyze_models(descriptor);
    if (analysis.error != ContextualRansCompactFormatError::none) {
        return analysis.error;
    }
    const auto limit_error =
        validate_limits(descriptor, analysis.serialized_size, limits);
    if (limit_error != ContextualRansCompactFormatError::none) {
        return limit_error;
    }
    serialized_size = analysis.serialized_size;
    return ContextualRansCompactFormatError::none;
}

ContextualRansCompactFormatError parse_contextual_rans_compact_descriptor(
    const std::span<const std::byte> input,
    const std::uint32_t expected_decision_count,
    const std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits,
    ContextualRansDescriptor& descriptor) noexcept {
    if (input.size() < contextual_rans_compact_prefix_size) {
        return ContextualRansCompactFormatError::truncated_descriptor;
    }
    if (input.size() < contextual_rans_compact_min_descriptor_size
        || input.size() > contextual_rans_compact_max_descriptor_size) {
        return ContextualRansCompactFormatError::invalid_descriptor_size;
    }

    ContextualRansDescriptor parsed{};
    std::uint32_t active_mask{};
    if (!core::load_le(input, 0, parsed.decision_count)
        || !core::load_le(input, 4, parsed.payload_size)
        || !core::load_le(input, 10, parsed.context_count)
        || !core::load_le(input, 12, parsed.frequency_entry_count)
        || !core::load_le(input, 16, active_mask)) {
        return ContextualRansCompactFormatError::truncated_descriptor;
    }
    parsed.table_log = std::to_integer<std::uint8_t>(input[8]);
    parsed.flags = std::to_integer<std::uint8_t>(input[9]);
    const auto field_error = validate_fields(
        parsed, expected_decision_count, expected_payload_size);
    if (field_error != ContextualRansCompactFormatError::none) {
        return field_error;
    }
    if (active_mask == 0 || (active_mask & UINT32_C(0x80000000)) != 0) {
        return ContextualRansCompactFormatError::invalid_active_context_mask;
    }

    std::size_t cursor = contextual_rans_compact_prefix_size;
    for (std::size_t context_id = 0;
         context_id < contextual_rans_context_count; ++context_id) {
        if ((active_mask & (UINT32_C(1) << context_id)) == 0) continue;
        std::uint8_t mode_value{};
        if (!read_byte(input, cursor, mode_value)) {
            return ContextualRansCompactFormatError::truncated_descriptor;
        }
        const auto alphabet =
            context::internal::lzss_field_context_alphabets[context_id];
        const auto offset =
            context::internal::lzss_field_context_offsets[context_id];
        std::size_t nonzero_count{};
        if (mode_value == static_cast<std::uint8_t>(RecordMode::dense)) {
            std::uint32_t sum{};
            for (std::uint16_t symbol = 0; symbol + 1 < alphabet; ++symbol) {
                std::uint16_t frequency{};
                if (!read_u16(input, cursor, frequency)) {
                    return ContextualRansCompactFormatError::
                        truncated_descriptor;
                }
                parsed.frequencies[offset + symbol] = frequency;
                sum += frequency;
                if (sum > contextual_rans_total_frequency) {
                    return ContextualRansCompactFormatError::
                        invalid_frequency_table;
                }
                if (frequency != 0) ++nonzero_count;
            }
            const auto final_frequency =
                contextual_rans_total_frequency - sum;
            parsed.frequencies[offset + alphabet - 1] =
                static_cast<std::uint16_t>(final_frequency);
            if (final_frequency != 0) ++nonzero_count;
        } else if (mode_value
                   == static_cast<std::uint8_t>(RecordMode::sparse)) {
            std::uint8_t count_minus_one{};
            if (!read_byte(input, cursor, count_minus_one)) {
                return ContextualRansCompactFormatError::truncated_descriptor;
            }
            nonzero_count = static_cast<std::size_t>(count_minus_one) + 1;
            if (nonzero_count > alphabet) {
                return ContextualRansCompactFormatError::
                    invalid_frequency_table;
            }
            std::uint32_t sum{};
            std::uint16_t previous{};
            bool have_previous{};
            for (std::size_t entry = 0; entry < nonzero_count; ++entry) {
                std::uint8_t symbol{};
                if (!read_byte(input, cursor, symbol)) {
                    return ContextualRansCompactFormatError::
                        truncated_descriptor;
                }
                if (symbol >= alphabet
                    || (have_previous && symbol <= previous)) {
                    return ContextualRansCompactFormatError::
                        invalid_frequency_table;
                }
                previous = symbol;
                have_previous = true;
                std::uint32_t frequency{};
                if (entry + 1 < nonzero_count) {
                    std::uint16_t stored{};
                    if (!read_u16(input, cursor, stored)) {
                        return ContextualRansCompactFormatError::
                            truncated_descriptor;
                    }
                    if (stored == 0) {
                        return ContextualRansCompactFormatError::
                            invalid_frequency_table;
                    }
                    sum += stored;
                    if (sum >= contextual_rans_total_frequency) {
                        return ContextualRansCompactFormatError::
                            invalid_frequency_table;
                    }
                    frequency = stored;
                } else {
                    frequency = contextual_rans_total_frequency - sum;
                }
                parsed.frequencies[offset + symbol] =
                    static_cast<std::uint16_t>(frequency);
            }
        } else {
            return ContextualRansCompactFormatError::invalid_mode;
        }
        const bool encoded_sparse =
            mode_value == static_cast<std::uint8_t>(RecordMode::sparse);
        if (encoded_sparse != sparse_is_canonical(alphabet, nonzero_count)) {
            return ContextualRansCompactFormatError::
                noncanonical_representation;
        }
    }
    if (cursor != input.size()) {
        return ContextualRansCompactFormatError::trailing_data;
    }

    std::size_t canonical_size{};
    const auto validation = validate_contextual_rans_compact_descriptor(
        parsed, expected_decision_count, expected_payload_size, limits,
        canonical_size);
    if (validation != ContextualRansCompactFormatError::none) {
        return validation;
    }
    if (canonical_size != input.size()) {
        return ContextualRansCompactFormatError::noncanonical_representation;
    }
    descriptor = parsed;
    return ContextualRansCompactFormatError::none;
}

ContextualRansCompactFormatError serialize_contextual_rans_compact_descriptor(
    const ContextualRansDescriptor& descriptor,
    const std::uint32_t expected_decision_count,
    const std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits,
    const std::span<std::byte> output,
    std::size_t& bytes_written) noexcept {
    std::size_t serialized_size{};
    const auto validation = validate_contextual_rans_compact_descriptor(
        descriptor, expected_decision_count, expected_payload_size, limits,
        serialized_size);
    if (validation != ContextualRansCompactFormatError::none) {
        return validation;
    }
    if (output.size() < serialized_size) {
        return ContextualRansCompactFormatError::output_too_small;
    }

    const auto analysis = analyze_models(descriptor);
    if (analysis.error != ContextualRansCompactFormatError::none
        || analysis.serialized_size != serialized_size) {
        return ContextualRansCompactFormatError::arithmetic_overflow;
    }
    std::array<std::byte, contextual_rans_compact_max_descriptor_size>
        encoded{};
    const std::span<std::byte> bytes{encoded};
    if (!core::store_le(bytes, 0, descriptor.decision_count)
        || !core::store_le(bytes, 4, descriptor.payload_size)
        || !core::store_le(bytes, 10, descriptor.context_count)
        || !core::store_le(bytes, 12, descriptor.frequency_entry_count)
        || !core::store_le(bytes, 16, analysis.active_mask)) {
        return ContextualRansCompactFormatError::arithmetic_overflow;
    }
    encoded[8] = static_cast<std::byte>(descriptor.table_log);
    encoded[9] = static_cast<std::byte>(descriptor.flags);

    std::size_t cursor = contextual_rans_compact_prefix_size;
    for (std::size_t context_id = 0;
         context_id < contextual_rans_context_count; ++context_id) {
        if ((analysis.active_mask & (UINT32_C(1) << context_id)) == 0) {
            continue;
        }
        const auto alphabet =
            context::internal::lzss_field_context_alphabets[context_id];
        const auto offset =
            context::internal::lzss_field_context_offsets[context_id];
        std::size_t nonzero_count{};
        for (std::uint16_t symbol = 0; symbol < alphabet; ++symbol) {
            if (descriptor.frequencies[offset + symbol] != 0) {
                ++nonzero_count;
            }
        }
        if (sparse_is_canonical(alphabet, nonzero_count)) {
            encoded[cursor++] = static_cast<std::byte>(
                static_cast<std::uint8_t>(RecordMode::sparse));
            encoded[cursor++] = static_cast<std::byte>(nonzero_count - 1);
            std::size_t emitted{};
            for (std::uint16_t symbol = 0; symbol < alphabet; ++symbol) {
                const auto frequency = descriptor.frequencies[offset + symbol];
                if (frequency == 0) continue;
                encoded[cursor++] = static_cast<std::byte>(symbol);
                ++emitted;
                if (emitted != nonzero_count
                    && !core::store_le(bytes, cursor, frequency)) {
                    return ContextualRansCompactFormatError::
                        arithmetic_overflow;
                }
                if (emitted != nonzero_count) cursor += sizeof(frequency);
            }
        } else {
            encoded[cursor++] = static_cast<std::byte>(
                static_cast<std::uint8_t>(RecordMode::dense));
            for (std::uint16_t symbol = 0; symbol + 1 < alphabet; ++symbol) {
                if (!core::store_le(
                        bytes, cursor,
                        descriptor.frequencies[offset + symbol])) {
                    return ContextualRansCompactFormatError::
                        arithmetic_overflow;
                }
                cursor += sizeof(std::uint16_t);
            }
        }
    }
    if (cursor != serialized_size) {
        return ContextualRansCompactFormatError::arithmetic_overflow;
    }
    std::copy_n(encoded.begin(), serialized_size, output.begin());
    bytes_written = serialized_size;
    return ContextualRansCompactFormatError::none;
}

static_assert(contextual_rans_compact_max_descriptor_size
              == contextual_rans_compact_prefix_size
                  + 3 * (1 + 2 * (2 - 1))
                  + 17 * (1 + 2 * (256 - 1))
                  + 3 * (1 + 2 * (8 - 1))
                  + 8 * (1 + 2 * (17 - 1)));

} // namespace marc::entropy::internal

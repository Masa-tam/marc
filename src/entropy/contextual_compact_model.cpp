#include "entropy/contextual_compact_model.hpp"

#include "core/checked_math.hpp"
#include "core/endian.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace marc::entropy::internal {
namespace {

[[nodiscard]] constexpr std::size_t maximum_records_size(
    const context::internal::LzssFieldContextVariant variant) noexcept {
    switch (variant) {
    case context::internal::LzssFieldContextVariant::field_context_64k:
        return contextual_compact_model_max_records_size_v1;
    case context::internal::LzssFieldContextVariant::field_context_1m:
        return contextual_compact_model_max_records_size_v2;
    case context::internal::LzssFieldContextVariant::field_context_4m:
        return contextual_compact_model_max_records_size_v3;
    case context::internal::LzssFieldContextVariant::field_context_16m:
        return contextual_compact_model_max_records_size_v4;
    }
    return 0;
}

enum class RecordMode : std::uint8_t {
    dense = 0,
    sparse = 1,
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

ContextualCompactModelAnalysis analyze_contextual_compact_model(
    const ContextualCompactFrequencies& frequencies,
    const context::internal::LzssFieldContextVariant variant) noexcept {
    ContextualCompactModelAnalysis analysis{};
    const auto selected = context::internal::get_lzss_field_context_layout(
        variant);
    if (selected.error
        != context::internal::LzssFieldContextLayoutError::none) {
        analysis.error =
            ContextualCompactModelError::unsupported_context_variant;
        return analysis;
    }
    const auto& layout = selected.layout;
    if (std::ranges::any_of(
            frequencies.begin() + layout.frequency_entries,
            frequencies.end(),
            [](const std::uint16_t value) { return value != 0; })) {
        analysis.error = ContextualCompactModelError::invalid_frequency_table;
        return analysis;
    }
    for (std::size_t context_id = 0;
         context_id < context::internal::lzss_field_context_count;
         ++context_id) {
        const auto begin = (*layout.offsets)[context_id];
        const auto end = (*layout.offsets)[context_id + 1];
        std::uint32_t sum{};
        std::size_t nonzero_count{};
        for (auto index = begin; index < end; ++index) {
            const auto frequency = frequencies[index];
            sum += frequency;
            if (frequency != 0) ++nonzero_count;
        }
        if (sum == 0) continue;
        if (sum != contextual_compact_model_total_frequency) {
            analysis.error =
                ContextualCompactModelError::invalid_frequency_table;
            return analysis;
        }
        analysis.active_mask |= UINT32_C(1) << context_id;
        const auto alphabet = (*layout.alphabets)[context_id];
        const auto record_size = sparse_is_canonical(alphabet, nonzero_count)
            ? sparse_record_size(nonzero_count)
            : dense_record_size(alphabet);
        if (!core::checked_add(
                analysis.records_size, record_size,
                analysis.records_size)) {
            analysis.error =
                ContextualCompactModelError::arithmetic_overflow;
            return analysis;
        }
    }
    if (analysis.active_mask == 0) {
        analysis.error =
            ContextualCompactModelError::invalid_active_context_mask;
    } else if (analysis.records_size > maximum_records_size(variant)) {
        analysis.error = ContextualCompactModelError::arithmetic_overflow;
    }
    return analysis;
}

ContextualCompactModelError parse_contextual_compact_model(
    const std::span<const std::byte> input,
    const std::uint32_t active_mask,
    ContextualCompactFrequencies& frequencies,
    const context::internal::LzssFieldContextVariant variant) noexcept {
    const auto selected = context::internal::get_lzss_field_context_layout(
        variant);
    if (selected.error
        != context::internal::LzssFieldContextLayoutError::none) {
        return ContextualCompactModelError::unsupported_context_variant;
    }
    const auto& layout = selected.layout;
    if (active_mask == 0 || (active_mask & UINT32_C(0x80000000)) != 0) {
        return ContextualCompactModelError::invalid_active_context_mask;
    }
    ContextualCompactFrequencies parsed{};
    std::size_t cursor{};
    for (std::size_t context_id = 0;
         context_id < context::internal::lzss_field_context_count;
         ++context_id) {
        if ((active_mask & (UINT32_C(1) << context_id)) == 0) continue;
        std::uint8_t mode_value{};
        if (!read_byte(input, cursor, mode_value)) {
            return ContextualCompactModelError::truncated_records;
        }
        const auto alphabet = (*layout.alphabets)[context_id];
        const auto offset = (*layout.offsets)[context_id];
        std::size_t nonzero_count{};
        if (mode_value == static_cast<std::uint8_t>(RecordMode::dense)) {
            std::uint32_t sum{};
            for (std::uint16_t symbol = 0; symbol + 1 < alphabet; ++symbol) {
                std::uint16_t frequency{};
                if (!read_u16(input, cursor, frequency)) {
                    return ContextualCompactModelError::truncated_records;
                }
                parsed[offset + symbol] = frequency;
                sum += frequency;
                if (sum > contextual_compact_model_total_frequency) {
                    return ContextualCompactModelError::
                        invalid_frequency_table;
                }
                if (frequency != 0) ++nonzero_count;
            }
            const auto final_frequency =
                contextual_compact_model_total_frequency - sum;
            parsed[offset + alphabet - 1] =
                static_cast<std::uint16_t>(final_frequency);
            if (final_frequency != 0) ++nonzero_count;
        } else if (mode_value
                   == static_cast<std::uint8_t>(RecordMode::sparse)) {
            std::uint8_t count_minus_one{};
            if (!read_byte(input, cursor, count_minus_one)) {
                return ContextualCompactModelError::truncated_records;
            }
            nonzero_count = static_cast<std::size_t>(count_minus_one) + 1;
            if (nonzero_count > alphabet) {
                return ContextualCompactModelError::invalid_frequency_table;
            }
            std::uint32_t sum{};
            std::uint16_t previous{};
            bool have_previous{};
            for (std::size_t entry = 0; entry < nonzero_count; ++entry) {
                std::uint8_t symbol{};
                if (!read_byte(input, cursor, symbol)) {
                    return ContextualCompactModelError::truncated_records;
                }
                if (symbol >= alphabet
                    || (have_previous && symbol <= previous)) {
                    return ContextualCompactModelError::
                        invalid_frequency_table;
                }
                previous = symbol;
                have_previous = true;
                std::uint32_t frequency{};
                if (entry + 1 < nonzero_count) {
                    std::uint16_t stored{};
                    if (!read_u16(input, cursor, stored)) {
                        return ContextualCompactModelError::truncated_records;
                    }
                    if (stored == 0) {
                        return ContextualCompactModelError::
                            invalid_frequency_table;
                    }
                    sum += stored;
                    if (sum >= contextual_compact_model_total_frequency) {
                        return ContextualCompactModelError::
                            invalid_frequency_table;
                    }
                    frequency = stored;
                } else {
                    frequency =
                        contextual_compact_model_total_frequency - sum;
                }
                parsed[offset + symbol] =
                    static_cast<std::uint16_t>(frequency);
            }
        } else {
            return ContextualCompactModelError::invalid_mode;
        }
        const bool encoded_sparse =
            mode_value == static_cast<std::uint8_t>(RecordMode::sparse);
        if (encoded_sparse != sparse_is_canonical(alphabet, nonzero_count)) {
            return ContextualCompactModelError::noncanonical_representation;
        }
    }
    if (cursor != input.size()) {
        return ContextualCompactModelError::trailing_data;
    }
    const auto analysis = analyze_contextual_compact_model(parsed, variant);
    if (analysis.error != ContextualCompactModelError::none) {
        return analysis.error;
    }
    if (analysis.active_mask != active_mask
        || analysis.records_size != input.size()) {
        return ContextualCompactModelError::noncanonical_representation;
    }
    frequencies = parsed;
    return ContextualCompactModelError::none;
}

ContextualCompactModelError serialize_contextual_compact_model(
    const ContextualCompactFrequencies& frequencies,
    const std::span<std::byte> output,
    std::size_t& bytes_written,
    const context::internal::LzssFieldContextVariant variant) noexcept {
    const auto selected = context::internal::get_lzss_field_context_layout(
        variant);
    if (selected.error
        != context::internal::LzssFieldContextLayoutError::none) {
        return ContextualCompactModelError::unsupported_context_variant;
    }
    const auto& layout = selected.layout;
    const auto analysis = analyze_contextual_compact_model(
        frequencies, variant);
    if (analysis.error != ContextualCompactModelError::none) {
        return analysis.error;
    }
    if (output.size() < analysis.records_size) {
        return ContextualCompactModelError::output_too_small;
    }
    std::array<std::byte, contextual_compact_model_record_capacity> encoded{};
    const std::span<std::byte> bytes{encoded};
    std::size_t cursor{};
    for (std::size_t context_id = 0;
         context_id < context::internal::lzss_field_context_count;
         ++context_id) {
        if ((analysis.active_mask & (UINT32_C(1) << context_id)) == 0) {
            continue;
        }
        const auto alphabet = (*layout.alphabets)[context_id];
        const auto offset = (*layout.offsets)[context_id];
        std::size_t nonzero_count{};
        for (std::uint16_t symbol = 0; symbol < alphabet; ++symbol) {
            if (frequencies[offset + symbol] != 0) ++nonzero_count;
        }
        if (sparse_is_canonical(alphabet, nonzero_count)) {
            encoded[cursor++] = static_cast<std::byte>(
                static_cast<std::uint8_t>(RecordMode::sparse));
            encoded[cursor++] = static_cast<std::byte>(nonzero_count - 1);
            std::size_t emitted{};
            for (std::uint16_t symbol = 0; symbol < alphabet; ++symbol) {
                const auto frequency = frequencies[offset + symbol];
                if (frequency == 0) continue;
                encoded[cursor++] = static_cast<std::byte>(symbol);
                ++emitted;
                if (emitted != nonzero_count
                    && !core::store_le(bytes, cursor, frequency)) {
                    return ContextualCompactModelError::arithmetic_overflow;
                }
                if (emitted != nonzero_count) cursor += sizeof(frequency);
            }
        } else {
            encoded[cursor++] = static_cast<std::byte>(
                static_cast<std::uint8_t>(RecordMode::dense));
            for (std::uint16_t symbol = 0; symbol + 1 < alphabet; ++symbol) {
                if (!core::store_le(
                        bytes, cursor, frequencies[offset + symbol])) {
                    return ContextualCompactModelError::arithmetic_overflow;
                }
                cursor += sizeof(std::uint16_t);
            }
        }
    }
    if (cursor != analysis.records_size) {
        return ContextualCompactModelError::arithmetic_overflow;
    }
    std::copy_n(encoded.begin(), cursor, output.begin());
    bytes_written = cursor;
    return ContextualCompactModelError::none;
}

static_assert(contextual_compact_model_max_records_size_v1
              == 3 * (1 + 2 * (2 - 1))
                  + 17 * (1 + 2 * (256 - 1))
                  + 3 * (1 + 2 * (8 - 1))
                  + 8 * (1 + 2 * (17 - 1)));
static_assert(contextual_compact_model_max_records_size_v2
              == 3 * (1 + 2 * (2 - 1))
                  + 17 * (1 + 2 * (256 - 1))
                  + 3 * (1 + 2 * (8 - 1))
                  + 8 * (1 + 2 * (21 - 1)));
static_assert(contextual_compact_model_max_records_size_v3
              == 3 * (1 + 2 * (2 - 1))
                  + 17 * (1 + 2 * (256 - 1))
                  + 3 * (1 + 2 * (8 - 1))
                  + 8 * (1 + 2 * (23 - 1)));
static_assert(contextual_compact_model_max_records_size_v4
              == 3 * (1 + 2 * (2 - 1))
                  + 17 * (1 + 2 * (256 - 1))
                  + 3 * (1 + 2 * (8 - 1))
                  + 8 * (1 + 2 * (25 - 1)));

} // namespace marc::entropy::internal

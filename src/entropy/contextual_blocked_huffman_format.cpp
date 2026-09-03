#include "entropy/contextual_blocked_huffman_format.hpp"

#include "core/checked_math.hpp"
#include "core/endian.hpp"
#include "entropy/huffman_decode_table.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

namespace marc::entropy::internal {
namespace {

enum class RecordMode : std::uint8_t {
    single = 0,
    sparse = 1,
    dense = 2,
};

struct SelectedLayout {
    std::array<std::uint16_t,
               contextual_blocked_huffman_field_table_count>
        field_alphabets{};
    const std::array<std::uint16_t,
                     context::internal::lzss_field_context_count>*
        context_alphabets{};
    std::size_t maximum_descriptor_size{};
};

[[nodiscard]] bool select_layout(
    const context::internal::LzssFieldContextVariant variant,
    SelectedLayout& selected) noexcept {
    using V = context::internal::LzssFieldContextVariant;
    switch (variant) {
    case V::field_context_64k:
        selected.context_alphabets =
            &context::internal::lzss_field_context_alphabets_v1;
        selected.maximum_descriptor_size =
            contextual_blocked_huffman_max_descriptor_size_v1;
        break;
    case V::field_context_1m:
        selected.context_alphabets =
            &context::internal::lzss_field_context_alphabets_v2;
        selected.maximum_descriptor_size =
            contextual_blocked_huffman_max_descriptor_size_v2;
        break;
    case V::field_context_4m:
        selected.context_alphabets =
            &context::internal::lzss_field_context_alphabets_v3;
        selected.maximum_descriptor_size =
            contextual_blocked_huffman_max_descriptor_size_v3;
        break;
    case V::field_context_16m:
        selected.context_alphabets =
            &context::internal::lzss_field_context_alphabets_v4;
        selected.maximum_descriptor_size =
            contextual_blocked_huffman_max_descriptor_size_v4;
        break;
    case V::field_context_64m:
        selected.context_alphabets =
            &context::internal::lzss_field_context_alphabets_v5;
        selected.maximum_descriptor_size =
            contextual_blocked_huffman_max_descriptor_size_v5;
        break;
    default: return false;
    }
    selected.field_alphabets = {
        2, 256, 8, (*selected.context_alphabets)[23]};
    return true;
}

[[nodiscard]] std::size_t dense_data_size(
    const std::uint16_t alphabet) noexcept {
    return (static_cast<std::size_t>(alphabet) + 1U) / 2U;
}

[[nodiscard]] bool sparse_is_canonical(
    const std::uint16_t alphabet,
    const std::size_t nonzero_count) noexcept {
    return 2U * nonzero_count < dense_data_size(alphabet);
}

[[nodiscard]] bool model_is_clear(
    const ContextualBlockedHuffmanModel& model) noexcept {
    return !model.active
        && model.single_symbol == contextual_blocked_huffman_no_single_symbol
        && std::ranges::all_of(
            model.lengths,
            [](const std::uint8_t length) { return length == 0; });
}

[[nodiscard]] ContextualBlockedHuffmanFormatError analyze_model(
    const ContextualBlockedHuffmanModel& model,
    const std::uint16_t alphabet,
    std::size_t& record_size) noexcept {
    if (!model.active) {
        return model_is_clear(model)
            ? ContextualBlockedHuffmanFormatError::none
            : ContextualBlockedHuffmanFormatError::noncanonical_representation;
    }
    if (model.single_symbol != contextual_blocked_huffman_no_single_symbol) {
        if (model.single_symbol >= alphabet) {
            return ContextualBlockedHuffmanFormatError::invalid_model_symbol;
        }
        if (!std::ranges::all_of(
                model.lengths,
                [](const std::uint8_t length) { return length == 0; })) {
            return ContextualBlockedHuffmanFormatError::
                noncanonical_representation;
        }
        record_size = 4;
        return ContextualBlockedHuffmanFormatError::none;
    }

    std::size_t nonzero_count{};
    for (std::size_t symbol = 0; symbol < model.lengths.size(); ++symbol) {
        const auto length = model.lengths[symbol];
        if (symbol >= alphabet && length != 0) {
            return ContextualBlockedHuffmanFormatError::invalid_code_length;
        }
        if (length > huffman_max_code_length) {
            return ContextualBlockedHuffmanFormatError::invalid_code_length;
        }
        if (length != 0) ++nonzero_count;
    }
    if (nonzero_count < 2) {
        return ContextualBlockedHuffmanFormatError::
            noncanonical_representation;
    }
    if (validate_code_lengths(model.lengths)
        != HuffmanTableError::none) {
        return ContextualBlockedHuffmanFormatError::invalid_huffman_table;
    }
    const auto data_size = sparse_is_canonical(alphabet, nonzero_count)
        ? 2U * nonzero_count
        : dense_data_size(alphabet);
    if (!core::checked_add(std::size_t{4}, data_size, record_size)) {
        return ContextualBlockedHuffmanFormatError::arithmetic_overflow;
    }
    return ContextualBlockedHuffmanFormatError::none;
}

[[nodiscard]] bool field_mask_valid(const std::uint8_t mask) noexcept {
    return mask == UINT8_C(0x03) || mask == UINT8_C(0x0F);
}

[[nodiscard]] bool override_mask_valid(
    const std::uint32_t mask,
    const std::uint8_t field_mask) noexcept {
    if ((mask & UINT32_C(0x80000000)) != 0) return false;
    for (std::size_t context_id = 0;
         context_id < context::internal::lzss_field_context_count;
         ++context_id) {
        if ((mask & (UINT32_C(1) << context_id)) == 0) continue;
        const auto field = contextual_blocked_huffman_field_for_context(
            static_cast<std::uint16_t>(context_id));
        if ((field_mask & (UINT8_C(1) << field)) == 0) return false;
    }
    return true;
}

[[nodiscard]] ContextualBlockedHuffmanFormatError validate_prefix(
    const ContextualBlockedHuffmanDescriptor& descriptor,
    const std::uint32_t expected_decision_count,
    const std::uint32_t expected_payload_size) noexcept {
    if (descriptor.decision_count == 0) {
        return ContextualBlockedHuffmanFormatError::invalid_decision_count;
    }
    if (descriptor.decision_count != expected_decision_count
        || descriptor.payload_size != expected_payload_size) {
        return ContextualBlockedHuffmanFormatError::contradictory_size;
    }
    std::uint64_t maximum_bits{};
    if (!core::checked_multiply(
            static_cast<std::uint64_t>(descriptor.decision_count),
            static_cast<std::uint64_t>(huffman_max_code_length),
            maximum_bits)) {
        return ContextualBlockedHuffmanFormatError::arithmetic_overflow;
    }
    const auto maximum_payload = (maximum_bits + 7U) / 8U;
    if (descriptor.payload_size > maximum_payload) {
        return ContextualBlockedHuffmanFormatError::invalid_payload_size;
    }
    if ((descriptor.payload_size == 0 && descriptor.final_valid_bits != 0)
        || (descriptor.payload_size != 0
            && (descriptor.final_valid_bits == 0
                || descriptor.final_valid_bits > 8))) {
        return ContextualBlockedHuffmanFormatError::invalid_final_bits;
    }
    if (descriptor.max_code_length != huffman_max_code_length) {
        return ContextualBlockedHuffmanFormatError::invalid_max_code_length;
    }
    if (!field_mask_valid(descriptor.field_active_mask)) {
        return ContextualBlockedHuffmanFormatError::invalid_field_mask;
    }
    if (!override_mask_valid(
            descriptor.override_mask, descriptor.field_active_mask)) {
        return ContextualBlockedHuffmanFormatError::invalid_override_mask;
    }
    return descriptor.flags == 0
        ? ContextualBlockedHuffmanFormatError::none
        : ContextualBlockedHuffmanFormatError::unknown_flags;
}

[[nodiscard]] ContextualBlockedHuffmanFormatError analyze_descriptor(
    const ContextualBlockedHuffmanDescriptor& descriptor,
    const SelectedLayout& layout, std::size_t& serialized_size,
    std::size_t& model_count) noexcept {
    serialized_size = contextual_blocked_huffman_prefix_size;
    model_count = 0;
    for (std::size_t field = 0; field < layout.field_alphabets.size(); ++field) {
        const bool expected_active =
            (descriptor.field_active_mask & (UINT8_C(1) << field)) != 0;
        if (descriptor.field_models[field].active != expected_active) {
            return ContextualBlockedHuffmanFormatError::
                noncanonical_representation;
        }
        std::size_t record_size{};
        const auto error = analyze_model(
            descriptor.field_models[field], layout.field_alphabets[field],
            record_size);
        if (error != ContextualBlockedHuffmanFormatError::none) return error;
        if (!expected_active) continue;
        ++model_count;
        if (!core::checked_add(
                serialized_size, record_size, serialized_size)) {
            return ContextualBlockedHuffmanFormatError::arithmetic_overflow;
        }
    }
    for (std::size_t context_id = 0;
         context_id < descriptor.context_models.size(); ++context_id) {
        const bool expected_active =
            (descriptor.override_mask & (UINT32_C(1) << context_id)) != 0;
        if (descriptor.context_models[context_id].active != expected_active) {
            return ContextualBlockedHuffmanFormatError::
                noncanonical_representation;
        }
        std::size_t record_size{};
        const auto error = analyze_model(
            descriptor.context_models[context_id],
            (*layout.context_alphabets)[context_id],
            record_size);
        if (error != ContextualBlockedHuffmanFormatError::none) return error;
        if (!expected_active) continue;
        ++model_count;
        if (!core::checked_add(
                serialized_size, record_size, serialized_size)) {
            return ContextualBlockedHuffmanFormatError::arithmetic_overflow;
        }
    }
    if (serialized_size < contextual_blocked_huffman_min_descriptor_size
        || serialized_size > layout.maximum_descriptor_size) {
        return ContextualBlockedHuffmanFormatError::invalid_descriptor_size;
    }
    return ContextualBlockedHuffmanFormatError::none;
}

[[nodiscard]] ContextualBlockedHuffmanFormatError parse_model(
    const std::span<const std::byte> input,
    std::size_t& cursor,
    const std::uint16_t alphabet,
    ContextualBlockedHuffmanModel& model) noexcept {
    if (cursor > input.size() || input.size() - cursor < 4) {
        return ContextualBlockedHuffmanFormatError::truncated_descriptor;
    }
    const auto mode = std::to_integer<std::uint8_t>(input[cursor]);
    const auto count_minus_one =
        std::to_integer<std::uint8_t>(input[cursor + 1]);
    std::uint16_t field{};
    if (!core::load_le(input, cursor + 2, field)) {
        return ContextualBlockedHuffmanFormatError::truncated_descriptor;
    }
    cursor += 4;
    ContextualBlockedHuffmanModel parsed{};
    parsed.active = true;
    if (mode == static_cast<std::uint8_t>(RecordMode::single)) {
        if (count_minus_one != 0 || field >= alphabet) {
            return ContextualBlockedHuffmanFormatError::invalid_model_symbol;
        }
        parsed.single_symbol = field;
        model = parsed;
        return ContextualBlockedHuffmanFormatError::none;
    }
    if (field != 0) {
        return ContextualBlockedHuffmanFormatError::noncanonical_representation;
    }
    const auto nonzero_count = static_cast<std::size_t>(count_minus_one) + 1U;
    if (nonzero_count < 2 || nonzero_count > alphabet) {
        return ContextualBlockedHuffmanFormatError::invalid_code_length;
    }
    if (mode == static_cast<std::uint8_t>(RecordMode::sparse)) {
        const auto data_size = 2U * nonzero_count;
        if (cursor > input.size() || input.size() - cursor < data_size) {
            return ContextualBlockedHuffmanFormatError::truncated_descriptor;
        }
        std::uint16_t previous{};
        bool have_previous{};
        for (std::size_t entry = 0; entry < nonzero_count; ++entry) {
            const auto symbol =
                std::to_integer<std::uint8_t>(input[cursor++]);
            const auto length =
                std::to_integer<std::uint8_t>(input[cursor++]);
            if (symbol >= alphabet || length == 0
                || length > huffman_max_code_length
                || (have_previous && symbol <= previous)) {
                return ContextualBlockedHuffmanFormatError::invalid_code_length;
            }
            parsed.lengths[symbol] = length;
            previous = symbol;
            have_previous = true;
        }
        if (!sparse_is_canonical(alphabet, nonzero_count)) {
            return ContextualBlockedHuffmanFormatError::
                noncanonical_representation;
        }
    } else if (mode == static_cast<std::uint8_t>(RecordMode::dense)) {
        const auto data_size = dense_data_size(alphabet);
        if (cursor > input.size() || input.size() - cursor < data_size) {
            return ContextualBlockedHuffmanFormatError::truncated_descriptor;
        }
        std::size_t counted{};
        for (std::uint16_t symbol = 0; symbol < alphabet; ++symbol) {
            const auto packed = std::to_integer<std::uint8_t>(
                input[cursor + symbol / 2U]);
            const auto length = static_cast<std::uint8_t>(
                symbol % 2U == 0 ? packed & 0x0FU : packed >> 4U);
            parsed.lengths[symbol] = length;
            if (length != 0) ++counted;
        }
        if ((alphabet & 1U) != 0
            && (std::to_integer<std::uint8_t>(
                    input[cursor + data_size - 1]) & 0xF0U) != 0) {
            return ContextualBlockedHuffmanFormatError::
                noncanonical_representation;
        }
        cursor += data_size;
        if (counted != nonzero_count
            || sparse_is_canonical(alphabet, nonzero_count)) {
            return ContextualBlockedHuffmanFormatError::
                noncanonical_representation;
        }
    } else {
        return ContextualBlockedHuffmanFormatError::invalid_model_mode;
    }
    if (validate_code_lengths(parsed.lengths)
        != HuffmanTableError::none) {
        return ContextualBlockedHuffmanFormatError::invalid_huffman_table;
    }
    model = parsed;
    return ContextualBlockedHuffmanFormatError::none;
}

[[nodiscard]] ContextualBlockedHuffmanFormatError serialize_model(
    const ContextualBlockedHuffmanModel& model,
    const std::uint16_t alphabet,
    const std::span<std::byte> output,
    std::size_t& cursor) noexcept {
    std::size_t record_size{};
    const auto analysis = analyze_model(model, alphabet, record_size);
    if (analysis != ContextualBlockedHuffmanFormatError::none) return analysis;
    if (!model.active || cursor > output.size()
        || output.size() - cursor < record_size) {
        return ContextualBlockedHuffmanFormatError::arithmetic_overflow;
    }
    if (model.single_symbol != contextual_blocked_huffman_no_single_symbol) {
        output[cursor] = static_cast<std::byte>(RecordMode::single);
        output[cursor + 1] = std::byte{0};
        if (!core::store_le(output, cursor + 2, model.single_symbol)) {
            return ContextualBlockedHuffmanFormatError::arithmetic_overflow;
        }
        cursor += 4;
        return ContextualBlockedHuffmanFormatError::none;
    }
    const auto nonzero_count = static_cast<std::size_t>(std::ranges::count_if(
        model.lengths,
        [](const std::uint8_t length) { return length != 0; }));
    const bool sparse = sparse_is_canonical(alphabet, nonzero_count);
    output[cursor] = static_cast<std::byte>(
        sparse ? RecordMode::sparse : RecordMode::dense);
    output[cursor + 1] = static_cast<std::byte>(nonzero_count - 1U);
    output[cursor + 2] = std::byte{0};
    output[cursor + 3] = std::byte{0};
    cursor += 4;
    if (sparse) {
        for (std::uint16_t symbol = 0; symbol < alphabet; ++symbol) {
            if (model.lengths[symbol] == 0) continue;
            output[cursor++] = static_cast<std::byte>(symbol);
            output[cursor++] = static_cast<std::byte>(model.lengths[symbol]);
        }
    } else {
        const auto data_size = dense_data_size(alphabet);
        for (std::size_t index = 0; index < data_size; ++index) {
            const auto first = model.lengths[2U * index];
            const auto second = 2U * index + 1U < alphabet
                ? model.lengths[2U * index + 1U]
                : 0U;
            output[cursor++] = static_cast<std::byte>(
                first | static_cast<std::uint8_t>(second << 4U));
        }
    }
    return ContextualBlockedHuffmanFormatError::none;
}

} // namespace

ContextualBlockedHuffmanFormatError
validate_contextual_blocked_huffman_descriptor(
    const ContextualBlockedHuffmanDescriptor& descriptor,
    const std::uint32_t expected_decision_count,
    const std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits,
    std::size_t& serialized_size,
    const context::internal::LzssFieldContextVariant variant) noexcept {
    SelectedLayout layout{};
    if (!select_layout(variant, layout)) {
        return ContextualBlockedHuffmanFormatError::
            unsupported_context_variant;
    }
    const auto prefix_error = validate_prefix(
        descriptor, expected_decision_count, expected_payload_size);
    if (prefix_error != ContextualBlockedHuffmanFormatError::none) {
        return prefix_error;
    }
    std::size_t size{};
    std::size_t model_count{};
    const auto analysis = analyze_descriptor(
        descriptor, layout, size, model_count);
    if (analysis != ContextualBlockedHuffmanFormatError::none) return analysis;
    std::size_t table_entries{};
    if (!core::checked_multiply(
            model_count, huffman_decode_node_capacity, table_entries)) {
        return ContextualBlockedHuffmanFormatError::arithmetic_overflow;
    }
    std::uint64_t buffered{};
    if (!core::checked_add(
            static_cast<std::uint64_t>(size),
            static_cast<std::uint64_t>(descriptor.payload_size), buffered)) {
        return ContextualBlockedHuffmanFormatError::arithmetic_overflow;
    }
    if (core::validate_limits(limits) != core::LimitError::none
        || descriptor.decision_count > limits.max_block_size
        || descriptor.payload_size > limits.max_compressed_payload_size
        || huffman_max_code_length > limits.max_huffman_code_length
        || table_entries > limits.max_entropy_table_entries
        || buffered > limits.max_internal_buffered_bytes) {
        return ContextualBlockedHuffmanFormatError::limit_exceeded;
    }
    serialized_size = size;
    return ContextualBlockedHuffmanFormatError::none;
}

ContextualBlockedHuffmanFormatError
parse_contextual_blocked_huffman_descriptor(
    const std::span<const std::byte> input,
    const std::uint32_t expected_decision_count,
    const std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits,
    ContextualBlockedHuffmanDescriptor& descriptor,
    const context::internal::LzssFieldContextVariant variant) noexcept {
    SelectedLayout layout{};
    if (!select_layout(variant, layout)) {
        return ContextualBlockedHuffmanFormatError::
            unsupported_context_variant;
    }
    if (input.size() < contextual_blocked_huffman_prefix_size) {
        return ContextualBlockedHuffmanFormatError::truncated_descriptor;
    }
    if (input.size() < contextual_blocked_huffman_min_descriptor_size
        || input.size() > layout.maximum_descriptor_size) {
        return ContextualBlockedHuffmanFormatError::invalid_descriptor_size;
    }
    ContextualBlockedHuffmanDescriptor parsed{};
    if (!core::load_le(input, 0, parsed.decision_count)
        || !core::load_le(input, 4, parsed.payload_size)
        || !core::load_le(input, 8, parsed.override_mask)) {
        return ContextualBlockedHuffmanFormatError::truncated_descriptor;
    }
    parsed.final_valid_bits = std::to_integer<std::uint8_t>(input[12]);
    parsed.max_code_length = std::to_integer<std::uint8_t>(input[13]);
    parsed.field_active_mask = std::to_integer<std::uint8_t>(input[14]);
    parsed.flags = std::to_integer<std::uint8_t>(input[15]);
    const auto prefix_error = validate_prefix(
        parsed, expected_decision_count, expected_payload_size);
    if (prefix_error != ContextualBlockedHuffmanFormatError::none) {
        return prefix_error;
    }
    std::size_t cursor = contextual_blocked_huffman_prefix_size;
    for (std::size_t field = 0; field < layout.field_alphabets.size(); ++field) {
        if ((parsed.field_active_mask & (UINT8_C(1) << field)) == 0) continue;
        const auto error = parse_model(
            input, cursor, layout.field_alphabets[field],
            parsed.field_models[field]);
        if (error != ContextualBlockedHuffmanFormatError::none) return error;
    }
    for (std::size_t context_id = 0;
         context_id < parsed.context_models.size(); ++context_id) {
        if ((parsed.override_mask & (UINT32_C(1) << context_id)) == 0) {
            continue;
        }
        const auto error = parse_model(
            input, cursor, (*layout.context_alphabets)[context_id],
            parsed.context_models[context_id]);
        if (error != ContextualBlockedHuffmanFormatError::none) return error;
    }
    if (cursor != input.size()) {
        return ContextualBlockedHuffmanFormatError::trailing_data;
    }
    std::size_t canonical_size{};
    const auto validation = validate_contextual_blocked_huffman_descriptor(
        parsed, expected_decision_count, expected_payload_size, limits,
        canonical_size, variant);
    if (validation != ContextualBlockedHuffmanFormatError::none) {
        return validation;
    }
    if (canonical_size != input.size()) {
        return ContextualBlockedHuffmanFormatError::
            noncanonical_representation;
    }
    descriptor = parsed;
    return ContextualBlockedHuffmanFormatError::none;
}

ContextualBlockedHuffmanFormatError
serialize_contextual_blocked_huffman_descriptor(
    const ContextualBlockedHuffmanDescriptor& descriptor,
    const std::uint32_t expected_decision_count,
    const std::uint32_t expected_payload_size,
    const core::DecoderLimits& limits,
    const std::span<std::byte> output,
    std::size_t& bytes_written,
    const context::internal::LzssFieldContextVariant variant) noexcept {
    SelectedLayout layout{};
    if (!select_layout(variant, layout)) {
        return ContextualBlockedHuffmanFormatError::
            unsupported_context_variant;
    }
    std::size_t serialized_size{};
    const auto validation = validate_contextual_blocked_huffman_descriptor(
        descriptor, expected_decision_count, expected_payload_size, limits,
        serialized_size, variant);
    if (validation != ContextualBlockedHuffmanFormatError::none) {
        return validation;
    }
    if (output.size() < serialized_size) {
        return ContextualBlockedHuffmanFormatError::output_too_small;
    }
    std::array<std::byte, contextual_blocked_huffman_descriptor_capacity>
        encoded{};
    const std::span<std::byte> bytes{encoded};
    if (!core::store_le(bytes, 0, descriptor.decision_count)
        || !core::store_le(bytes, 4, descriptor.payload_size)
        || !core::store_le(bytes, 8, descriptor.override_mask)) {
        return ContextualBlockedHuffmanFormatError::arithmetic_overflow;
    }
    encoded[12] = static_cast<std::byte>(descriptor.final_valid_bits);
    encoded[13] = static_cast<std::byte>(descriptor.max_code_length);
    encoded[14] = static_cast<std::byte>(descriptor.field_active_mask);
    encoded[15] = static_cast<std::byte>(descriptor.flags);
    std::size_t cursor = contextual_blocked_huffman_prefix_size;
    for (std::size_t field = 0; field < layout.field_alphabets.size(); ++field) {
        if (!descriptor.field_models[field].active) continue;
        const auto error = serialize_model(
            descriptor.field_models[field], layout.field_alphabets[field], bytes,
            cursor);
        if (error != ContextualBlockedHuffmanFormatError::none) return error;
    }
    for (std::size_t context_id = 0;
         context_id < descriptor.context_models.size(); ++context_id) {
        if (!descriptor.context_models[context_id].active) continue;
        const auto error = serialize_model(
            descriptor.context_models[context_id],
            (*layout.context_alphabets)[context_id], bytes,
            cursor);
        if (error != ContextualBlockedHuffmanFormatError::none) return error;
    }
    if (cursor != serialized_size) {
        return ContextualBlockedHuffmanFormatError::arithmetic_overflow;
    }
    std::copy_n(encoded.begin(), serialized_size, output.begin());
    bytes_written = serialized_size;
    return ContextualBlockedHuffmanFormatError::none;
}

static_assert(contextual_blocked_huffman_max_descriptor_size
              == contextual_blocked_huffman_prefix_size
                  + 5 + 132 + 8 + 13
                  + 3 * 5 + 17 * 132 + 3 * 8 + 8 * 13);
static_assert(contextual_blocked_huffman_max_descriptor_size_v2
              == contextual_blocked_huffman_prefix_size
                  + 5 + 132 + 8 + 15
                  + 3 * 5 + 17 * 132 + 3 * 8 + 8 * 15);

static_assert(contextual_blocked_huffman_max_descriptor_size_v5
              == contextual_blocked_huffman_prefix_size
                  + 5 + 132 + 8 + 18
                  + 3 * 5 + 17 * 132 + 3 * 8 + 8 * 18);

} // namespace marc::entropy::internal

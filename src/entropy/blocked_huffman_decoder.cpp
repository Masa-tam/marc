#include "entropy/blocked_huffman_decoder.hpp"

#include "entropy/canonical_huffman.hpp"
#include "entropy/huffman_decode_table.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace marc::entropy::internal {
namespace {

[[nodiscard]] BlockedHuffmanDecodeError scan_huffman_payload(
    const HuffmanDecodeTable& table,
    const std::span<const std::byte> payload,
    const std::uint64_t total_bits,
    const std::uint32_t symbol_count,
    const std::span<std::byte> output) noexcept {
    std::uint64_t bit_offset{};
    for (std::uint32_t output_index = 0;
         output_index < symbol_count;
         ++output_index) {
        const auto remaining = total_bits - bit_offset;
        const auto available = static_cast<std::uint8_t>(
            std::min<std::uint64_t>(huffman_max_code_length, remaining));
        std::uint16_t bits{};
        for (std::uint8_t bit = 0; bit < available; ++bit) {
            const auto source_offset = bit_offset + bit;
            const auto byte_value = std::to_integer<std::uint8_t>(
                payload[static_cast<std::size_t>(source_offset / 8)]);
            bits |= static_cast<std::uint16_t>(
                ((byte_value >> (source_offset % 8)) & 1U) << bit);
        }

        const auto decoded = decode_symbol(table, bits, available);
        if (decoded.status == HuffmanDecodeStatus::need_input) {
            return BlockedHuffmanDecodeError::truncated_payload;
        }
        if (decoded.status == HuffmanDecodeStatus::invalid_code) {
            return BlockedHuffmanDecodeError::invalid_code;
        }
        if (decoded.status != HuffmanDecodeStatus::symbol
            || decoded.bits_consumed == 0
            || decoded.bits_consumed > remaining) {
            return BlockedHuffmanDecodeError::internal_error;
        }
        if (!output.empty()) {
            output[output_index] = static_cast<std::byte>(decoded.symbol);
        }
        bit_offset += decoded.bits_consumed;
    }
    return bit_offset == total_bits
        ? BlockedHuffmanDecodeError::none
        : BlockedHuffmanDecodeError::trailing_bits;
}

} // namespace

BlockedHuffmanDecodeResult decode_blocked_huffman_block(
    const BlockedHuffmanDescriptor& descriptor,
    const std::span<const std::byte> model,
    const std::span<const std::byte> payload,
    const core::DecoderLimits& limits,
    const std::span<std::byte> output) noexcept {
    const auto output_size = static_cast<std::size_t>(descriptor.symbol_count);
    const auto validation = validate_blocked_huffman_block(
        descriptor, model, payload, limits);
    if (validation.error != BlockedHuffmanDecodeError::none) {
        return validation;
    }
    if (output.size() < output_size) {
        return {output_size, BlockedHuffmanDecodeError::output_too_small};
    }

    if ((descriptor.flags & blocked_huffman_raw_flag) != 0) {
        std::ranges::copy(payload, output.begin());
        return {output_size, BlockedHuffmanDecodeError::none};
    }

    HuffmanCodeLengths lengths{};
    for (std::size_t symbol = 0; symbol < lengths.size(); ++symbol) {
        lengths[symbol] = std::to_integer<std::uint8_t>(model[symbol]);
    }
    HuffmanDecodeTable table{};
    if (build_decode_table(lengths, table) != HuffmanTableError::none) {
        return {output_size, BlockedHuffmanDecodeError::internal_error};
    }
    const auto total_bits =
        static_cast<std::uint64_t>(payload.size() - 1) * 8
        + descriptor.final_valid_bits;
    const auto decoded = scan_huffman_payload(
        table, payload, total_bits, descriptor.symbol_count,
        output.first(output_size));
    return {output_size, decoded};
}

BlockedHuffmanDecodeResult validate_blocked_huffman_block(
    const BlockedHuffmanDescriptor& descriptor,
    const std::span<const std::byte> model,
    const std::span<const std::byte> payload,
    const core::DecoderLimits& limits) noexcept {
    const auto output_size = static_cast<std::size_t>(descriptor.symbol_count);
    if (validate_block_descriptor(
            descriptor, descriptor.symbol_count, limits)
        != BlockedHuffmanFormatError::none) {
        return {output_size, BlockedHuffmanDecodeError::invalid_descriptor};
    }
    if (model.size() != descriptor.model_size) {
        return {output_size, BlockedHuffmanDecodeError::model_size_mismatch};
    }
    if (payload.size() != descriptor.payload_size) {
        return {output_size, BlockedHuffmanDecodeError::payload_size_mismatch};
    }

    if ((descriptor.flags & blocked_huffman_raw_flag) != 0) {
        return {output_size, BlockedHuffmanDecodeError::none};
    }

    HuffmanCodeLengths lengths{};
    for (std::size_t symbol = 0; symbol < lengths.size(); ++symbol) {
        lengths[symbol] = std::to_integer<std::uint8_t>(model[symbol]);
        if (lengths[symbol] > limits.max_huffman_code_length) {
            return {output_size,
                    BlockedHuffmanDecodeError::code_length_limit};
        }
    }
    HuffmanDecodeTable table{};
    if (build_decode_table(lengths, table) != HuffmanTableError::none) {
        return {output_size, BlockedHuffmanDecodeError::invalid_model};
    }
    if (table.node_count > limits.max_entropy_table_entries) {
        return {output_size, BlockedHuffmanDecodeError::decode_table_limit};
    }

    const auto last_byte = std::to_integer<std::uint8_t>(payload.back());
    if (descriptor.final_valid_bits < 8) {
        const auto padding_mask = static_cast<std::uint8_t>(
            0xffU << descriptor.final_valid_bits);
        if ((last_byte & padding_mask) != 0) {
            return {output_size, BlockedHuffmanDecodeError::nonzero_padding};
        }
    }
    const auto total_bits =
        static_cast<std::uint64_t>(payload.size() - 1) * 8
        + descriptor.final_valid_bits;

    const auto validation = scan_huffman_payload(
        table, payload, total_bits, descriptor.symbol_count, {});
    return {output_size, validation};
}

} // namespace marc::entropy::internal

#include "entropy/blocked_huffman_encoder.hpp"

#include "core/checked_math.hpp"
#include "entropy/canonical_huffman.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace marc::entropy::internal {

BlockedHuffmanEncodeResult plan_blocked_huffman_block(
    const std::span<const std::byte> input,
    const core::DecoderLimits& limits,
    BlockedHuffmanDescriptor& descriptor) noexcept {
    if (input.empty()) {
        return {0, 0, BlockedHuffmanEncodeError::empty_input};
    }
    if (input.size() > std::numeric_limits<std::uint32_t>::max()
        || input.size() > limits.max_block_size) {
        return {0, input.size(), BlockedHuffmanEncodeError::block_too_large};
    }

    HuffmanFrequencies frequencies{};
    HuffmanCodeLengths lengths{};
    if (count_frequencies(input, frequencies) != HuffmanBuildError::none
        || build_length_limited_code_lengths(frequencies, lengths)
            != HuffmanBuildError::none) {
        return {0, 0, BlockedHuffmanEncodeError::internal_error};
    }

    std::uint64_t payload_bits{};
    for (std::size_t symbol = 0; symbol < frequencies.size(); ++symbol) {
        std::uint64_t symbol_bits{};
        if (!core::checked_multiply(
                frequencies[symbol],
                static_cast<std::uint64_t>(lengths[symbol]), symbol_bits)
            || !core::checked_add(payload_bits, symbol_bits, payload_bits)) {
            return {0, 0, BlockedHuffmanEncodeError::arithmetic_overflow};
        }
    }
    std::uint64_t rounded_bits{};
    if (!core::checked_add(payload_bits, UINT64_C(7), rounded_bits)) {
        return {0, 0, BlockedHuffmanEncodeError::arithmetic_overflow};
    }
    const auto huffman_payload_size = rounded_bits / 8;
    std::uint64_t huffman_stored_size{};
    if (!core::checked_add(
            huffman_payload_size,
            static_cast<std::uint64_t>(blocked_huffman_model_size),
            huffman_stored_size)) {
        return {0, 0, BlockedHuffmanEncodeError::arithmetic_overflow};
    }

    const bool use_huffman = huffman_stored_size < input.size();
    const auto model_size = use_huffman
        ? static_cast<std::size_t>(blocked_huffman_model_size)
        : std::size_t{0};
    const auto payload_size = use_huffman
        ? static_cast<std::size_t>(huffman_payload_size)
        : input.size();
    if (payload_size > limits.max_compressed_payload_size) {
        return {model_size, payload_size,
                BlockedHuffmanEncodeError::limit_exceeded};
    }
    std::uint64_t buffered_size{};
    if (!core::checked_add(
            static_cast<std::uint64_t>(model_size),
            static_cast<std::uint64_t>(payload_size), buffered_size)) {
        return {model_size, payload_size,
                BlockedHuffmanEncodeError::arithmetic_overflow};
    }
    if (buffered_size > limits.max_internal_buffered_bytes) {
        return {model_size, payload_size,
                BlockedHuffmanEncodeError::limit_exceeded};
    }

    BlockedHuffmanDescriptor planned{};
    planned.symbol_count = static_cast<std::uint32_t>(input.size());
    planned.payload_size = static_cast<std::uint32_t>(payload_size);
    planned.model_size = static_cast<std::uint16_t>(model_size);
    if (use_huffman) {
        planned.final_valid_bits = static_cast<std::uint8_t>(
            payload_bits % 8 == 0 ? 8 : payload_bits % 8);
    } else {
        planned.flags = blocked_huffman_raw_flag;
        planned.final_valid_bits = 8;
    }
    if (validate_block_descriptor(planned, planned.symbol_count, limits)
        != BlockedHuffmanFormatError::none) {
        return {model_size, payload_size,
                BlockedHuffmanEncodeError::internal_error};
    }
    descriptor = planned;
    return {model_size, payload_size, BlockedHuffmanEncodeError::none};
}

BlockedHuffmanEncodeResult encode_blocked_huffman_block(
    const std::span<const std::byte> input,
    const core::DecoderLimits& limits,
    const std::span<std::byte> model_output,
    const std::span<std::byte> payload_output,
    BlockedHuffmanDescriptor& descriptor) noexcept {
    BlockedHuffmanDescriptor planned{};
    const auto result = plan_blocked_huffman_block(input, limits, planned);
    if (result.error != BlockedHuffmanEncodeError::none) {
        return result;
    }
    if (model_output.size() < result.model_size) {
        return {result.model_size, result.payload_size,
                BlockedHuffmanEncodeError::model_output_too_small};
    }
    if (payload_output.size() < result.payload_size) {
        return {result.model_size, result.payload_size,
                BlockedHuffmanEncodeError::payload_output_too_small};
    }

    if ((planned.flags & blocked_huffman_raw_flag) != 0) {
        std::ranges::copy(input, payload_output.begin());
        descriptor = planned;
        return result;
    }

    HuffmanFrequencies frequencies{};
    HuffmanCodeLengths lengths{};
    CanonicalHuffmanTable codes{};
    if (count_frequencies(input, frequencies) != HuffmanBuildError::none
        || build_length_limited_code_lengths(frequencies, lengths)
            != HuffmanBuildError::none
        || build_canonical_table(lengths, codes)
            != HuffmanTableError::none) {
        return {result.model_size, result.payload_size,
                BlockedHuffmanEncodeError::internal_error};
    }
    for (std::size_t symbol = 0; symbol < lengths.size(); ++symbol) {
        model_output[symbol] = static_cast<std::byte>(lengths[symbol]);
    }
    std::ranges::fill(
        payload_output.first(result.payload_size), std::byte{0});
    std::uint64_t bit_offset{};
    for (const auto value : input) {
        const auto& code = codes[std::to_integer<std::uint8_t>(value)];
        for (std::uint8_t bit = 0; bit < code.length; ++bit) {
            if (((code.lsb_first >> bit) & 1U) != 0) {
                const auto byte_index =
                    static_cast<std::size_t>(bit_offset / 8);
                const auto bit_index =
                    static_cast<unsigned int>(bit_offset % 8);
                payload_output[byte_index] |=
                    static_cast<std::byte>(1U << bit_index);
            }
            ++bit_offset;
        }
    }
    descriptor = planned;
    return result;
}

} // namespace marc::entropy::internal

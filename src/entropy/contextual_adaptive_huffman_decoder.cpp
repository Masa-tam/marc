#include "entropy/contextual_adaptive_huffman_decoder.hpp"

#include "core/checked_math.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>

namespace marc::entropy::internal {
namespace {

enum class OverlapCheck : std::uint8_t {
    disjoint,
    overlap,
    arithmetic_overflow,
};

[[nodiscard]] OverlapCheck ranges_overlap(
    const void* first_data, const std::size_t first_size,
    const void* second_data, const std::size_t second_size) noexcept {
    if (first_size == 0 || second_size == 0) return OverlapCheck::disjoint;
    const auto first_begin = reinterpret_cast<std::uintptr_t>(first_data);
    const auto second_begin = reinterpret_cast<std::uintptr_t>(second_data);
    std::uintptr_t first_end{};
    std::uintptr_t second_end{};
    if (!core::checked_add(
            first_begin, static_cast<std::uintptr_t>(first_size), first_end)
        || !core::checked_add(
            second_begin, static_cast<std::uintptr_t>(second_size),
            second_end)) {
        return OverlapCheck::arithmetic_overflow;
    }
    return first_begin < second_end && second_begin < first_end
        ? OverlapCheck::overlap
        : OverlapCheck::disjoint;
}

} // namespace

void ContextualAdaptiveHuffmanDecoder::reset() noexcept {
    payload_ = {};
    models_ = {};
    total_bits_ = 0;
    bit_offset_ = 0;
    expected_decisions_ = 0;
    event_count_ = 0;
    decision_count_ = 0;
    error_ = ContextualAdaptiveHuffmanDecodeError::none;
    started_ = false;
    finished_ = false;
}

ContextualAdaptiveHuffmanDecodeResult
ContextualAdaptiveHuffmanDecoder::result() const noexcept {
    return {event_count_, decision_count_, bit_offset_, error_};
}

ContextualAdaptiveHuffmanDecodeResult ContextualAdaptiveHuffmanDecoder::fail(
    const ContextualAdaptiveHuffmanDecodeError error) noexcept {
    error_ = error;
    return result();
}

ContextualAdaptiveHuffmanDecodeResult ContextualAdaptiveHuffmanDecoder::begin(
    const ContextualAdaptiveHuffmanDescriptor& descriptor,
    const std::span<const std::byte> payload,
    const core::DecoderLimits& limits,
    const std::span<AdaptiveHuffmanNode> node_storage,
    const std::span<std::uint16_t> symbol_storage) noexcept {
    reset();
    if (payload.size() != descriptor.payload_size) {
        return fail(ContextualAdaptiveHuffmanDecodeError::payload_size_mismatch);
    }
    if (validate_contextual_adaptive_huffman_descriptor(
            descriptor, descriptor.decision_count, descriptor.payload_size,
            limits)
        != ContextualAdaptiveHuffmanFormatError::none) {
        return fail(ContextualAdaptiveHuffmanDecodeError::invalid_descriptor);
    }
    if (core::validate_limits(limits) != core::LimitError::none) {
        return fail(ContextualAdaptiveHuffmanDecodeError::invalid_descriptor);
    }
    if (node_storage.size() < contextual_adaptive_huffman_node_entries) {
        return fail(
            ContextualAdaptiveHuffmanDecodeError::node_workspace_too_small);
    }
    if (symbol_storage.size() < contextual_adaptive_huffman_symbol_entries) {
        return fail(
            ContextualAdaptiveHuffmanDecodeError::symbol_workspace_too_small);
    }

    std::size_t total_bits{};
    if (!core::checked_multiply(payload.size() - 1U, std::size_t{8}, total_bits)
        || !core::checked_add(
            total_bits, static_cast<std::size_t>(descriptor.final_valid_bits),
            total_bits)) {
        return fail(ContextualAdaptiveHuffmanDecodeError::arithmetic_overflow);
    }
    if (descriptor.final_valid_bits < 8) {
        const auto high_mask = static_cast<std::uint8_t>(
            0xffU << descriptor.final_valid_bits);
        if ((std::to_integer<std::uint8_t>(payload.back()) & high_mask) != 0) {
            return fail(ContextualAdaptiveHuffmanDecodeError::nonzero_padding);
        }
    }

    std::size_t node_bytes{};
    std::size_t symbol_bytes{};
    if (!core::checked_multiply(contextual_adaptive_huffman_node_entries,
                                sizeof(AdaptiveHuffmanNode), node_bytes)
        || !core::checked_multiply(
            contextual_adaptive_huffman_symbol_entries,
            sizeof(std::uint16_t), symbol_bytes)) {
        return fail(ContextualAdaptiveHuffmanDecodeError::arithmetic_overflow);
    }
    std::size_t model_bytes{};
    std::size_t aggregate_bytes{};
    if (!core::checked_add(node_bytes, symbol_bytes, model_bytes)
        || !core::checked_add(model_bytes, payload.size(), aggregate_bytes)) {
        return fail(ContextualAdaptiveHuffmanDecodeError::arithmetic_overflow);
    }
    if (contextual_adaptive_huffman_node_entries
                + contextual_adaptive_huffman_symbol_entries
            > limits.max_entropy_table_entries
        || aggregate_bytes > limits.max_internal_buffered_bytes) {
        return fail(ContextualAdaptiveHuffmanDecodeError::limit_exceeded);
    }
    const auto payload_nodes = ranges_overlap(
        payload.data(), payload.size(), node_storage.data(), node_bytes);
    const auto payload_symbols = ranges_overlap(
        payload.data(), payload.size(), symbol_storage.data(), symbol_bytes);
    const auto nodes_symbols = ranges_overlap(
        node_storage.data(), node_bytes, symbol_storage.data(), symbol_bytes);
    if (payload_nodes == OverlapCheck::arithmetic_overflow
        || payload_symbols == OverlapCheck::arithmetic_overflow
        || nodes_symbols == OverlapCheck::arithmetic_overflow) {
        return fail(ContextualAdaptiveHuffmanDecodeError::arithmetic_overflow);
    }
    if (payload_nodes == OverlapCheck::overlap
        || payload_symbols == OverlapCheck::overlap
        || nodes_symbols == OverlapCheck::overlap) {
        return fail(ContextualAdaptiveHuffmanDecodeError::overlapping_buffers);
    }

    const auto model_error = models_.initialize(
        node_storage.first(contextual_adaptive_huffman_node_entries),
        symbol_storage.first(contextual_adaptive_huffman_symbol_entries));
    if (model_error != ContextualAdaptiveHuffmanModelError::none
        || !models_.validate()) {
        return fail(ContextualAdaptiveHuffmanDecodeError::tree_error);
    }
    payload_ = payload;
    total_bits_ = total_bits;
    expected_decisions_ = descriptor.decision_count;
    started_ = true;
    return result();
}

bool ContextualAdaptiveHuffmanDecoder::read_bit(
    std::size_t& offset, std::uint8_t& bit) const noexcept {
    if (offset >= total_bits_) return false;
    const auto byte = std::to_integer<std::uint8_t>(payload_[offset / 8U]);
    bit = static_cast<std::uint8_t>((byte >> (offset % 8U)) & 1U);
    ++offset;
    return true;
}

ContextualAdaptiveHuffmanDecodeResult
ContextualAdaptiveHuffmanDecoder::decode_symbol(
    const std::uint16_t expected_context,
    const std::uint16_t expected_alphabet,
    std::uint32_t& value) noexcept {
    if (!started_) {
        if (error_ == ContextualAdaptiveHuffmanDecodeError::none
            || error_ == ContextualAdaptiveHuffmanDecodeError::not_started) {
            return fail(ContextualAdaptiveHuffmanDecodeError::not_started);
        }
        return result();
    }
    if (error_ != ContextualAdaptiveHuffmanDecodeError::none) return result();
    if (finished_) {
        return fail(ContextualAdaptiveHuffmanDecodeError::already_finished);
    }
    if (expected_context >= context::internal::lzss_field_context_count) {
        return fail(ContextualAdaptiveHuffmanDecodeError::invalid_context);
    }
    if (expected_alphabet
        != context::internal::lzss_field_context_alphabets[expected_context]) {
        return fail(ContextualAdaptiveHuffmanDecodeError::invalid_alphabet);
    }
    if (decision_count_ >= expected_decisions_) {
        return fail(
            ContextualAdaptiveHuffmanDecodeError::decision_count_exceeded);
    }
    auto* tree = models_.tree(expected_context);
    if (tree == nullptr) {
        return fail(ContextualAdaptiveHuffmanDecodeError::tree_error);
    }

    auto offset = bit_offset_;
    auto node_index = tree->root();
    for (std::size_t depth = 0; depth <= tree->node_count(); ++depth) {
        if (node_index >= tree->node_count()) {
            return fail(ContextualAdaptiveHuffmanDecodeError::invalid_path);
        }
        const auto& node = tree->node(node_index);
        if (node.kind == AdaptiveHuffmanNodeKind::internal) {
            std::uint8_t bit{};
            if (!read_bit(offset, bit)) {
                return fail(ContextualAdaptiveHuffmanDecodeError::truncated_bits);
            }
            node_index = bit == 0 ? node.left : node.right;
            continue;
        }

        std::uint16_t decoded{};
        bool is_new{};
        if (node.kind == AdaptiveHuffmanNodeKind::nyt) {
            const auto raw_width = static_cast<std::uint8_t>(
                std::bit_width(static_cast<std::uint16_t>(
                    expected_alphabet - 1U)));
            for (std::uint8_t bit_index = 0; bit_index < raw_width;
                 ++bit_index) {
                std::uint8_t bit{};
                if (!read_bit(offset, bit)) {
                    return fail(
                        ContextualAdaptiveHuffmanDecodeError::truncated_bits);
                }
                decoded |= static_cast<std::uint16_t>(bit << bit_index);
            }
            if (decoded >= expected_alphabet || tree->contains(decoded)) {
                return fail(
                    ContextualAdaptiveHuffmanDecodeError::invalid_nyt_symbol);
            }
            is_new = true;
        } else if (node.kind == AdaptiveHuffmanNodeKind::symbol) {
            decoded = node.symbol;
            if (decoded >= expected_alphabet || !tree->contains(decoded)) {
                return fail(ContextualAdaptiveHuffmanDecodeError::tree_error);
            }
        } else {
            return fail(ContextualAdaptiveHuffmanDecodeError::invalid_path);
        }

        const auto tree_error = is_new
            ? tree->observe_new(decoded)
            : tree->observe_existing(decoded);
        if (tree_error != ContextualAdaptiveHuffmanTreeError::none) {
            return fail(ContextualAdaptiveHuffmanDecodeError::tree_error);
        }
        bit_offset_ = offset;
        ++event_count_;
        ++decision_count_;
        value = decoded;
        return result();
    }
    return fail(ContextualAdaptiveHuffmanDecodeError::invalid_path);
}

ContextualAdaptiveHuffmanDecodeResult
ContextualAdaptiveHuffmanDecoder::decode_bypass(
    const std::uint8_t expected_bit_count, std::uint32_t& value) noexcept {
    if (!started_) {
        if (error_ == ContextualAdaptiveHuffmanDecodeError::none
            || error_ == ContextualAdaptiveHuffmanDecodeError::not_started) {
            return fail(ContextualAdaptiveHuffmanDecodeError::not_started);
        }
        return result();
    }
    if (error_ != ContextualAdaptiveHuffmanDecodeError::none) return result();
    if (finished_) {
        return fail(ContextualAdaptiveHuffmanDecodeError::already_finished);
    }
    if (expected_bit_count == 0 || expected_bit_count > 16) {
        return fail(
            ContextualAdaptiveHuffmanDecodeError::invalid_bypass_width);
    }
    if (decision_count_ > expected_decisions_
        || expected_bit_count > expected_decisions_ - decision_count_) {
        return fail(
            ContextualAdaptiveHuffmanDecodeError::decision_count_exceeded);
    }

    auto offset = bit_offset_;
    std::uint32_t decoded{};
    for (std::uint8_t bit_index = 0; bit_index < expected_bit_count;
         ++bit_index) {
        std::uint8_t bit{};
        if (!read_bit(offset, bit)) {
            return fail(ContextualAdaptiveHuffmanDecodeError::truncated_bits);
        }
        decoded |= static_cast<std::uint32_t>(bit) << bit_index;
    }
    bit_offset_ = offset;
    ++event_count_;
    decision_count_ += expected_bit_count;
    value = decoded;
    return result();
}

ContextualAdaptiveHuffmanDecodeResult
ContextualAdaptiveHuffmanDecoder::finish(
    const std::uint32_t expected_event_count,
    const std::uint32_t expected_decision_count) noexcept {
    if (!started_) {
        if (error_ == ContextualAdaptiveHuffmanDecodeError::none
            || error_ == ContextualAdaptiveHuffmanDecodeError::not_started) {
            return fail(ContextualAdaptiveHuffmanDecodeError::not_started);
        }
        return result();
    }
    if (error_ != ContextualAdaptiveHuffmanDecodeError::none) return result();
    if (finished_) {
        return fail(ContextualAdaptiveHuffmanDecodeError::already_finished);
    }
    if (event_count_ != expected_event_count
        || decision_count_ != expected_decision_count
        || decision_count_ != expected_decisions_) {
        return fail(ContextualAdaptiveHuffmanDecodeError::count_mismatch);
    }
    if (bit_offset_ != total_bits_) {
        return fail(ContextualAdaptiveHuffmanDecodeError::trailing_bits);
    }
    if (!models_.validate()) {
        return fail(ContextualAdaptiveHuffmanDecodeError::tree_error);
    }
    finished_ = true;
    return result();
}

} // namespace marc::entropy::internal

#include "entropy/position_rans_1m_format.hpp"
#include "core/endian.hpp"
#include <algorithm>

namespace marc::entropy::internal {
namespace {
using Error = PositionRans1mFormatError;
constexpr auto& alphabets = context::internal::lzss_position_distance_1m_alphabets;
constexpr auto& offsets = context::internal::lzss_position_distance_1m_offsets;
using Buffer = std::array<std::byte, position_rans_1m_descriptor_capacity>;

Error metadata(const PositionRans1mDescriptor& d,
               const core::DecoderLimits& limits) noexcept {
    if (d.payload_size < 8 || d.payload_size > 8 + UINT64_C(2)*d.decision_count
        || (d.decision_count == 0 && d.payload_size != 8))
        return Error::invalid_payload_size;
    if (d.decision_count > limits.max_block_size
        || d.payload_size > limits.max_compressed_payload_size)
        return Error::limit_exceeded;
    return Error::none;
}

// Called only with fixed staging: no externally observable partial writes.
Error pack(const PositionRans1mDescriptor& d, Buffer& bytes,
           std::size_t& size) noexcept {
    (void)core::store_le<std::uint32_t>(bytes, 0, d.decision_count);
    (void)core::store_le<std::uint32_t>(bytes, 4, d.payload_size);
    bytes[8] = std::byte{12};
    (void)core::store_le<std::uint16_t>(bytes, 10, 44);
    (void)core::store_le<std::uint32_t>(bytes, 12, 2566);
    std::size_t cursor = 22;
    bool active = false;
    for (std::size_t c = 0; c < alphabets.size(); ++c) {
        const auto begin = offsets[c];
        const auto alphabet = alphabets[c];
        std::uint32_t sum = 0;
        std::size_t count = 0, last = 0;
        for (std::size_t s = 0; s < alphabet; ++s) {
            const auto f = d.frequencies[begin+s];
            sum += f;
            if (f != 0) { ++count; last = s; }
        }
        if (count == 0) continue;
        if (sum != 4096) return Error::invalid_frequencies;
        active = true;
        bytes[16+c/8] |= static_cast<std::byte>(1U << (c%8));
        if (count == 1) {
            bytes[cursor++] = std::byte{0};
            bytes[cursor++] = static_cast<std::byte>(last);
        } else if (1+2*(alphabet-1) <= 1+3*count) {
            bytes[cursor++] = std::byte{1};
            for (std::size_t s = 0; s+1 < alphabet; ++s) {
                (void)core::store_le<std::uint16_t>(bytes, cursor,
                    d.frequencies[begin+s]);
                cursor += 2;
            }
        } else {
            bytes[cursor++] = std::byte{2};
            (void)core::store_le<std::uint16_t>(bytes, cursor,
                static_cast<std::uint16_t>(count));
            cursor += 2;
            for (std::size_t s = 0; s < alphabet; ++s) {
                const auto f = d.frequencies[begin+s];
                if (f == 0) continue;
                bytes[cursor++] = static_cast<std::byte>(s);
                if (s != last) {
                    (void)core::store_le<std::uint16_t>(bytes, cursor, f);
                    cursor += 2;
                }
            }
        }
    }
    if (active != (d.decision_count != 0)) return Error::invalid_frequencies;
    size = cursor;
    return Error::none;
}
} // namespace

PositionRans1mFormatError serialize_position_rans_1m_descriptor(
    const PositionRans1mDescriptor& d, const core::DecoderLimits& limits,
    const std::span<std::byte> output, std::size_t& bytes_written) noexcept {
    if (const auto error = metadata(d, limits); error != Error::none) return error;
    Buffer bytes{};
    std::size_t size{};
    if (const auto error = pack(d, bytes, size); error != Error::none) return error;
    if (size > limits.max_internal_buffered_bytes) return Error::limit_exceeded;
    if (output.size() < size) return Error::output_too_small;
    std::copy_n(bytes.begin(), size, output.begin());
    bytes_written = size;
    return Error::none;
}

PositionRans1mFormatError parse_position_rans_1m_descriptor(
    const std::span<const std::byte> input, const std::uint32_t expected_decisions,
    const std::uint32_t expected_payload_size, const core::DecoderLimits& limits,
    PositionRans1mDescriptor& output) noexcept {
    if (input.size() < 22) return Error::truncated;
    if (input.size() > position_rans_1m_descriptor_capacity)
        return Error::trailing_data;
    if (input.size() > limits.max_internal_buffered_bytes) return Error::limit_exceeded;
    PositionRans1mDescriptor d{};
    std::uint16_t contexts{};
    std::uint32_t entries{};
    (void)core::load_le(input, 0, d.decision_count);
    (void)core::load_le(input, 4, d.payload_size);
    (void)core::load_le(input, 10, contexts);
    (void)core::load_le(input, 12, entries);
    if (d.decision_count != expected_decisions || d.payload_size != expected_payload_size
        || input[8] != std::byte{12} || input[9] != std::byte{0}
        || contexts != 44 || entries != 2566) return Error::invalid_metadata;
    if (const auto error = metadata(d, limits); error != Error::none) return error;
    if ((std::to_integer<unsigned>(input[21]) & 0xf0U) != 0) return Error::invalid_mask;
    std::size_t cursor = 22;
    for (std::size_t c = 0; c < alphabets.size(); ++c) {
        if ((std::to_integer<unsigned>(input[16+c/8]) & (1U << (c%8))) == 0) continue;
        if (cursor == input.size()) return Error::truncated;
        const auto mode = std::to_integer<unsigned>(input[cursor++]);
        const auto begin = offsets[c];
        const auto alphabet = alphabets[c];
        if (mode == 0) {
            if (cursor == input.size()) return Error::truncated;
            const auto symbol = std::to_integer<unsigned>(input[cursor++]);
            if (symbol >= alphabet) return Error::invalid_frequencies;
            d.frequencies[begin+symbol] = 4096;
        } else if (mode == 1) {
            std::uint32_t sum{};
            for (std::size_t s = 0; s+1 < alphabet; ++s) {
                std::uint16_t f{};
                if (!core::load_le(input, cursor, f)) return Error::truncated;
                cursor += 2;
                sum += f;
                if (sum > 4096) return Error::invalid_frequencies;
                d.frequencies[begin+s] = f;
            }
            d.frequencies[begin+alphabet-1] = static_cast<std::uint16_t>(4096-sum);
        } else if (mode == 2) {
            std::uint16_t count{};
            if (!core::load_le(input, cursor, count)) return Error::truncated;
            cursor += 2;
            if (count < 2 || count > alphabet) return Error::invalid_frequencies;
            std::uint32_t sum{};
            int previous = -1;
            for (std::size_t i = 0; i < count; ++i) {
                if (cursor == input.size()) return Error::truncated;
                const auto symbol = std::to_integer<unsigned>(input[cursor++]);
                if (static_cast<int>(symbol) <= previous || symbol >= alphabet)
                    return Error::invalid_frequencies;
                previous = static_cast<int>(symbol);
                std::uint16_t f{};
                if (i+1 < count) {
                    if (!core::load_le(input, cursor, f)) return Error::truncated;
                    cursor += 2;
                    if (f == 0 || sum+f >= 4096) return Error::invalid_frequencies;
                } else f = static_cast<std::uint16_t>(4096-sum);
                sum += f;
                d.frequencies[begin+symbol] = f;
            }
        } else return Error::invalid_mode;
    }
    if (cursor != input.size()) return Error::trailing_data;
    Buffer canonical{};
    std::size_t size{};
    if (const auto error = pack(d, canonical, size); error != Error::none) return error;
    if (size != input.size() || !std::equal(input.begin(), input.end(), canonical.begin()))
        return Error::noncanonical;
    output = d;
    return Error::none;
}
} // namespace marc::entropy::internal

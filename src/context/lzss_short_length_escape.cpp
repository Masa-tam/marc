#include "context/lzss_short_length_escape.hpp"

#include <bit>
#include <cstdint>

namespace marc::context::internal {

LzssShortLengthEscapeEncodeResult encode_lzss_short_length_escape(
    const std::uint32_t length) noexcept {
    if (length < 3 || length > 258) {
        return {.error = LzssShortLengthEscapeError::invalid_length};
    }
    if (length <= 4) {
        return {8, 1, length - 3, LzssShortLengthEscapeError::none};
    }
    const auto value = length - 4;
    const auto length_class = static_cast<std::uint8_t>(
        std::bit_width(value) - 1U);
    return {length_class, length_class,
            value - (UINT32_C(1) << length_class),
            LzssShortLengthEscapeError::none};
}

LzssShortLengthEscapeDecodeResult decode_lzss_short_length_escape(
    const std::uint32_t length_class, const std::uint8_t bit_count,
    const std::uint32_t extra) noexcept {
    if (length_class > 8) {
        return {.error = LzssShortLengthEscapeError::invalid_class};
    }
    const auto required_bits = static_cast<std::uint8_t>(
        length_class == 8 ? 1 : length_class);
    if (bit_count != required_bits) {
        return {.error = LzssShortLengthEscapeError::invalid_width};
    }
    if (extra >= (UINT32_C(1) << required_bits)) {
        return {.error = LzssShortLengthEscapeError::invalid_extra};
    }
    const auto length = length_class == 8
        ? 3 + extra
        : 4 + (UINT32_C(1) << length_class) + extra;
    if (length > 258) {
        return {.error = LzssShortLengthEscapeError::invalid_length};
    }
    return {length, LzssShortLengthEscapeError::none};
}

} // namespace marc::context::internal

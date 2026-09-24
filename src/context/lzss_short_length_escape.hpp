#ifndef MARC_CONTEXT_LZSS_SHORT_LENGTH_ESCAPE_HPP
#define MARC_CONTEXT_LZSS_SHORT_LENGTH_ESCAPE_HPP

#include <cstdint>

namespace marc::context::internal {

// Decoder-visible length field for the private 2/8 + 1/7 identity only.
enum class LzssShortLengthEscapeError : std::uint8_t {
    none,
    invalid_length,
    invalid_class,
    invalid_width,
    invalid_extra,
};

struct LzssShortLengthEscapeEncodeResult {
    std::uint8_t length_class{};
    std::uint8_t bit_count{};
    std::uint32_t extra{};
    LzssShortLengthEscapeError error{LzssShortLengthEscapeError::none};
};

struct LzssShortLengthEscapeDecodeResult {
    std::uint32_t length{};
    LzssShortLengthEscapeError error{LzssShortLengthEscapeError::none};
};

[[nodiscard]] LzssShortLengthEscapeEncodeResult
encode_lzss_short_length_escape(std::uint32_t length) noexcept;

[[nodiscard]] LzssShortLengthEscapeDecodeResult
decode_lzss_short_length_escape(std::uint32_t length_class,
                                std::uint8_t bit_count,
                                std::uint32_t extra) noexcept;

} // namespace marc::context::internal

#endif

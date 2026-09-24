#ifndef MARC_FRAME_LZSS_SHORT_MATCH_STREAM_DECODER_HPP
#define MARC_FRAME_LZSS_SHORT_MATCH_STREAM_DECODER_HPP

#include "frame/lzss_short_match_frame_decoder.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame::internal {

enum class LzssShortMatchStreamDecodeError : std::uint8_t {
    none,
    stream_header_error,
    frame_error,
    trailing_data,
    output_size_unsupported,
    output_too_small,
    overlapping_workspaces,
    arithmetic_overflow,
    internal_error,
};

struct LzssShortMatchStreamDecodeResult {
    std::size_t serialized_consumed{};
    std::size_t raw_produced{};
    std::uint64_t frame_count{};
    std::size_t error_offset{};
    LzssShortMatchPreflightError stream_header_error{
        LzssShortMatchPreflightError::none};
    LzssShortMatchFrameDecodeResult frame{};
    LzssShortMatchStreamDecodeError error{
        LzssShortMatchStreamDecodeError::none};
};

// Strict private one-shot decoder. The caller supplies bounded frame/token
// scratch and a whole-stream output region. All regions must be disjoint and
// remain stable throughout both validation and publication passes. Malformed
// input does not modify whole-stream output. No public selector calls this.
[[nodiscard]] LzssShortMatchStreamDecodeResult
decode_lzss_short_match_stream(
    std::span<const std::byte> serialized_stream,
    const core::DecoderLimits& limits,
    std::span<dictionary::internal::LzssTypedToken> token_workspace,
    std::span<std::byte> raw_frame_workspace,
    std::span<std::byte> raw_stream_output) noexcept;

} // namespace marc::frame::internal

#endif

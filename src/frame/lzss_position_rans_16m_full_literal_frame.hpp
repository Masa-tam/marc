#ifndef MARC_FRAME_LZSS_POSITION_RANS_16M_FULL_LITERAL_FRAME_HPP
#define MARC_FRAME_LZSS_POSITION_RANS_16M_FULL_LITERAL_FRAME_HPP
#include "frame/lzss_position_rans_16m_full_literal_format.hpp"
#include "dictionary/lzss_typed_reconstructor.hpp"

namespace marc::frame::internal {
enum class PositionRans16mFullLiteralFrameError : std::uint8_t {
    none, format_error, token_error, token_output_too_small, raw_output_too_small,
    serialized_output_too_small, overlapping_buffers, arithmetic_overflow, reconstruction_error
};
struct PositionRans16mFullLiteralFrameResult {
    std::size_t serialized_size{};
    PositionRans16mFullLiteralFrameError error{};
    PositionRans16mFullLiteralFormatError format_error{};
    context::internal::LzssPositionRans16mFullLiteralTokenResult token_result{};
    PositionRans16mFullLiteralFrameRequirements requirements{};
};
[[nodiscard]] PositionRans16mFullLiteralFrameResult encode_position_rans_16m_full_literal_frame(
    std::span<const dictionary::internal::LzssTypedToken> tokens,
    const PositionRans16mFullLiteralFrameContext& context,std::span<std::byte> output) noexcept;
[[nodiscard]] PositionRans16mFullLiteralFrameResult decode_position_rans_16m_full_literal_frame(
    std::span<const std::byte> input,const PositionRans16mFullLiteralFrameContext& context,
    std::span<dictionary::internal::LzssTypedToken> tokens,std::span<std::byte> raw) noexcept;
[[nodiscard]] PositionRans16mFullLiteralFrameResult decode_position_rans_16m_full_literal_frame_scratch(
    std::span<const std::byte> input,const PositionRans16mFullLiteralFrameContext& context,
    std::span<dictionary::internal::LzssTypedToken> tokens,std::span<std::byte> raw) noexcept;
} // namespace marc::frame::internal
#endif

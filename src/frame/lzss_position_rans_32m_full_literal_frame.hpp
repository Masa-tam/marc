#ifndef MARC_FRAME_LZSS_POSITION_RANS_32M_FULL_LITERAL_FRAME_HPP
#define MARC_FRAME_LZSS_POSITION_RANS_32M_FULL_LITERAL_FRAME_HPP
#include "frame/lzss_position_rans_32m_full_literal_format.hpp"
#include "dictionary/lzss_typed_reconstructor.hpp"

namespace marc::frame::internal {
enum class PositionRans32mFullLiteralFrameError : std::uint8_t {
    none, format_error, token_error, token_output_too_small, raw_output_too_small,
    serialized_output_too_small, overlapping_buffers, arithmetic_overflow, reconstruction_error
};
struct PositionRans32mFullLiteralFrameResult {
    std::size_t serialized_size{};
    PositionRans32mFullLiteralFrameError error{};
    PositionRans32mFullLiteralFormatError format_error{};
    context::internal::LzssPositionRans32mFullLiteralTokenResult token_result{};
    PositionRans32mFullLiteralFrameRequirements requirements{};
};
[[nodiscard]] PositionRans32mFullLiteralFrameResult encode_position_rans_32m_full_literal_frame(
    std::span<const dictionary::internal::LzssTypedToken> tokens,
    const PositionRans32mFullLiteralFrameContext& context,std::span<std::byte> output) noexcept;
[[nodiscard]] PositionRans32mFullLiteralFrameResult decode_position_rans_32m_full_literal_frame(
    std::span<const std::byte> input,const PositionRans32mFullLiteralFrameContext& context,
    std::span<dictionary::internal::LzssTypedToken> tokens,std::span<std::byte> raw) noexcept;
[[nodiscard]] PositionRans32mFullLiteralFrameResult decode_position_rans_32m_full_literal_frame_scratch(
    std::span<const std::byte> input,const PositionRans32mFullLiteralFrameContext& context,
    std::span<dictionary::internal::LzssTypedToken> tokens,std::span<std::byte> raw) noexcept;
} // namespace marc::frame::internal
#endif

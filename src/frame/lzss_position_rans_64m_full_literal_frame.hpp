#ifndef MARC_FRAME_LZSS_POSITION_RANS_64M_FULL_LITERAL_FRAME_HPP
#define MARC_FRAME_LZSS_POSITION_RANS_64M_FULL_LITERAL_FRAME_HPP
#include "frame/lzss_position_rans_64m_full_literal_format.hpp"
#include "dictionary/lzss_typed_reconstructor.hpp"

namespace marc::frame::internal {
enum class PositionRans64mFullLiteralFrameError : std::uint8_t {
    none, format_error, token_error, token_output_too_small, raw_output_too_small,
    serialized_output_too_small, overlapping_buffers, arithmetic_overflow, reconstruction_error
};
struct PositionRans64mFullLiteralFrameResult {
    std::size_t serialized_size{};
    PositionRans64mFullLiteralFrameError error{};
    PositionRans64mFullLiteralFormatError format_error{};
    context::internal::LzssPositionRans64mFullLiteralTokenResult token_result{};
    PositionRans64mFullLiteralFrameRequirements requirements{};
};
[[nodiscard]] PositionRans64mFullLiteralFrameResult encode_position_rans_64m_full_literal_frame(
    std::span<const dictionary::internal::LzssTypedToken> tokens,
    const PositionRans64mFullLiteralFrameContext& context,std::span<std::byte> output) noexcept;
[[nodiscard]] PositionRans64mFullLiteralFrameResult decode_position_rans_64m_full_literal_frame(
    std::span<const std::byte> input,const PositionRans64mFullLiteralFrameContext& context,
    std::span<dictionary::internal::LzssTypedToken> tokens,std::span<std::byte> raw) noexcept;
[[nodiscard]] PositionRans64mFullLiteralFrameResult decode_position_rans_64m_full_literal_frame_scratch(
    std::span<const std::byte> input,const PositionRans64mFullLiteralFrameContext& context,
    std::span<dictionary::internal::LzssTypedToken> tokens,std::span<std::byte> raw) noexcept;
} // namespace marc::frame::internal
#endif

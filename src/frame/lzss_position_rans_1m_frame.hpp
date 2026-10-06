#ifndef MARC_FRAME_LZSS_POSITION_RANS_1M_FRAME_HPP
#define MARC_FRAME_LZSS_POSITION_RANS_1M_FRAME_HPP
#include "frame/lzss_position_rans_1m_format.hpp"
#include "dictionary/lzss_typed_reconstructor.hpp"

namespace marc::frame::internal {
enum class PositionRans1mFrameError : std::uint8_t {
    none, format_error, token_error, token_output_too_small, raw_output_too_small,
    serialized_output_too_small, overlapping_buffers, arithmetic_overflow, reconstruction_error
};
struct PositionRans1mFrameResult {
    std::size_t serialized_size{};
    PositionRans1mFrameError error{};
    PositionRans1mFormatError format_error{};
    context::internal::LzssPositionRans1mTokenResult token_result{};
    PositionRans1mFrameRequirements requirements{};
};
[[nodiscard]] PositionRans1mFrameResult encode_position_rans_1m_frame(
    std::span<const dictionary::internal::LzssTypedToken> tokens,
    const PositionRans1mFrameContext& context,std::span<std::byte> output) noexcept;
[[nodiscard]] PositionRans1mFrameResult decode_position_rans_1m_frame(
    std::span<const std::byte> input,const PositionRans1mFrameContext& context,
    std::span<dictionary::internal::LzssTypedToken> tokens,std::span<std::byte> raw) noexcept;
[[nodiscard]] PositionRans1mFrameResult decode_position_rans_1m_frame_scratch(
    std::span<const std::byte> input,const PositionRans1mFrameContext& context,
    std::span<dictionary::internal::LzssTypedToken> tokens,std::span<std::byte> raw) noexcept;
} // namespace marc::frame::internal
#endif

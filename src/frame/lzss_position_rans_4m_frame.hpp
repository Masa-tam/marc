#ifndef MARC_FRAME_LZSS_POSITION_RANS_4M_FRAME_HPP
#define MARC_FRAME_LZSS_POSITION_RANS_4M_FRAME_HPP
#include "frame/lzss_position_rans_4m_format.hpp"
#include "dictionary/lzss_typed_reconstructor.hpp"

namespace marc::frame::internal {
enum class PositionRans4mFrameError : std::uint8_t {
    none, format_error, token_error, token_output_too_small, raw_output_too_small,
    serialized_output_too_small, overlapping_buffers, arithmetic_overflow, reconstruction_error
};
struct PositionRans4mFrameResult {
    std::size_t serialized_size{};
    PositionRans4mFrameError error{};
    PositionRans4mFormatError format_error{};
    context::internal::LzssPositionRans4mTokenResult token_result{};
    PositionRans4mFrameRequirements requirements{};
};
[[nodiscard]] PositionRans4mFrameResult encode_position_rans_4m_frame(
    std::span<const dictionary::internal::LzssTypedToken> tokens,
    const PositionRans4mFrameContext& context,std::span<std::byte> output) noexcept;
[[nodiscard]] PositionRans4mFrameResult decode_position_rans_4m_frame(
    std::span<const std::byte> input,const PositionRans4mFrameContext& context,
    std::span<dictionary::internal::LzssTypedToken> tokens,std::span<std::byte> raw) noexcept;
[[nodiscard]] PositionRans4mFrameResult decode_position_rans_4m_frame_scratch(
    std::span<const std::byte> input,const PositionRans4mFrameContext& context,
    std::span<dictionary::internal::LzssTypedToken> tokens,std::span<std::byte> raw) noexcept;
} // namespace marc::frame::internal
#endif

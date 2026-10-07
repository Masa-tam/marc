#ifndef MARC_FRAME_LZSS_POSITION_RANS_64K_FRAME_HPP
#define MARC_FRAME_LZSS_POSITION_RANS_64K_FRAME_HPP
#include "frame/lzss_position_rans_64k_format.hpp"
#include "dictionary/lzss_typed_reconstructor.hpp"

namespace marc::frame::internal {
enum class PositionRans64kFrameError : std::uint8_t {
    none, format_error, token_error, token_output_too_small, raw_output_too_small,
    serialized_output_too_small, overlapping_buffers, arithmetic_overflow, reconstruction_error
};
struct PositionRans64kFrameResult {
    std::size_t serialized_size{};
    PositionRans64kFrameError error{};
    PositionRans64kFormatError format_error{};
    context::internal::LzssPositionRans64kTokenResult token_result{};
    PositionRans64kFrameRequirements requirements{};
};
[[nodiscard]] PositionRans64kFrameResult encode_position_rans_64k_frame(
    std::span<const dictionary::internal::LzssTypedToken> tokens,
    const PositionRans64kFrameContext& context,std::span<std::byte> output) noexcept;
[[nodiscard]] PositionRans64kFrameResult decode_position_rans_64k_frame(
    std::span<const std::byte> input,const PositionRans64kFrameContext& context,
    std::span<dictionary::internal::LzssTypedToken> tokens,std::span<std::byte> raw) noexcept;
[[nodiscard]] PositionRans64kFrameResult decode_position_rans_64k_frame_scratch(
    std::span<const std::byte> input,const PositionRans64kFrameContext& context,
    std::span<dictionary::internal::LzssTypedToken> tokens,std::span<std::byte> raw) noexcept;
} // namespace marc::frame::internal
#endif

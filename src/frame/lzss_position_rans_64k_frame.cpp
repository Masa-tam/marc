#include "frame/lzss_position_rans_64k_frame.hpp"
#include "frame/lzss_position_rans_frame_common.hpp"

namespace marc::frame::internal {
namespace {
struct Profile {
    using FrameResult=PositionRans64kFrameResult;
    using FrameContext=PositionRans64kFrameContext;
    using FrameLayout=PositionRans64kFrameLayout;
    using FrameHeader=PositionRans64kFrameHeader;
    using FormatError=PositionRans64kFormatError;
    using FrameError=PositionRans64kFrameError;
    using Descriptor=entropy::internal::PositionRans64kDescriptor;
    using TokenError=context::internal::LzssPositionRans64kTokenError;
    static constexpr auto token_variant=dictionary::internal::LzssTypedTokenVariant::field_context_64k_short_length_escape;
    static constexpr auto preflight_frame=preflight_position_rans_64k_frame;
    static constexpr auto validate_stream=validate_position_rans_64k_stream;
    static constexpr auto plan_tokens=context::internal::plan_lzss_position_rans_64k_tokens;
    static constexpr auto validate_frame_header=validate_position_rans_64k_frame_header;
    static constexpr auto encode_tokens=context::internal::encode_lzss_position_rans_64k_tokens;
    static constexpr auto serialize_frame_header=serialize_position_rans_64k_frame_header;
    static constexpr auto decode_tokens=context::internal::decode_lzss_position_rans_64k_tokens;
    static constexpr auto decode_token_scratch=context::internal::decode_lzss_position_rans_64k_token_scratch;
};
using Mechanics=PositionRansFrameMechanics<Profile>;
using Token=dictionary::internal::LzssTypedToken;
} // namespace
PositionRans64kFrameResult encode_position_rans_64k_frame(
    const std::span<const Token> tokens,const PositionRans64kFrameContext& context,std::span<std::byte> output) noexcept {return Mechanics::encode(tokens,context,output);}
PositionRans64kFrameResult decode_position_rans_64k_frame(std::span<const std::byte> input,
    const PositionRans64kFrameContext& context,std::span<Token> tokens,std::span<std::byte> raw) noexcept {
    return Mechanics::decode(input,context,tokens,raw,false);
}
PositionRans64kFrameResult decode_position_rans_64k_frame_scratch(std::span<const std::byte> input,
    const PositionRans64kFrameContext& context,std::span<Token> tokens,std::span<std::byte> raw) noexcept {
    return Mechanics::decode(input,context,tokens,raw,true);
}
} // namespace marc::frame::internal

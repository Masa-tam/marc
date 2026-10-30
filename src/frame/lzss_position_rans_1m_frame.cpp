#include "frame/lzss_position_rans_1m_frame.hpp"
#include "frame/lzss_position_rans_frame_common.hpp"

namespace marc::frame::internal {
namespace {
struct Profile {
    using FrameResult=PositionRans1mFrameResult;
    using FrameContext=PositionRans1mFrameContext;
    using FrameLayout=PositionRans1mFrameLayout;
    using FrameHeader=PositionRans1mFrameHeader;
    using FormatError=PositionRans1mFormatError;
    using FrameError=PositionRans1mFrameError;
    using Descriptor=entropy::internal::PositionRans1mDescriptor;
    using TokenError=context::internal::LzssPositionRans1mTokenError;
    static constexpr auto token_variant=dictionary::internal::LzssTypedTokenVariant::field_context_1m_short_length_escape;
    static constexpr auto preflight_frame=preflight_position_rans_1m_frame;
    static constexpr auto validate_stream=validate_position_rans_1m_stream;
    static constexpr auto plan_tokens=context::internal::plan_lzss_position_rans_1m_tokens;
    static constexpr auto validate_frame_header=validate_position_rans_1m_frame_header;
    static constexpr auto encode_tokens=context::internal::encode_lzss_position_rans_1m_tokens;
    static constexpr auto serialize_frame_header=serialize_position_rans_1m_frame_header;
    static constexpr auto decode_tokens=context::internal::decode_lzss_position_rans_1m_tokens;
    static constexpr auto decode_token_scratch=context::internal::decode_lzss_position_rans_1m_token_scratch;
};
using Mechanics=PositionRansFrameMechanics<Profile>;
using Token=dictionary::internal::LzssTypedToken;
} // namespace
PositionRans1mFrameResult encode_position_rans_1m_frame(
    const std::span<const Token> tokens,const PositionRans1mFrameContext& context,std::span<std::byte> output) noexcept {return Mechanics::encode(tokens,context,output);}
PositionRans1mFrameResult decode_position_rans_1m_frame(std::span<const std::byte> input,
    const PositionRans1mFrameContext& context,std::span<Token> tokens,std::span<std::byte> raw) noexcept {
    return Mechanics::decode(input,context,tokens,raw,false);
}
PositionRans1mFrameResult decode_position_rans_1m_frame_scratch(std::span<const std::byte> input,
    const PositionRans1mFrameContext& context,std::span<Token> tokens,std::span<std::byte> raw) noexcept {
    return Mechanics::decode(input,context,tokens,raw,true);
}
} // namespace marc::frame::internal

#include "frame/lzss_position_tans_64m_frame.hpp"
#include "frame/lzss_position_tans_frame_common.hpp"

namespace marc::frame::internal {
namespace {
struct Profile {
    using Error=PositionTans64mFrameError;
    using Result=PositionTans64mFrameResult;
    using FrameContext=PositionTans64mFrameContext;
    using FrameLayout=PositionTans64mFrameLayout;
    using FrameHeader=PositionTans64mFrameHeader;
    using FormatError=PositionTans64mFormatError;
    using TokenContext=context::internal::PositionTans64mTokenContext;
    using TokenError=context::internal::PositionTans64mTokenError;
    using Descriptor=entropy::internal::PositionTans64mDescriptor;
    static constexpr auto preflight_frame=preflight_position_tans_64m_frame;
    static constexpr auto validate_stream=validate_position_tans_64m_stream;
    static constexpr auto plan_tokens=context::internal::plan_position_tans_64m_tokens;
    static constexpr auto validate_frame_header=validate_position_tans_64m_frame_header;
    static constexpr auto encode_tokens=context::internal::encode_position_tans_64m_tokens;
    static constexpr auto serialize_frame_header=serialize_position_tans_64m_frame_header;
    static constexpr auto decode_tokens=context::internal::decode_position_tans_64m_token_scratch;
    static constexpr auto token_variant=dictionary::internal::LzssTypedTokenVariant::field_context_64m_short_length_escape;
};
using Mechanics=PositionTansFrameMechanics<Profile>;
using Token=dictionary::internal::LzssTypedToken;
using Entry=entropy::internal::TansDecodeEntry;
} // namespace
PositionTans64mFrameResult decode_position_tans_64m_frame(
    std::span<const std::byte> input,const PositionTans64mFrameContext& c,
    std::span<Entry> tables,std::span<Token> tokens,std::span<std::byte> scratch,std::span<std::byte> output) noexcept {
    return Mechanics::decode(input,c,tables,tokens,scratch,output,true);
}
PositionTans64mFrameResult decode_position_tans_64m_frame_scratch(
    std::span<const std::byte> input,const PositionTans64mFrameContext& c,
    std::span<Entry> tables,std::span<Token> tokens,std::span<std::byte> scratch) noexcept {
    return Mechanics::decode(input,c,tables,tokens,scratch,{},false);
}
PositionTans64mFrameResult encode_position_tans_64m_frame(std::span<const Token> tokens,
    const PositionTans64mFrameContext& c,std::span<std::uint16_t> tables,std::span<std::byte> payload,
    std::span<std::byte> scratch,std::span<std::byte> output) noexcept {
    return Mechanics::encode(tokens,c,tables,payload,scratch,output);
}
} // namespace marc::frame::internal

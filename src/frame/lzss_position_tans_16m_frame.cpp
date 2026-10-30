#include "frame/lzss_position_tans_16m_frame.hpp"
#include "frame/lzss_position_tans_frame_common.hpp"

namespace marc::frame::internal {
namespace {
struct Profile {
    using Error=PositionTans16mFrameError;
    using Result=PositionTans16mFrameResult;
    using FrameContext=PositionTans16mFrameContext;
    using FrameLayout=PositionTans16mFrameLayout;
    using FrameHeader=PositionTans16mFrameHeader;
    using FormatError=PositionTans16mFormatError;
    using TokenContext=context::internal::PositionTans16mTokenContext;
    using TokenError=context::internal::PositionTans16mTokenError;
    using Descriptor=entropy::internal::PositionTans16mDescriptor;
    static constexpr auto preflight_frame=preflight_position_tans_16m_frame;
    static constexpr auto validate_stream=validate_position_tans_16m_stream;
    static constexpr auto plan_tokens=context::internal::plan_position_tans_16m_tokens;
    static constexpr auto validate_frame_header=validate_position_tans_16m_frame_header;
    static constexpr auto encode_tokens=context::internal::encode_position_tans_16m_tokens;
    static constexpr auto serialize_frame_header=serialize_position_tans_16m_frame_header;
    static constexpr auto decode_tokens=context::internal::decode_position_tans_16m_token_scratch;
    static constexpr auto token_variant=dictionary::internal::LzssTypedTokenVariant::field_context_16m_short_length_escape;
};
using Mechanics=PositionTansFrameMechanics<Profile>;
using Token=dictionary::internal::LzssTypedToken;
using Entry=entropy::internal::TansDecodeEntry;
} // namespace
PositionTans16mFrameResult decode_position_tans_16m_frame(
    std::span<const std::byte> input,const PositionTans16mFrameContext& c,
    std::span<Entry> tables,std::span<Token> tokens,std::span<std::byte> scratch,std::span<std::byte> output) noexcept {
    return Mechanics::decode(input,c,tables,tokens,scratch,output,true);
}
PositionTans16mFrameResult decode_position_tans_16m_frame_scratch(
    std::span<const std::byte> input,const PositionTans16mFrameContext& c,
    std::span<Entry> tables,std::span<Token> tokens,std::span<std::byte> scratch) noexcept {
    return Mechanics::decode(input,c,tables,tokens,scratch,{},false);
}
PositionTans16mFrameResult encode_position_tans_16m_frame(std::span<const Token> tokens,
    const PositionTans16mFrameContext& c,std::span<std::uint16_t> tables,std::span<std::byte> payload,
    std::span<std::byte> scratch,std::span<std::byte> output) noexcept {
    return Mechanics::encode(tokens,c,tables,payload,scratch,output);
}
} // namespace marc::frame::internal

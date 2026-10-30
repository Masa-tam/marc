#include "frame/lzss_position_tans_1m_frame.hpp"
#include "frame/lzss_position_tans_frame_common.hpp"

namespace marc::frame::internal {
namespace {
struct Profile {
    using Error=PositionTans1mFrameError;
    using Result=PositionTans1mFrameResult;
    using FrameContext=PositionTans1mFrameContext;
    using FrameLayout=PositionTans1mFrameLayout;
    using FrameHeader=PositionTans1mFrameHeader;
    using FormatError=PositionTans1mFormatError;
    using TokenContext=context::internal::PositionTans1mTokenContext;
    using TokenError=context::internal::PositionTans1mTokenError;
    using Descriptor=entropy::internal::PositionTans1mDescriptor;
    static constexpr auto preflight_frame=preflight_position_tans_1m_frame;
    static constexpr auto validate_stream=validate_position_tans_1m_stream;
    static constexpr auto plan_tokens=context::internal::plan_position_tans_1m_tokens;
    static constexpr auto validate_frame_header=validate_position_tans_1m_frame_header;
    static constexpr auto encode_tokens=context::internal::encode_position_tans_1m_tokens;
    static constexpr auto serialize_frame_header=serialize_position_tans_1m_frame_header;
    static constexpr auto decode_tokens=context::internal::decode_position_tans_1m_token_scratch;
    static constexpr auto token_variant=dictionary::internal::LzssTypedTokenVariant::field_context_1m_short_length_escape;
};
using Mechanics=PositionTansFrameMechanics<Profile>;
using Token=dictionary::internal::LzssTypedToken;
using Entry=entropy::internal::TansDecodeEntry;
} // namespace
PositionTans1mFrameResult decode_position_tans_1m_frame(
    std::span<const std::byte> input,const PositionTans1mFrameContext& c,
    std::span<Entry> tables,std::span<Token> tokens,std::span<std::byte> scratch,std::span<std::byte> output) noexcept {
    return Mechanics::decode(input,c,tables,tokens,scratch,output,true);
}
PositionTans1mFrameResult decode_position_tans_1m_frame_scratch(
    std::span<const std::byte> input,const PositionTans1mFrameContext& c,
    std::span<Entry> tables,std::span<Token> tokens,std::span<std::byte> scratch) noexcept {
    return Mechanics::decode(input,c,tables,tokens,scratch,{},false);
}
PositionTans1mFrameResult encode_position_tans_1m_frame(std::span<const Token> tokens,
    const PositionTans1mFrameContext& c,std::span<std::uint16_t> tables,std::span<std::byte> payload,
    std::span<std::byte> scratch,std::span<std::byte> output) noexcept {
    return Mechanics::encode(tokens,c,tables,payload,scratch,output);
}
} // namespace marc::frame::internal

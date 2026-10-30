#include "frame/lzss_position_tans_32m_frame.hpp"
#include "frame/lzss_position_tans_frame_common.hpp"

namespace marc::frame::internal {
namespace {
struct Profile {
    using Error=PositionTans32mFrameError;
    using Result=PositionTans32mFrameResult;
    using FrameContext=PositionTans32mFrameContext;
    using FrameLayout=PositionTans32mFrameLayout;
    using FrameHeader=PositionTans32mFrameHeader;
    using FormatError=PositionTans32mFormatError;
    using TokenContext=context::internal::PositionTans32mTokenContext;
    using TokenError=context::internal::PositionTans32mTokenError;
    using Descriptor=entropy::internal::PositionTans32mDescriptor;
    static constexpr auto preflight_frame=preflight_position_tans_32m_frame;
    static constexpr auto validate_stream=validate_position_tans_32m_stream;
    static constexpr auto plan_tokens=context::internal::plan_position_tans_32m_tokens;
    static constexpr auto validate_frame_header=validate_position_tans_32m_frame_header;
    static constexpr auto encode_tokens=context::internal::encode_position_tans_32m_tokens;
    static constexpr auto serialize_frame_header=serialize_position_tans_32m_frame_header;
    static constexpr auto decode_tokens=context::internal::decode_position_tans_32m_token_scratch;
    static constexpr auto token_variant=dictionary::internal::LzssTypedTokenVariant::field_context_32m_short_length_escape;
};
using Mechanics=PositionTansFrameMechanics<Profile>;
using Token=dictionary::internal::LzssTypedToken;
using Entry=entropy::internal::TansDecodeEntry;
} // namespace
PositionTans32mFrameResult decode_position_tans_32m_frame(
    std::span<const std::byte> input,const PositionTans32mFrameContext& c,
    std::span<Entry> tables,std::span<Token> tokens,std::span<std::byte> scratch,std::span<std::byte> output) noexcept {
    return Mechanics::decode(input,c,tables,tokens,scratch,output,true);
}
PositionTans32mFrameResult decode_position_tans_32m_frame_scratch(
    std::span<const std::byte> input,const PositionTans32mFrameContext& c,
    std::span<Entry> tables,std::span<Token> tokens,std::span<std::byte> scratch) noexcept {
    return Mechanics::decode(input,c,tables,tokens,scratch,{},false);
}
PositionTans32mFrameResult encode_position_tans_32m_frame(std::span<const Token> tokens,
    const PositionTans32mFrameContext& c,std::span<std::uint16_t> tables,std::span<std::byte> payload,
    std::span<std::byte> scratch,std::span<std::byte> output) noexcept {
    return Mechanics::encode(tokens,c,tables,payload,scratch,output);
}
} // namespace marc::frame::internal

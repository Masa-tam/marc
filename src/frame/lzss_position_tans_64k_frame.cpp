#include "frame/lzss_position_tans_64k_frame.hpp"
#include "frame/lzss_position_tans_frame_common.hpp"

namespace marc::frame::internal {
namespace {
struct Profile {
    using Error=PositionTans64kFrameError;
    using Result=PositionTans64kFrameResult;
    using FrameContext=PositionTans64kFrameContext;
    using FrameLayout=PositionTans64kFrameLayout;
    using FrameHeader=PositionTans64kFrameHeader;
    using FormatError=PositionTans64kFormatError;
    using TokenContext=context::internal::PositionTans64kTokenContext;
    using TokenError=context::internal::PositionTans64kTokenError;
    using Descriptor=entropy::internal::PositionTans64kDescriptor;
    static constexpr auto preflight_frame=preflight_position_tans_64k_frame;
    static constexpr auto validate_stream=validate_position_tans_64k_stream;
    static constexpr auto plan_tokens=context::internal::plan_position_tans_64k_tokens;
    static constexpr auto validate_frame_header=validate_position_tans_64k_frame_header;
    static constexpr auto encode_tokens=context::internal::encode_position_tans_64k_tokens;
    static constexpr auto serialize_frame_header=serialize_position_tans_64k_frame_header;
    static constexpr auto decode_tokens=context::internal::decode_position_tans_64k_token_scratch;
    static constexpr auto token_variant=dictionary::internal::LzssTypedTokenVariant::field_context_64k_short_length_escape;
};
using Mechanics=PositionTansFrameMechanics<Profile>;
using Token=dictionary::internal::LzssTypedToken;
using Entry=entropy::internal::TansDecodeEntry;
} // namespace
PositionTans64kFrameResult decode_position_tans_64k_frame(
    std::span<const std::byte> input,const PositionTans64kFrameContext& c,
    std::span<Entry> tables,std::span<Token> tokens,std::span<std::byte> scratch,std::span<std::byte> output) noexcept {
    return Mechanics::decode(input,c,tables,tokens,scratch,output,true);
}
PositionTans64kFrameResult decode_position_tans_64k_frame_scratch(
    std::span<const std::byte> input,const PositionTans64kFrameContext& c,
    std::span<Entry> tables,std::span<Token> tokens,std::span<std::byte> scratch) noexcept {
    return Mechanics::decode(input,c,tables,tokens,scratch,{},false);
}
PositionTans64kFrameResult encode_position_tans_64k_frame(std::span<const Token> tokens,
    const PositionTans64kFrameContext& c,std::span<std::uint16_t> tables,std::span<std::byte> payload,
    std::span<std::byte> scratch,std::span<std::byte> output) noexcept {
    return Mechanics::encode(tokens,c,tables,payload,scratch,output);
}
} // namespace marc::frame::internal

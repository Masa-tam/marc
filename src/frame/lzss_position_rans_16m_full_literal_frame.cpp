#include "frame/lzss_position_rans_16m_full_literal_frame.hpp"
#include "frame/lzss_position_rans_frame_common.hpp"

namespace marc::frame::internal {
namespace {
struct Profile {
    using FrameResult=PositionRans16mFullLiteralFrameResult;
    using FrameContext=PositionRans16mFullLiteralFrameContext;
    using FrameLayout=PositionRans16mFullLiteralFrameLayout;
    using FrameHeader=PositionRans16mFullLiteralFrameHeader;
    using FormatError=PositionRans16mFullLiteralFormatError;
    using FrameError=PositionRans16mFullLiteralFrameError;
    using Descriptor=entropy::internal::PositionRans16mFullLiteralDescriptor;
    using TokenError=context::internal::LzssPositionRans16mFullLiteralTokenError;
    static constexpr auto token_variant=dictionary::internal::LzssTypedTokenVariant::field_context_16m_short_length_escape;
    static constexpr auto preflight_frame=preflight_position_rans_16m_full_literal_frame;
    static constexpr auto validate_stream=validate_position_rans_16m_full_literal_stream;
    static constexpr auto plan_tokens=context::internal::plan_lzss_position_rans_16m_full_literal_tokens;
    static constexpr auto validate_frame_header=validate_position_rans_16m_full_literal_frame_header;
    static constexpr auto encode_tokens=context::internal::encode_lzss_position_rans_16m_full_literal_tokens;
    static constexpr auto serialize_frame_header=serialize_position_rans_16m_full_literal_frame_header;
    static constexpr auto decode_tokens=context::internal::decode_lzss_position_rans_16m_full_literal_tokens;
    static constexpr auto decode_token_scratch=context::internal::decode_lzss_position_rans_16m_full_literal_token_scratch;
};
using Mechanics=PositionRansFrameMechanics<Profile>;
using Token=dictionary::internal::LzssTypedToken;
} // namespace
PositionRans16mFullLiteralFrameResult encode_position_rans_16m_full_literal_frame(
    const std::span<const Token> tokens,const PositionRans16mFullLiteralFrameContext& context,std::span<std::byte> output) noexcept {return Mechanics::encode(tokens,context,output);}
PositionRans16mFullLiteralFrameResult decode_position_rans_16m_full_literal_frame(std::span<const std::byte> input,
    const PositionRans16mFullLiteralFrameContext& context,std::span<Token> tokens,std::span<std::byte> raw) noexcept {
    return Mechanics::decode(input,context,tokens,raw,false);
}
PositionRans16mFullLiteralFrameResult decode_position_rans_16m_full_literal_frame_scratch(std::span<const std::byte> input,
    const PositionRans16mFullLiteralFrameContext& context,std::span<Token> tokens,std::span<std::byte> raw) noexcept {
    return Mechanics::decode(input,context,tokens,raw,true);
}
} // namespace marc::frame::internal

#include "frame/lzss_position_blocked_huffman_1m_frame.hpp"
#include "frame/lzss_position_blocked_huffman_frame_common.hpp"

namespace marc::frame::internal {
namespace {
struct Profile {
    using Error=PositionBlockedHuffman1mFrameError;
    using Result=PositionBlockedHuffman1mFrameResult;
    using FrameContext=PositionBlockedHuffman1mFrameContext;
    using FrameLayout=PositionBlockedHuffman1mFrameLayout;
    using FrameHeader=PositionBlockedHuffman1mFrameHeader;
    using FormatError=PositionBlockedHuffman1mFormatError;
    using TokenContext=context::internal::PositionBlockedHuffman1mTokenContext;
    using TokenError=context::internal::PositionBlockedHuffman1mTokenError;
    using Descriptor=entropy::internal::PositionBlockedHuffman1mDescriptor;
    static constexpr auto preflight_frame=preflight_position_blocked_huffman_1m_frame;
    static constexpr auto validate_stream=validate_position_blocked_huffman_1m_stream;
    static constexpr auto plan_tokens=context::internal::plan_position_blocked_huffman_1m_tokens;
    static constexpr auto validate_frame_header=validate_position_blocked_huffman_1m_frame_header;
    static constexpr auto encode_tokens=context::internal::encode_position_blocked_huffman_1m_tokens;
    static constexpr auto serialize_frame_header=serialize_position_blocked_huffman_1m_frame_header;
    static constexpr auto decode_tokens=context::internal::decode_position_blocked_huffman_1m_token_scratch;
    static constexpr auto decode_working_bytes=position_blocked_huffman_1m_decode_frame_workspace;
    static constexpr auto encode_working_bytes=position_blocked_huffman_1m_encode_frame_workspace;
    static constexpr auto token_variant=dictionary::internal::LzssTypedTokenVariant::field_context_1m_short_length_escape;
};
using Mechanics=PositionBlockedHuffmanFrameMechanics<Profile>;
using Token=dictionary::internal::LzssTypedToken;
using Entry=entropy::internal::HuffmanDecodeTable;
} // namespace
PositionBlockedHuffman1mFrameResult decode_position_blocked_huffman_1m_frame(
    std::span<const std::byte> input,const PositionBlockedHuffman1mFrameContext& c,
    std::span<Entry> tables,std::span<Token> tokens,std::span<std::byte> scratch,std::span<std::byte> output) noexcept {
    return Mechanics::decode(input,c,tables,tokens,scratch,output,true);
}
PositionBlockedHuffman1mFrameResult decode_position_blocked_huffman_1m_frame_scratch(
    std::span<const std::byte> input,const PositionBlockedHuffman1mFrameContext& c,
    std::span<Entry> tables,std::span<Token> tokens,std::span<std::byte> scratch) noexcept {
    return Mechanics::decode(input,c,tables,tokens,scratch,{},false);
}
PositionBlockedHuffman1mFrameResult encode_position_blocked_huffman_1m_frame(std::span<const Token> tokens,
    const PositionBlockedHuffman1mFrameContext& c,std::span<entropy::internal::CanonicalHuffmanTable> tables,std::span<std::byte> payload,
    std::span<std::byte> scratch,std::span<std::byte> output) noexcept {
    return Mechanics::encode(tokens,c,tables,payload,scratch,output);
}
} // namespace marc::frame::internal

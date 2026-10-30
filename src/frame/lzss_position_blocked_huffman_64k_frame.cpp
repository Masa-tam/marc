#include "frame/lzss_position_blocked_huffman_64k_frame.hpp"
#include "frame/lzss_position_blocked_huffman_frame_common.hpp"

namespace marc::frame::internal {
namespace {
struct Profile {
    using Error=PositionBlockedHuffman64kFrameError;
    using Result=PositionBlockedHuffman64kFrameResult;
    using FrameContext=PositionBlockedHuffman64kFrameContext;
    using FrameLayout=PositionBlockedHuffman64kFrameLayout;
    using FrameHeader=PositionBlockedHuffman64kFrameHeader;
    using FormatError=PositionBlockedHuffman64kFormatError;
    using TokenContext=context::internal::PositionBlockedHuffman64kTokenContext;
    using TokenError=context::internal::PositionBlockedHuffman64kTokenError;
    using Descriptor=entropy::internal::PositionBlockedHuffman64kDescriptor;
    static constexpr auto preflight_frame=preflight_position_blocked_huffman_64k_frame;
    static constexpr auto validate_stream=validate_position_blocked_huffman_64k_stream;
    static constexpr auto plan_tokens=context::internal::plan_position_blocked_huffman_64k_tokens;
    static constexpr auto validate_frame_header=validate_position_blocked_huffman_64k_frame_header;
    static constexpr auto encode_tokens=context::internal::encode_position_blocked_huffman_64k_tokens;
    static constexpr auto serialize_frame_header=serialize_position_blocked_huffman_64k_frame_header;
    static constexpr auto decode_tokens=context::internal::decode_position_blocked_huffman_64k_token_scratch;
    static constexpr auto decode_working_bytes=position_blocked_huffman_64k_decode_frame_workspace;
    static constexpr auto encode_working_bytes=position_blocked_huffman_64k_encode_frame_workspace;
    static constexpr auto token_variant=dictionary::internal::LzssTypedTokenVariant::field_context_64k_short_length_escape;
};
using Mechanics=PositionBlockedHuffmanFrameMechanics<Profile>;
using Token=dictionary::internal::LzssTypedToken;
using Entry=entropy::internal::HuffmanDecodeTable;
} // namespace
PositionBlockedHuffman64kFrameResult decode_position_blocked_huffman_64k_frame(
    std::span<const std::byte> input,const PositionBlockedHuffman64kFrameContext& c,
    std::span<Entry> tables,std::span<Token> tokens,std::span<std::byte> scratch,std::span<std::byte> output) noexcept {
    return Mechanics::decode(input,c,tables,tokens,scratch,output,true);
}
PositionBlockedHuffman64kFrameResult decode_position_blocked_huffman_64k_frame_scratch(
    std::span<const std::byte> input,const PositionBlockedHuffman64kFrameContext& c,
    std::span<Entry> tables,std::span<Token> tokens,std::span<std::byte> scratch) noexcept {
    return Mechanics::decode(input,c,tables,tokens,scratch,{},false);
}
PositionBlockedHuffman64kFrameResult encode_position_blocked_huffman_64k_frame(std::span<const Token> tokens,
    const PositionBlockedHuffman64kFrameContext& c,std::span<entropy::internal::CanonicalHuffmanTable> tables,std::span<std::byte> payload,
    std::span<std::byte> scratch,std::span<std::byte> output) noexcept {
    return Mechanics::encode(tokens,c,tables,payload,scratch,output);
}
} // namespace marc::frame::internal

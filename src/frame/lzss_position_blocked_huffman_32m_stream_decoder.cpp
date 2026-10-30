#include "frame/lzss_position_blocked_huffman_32m_stream_decoder.hpp"
#include "frame/lzss_position_blocked_huffman_stream_decoder_common.hpp"

namespace marc::frame::internal {
namespace {
struct Profile {
    using FormatError=PositionBlockedHuffman32mFormatError;
    using FrameError=PositionBlockedHuffman32mFrameError;
    using TokenError=context::internal::PositionBlockedHuffman32mTokenError;
    static constexpr auto parse_stream=parse_position_blocked_huffman_32m_stream;
    static constexpr auto parse_frame_header=parse_position_blocked_huffman_32m_frame_header;
    static constexpr auto preflight_prefix=preflight_position_blocked_huffman_32m_frame_prefix;
    static constexpr auto decode_frame=decode_position_blocked_huffman_32m_frame_scratch;
    static constexpr auto decode_working_bytes=position_blocked_huffman_32m_stream_decode_working_bytes;
    static constexpr auto tables=entropy::internal::position_blocked_huffman_32m_tables;
};
} // namespace
bool PositionBlockedHuffman32mStreamDecoder::disjoint(std::span<const std::byte> input,std::span<std::byte> output) const noexcept {
    return PositionBlockedHuffmanStreamMechanics::disjoint(*this,input,output);
}
PositionBlockedHuffman32mStreamDecoder::PositionBlockedHuffman32mStreamDecoder(core::DecoderLimits limits,
    std::span<std::byte> serialized,std::span<dictionary::internal::LzssTypedToken> tokens,
    std::span<std::byte> raw,std::span<entropy::internal::HuffmanDecodeTable> tables,std::size_t additional_owner_bytes) noexcept
    : limits_(limits),serialized_(serialized),raw_(raw),tokens_(tokens),tables_(tables) {
    PositionBlockedHuffmanStreamMechanics::initialize<Profile>(*this,additional_owner_bytes);
}
core::ProcessResult PositionBlockedHuffman32mStreamDecoder::fail(core::ErrorCode code,std::uint64_t position,
    std::size_t consumed,std::size_t produced) noexcept {
    return PositionBlockedHuffmanStreamMechanics::fail(*this,code,position,consumed,produced);
}
core::ProcessResult PositionBlockedHuffman32mStreamDecoder::process(std::span<const std::byte> input,
    std::span<std::byte> output,std::uint32_t flags) noexcept {
    return PositionBlockedHuffmanStreamMechanics::process<Profile>(*this,input,output,flags);
}
} // namespace marc::frame::internal

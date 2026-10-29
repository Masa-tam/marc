#include "frame/lzss_position_blocked_huffman_32m_owned.hpp"
#include "dictionary/lzss_position_distance_32m_five_prefix_candidate.hpp"
#include "core/buffer_overlap.hpp"
#include <algorithm>
#include <cstring>
#include <new>

#include "frame/lzss_position_blocked_huffman_owned_common.hpp"

namespace marc::frame::internal {
namespace {
using Code = core::ErrorCode;
struct Profile {
    using StreamHeader = PositionBlockedHuffman32mStreamHeader;
    using Workspace = PositionBlockedHuffman32mWorkspace;
    using StreamDecoder = PositionBlockedHuffman32mStreamDecoder;
    using FormatError = PositionBlockedHuffman32mFormatError;
    using FrameError = PositionBlockedHuffman32mFrameError;
    using TokenError = context::internal::PositionBlockedHuffman32mTokenError;
    static constexpr auto tables = entropy::internal::position_blocked_huffman_32m_tables;
    static constexpr auto encode_working_bytes = position_blocked_huffman_32m_stream_encode_working_bytes;
    static constexpr auto decode_working_bytes = position_blocked_huffman_32m_stream_decode_working_bytes;
    static constexpr auto decode_frame_workspace = position_blocked_huffman_32m_decode_frame_workspace;
    static constexpr auto payload_bits = std::size_t{150};
    static constexpr auto serialized_overhead = std::size_t{64+2578};
    static constexpr auto validate_stream = validate_position_blocked_huffman_32m_stream;
    static constexpr auto serialize_stream = serialize_position_blocked_huffman_32m_stream;
    static constexpr auto encode_frame = encode_position_blocked_huffman_32m_frame;
    static auto finder_workspace(std::size_t size,
        const dictionary::internal::LzssParameters& parameters,
        const core::DecoderLimits& limits,
        dictionary::internal::LzssTypedTokenVariant variant) noexcept {
        return dictionary::internal::calculate_lzss_position_distance_32m_five_prefix_workspace(size,parameters,limits,variant);
    }
    static constexpr auto tokenize = dictionary::internal::tokenize_lzss_position_distance_32m_five_prefix_candidate;
    static constexpr auto token_variant = dictionary::internal::LzssTypedTokenVariant::field_context_32m_short_length_escape;
};
} // namespace

Code PositionBlockedHuffman32mOwnedEncoder::requirements(const PositionBlockedHuffman32mStreamHeader& s,
    const core::DecoderLimits& l,PositionBlockedHuffman32mWorkspace& r) noexcept {
    return PositionBlockedHuffmanOwnedMechanics::encoder_requirements<Profile, PositionBlockedHuffman32mOwnedEncoder>(s,l,r);
}
std::unique_ptr<PositionBlockedHuffman32mOwnedEncoder> PositionBlockedHuffman32mOwnedEncoder::create(
    const PositionBlockedHuffman32mStreamHeader& stream,const core::DecoderLimits& limits,Code& error) noexcept {
    return PositionBlockedHuffmanOwnedMechanics::create_encoder<Profile, PositionBlockedHuffman32mOwnedEncoder>(stream,limits,error);
}
bool PositionBlockedHuffman32mOwnedEncoder::disjoint(std::span<const std::byte> input,std::span<std::byte> output) const noexcept {
    return PositionBlockedHuffmanOwnedMechanics::encoder_disjoint(*this,input,output);
}
core::ProcessResult PositionBlockedHuffman32mOwnedEncoder::fail(Code code,std::size_t consumed,std::size_t produced) noexcept {
    return PositionBlockedHuffmanOwnedMechanics::fail_encoder(*this,code,consumed,produced);
}
core::ProcessResult PositionBlockedHuffman32mOwnedEncoder::process(std::span<const std::byte> input,
    std::span<std::byte> output,std::uint32_t flags) noexcept {
    return PositionBlockedHuffmanOwnedMechanics::encode<Profile>(*this,input,output,flags);
}
Code PositionBlockedHuffman32mOwnedDecoder::requirements(std::uint32_t capacity,const core::DecoderLimits& l,PositionBlockedHuffman32mWorkspace& r) noexcept {
    return PositionBlockedHuffmanOwnedMechanics::decoder_requirements<Profile, PositionBlockedHuffman32mOwnedDecoder>(capacity,l,r);
}
std::unique_ptr<PositionBlockedHuffman32mOwnedDecoder> PositionBlockedHuffman32mOwnedDecoder::create(
    std::uint32_t capacity,const core::DecoderLimits& limits,Code& error) noexcept {
    return PositionBlockedHuffmanOwnedMechanics::create_decoder<Profile, PositionBlockedHuffman32mOwnedDecoder>(capacity,limits,error);
}
core::ProcessResult PositionBlockedHuffman32mOwnedDecoder::process(std::span<const std::byte> input,std::span<std::byte> output,std::uint32_t flags) noexcept {
    return PositionBlockedHuffmanOwnedMechanics::decode(*this,input,output,flags);
}
} // namespace marc::frame::internal

#include "frame/lzss_position_blocked_huffman_16m_owned.hpp"
#include "dictionary/lzss_position_distance_16m_five_prefix_candidate.hpp"
#include "core/buffer_overlap.hpp"
#include <algorithm>
#include <cstring>
#include <new>

#include "frame/lzss_position_blocked_huffman_owned_common.hpp"

namespace marc::frame::internal {
namespace {
using Code = core::ErrorCode;
struct Profile {
    using StreamHeader = PositionBlockedHuffman16mStreamHeader;
    using Workspace = PositionBlockedHuffman16mWorkspace;
    using StreamDecoder = PositionBlockedHuffman16mStreamDecoder;
    using FormatError = PositionBlockedHuffman16mFormatError;
    using FrameError = PositionBlockedHuffman16mFrameError;
    using TokenError = context::internal::PositionBlockedHuffman16mTokenError;
    static constexpr auto tables = entropy::internal::position_blocked_huffman_16m_tables;
    static constexpr auto encode_working_bytes = position_blocked_huffman_16m_stream_encode_working_bytes;
    static constexpr auto decode_working_bytes = position_blocked_huffman_16m_stream_decode_working_bytes;
    static constexpr auto decode_frame_workspace = position_blocked_huffman_16m_decode_frame_workspace;
    static constexpr auto payload_bits = std::size_t{135};
    static constexpr auto serialized_overhead = std::size_t{64+2575};
    static constexpr auto validate_stream = validate_position_blocked_huffman_16m_stream;
    static constexpr auto serialize_stream = serialize_position_blocked_huffman_16m_stream;
    static constexpr auto encode_frame = encode_position_blocked_huffman_16m_frame;
    static auto finder_workspace(std::size_t size,
        const dictionary::internal::LzssParameters& parameters,
        const core::DecoderLimits& limits,
        dictionary::internal::LzssTypedTokenVariant variant) noexcept {
        return dictionary::internal::calculate_lzss_position_distance_16m_five_prefix_workspace(size,parameters,limits,variant);
    }
    static constexpr auto tokenize = dictionary::internal::tokenize_lzss_position_distance_16m_five_prefix_candidate;
    static constexpr auto token_variant = dictionary::internal::LzssTypedTokenVariant::field_context_16m_short_length_escape;
};
} // namespace

Code PositionBlockedHuffman16mOwnedEncoder::requirements(const PositionBlockedHuffman16mStreamHeader& s,
    const core::DecoderLimits& l,PositionBlockedHuffman16mWorkspace& r) noexcept {
    return PositionBlockedHuffmanOwnedMechanics::encoder_requirements<Profile, PositionBlockedHuffman16mOwnedEncoder>(s,l,r);
}
std::unique_ptr<PositionBlockedHuffman16mOwnedEncoder> PositionBlockedHuffman16mOwnedEncoder::create(
    const PositionBlockedHuffman16mStreamHeader& stream,const core::DecoderLimits& limits,Code& error) noexcept {
    return PositionBlockedHuffmanOwnedMechanics::create_encoder<Profile, PositionBlockedHuffman16mOwnedEncoder>(stream,limits,error);
}
bool PositionBlockedHuffman16mOwnedEncoder::disjoint(std::span<const std::byte> input,std::span<std::byte> output) const noexcept {
    return PositionBlockedHuffmanOwnedMechanics::encoder_disjoint(*this,input,output);
}
core::ProcessResult PositionBlockedHuffman16mOwnedEncoder::fail(Code code,std::size_t consumed,std::size_t produced) noexcept {
    return PositionBlockedHuffmanOwnedMechanics::fail_encoder(*this,code,consumed,produced);
}
core::ProcessResult PositionBlockedHuffman16mOwnedEncoder::process(std::span<const std::byte> input,
    std::span<std::byte> output,std::uint32_t flags) noexcept {
    return PositionBlockedHuffmanOwnedMechanics::encode<Profile>(*this,input,output,flags);
}
Code PositionBlockedHuffman16mOwnedDecoder::requirements(std::uint32_t capacity,const core::DecoderLimits& l,PositionBlockedHuffman16mWorkspace& r) noexcept {
    return PositionBlockedHuffmanOwnedMechanics::decoder_requirements<Profile, PositionBlockedHuffman16mOwnedDecoder>(capacity,l,r);
}
std::unique_ptr<PositionBlockedHuffman16mOwnedDecoder> PositionBlockedHuffman16mOwnedDecoder::create(
    std::uint32_t capacity,const core::DecoderLimits& limits,Code& error) noexcept {
    return PositionBlockedHuffmanOwnedMechanics::create_decoder<Profile, PositionBlockedHuffman16mOwnedDecoder>(capacity,limits,error);
}
core::ProcessResult PositionBlockedHuffman16mOwnedDecoder::process(std::span<const std::byte> input,std::span<std::byte> output,std::uint32_t flags) noexcept {
    return PositionBlockedHuffmanOwnedMechanics::decode(*this,input,output,flags);
}
} // namespace marc::frame::internal

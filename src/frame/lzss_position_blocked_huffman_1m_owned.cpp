#include "frame/lzss_position_blocked_huffman_1m_owned.hpp"
#include "dictionary/lzss_position_distance_1m_five_prefix_candidate.hpp"
#include "core/buffer_overlap.hpp"
#include <algorithm>
#include <cstring>
#include <new>

#include "frame/lzss_position_blocked_huffman_owned_common.hpp"

namespace marc::frame::internal {
namespace {
using Code = core::ErrorCode;
struct Profile {
    using StreamHeader = PositionBlockedHuffman1mStreamHeader;
    using Workspace = PositionBlockedHuffman1mWorkspace;
    using StreamDecoder = PositionBlockedHuffman1mStreamDecoder;
    using FormatError = PositionBlockedHuffman1mFormatError;
    using FrameError = PositionBlockedHuffman1mFrameError;
    using TokenError = context::internal::PositionBlockedHuffman1mTokenError;
    static constexpr auto tables = entropy::internal::position_blocked_huffman_1m_tables;
    static constexpr auto encode_working_bytes = position_blocked_huffman_1m_stream_encode_working_bytes;
    static constexpr auto decode_working_bytes = position_blocked_huffman_1m_stream_decode_working_bytes;
    static constexpr auto decode_frame_workspace = position_blocked_huffman_1m_decode_frame_workspace;
    static constexpr auto payload_bits = std::size_t{120};
    static constexpr auto serialized_overhead = std::size_t{64+2547};
    static constexpr auto validate_stream = validate_position_blocked_huffman_1m_stream;
    static constexpr auto serialize_stream = serialize_position_blocked_huffman_1m_stream;
    static constexpr auto encode_frame = encode_position_blocked_huffman_1m_frame;
    static auto finder_workspace(std::size_t size,
        const dictionary::internal::LzssParameters& parameters,
        const core::DecoderLimits& limits,
        dictionary::internal::LzssTypedTokenVariant variant) noexcept {
        return dictionary::internal::calculate_lzss_position_distance_1m_five_prefix_workspace(size,parameters,limits,variant);
    }
    static constexpr auto tokenize = dictionary::internal::tokenize_lzss_position_distance_1m_five_prefix_candidate;
    static constexpr auto token_variant = dictionary::internal::LzssTypedTokenVariant::field_context_1m_short_length_escape;
};
} // namespace

Code PositionBlockedHuffman1mOwnedEncoder::requirements(const PositionBlockedHuffman1mStreamHeader& s,
    const core::DecoderLimits& l,PositionBlockedHuffman1mWorkspace& r) noexcept {
    return PositionBlockedHuffmanOwnedMechanics::encoder_requirements<Profile, PositionBlockedHuffman1mOwnedEncoder>(s,l,r);
}
std::unique_ptr<PositionBlockedHuffman1mOwnedEncoder> PositionBlockedHuffman1mOwnedEncoder::create(
    const PositionBlockedHuffman1mStreamHeader& stream,const core::DecoderLimits& limits,Code& error) noexcept {
    return PositionBlockedHuffmanOwnedMechanics::create_encoder<Profile, PositionBlockedHuffman1mOwnedEncoder>(stream,limits,error);
}
bool PositionBlockedHuffman1mOwnedEncoder::disjoint(std::span<const std::byte> input,std::span<std::byte> output) const noexcept {
    return PositionBlockedHuffmanOwnedMechanics::encoder_disjoint(*this,input,output);
}
core::ProcessResult PositionBlockedHuffman1mOwnedEncoder::fail(Code code,std::size_t consumed,std::size_t produced) noexcept {
    return PositionBlockedHuffmanOwnedMechanics::fail_encoder(*this,code,consumed,produced);
}
core::ProcessResult PositionBlockedHuffman1mOwnedEncoder::process(std::span<const std::byte> input,
    std::span<std::byte> output,std::uint32_t flags) noexcept {
    return PositionBlockedHuffmanOwnedMechanics::encode<Profile>(*this,input,output,flags);
}
Code PositionBlockedHuffman1mOwnedDecoder::requirements(std::uint32_t capacity,const core::DecoderLimits& l,PositionBlockedHuffman1mWorkspace& r) noexcept {
    return PositionBlockedHuffmanOwnedMechanics::decoder_requirements<Profile, PositionBlockedHuffman1mOwnedDecoder>(capacity,l,r);
}
std::unique_ptr<PositionBlockedHuffman1mOwnedDecoder> PositionBlockedHuffman1mOwnedDecoder::create(
    std::uint32_t capacity,const core::DecoderLimits& limits,Code& error) noexcept {
    return PositionBlockedHuffmanOwnedMechanics::create_decoder<Profile, PositionBlockedHuffman1mOwnedDecoder>(capacity,limits,error);
}
core::ProcessResult PositionBlockedHuffman1mOwnedDecoder::process(std::span<const std::byte> input,std::span<std::byte> output,std::uint32_t flags) noexcept {
    return PositionBlockedHuffmanOwnedMechanics::decode(*this,input,output,flags);
}
} // namespace marc::frame::internal

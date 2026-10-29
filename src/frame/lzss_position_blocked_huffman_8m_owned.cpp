#include "frame/lzss_position_blocked_huffman_8m_owned.hpp"
#include "dictionary/lzss_position_distance_8m_five_prefix_candidate.hpp"
#include "core/buffer_overlap.hpp"
#include <algorithm>
#include <cstring>
#include <new>

#include "frame/lzss_position_blocked_huffman_owned_common.hpp"

namespace marc::frame::internal {
namespace {
using Code = core::ErrorCode;
struct Profile {
    using StreamHeader = PositionBlockedHuffman8mStreamHeader;
    using Workspace = PositionBlockedHuffman8mWorkspace;
    using StreamDecoder = PositionBlockedHuffman8mStreamDecoder;
    using FormatError = PositionBlockedHuffman8mFormatError;
    using FrameError = PositionBlockedHuffman8mFrameError;
    using TokenError = context::internal::PositionBlockedHuffman8mTokenError;
    static constexpr auto tables = entropy::internal::position_blocked_huffman_8m_tables;
    static constexpr auto encode_working_bytes = position_blocked_huffman_8m_stream_encode_working_bytes;
    static constexpr auto decode_working_bytes = position_blocked_huffman_8m_stream_decode_working_bytes;
    static constexpr auto decode_frame_workspace = position_blocked_huffman_8m_decode_frame_workspace;
    static constexpr auto payload_bits = std::size_t{135};
    static constexpr auto serialized_overhead = std::size_t{64+2563};
    static constexpr auto validate_stream = validate_position_blocked_huffman_8m_stream;
    static constexpr auto serialize_stream = serialize_position_blocked_huffman_8m_stream;
    static constexpr auto encode_frame = encode_position_blocked_huffman_8m_frame;
    static auto finder_workspace(std::size_t size,
        const dictionary::internal::LzssParameters& parameters,
        const core::DecoderLimits& limits,
        dictionary::internal::LzssTypedTokenVariant variant) noexcept {
        return dictionary::internal::calculate_lzss_position_distance_8m_five_prefix_workspace(size,parameters,limits,variant);
    }
    static constexpr auto tokenize = dictionary::internal::tokenize_lzss_position_distance_8m_five_prefix_candidate;
    static constexpr auto token_variant = dictionary::internal::LzssTypedTokenVariant::field_context_8m_short_length_escape;
};
} // namespace

Code PositionBlockedHuffman8mOwnedEncoder::requirements(const PositionBlockedHuffman8mStreamHeader& s,
    const core::DecoderLimits& l,PositionBlockedHuffman8mWorkspace& r) noexcept {
    return PositionBlockedHuffmanOwnedMechanics::encoder_requirements<Profile, PositionBlockedHuffman8mOwnedEncoder>(s,l,r);
}
std::unique_ptr<PositionBlockedHuffman8mOwnedEncoder> PositionBlockedHuffman8mOwnedEncoder::create(
    const PositionBlockedHuffman8mStreamHeader& stream,const core::DecoderLimits& limits,Code& error) noexcept {
    return PositionBlockedHuffmanOwnedMechanics::create_encoder<Profile, PositionBlockedHuffman8mOwnedEncoder>(stream,limits,error);
}
bool PositionBlockedHuffman8mOwnedEncoder::disjoint(std::span<const std::byte> input,std::span<std::byte> output) const noexcept {
    return PositionBlockedHuffmanOwnedMechanics::encoder_disjoint(*this,input,output);
}
core::ProcessResult PositionBlockedHuffman8mOwnedEncoder::fail(Code code,std::size_t consumed,std::size_t produced) noexcept {
    return PositionBlockedHuffmanOwnedMechanics::fail_encoder(*this,code,consumed,produced);
}
core::ProcessResult PositionBlockedHuffman8mOwnedEncoder::process(std::span<const std::byte> input,
    std::span<std::byte> output,std::uint32_t flags) noexcept {
    return PositionBlockedHuffmanOwnedMechanics::encode<Profile>(*this,input,output,flags);
}
Code PositionBlockedHuffman8mOwnedDecoder::requirements(std::uint32_t capacity,const core::DecoderLimits& l,PositionBlockedHuffman8mWorkspace& r) noexcept {
    return PositionBlockedHuffmanOwnedMechanics::decoder_requirements<Profile, PositionBlockedHuffman8mOwnedDecoder>(capacity,l,r);
}
std::unique_ptr<PositionBlockedHuffman8mOwnedDecoder> PositionBlockedHuffman8mOwnedDecoder::create(
    std::uint32_t capacity,const core::DecoderLimits& limits,Code& error) noexcept {
    return PositionBlockedHuffmanOwnedMechanics::create_decoder<Profile, PositionBlockedHuffman8mOwnedDecoder>(capacity,limits,error);
}
core::ProcessResult PositionBlockedHuffman8mOwnedDecoder::process(std::span<const std::byte> input,std::span<std::byte> output,std::uint32_t flags) noexcept {
    return PositionBlockedHuffmanOwnedMechanics::decode(*this,input,output,flags);
}
} // namespace marc::frame::internal

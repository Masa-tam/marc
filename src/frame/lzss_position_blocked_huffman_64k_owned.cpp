#include "frame/lzss_position_blocked_huffman_64k_owned.hpp"
#include "dictionary/lzss_short_length_escape_candidate.hpp"
#include "core/buffer_overlap.hpp"
#include <algorithm>
#include <cstring>
#include <new>

#include "frame/lzss_position_blocked_huffman_owned_common.hpp"

namespace marc::frame::internal {
namespace {
using Code = core::ErrorCode;
struct Profile {
    using StreamHeader = PositionBlockedHuffman64kStreamHeader;
    using Workspace = PositionBlockedHuffman64kWorkspace;
    using StreamDecoder = PositionBlockedHuffman64kStreamDecoder;
    using FormatError = PositionBlockedHuffman64kFormatError;
    using FrameError = PositionBlockedHuffman64kFrameError;
    using TokenError = context::internal::PositionBlockedHuffman64kTokenError;
    static constexpr auto tables = entropy::internal::position_blocked_huffman_64k_tables;
    static constexpr auto encode_working_bytes = position_blocked_huffman_64k_stream_encode_working_bytes;
    static constexpr auto decode_working_bytes = position_blocked_huffman_64k_stream_decode_working_bytes;
    static constexpr auto decode_frame_workspace = position_blocked_huffman_64k_decode_frame_workspace;
    static constexpr auto payload_bits = std::size_t{105};
    static constexpr auto serialized_overhead = std::size_t{64+2518};
    static constexpr auto validate_stream = validate_position_blocked_huffman_64k_stream;
    static constexpr auto serialize_stream = serialize_position_blocked_huffman_64k_stream;
    static constexpr auto encode_frame = encode_position_blocked_huffman_64k_frame;
    static auto finder_workspace(std::size_t size,
        const dictionary::internal::LzssParameters& parameters,
        const core::DecoderLimits& limits,
        dictionary::internal::LzssTypedTokenVariant variant) noexcept {
        return dictionary::internal::calculate_lzss_short_prefix_workspace(size,parameters,limits,variant);
    }
    static constexpr auto tokenize = dictionary::internal::tokenize_lzss_short_length_escape_candidate_indexed_scratch;
    static constexpr auto token_variant = dictionary::internal::LzssTypedTokenVariant::field_context_64k_short_length_escape;
};
} // namespace

Code PositionBlockedHuffman64kOwnedEncoder::requirements(const PositionBlockedHuffman64kStreamHeader& s,
    const core::DecoderLimits& l,PositionBlockedHuffman64kWorkspace& r) noexcept {
    return PositionBlockedHuffmanOwnedMechanics::encoder_requirements<Profile, PositionBlockedHuffman64kOwnedEncoder>(s,l,r);
}
std::unique_ptr<PositionBlockedHuffman64kOwnedEncoder> PositionBlockedHuffman64kOwnedEncoder::create(
    const PositionBlockedHuffman64kStreamHeader& stream,const core::DecoderLimits& limits,Code& error) noexcept {
    return PositionBlockedHuffmanOwnedMechanics::create_encoder<Profile, PositionBlockedHuffman64kOwnedEncoder>(stream,limits,error);
}
bool PositionBlockedHuffman64kOwnedEncoder::disjoint(std::span<const std::byte> input,std::span<std::byte> output) const noexcept {
    return PositionBlockedHuffmanOwnedMechanics::encoder_disjoint(*this,input,output);
}
core::ProcessResult PositionBlockedHuffman64kOwnedEncoder::fail(Code code,std::size_t consumed,std::size_t produced) noexcept {
    return PositionBlockedHuffmanOwnedMechanics::fail_encoder(*this,code,consumed,produced);
}
core::ProcessResult PositionBlockedHuffman64kOwnedEncoder::process(std::span<const std::byte> input,
    std::span<std::byte> output,std::uint32_t flags) noexcept {
    return PositionBlockedHuffmanOwnedMechanics::encode<Profile>(*this,input,output,flags);
}
Code PositionBlockedHuffman64kOwnedDecoder::requirements(std::uint32_t capacity,const core::DecoderLimits& l,PositionBlockedHuffman64kWorkspace& r) noexcept {
    return PositionBlockedHuffmanOwnedMechanics::decoder_requirements<Profile, PositionBlockedHuffman64kOwnedDecoder>(capacity,l,r);
}
std::unique_ptr<PositionBlockedHuffman64kOwnedDecoder> PositionBlockedHuffman64kOwnedDecoder::create(
    std::uint32_t capacity,const core::DecoderLimits& limits,Code& error) noexcept {
    return PositionBlockedHuffmanOwnedMechanics::create_decoder<Profile, PositionBlockedHuffman64kOwnedDecoder>(capacity,limits,error);
}
core::ProcessResult PositionBlockedHuffman64kOwnedDecoder::process(std::span<const std::byte> input,std::span<std::byte> output,std::uint32_t flags) noexcept {
    return PositionBlockedHuffmanOwnedMechanics::decode(*this,input,output,flags);
}
} // namespace marc::frame::internal

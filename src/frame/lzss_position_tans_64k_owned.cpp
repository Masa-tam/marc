#include "frame/lzss_position_tans_64k_owned.hpp"
#include "dictionary/lzss_short_length_escape_candidate.hpp"
#include "core/buffer_overlap.hpp"
#include <algorithm>
#include <cstring>
#include <new>

#include "frame/lzss_position_tans_owned_common.hpp"

namespace marc::frame::internal {
namespace {
using Code=core::ErrorCode;
struct Profile {
    using StreamHeader=PositionTans64kStreamHeader;
    using Workspace=PositionTans64kWorkspace;
    using StreamDecoder=PositionTans64kStreamDecoder;
    using FormatError=PositionTans64kFormatError;
    using FrameError=PositionTans64kFrameError;
    using TokenError=context::internal::PositionTans64kTokenError;
    using TokenContext=context::internal::PositionTans64kTokenContext;
    using Descriptor=entropy::internal::PositionTans64kDescriptor;
    static constexpr auto validate_stream=validate_position_tans_64k_stream;
    static constexpr auto serialize_stream=serialize_position_tans_64k_stream;
    static constexpr auto encode_frame=encode_position_tans_64k_frame;
    static constexpr auto table_entries=entropy::internal::position_tans_64k_table_entries;
    static constexpr auto payload_bits=std::size_t{84};
    static constexpr auto serialized_overhead=std::size_t{64+9122};
    static constexpr auto tokenize=dictionary::internal::tokenize_lzss_short_length_escape_candidate_indexed_scratch;
    static auto finder_workspace(std::size_t size,const dictionary::internal::LzssParameters& parameters,const core::DecoderLimits& limits) noexcept {
        return dictionary::internal::calculate_lzss_short_prefix_workspace(size,parameters,limits,dictionary::internal::LzssTypedTokenVariant::field_context_64k_short_length_escape);
    }
};
} // namespace
Code PositionTans64kOwnedEncoder::requirements(const PositionTans64kStreamHeader& s,
    const core::DecoderLimits& l,PositionTans64kWorkspace& r) noexcept {
    return PositionTansOwnedMechanics::encoder_requirements<Profile, PositionTans64kOwnedEncoder>(s,l,r);
}
std::unique_ptr<PositionTans64kOwnedEncoder> PositionTans64kOwnedEncoder::create(
    const PositionTans64kStreamHeader& stream,const core::DecoderLimits& limits,Code& error) noexcept {
    return PositionTansOwnedMechanics::create_encoder<Profile, PositionTans64kOwnedEncoder>(stream,limits,error);
}
bool PositionTans64kOwnedEncoder::disjoint(std::span<const std::byte> input,std::span<std::byte> output) const noexcept {
    return PositionTansOwnedMechanics::encoder_disjoint(*this,input,output);
}
core::ProcessResult PositionTans64kOwnedEncoder::fail(Code code,std::size_t consumed,std::size_t produced) noexcept {
    return PositionTansOwnedMechanics::fail_encoder(*this,code,consumed,produced);
}
core::ProcessResult PositionTans64kOwnedEncoder::process(std::span<const std::byte> input,
    std::span<std::byte> output,std::uint32_t flags) noexcept {
    return PositionTansOwnedMechanics::encode<Profile>(*this,input,output,flags);
}
Code PositionTans64kOwnedDecoder::requirements(std::uint32_t capacity,const core::DecoderLimits& l,PositionTans64kWorkspace& r) noexcept {
    return PositionTansOwnedMechanics::decoder_requirements<Profile, PositionTans64kOwnedDecoder>(capacity,l,r);
}
std::unique_ptr<PositionTans64kOwnedDecoder> PositionTans64kOwnedDecoder::create(
    std::uint32_t capacity,const core::DecoderLimits& limits,Code& error) noexcept {
    return PositionTansOwnedMechanics::create_decoder<Profile, PositionTans64kOwnedDecoder>(capacity,limits,error);
}
core::ProcessResult PositionTans64kOwnedDecoder::process(std::span<const std::byte> input,std::span<std::byte> output,std::uint32_t flags) noexcept {
    return PositionTansOwnedMechanics::decode(*this,input,output,flags);
}
} // namespace marc::frame::internal

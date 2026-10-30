#include "frame/lzss_position_rans_64k_owned.hpp"
#include "dictionary/lzss_short_length_escape_candidate.hpp"
#include "core/buffer_overlap.hpp"
#include <algorithm>
#include <cstring>
#include <new>

#include "frame/lzss_position_rans_owned_common.hpp"

namespace marc::frame::internal {
namespace {
using Code=core::ErrorCode;
struct Profile {
    using StreamHeader=PositionRans64kStreamHeader;
    using Workspace=PositionRans64kWorkspace;
    using StreamDecoder=PositionRans64kStreamDecoder;
    using FormatError=PositionRans64kFormatError;
    using FrameError=PositionRans64kFrameError;
    using TokenError=context::internal::LzssPositionRans64kTokenError;
    static constexpr auto validate_stream=validate_position_rans_64k_stream;
    static constexpr auto serialize_stream=serialize_position_rans_64k_stream;
    static constexpr auto encode_frame=encode_position_rans_64k_frame;
    static constexpr auto fixed_working_bytes=context::internal::lzss_position_rans_64k_fixed_working_bytes;
    static constexpr auto serialized_factor=std::size_t{18};
    static constexpr auto serialized_overhead=std::size_t{5098};
    static constexpr auto tokenize=dictionary::internal::tokenize_lzss_short_length_escape_candidate_indexed_scratch;
    static auto finder_workspace(std::size_t size,const dictionary::internal::LzssParameters& parameters,const core::DecoderLimits& limits) noexcept {
        return dictionary::internal::calculate_lzss_short_prefix_workspace(size,parameters,limits,dictionary::internal::LzssTypedTokenVariant::field_context_64k_short_length_escape);
    }
};
} // namespace
Code PositionRans64kOwnedEncoder::requirements(const PositionRans64kStreamHeader& s,
    const core::DecoderLimits& l,PositionRans64kWorkspace& r) noexcept {
    return PositionRansOwnedMechanics::encoder_requirements<Profile, PositionRans64kOwnedEncoder>(s,l,r);
}
std::unique_ptr<PositionRans64kOwnedEncoder> PositionRans64kOwnedEncoder::create(
    const PositionRans64kStreamHeader& stream,const core::DecoderLimits& limits,Code& error,std::uint32_t eligibility) noexcept {
    return PositionRansOwnedMechanics::create_encoder<Profile, PositionRans64kOwnedEncoder>(stream,limits,error,eligibility);
}
bool PositionRans64kOwnedEncoder::disjoint(std::span<const std::byte> input,std::span<std::byte> output) const noexcept {
    return PositionRansOwnedMechanics::encoder_disjoint(*this,input,output);
}
core::ProcessResult PositionRans64kOwnedEncoder::fail(Code code,std::size_t consumed,std::size_t produced) noexcept {
    return PositionRansOwnedMechanics::fail_encoder(*this,code,consumed,produced);
}
core::ProcessResult PositionRans64kOwnedEncoder::process(std::span<const std::byte> input,
    std::span<std::byte> output,std::uint32_t flags) noexcept {
    return PositionRansOwnedMechanics::encode<Profile>(*this,input,output,flags);
}
Code PositionRans64kOwnedDecoder::requirements(std::uint32_t capacity,const core::DecoderLimits& l,PositionRans64kWorkspace& r) noexcept {
    return PositionRansOwnedMechanics::decoder_requirements<Profile, PositionRans64kOwnedDecoder>(capacity,l,r);
}
std::unique_ptr<PositionRans64kOwnedDecoder> PositionRans64kOwnedDecoder::create(
    std::uint32_t capacity,const core::DecoderLimits& limits,Code& error) noexcept {
    return PositionRansOwnedMechanics::create_decoder<Profile, PositionRans64kOwnedDecoder>(capacity,limits,error);
}
core::ProcessResult PositionRans64kOwnedDecoder::process(std::span<const std::byte> input,std::span<std::byte> output,std::uint32_t flags) noexcept {
    return PositionRansOwnedMechanics::decode(*this,input,output,flags);
}
} // namespace marc::frame::internal

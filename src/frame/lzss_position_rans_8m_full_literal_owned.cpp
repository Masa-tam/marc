#include "frame/lzss_position_rans_8m_full_literal_owned.hpp"
#include "dictionary/lzss_position_distance_8m_five_prefix_candidate.hpp"
#include "core/buffer_overlap.hpp"
#include <algorithm>
#include <cstring>
#include <new>

#include "frame/lzss_position_rans_owned_common.hpp"

namespace marc::frame::internal {
namespace {
using Code=core::ErrorCode;
struct Profile {
    using StreamHeader=PositionRans8mFullLiteralStreamHeader;
    using Workspace=PositionRans8mFullLiteralWorkspace;
    using StreamDecoder=PositionRans8mFullLiteralStreamDecoder;
    using FormatError=PositionRans8mFullLiteralFormatError;
    using FrameError=PositionRans8mFullLiteralFrameError;
    using TokenError=context::internal::LzssPositionRans8mFullLiteralTokenError;
    static constexpr auto validate_stream=validate_position_rans_8m_full_literal_stream;
    static constexpr auto serialize_stream=serialize_position_rans_8m_full_literal_stream;
    static constexpr auto encode_frame=encode_position_rans_8m_full_literal_frame;
    static constexpr auto fixed_working_bytes=context::internal::lzss_position_rans_8m_full_literal_fixed_working_bytes;
    static constexpr auto serialized_factor=std::size_t{18};
    static constexpr auto serialized_overhead=std::size_t{9334};
    static constexpr auto tokenize=dictionary::internal::tokenize_lzss_position_distance_8m_five_prefix_candidate;
    static auto finder_workspace(std::size_t size,const dictionary::internal::LzssParameters& parameters,const core::DecoderLimits& limits) noexcept {
        return dictionary::internal::calculate_lzss_position_distance_8m_five_prefix_workspace(size,parameters,limits);
    }
};
} // namespace
Code PositionRans8mFullLiteralOwnedEncoder::requirements(const PositionRans8mFullLiteralStreamHeader& s,
    const core::DecoderLimits& l,PositionRans8mFullLiteralWorkspace& r) noexcept {
    return PositionRansOwnedMechanics::encoder_requirements<Profile, PositionRans8mFullLiteralOwnedEncoder>(s,l,r);
}
std::unique_ptr<PositionRans8mFullLiteralOwnedEncoder> PositionRans8mFullLiteralOwnedEncoder::create(
    const PositionRans8mFullLiteralStreamHeader& stream,const core::DecoderLimits& limits,Code& error,std::uint32_t eligibility) noexcept {
    return PositionRansOwnedMechanics::create_encoder<Profile, PositionRans8mFullLiteralOwnedEncoder>(stream,limits,error,eligibility);
}
bool PositionRans8mFullLiteralOwnedEncoder::disjoint(std::span<const std::byte> input,std::span<std::byte> output) const noexcept {
    return PositionRansOwnedMechanics::encoder_disjoint(*this,input,output);
}
core::ProcessResult PositionRans8mFullLiteralOwnedEncoder::fail(Code code,std::size_t consumed,std::size_t produced) noexcept {
    return PositionRansOwnedMechanics::fail_encoder(*this,code,consumed,produced);
}
core::ProcessResult PositionRans8mFullLiteralOwnedEncoder::process(std::span<const std::byte> input,
    std::span<std::byte> output,std::uint32_t flags) noexcept {
    return PositionRansOwnedMechanics::encode<Profile>(*this,input,output,flags);
}
Code PositionRans8mFullLiteralOwnedDecoder::requirements(std::uint32_t capacity,const core::DecoderLimits& l,PositionRans8mFullLiteralWorkspace& r) noexcept {
    return PositionRansOwnedMechanics::decoder_requirements<Profile, PositionRans8mFullLiteralOwnedDecoder>(capacity,l,r);
}
std::unique_ptr<PositionRans8mFullLiteralOwnedDecoder> PositionRans8mFullLiteralOwnedDecoder::create(
    std::uint32_t capacity,const core::DecoderLimits& limits,Code& error) noexcept {
    return PositionRansOwnedMechanics::create_decoder<Profile, PositionRans8mFullLiteralOwnedDecoder>(capacity,limits,error);
}
core::ProcessResult PositionRans8mFullLiteralOwnedDecoder::process(std::span<const std::byte> input,std::span<std::byte> output,std::uint32_t flags) noexcept {
    return PositionRansOwnedMechanics::decode(*this,input,output,flags);
}
} // namespace marc::frame::internal

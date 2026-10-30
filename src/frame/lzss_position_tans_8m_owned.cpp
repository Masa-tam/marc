#include "frame/lzss_position_tans_8m_owned.hpp"
#include "dictionary/lzss_position_distance_8m_five_prefix_candidate.hpp"
#include "core/buffer_overlap.hpp"
#include <algorithm>
#include <cstring>
#include <new>

#include "frame/lzss_position_tans_owned_common.hpp"

namespace marc::frame::internal {
namespace {
using Code=core::ErrorCode;
struct Profile {
    using StreamHeader=PositionTans8mStreamHeader;
    using Workspace=PositionTans8mWorkspace;
    using StreamDecoder=PositionTans8mStreamDecoder;
    using FormatError=PositionTans8mFormatError;
    using FrameError=PositionTans8mFrameError;
    using TokenError=context::internal::PositionTans8mTokenError;
    using TokenContext=context::internal::PositionTans8mTokenContext;
    using Descriptor=entropy::internal::PositionTans8mDescriptor;
    static constexpr auto validate_stream=validate_position_tans_8m_stream;
    static constexpr auto serialize_stream=serialize_position_tans_8m_stream;
    static constexpr auto encode_frame=encode_position_tans_8m_frame;
    static constexpr auto table_entries=entropy::internal::position_tans_8m_table_entries;
    static constexpr auto payload_bits=std::size_t{96};
    static constexpr auto serialized_overhead=std::size_t{64+9270};
    static constexpr auto tokenize=dictionary::internal::tokenize_lzss_position_distance_8m_five_prefix_candidate;
    static auto finder_workspace(std::size_t size,const dictionary::internal::LzssParameters& parameters,const core::DecoderLimits& limits) noexcept {
        return dictionary::internal::calculate_lzss_position_distance_8m_five_prefix_workspace(size,parameters,limits);
    }
};
} // namespace
Code PositionTans8mOwnedEncoder::requirements(const PositionTans8mStreamHeader& s,
    const core::DecoderLimits& l,PositionTans8mWorkspace& r) noexcept {
    return PositionTansOwnedMechanics::encoder_requirements<Profile, PositionTans8mOwnedEncoder>(s,l,r);
}
std::unique_ptr<PositionTans8mOwnedEncoder> PositionTans8mOwnedEncoder::create(
    const PositionTans8mStreamHeader& stream,const core::DecoderLimits& limits,Code& error) noexcept {
    return PositionTansOwnedMechanics::create_encoder<Profile, PositionTans8mOwnedEncoder>(stream,limits,error);
}
bool PositionTans8mOwnedEncoder::disjoint(std::span<const std::byte> input,std::span<std::byte> output) const noexcept {
    return PositionTansOwnedMechanics::encoder_disjoint(*this,input,output);
}
core::ProcessResult PositionTans8mOwnedEncoder::fail(Code code,std::size_t consumed,std::size_t produced) noexcept {
    return PositionTansOwnedMechanics::fail_encoder(*this,code,consumed,produced);
}
core::ProcessResult PositionTans8mOwnedEncoder::process(std::span<const std::byte> input,
    std::span<std::byte> output,std::uint32_t flags) noexcept {
    return PositionTansOwnedMechanics::encode<Profile>(*this,input,output,flags);
}
Code PositionTans8mOwnedDecoder::requirements(std::uint32_t capacity,const core::DecoderLimits& l,PositionTans8mWorkspace& r) noexcept {
    return PositionTansOwnedMechanics::decoder_requirements<Profile, PositionTans8mOwnedDecoder>(capacity,l,r);
}
std::unique_ptr<PositionTans8mOwnedDecoder> PositionTans8mOwnedDecoder::create(
    std::uint32_t capacity,const core::DecoderLimits& limits,Code& error) noexcept {
    return PositionTansOwnedMechanics::create_decoder<Profile, PositionTans8mOwnedDecoder>(capacity,limits,error);
}
core::ProcessResult PositionTans8mOwnedDecoder::process(std::span<const std::byte> input,std::span<std::byte> output,std::uint32_t flags) noexcept {
    return PositionTansOwnedMechanics::decode(*this,input,output,flags);
}
} // namespace marc::frame::internal

#include "frame/lzss_position_tans_4m_owned.hpp"
#include "dictionary/lzss_position_distance_4m_five_prefix_candidate.hpp"
#include "core/buffer_overlap.hpp"
#include <algorithm>
#include <cstring>
#include <new>

#include "frame/lzss_position_tans_owned_common.hpp"

namespace marc::frame::internal {
namespace {
using Code=core::ErrorCode;
struct Profile {
    using StreamHeader=PositionTans4mStreamHeader;
    using Workspace=PositionTans4mWorkspace;
    using StreamDecoder=PositionTans4mStreamDecoder;
    using FormatError=PositionTans4mFormatError;
    using FrameError=PositionTans4mFrameError;
    using TokenError=context::internal::PositionTans4mTokenError;
    using TokenContext=context::internal::PositionTans4mTokenContext;
    using Descriptor=entropy::internal::PositionTans4mDescriptor;
    static constexpr auto validate_stream=validate_position_tans_4m_stream;
    static constexpr auto serialize_stream=serialize_position_tans_4m_stream;
    static constexpr auto encode_frame=encode_position_tans_4m_frame;
    static constexpr auto table_entries=entropy::internal::position_tans_4m_table_entries;
    static constexpr auto payload_bits=std::size_t{96};
    static constexpr auto serialized_overhead=std::size_t{64+9249};
    static constexpr auto tokenize=dictionary::internal::tokenize_lzss_position_distance_4m_five_prefix_candidate;
    static auto finder_workspace(std::size_t size,const dictionary::internal::LzssParameters& parameters,const core::DecoderLimits& limits) noexcept {
        return dictionary::internal::calculate_lzss_position_distance_4m_five_prefix_workspace(size,parameters,limits);
    }
};
} // namespace
Code PositionTans4mOwnedEncoder::requirements(const PositionTans4mStreamHeader& s,
    const core::DecoderLimits& l,PositionTans4mWorkspace& r) noexcept {
    return PositionTansOwnedMechanics::encoder_requirements<Profile, PositionTans4mOwnedEncoder>(s,l,r);
}
std::unique_ptr<PositionTans4mOwnedEncoder> PositionTans4mOwnedEncoder::create(
    const PositionTans4mStreamHeader& stream,const core::DecoderLimits& limits,Code& error) noexcept {
    return PositionTansOwnedMechanics::create_encoder<Profile, PositionTans4mOwnedEncoder>(stream,limits,error);
}
bool PositionTans4mOwnedEncoder::disjoint(std::span<const std::byte> input,std::span<std::byte> output) const noexcept {
    return PositionTansOwnedMechanics::encoder_disjoint(*this,input,output);
}
core::ProcessResult PositionTans4mOwnedEncoder::fail(Code code,std::size_t consumed,std::size_t produced) noexcept {
    return PositionTansOwnedMechanics::fail_encoder(*this,code,consumed,produced);
}
core::ProcessResult PositionTans4mOwnedEncoder::process(std::span<const std::byte> input,
    std::span<std::byte> output,std::uint32_t flags) noexcept {
    return PositionTansOwnedMechanics::encode<Profile>(*this,input,output,flags);
}
Code PositionTans4mOwnedDecoder::requirements(std::uint32_t capacity,const core::DecoderLimits& l,PositionTans4mWorkspace& r) noexcept {
    return PositionTansOwnedMechanics::decoder_requirements<Profile, PositionTans4mOwnedDecoder>(capacity,l,r);
}
std::unique_ptr<PositionTans4mOwnedDecoder> PositionTans4mOwnedDecoder::create(
    std::uint32_t capacity,const core::DecoderLimits& limits,Code& error) noexcept {
    return PositionTansOwnedMechanics::create_decoder<Profile, PositionTans4mOwnedDecoder>(capacity,limits,error);
}
core::ProcessResult PositionTans4mOwnedDecoder::process(std::span<const std::byte> input,std::span<std::byte> output,std::uint32_t flags) noexcept {
    return PositionTansOwnedMechanics::decode(*this,input,output,flags);
}
} // namespace marc::frame::internal

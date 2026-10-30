#include "frame/lzss_position_tans_32m_owned.hpp"
#include "dictionary/lzss_position_distance_32m_five_prefix_candidate.hpp"
#include "core/buffer_overlap.hpp"
#include <algorithm>
#include <cstring>
#include <new>

#include "frame/lzss_position_tans_owned_common.hpp"

namespace marc::frame::internal {
namespace {
using Code=core::ErrorCode;
struct Profile {
    using StreamHeader=PositionTans32mStreamHeader;
    using Workspace=PositionTans32mWorkspace;
    using StreamDecoder=PositionTans32mStreamDecoder;
    using FormatError=PositionTans32mFormatError;
    using FrameError=PositionTans32mFrameError;
    using TokenError=context::internal::PositionTans32mTokenError;
    using TokenContext=context::internal::PositionTans32mTokenContext;
    using Descriptor=entropy::internal::PositionTans32mDescriptor;
    static constexpr auto validate_stream=validate_position_tans_32m_stream;
    static constexpr auto serialize_stream=serialize_position_tans_32m_stream;
    static constexpr auto encode_frame=encode_position_tans_32m_frame;
    static constexpr auto table_entries=entropy::internal::position_tans_32m_table_entries;
    static constexpr auto payload_bits=std::size_t{96};
    static constexpr auto serialized_overhead=std::size_t{64+9313};
    static constexpr auto tokenize=dictionary::internal::tokenize_lzss_position_distance_32m_five_prefix_candidate;
    static auto finder_workspace(std::size_t size,const dictionary::internal::LzssParameters& parameters,const core::DecoderLimits& limits) noexcept {
        return dictionary::internal::calculate_lzss_position_distance_32m_five_prefix_workspace(size,parameters,limits);
    }
};
} // namespace
Code PositionTans32mOwnedEncoder::requirements(const PositionTans32mStreamHeader& s,
    const core::DecoderLimits& l,PositionTans32mWorkspace& r) noexcept {
    return PositionTansOwnedMechanics::encoder_requirements<Profile, PositionTans32mOwnedEncoder>(s,l,r);
}
std::unique_ptr<PositionTans32mOwnedEncoder> PositionTans32mOwnedEncoder::create(
    const PositionTans32mStreamHeader& stream,const core::DecoderLimits& limits,Code& error) noexcept {
    return PositionTansOwnedMechanics::create_encoder<Profile, PositionTans32mOwnedEncoder>(stream,limits,error);
}
bool PositionTans32mOwnedEncoder::disjoint(std::span<const std::byte> input,std::span<std::byte> output) const noexcept {
    return PositionTansOwnedMechanics::encoder_disjoint(*this,input,output);
}
core::ProcessResult PositionTans32mOwnedEncoder::fail(Code code,std::size_t consumed,std::size_t produced) noexcept {
    return PositionTansOwnedMechanics::fail_encoder(*this,code,consumed,produced);
}
core::ProcessResult PositionTans32mOwnedEncoder::process(std::span<const std::byte> input,
    std::span<std::byte> output,std::uint32_t flags) noexcept {
    return PositionTansOwnedMechanics::encode<Profile>(*this,input,output,flags);
}
Code PositionTans32mOwnedDecoder::requirements(std::uint32_t capacity,const core::DecoderLimits& l,PositionTans32mWorkspace& r) noexcept {
    return PositionTansOwnedMechanics::decoder_requirements<Profile, PositionTans32mOwnedDecoder>(capacity,l,r);
}
std::unique_ptr<PositionTans32mOwnedDecoder> PositionTans32mOwnedDecoder::create(
    std::uint32_t capacity,const core::DecoderLimits& limits,Code& error) noexcept {
    return PositionTansOwnedMechanics::create_decoder<Profile, PositionTans32mOwnedDecoder>(capacity,limits,error);
}
core::ProcessResult PositionTans32mOwnedDecoder::process(std::span<const std::byte> input,std::span<std::byte> output,std::uint32_t flags) noexcept {
    return PositionTansOwnedMechanics::decode(*this,input,output,flags);
}
} // namespace marc::frame::internal

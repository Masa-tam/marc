#include "frame/lzss_position_rans_1m_stream_decoder.hpp"
#include "frame/lzss_position_rans_stream_decoder_common.hpp"

namespace marc::frame::internal {
namespace {
struct Profile {
    using FormatError=PositionRans1mFormatError;
    using FrameError=PositionRans1mFrameError;
    static constexpr auto parse_stream=parse_position_rans_1m_stream;
    static constexpr auto parse_frame_header=parse_position_rans_1m_frame_header;
    static constexpr auto preflight_prefix=preflight_position_rans_1m_frame_prefix;
    static constexpr auto decode_frame=decode_position_rans_1m_frame_scratch;
    static constexpr auto fixed_working_bytes=context::internal::lzss_position_rans_1m_fixed_working_bytes;
};
} // namespace
bool PositionRans1mStreamDecoder::disjoint(std::span<const std::byte> input,std::span<std::byte> output) const noexcept {return PositionRansStreamMechanics::disjoint(*this,input,output);}
PositionRans1mStreamDecoder::PositionRans1mStreamDecoder(core::DecoderLimits limits,
    std::span<std::byte> serialized,std::span<dictionary::internal::LzssTypedToken> tokens,
    std::span<std::byte> raw,std::size_t additional_owner_bytes) noexcept
    : limits_(limits),serialized_(serialized),raw_(raw),tokens_(tokens) {PositionRansStreamMechanics::initialize<Profile>(*this,additional_owner_bytes);}
core::ProcessResult PositionRans1mStreamDecoder::fail(core::ErrorCode code,std::uint64_t position,
    std::size_t consumed,std::size_t produced) noexcept {return PositionRansStreamMechanics::fail(*this,code,position,consumed,produced);}
core::ProcessResult PositionRans1mStreamDecoder::process(std::span<const std::byte> input,
    std::span<std::byte> output,std::uint32_t flags) noexcept {return PositionRansStreamMechanics::process<Profile>(*this,input,output,flags);}
} // namespace marc::frame::internal

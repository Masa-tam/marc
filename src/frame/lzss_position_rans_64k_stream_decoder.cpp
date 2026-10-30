#include "frame/lzss_position_rans_64k_stream_decoder.hpp"
#include "frame/lzss_position_rans_stream_decoder_common.hpp"

namespace marc::frame::internal {
namespace {
struct Profile {
    using FormatError=PositionRans64kFormatError;
    using FrameError=PositionRans64kFrameError;
    static constexpr auto parse_stream=parse_position_rans_64k_stream;
    static constexpr auto parse_frame_header=parse_position_rans_64k_frame_header;
    static constexpr auto preflight_prefix=preflight_position_rans_64k_frame_prefix;
    static constexpr auto decode_frame=decode_position_rans_64k_frame_scratch;
    static constexpr auto fixed_working_bytes=context::internal::lzss_position_rans_64k_fixed_working_bytes;
};
} // namespace
bool PositionRans64kStreamDecoder::disjoint(std::span<const std::byte> input,std::span<std::byte> output) const noexcept {return PositionRansStreamMechanics::disjoint(*this,input,output);}
PositionRans64kStreamDecoder::PositionRans64kStreamDecoder(core::DecoderLimits limits,
    std::span<std::byte> serialized,std::span<dictionary::internal::LzssTypedToken> tokens,
    std::span<std::byte> raw,std::size_t additional_owner_bytes) noexcept
    : limits_(limits),serialized_(serialized),raw_(raw),tokens_(tokens) {PositionRansStreamMechanics::initialize<Profile>(*this,additional_owner_bytes);}
core::ProcessResult PositionRans64kStreamDecoder::fail(core::ErrorCode code,std::uint64_t position,
    std::size_t consumed,std::size_t produced) noexcept {return PositionRansStreamMechanics::fail(*this,code,position,consumed,produced);}
core::ProcessResult PositionRans64kStreamDecoder::process(std::span<const std::byte> input,
    std::span<std::byte> output,std::uint32_t flags) noexcept {return PositionRansStreamMechanics::process<Profile>(*this,input,output,flags);}
} // namespace marc::frame::internal

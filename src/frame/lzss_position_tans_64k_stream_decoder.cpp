#include "frame/lzss_position_tans_64k_stream_decoder.hpp"
#include "frame/lzss_position_tans_stream_decoder_common.hpp"

namespace marc::frame::internal {
namespace {
struct Profile {
    using FormatError=PositionTans64kFormatError;
    using FrameError=PositionTans64kFrameError;
    using TokenError=context::internal::PositionTans64kTokenError;
    using Descriptor=entropy::internal::PositionTans64kDescriptor;
    using TokenContext=context::internal::PositionTans64kTokenContext;
    static constexpr auto parse_stream=parse_position_tans_64k_stream;
    static constexpr auto parse_frame_header=parse_position_tans_64k_frame_header;
    static constexpr auto preflight_prefix=preflight_position_tans_64k_frame_prefix;
    static constexpr auto decode_frame=decode_position_tans_64k_frame_scratch;
    static constexpr auto table_entries=entropy::internal::position_tans_64k_table_entries;
};
} // namespace
bool PositionTans64kStreamDecoder::disjoint(std::span<const std::byte> input,std::span<std::byte> output) const noexcept {
    return PositionTansStreamMechanics::disjoint(*this,input,output);
}
PositionTans64kStreamDecoder::PositionTans64kStreamDecoder(core::DecoderLimits limits,
    std::span<std::byte> serialized,std::span<dictionary::internal::LzssTypedToken> tokens,
    std::span<std::byte> raw,std::span<entropy::internal::TansDecodeEntry> tables,std::size_t additional_owner_bytes) noexcept
    : limits_(limits),serialized_(serialized),raw_(raw),tokens_(tokens),tables_(tables) {
    PositionTansStreamMechanics::initialize<Profile>(*this,additional_owner_bytes);
}
core::ProcessResult PositionTans64kStreamDecoder::fail(core::ErrorCode code,std::uint64_t position,
    std::size_t consumed,std::size_t produced) noexcept {
    return PositionTansStreamMechanics::fail(*this,code,position,consumed,produced);
}
core::ProcessResult PositionTans64kStreamDecoder::process(std::span<const std::byte> input,
    std::span<std::byte> output,std::uint32_t flags) noexcept {
    return PositionTansStreamMechanics::process<Profile>(*this,input,output,flags);
}
} // namespace marc::frame::internal

#ifndef MARC_FRAME_LZSS_POSITION_TANS_4M_STREAM_DECODER_HPP
#define MARC_FRAME_LZSS_POSITION_TANS_4M_STREAM_DECODER_HPP
#include "frame/lzss_position_tans_8m_frame.hpp"
#include "core/status.hpp"

namespace marc::frame::internal {
// Caller owns bounded scratch. Failed frames never enter the draining state.
class PositionTans8mStreamDecoder final : public core::Transform {
public:
    PositionTans8mStreamDecoder(core::DecoderLimits limits,std::span<std::byte> serialized,
        std::span<dictionary::internal::LzssTypedToken> tokens,std::span<std::byte> raw,
        std::span<entropy::internal::TansDecodeEntry> private_tables,
        std::size_t additional_owner_bytes=0) noexcept;
    PositionTans8mStreamDecoder(const PositionTans8mStreamDecoder&)=delete;
    PositionTans8mStreamDecoder& operator=(const PositionTans8mStreamDecoder&)=delete;
    [[nodiscard]] core::ProcessResult process(std::span<const std::byte> input,
        std::span<std::byte> output,std::uint32_t flags) noexcept override;
private:
    enum class State { stream_header,frame_header,descriptor,payload,draining,awaiting_end,ended,error };
    [[nodiscard]] bool disjoint(std::span<const std::byte> input,std::span<std::byte> output) const noexcept;
    [[nodiscard]] core::ProcessResult fail(core::ErrorCode code,std::uint64_t position,
        std::size_t consumed,std::size_t produced) noexcept;
    core::DecoderLimits limits_{};
    std::span<std::byte> serialized_{},raw_{};
    std::span<dictionary::internal::LzssTypedToken> tokens_{};
    std::span<entropy::internal::TansDecodeEntry> tables_{};
    std::size_t token_bytes_{},table_bytes_{};
    std::array<std::byte,112> header_{};
    PositionTans8mStreamHeader stream_{};
    PositionTans8mFrameLayout layout_{};
    std::size_t collected_{},drained_{};
    std::uint64_t position_{},frame_start_{},raw_validated_{},sequence_{};
    State state_{State::stream_header};
    core::StreamError error_{};
    bool end_seen_{};
};
} // namespace marc::frame::internal
#endif




#ifndef MARC_FRAME_LZSS_POSITION_TANS_64M_STREAM_DECODER_HPP
#define MARC_FRAME_LZSS_POSITION_TANS_64M_STREAM_DECODER_HPP
#include "frame/lzss_position_tans_64m_frame.hpp"
#include "core/status.hpp"

namespace marc::frame::internal {
struct PositionTansStreamMechanics;
// Caller owns bounded scratch. Failed frames never enter the draining state.
class PositionTans64mStreamDecoder final : public core::Transform {
public:
    PositionTans64mStreamDecoder(core::DecoderLimits limits,std::span<std::byte> serialized,
        std::span<dictionary::internal::LzssTypedToken> tokens,std::span<std::byte> raw,
        std::span<entropy::internal::TansDecodeEntry> private_tables,
        std::size_t additional_owner_bytes=0) noexcept;
    PositionTans64mStreamDecoder(const PositionTans64mStreamDecoder&)=delete;
    PositionTans64mStreamDecoder& operator=(const PositionTans64mStreamDecoder&)=delete;
    [[nodiscard]] core::ProcessResult process(std::span<const std::byte> input,
        std::span<std::byte> output,std::uint32_t flags) noexcept override;
private:
    friend struct PositionTansStreamMechanics;
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
    PositionTans64mStreamHeader stream_{};
    PositionTans64mFrameLayout layout_{};
    std::size_t collected_{},drained_{};
    std::uint64_t position_{},frame_start_{},raw_validated_{},sequence_{};
    State state_{State::stream_header};
    core::StreamError error_{};
    bool end_seen_{};
};
} // namespace marc::frame::internal
#endif

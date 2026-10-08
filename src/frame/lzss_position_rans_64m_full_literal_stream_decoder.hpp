#ifndef MARC_FRAME_LZSS_POSITION_RANS_64M_FULL_LITERAL_STREAM_DECODER_HPP
#define MARC_FRAME_LZSS_POSITION_RANS_64M_FULL_LITERAL_STREAM_DECODER_HPP
#include "frame/lzss_position_rans_64m_full_literal_frame.hpp"
#include "core/status.hpp"

namespace marc::frame::internal {
class PositionRans64mFullLiteralStreamDecoder final : public core::Transform {
public:
    PositionRans64mFullLiteralStreamDecoder(core::DecoderLimits limits,std::span<std::byte> serialized,
        std::span<dictionary::internal::LzssTypedToken> tokens,std::span<std::byte> raw,
        std::size_t additional_owner_bytes=0) noexcept;
    PositionRans64mFullLiteralStreamDecoder(const PositionRans64mFullLiteralStreamDecoder&)=delete;
    PositionRans64mFullLiteralStreamDecoder& operator=(const PositionRans64mFullLiteralStreamDecoder&)=delete;
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
    std::size_t token_bytes_{};
    std::array<std::byte,112> header_{};
    PositionRans64mFullLiteralStreamHeader stream_{};
    PositionRans64mFullLiteralFrameLayout layout_{};
    std::size_t collected_{},drained_{};
    std::uint64_t position_{},frame_start_{},raw_validated_{},sequence_{};
    State state_{State::stream_header};
    core::StreamError error_{};
    bool end_seen_{};
};
} // namespace marc::frame::internal
#endif

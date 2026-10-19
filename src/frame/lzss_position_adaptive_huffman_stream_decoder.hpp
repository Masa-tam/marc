#ifndef MARC_FRAME_LZSS_POSITION_ADAPTIVE_HUFFMAN_STREAM_DECODER_HPP
#define MARC_FRAME_LZSS_POSITION_ADAPTIVE_HUFFMAN_STREAM_DECODER_HPP
#include "frame/lzss_position_adaptive_huffman_frame.hpp"
#include "core/status.hpp"

namespace marc::frame::internal {
// Logical nested controls plus conservative stack allowance; not measured RSS.
inline constexpr std::size_t position_adaptive_huffman_stream_decode_control_bytes=147456;
class PositionAdaptiveHuffmanStreamDecoder final : public core::Transform {
public:
    PositionAdaptiveHuffmanStreamDecoder(entropy::internal::PositionAdaptiveHuffmanProfile,
        core::DecoderLimits,std::span<std::byte> serialized,
        std::span<dictionary::internal::LzssTypedToken> private_tokens,
        std::span<std::byte> private_raw,std::span<entropy::internal::AdaptiveHuffmanNode>,
        std::span<std::uint16_t> symbols,std::size_t additional_owner_bytes=0) noexcept;
    PositionAdaptiveHuffmanStreamDecoder(const PositionAdaptiveHuffmanStreamDecoder&)=delete;
    PositionAdaptiveHuffmanStreamDecoder& operator=(const PositionAdaptiveHuffmanStreamDecoder&)=delete;
    [[nodiscard]] core::ProcessResult process(std::span<const std::byte>,
        std::span<std::byte>,std::uint32_t flags) noexcept override;
private:
    enum class State { stream_header,frame_header,descriptor,payload,drain,await_end,ended,error };
    [[nodiscard]] bool disjoint(std::span<const std::byte>,std::span<std::byte>) const noexcept;
    [[nodiscard]] core::ProcessResult fail(core::ErrorCode,std::uint64_t,std::size_t) noexcept;
    entropy::internal::PositionAdaptiveHuffmanProfile profile_{};
    core::DecoderLimits limits_{};
    std::span<std::byte> serialized_,raw_;
    std::span<dictionary::internal::LzssTypedToken> tokens_;
    std::span<entropy::internal::AdaptiveHuffmanNode> nodes_;
    std::span<std::uint16_t> symbols_;
    std::size_t token_bytes_{},node_bytes_{},symbol_bytes_{},collected_{},drained_{};
    std::uint64_t position_{},frame_start_{},validated_{},sequence_{};
    std::array<std::byte,112> header_{};
    PositionAdaptiveHuffmanStreamHeader stream_{};
    PositionAdaptiveHuffmanFrameLayout layout_{};
    State state_{State::stream_header};
    core::StreamError error_{};
    bool end_seen_{};
};
} // namespace marc::frame::internal
#endif

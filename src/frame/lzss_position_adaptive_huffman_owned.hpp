#ifndef MARC_FRAME_LZSS_POSITION_ADAPTIVE_HUFFMAN_OWNED_HPP
#define MARC_FRAME_LZSS_POSITION_ADAPTIVE_HUFFMAN_OWNED_HPP
#include "frame/lzss_position_adaptive_huffman_stream_decoder.hpp"
#include <memory>
#include <optional>
namespace marc::frame::internal {
struct PositionAdaptiveHuffmanWorkspace {
    std::size_t raw_bytes{},token_count{},token_bytes{},node_count{},node_bytes{},symbol_count{},symbol_bytes{};
    std::size_t serialized_bytes{},payload_bytes{},private_serialized_bytes{},finder_bytes{};
    std::size_t owner_bytes{},control_bytes{},owned_bytes{},aggregate_bytes{};
    bool operator==(const PositionAdaptiveHuffmanWorkspace&) const=default;
};
class PositionAdaptiveHuffmanOwnedEncoder final : public core::Transform {
public:
    [[nodiscard]] static core::ErrorCode requirements(const PositionAdaptiveHuffmanStreamHeader&,
        const core::DecoderLimits&,PositionAdaptiveHuffmanWorkspace&) noexcept;
    [[nodiscard]] static std::unique_ptr<PositionAdaptiveHuffmanOwnedEncoder> create(
        const PositionAdaptiveHuffmanStreamHeader&,const core::DecoderLimits&,core::ErrorCode&) noexcept;
    PositionAdaptiveHuffmanOwnedEncoder(const PositionAdaptiveHuffmanOwnedEncoder&)=delete;
    PositionAdaptiveHuffmanOwnedEncoder& operator=(const PositionAdaptiveHuffmanOwnedEncoder&)=delete;
    [[nodiscard]] core::ProcessResult process(std::span<const std::byte>,std::span<std::byte>,std::uint32_t) noexcept override;
private:
    PositionAdaptiveHuffmanOwnedEncoder() noexcept=default;
    enum class State { header,collect,drain,await_end,ended,error };
    [[nodiscard]] bool disjoint(std::span<const std::byte>,std::span<std::byte>) const noexcept;
    [[nodiscard]] core::ProcessResult fail(core::ErrorCode,std::size_t=0) noexcept;
    PositionAdaptiveHuffmanStreamHeader stream_{};
    core::DecoderLimits limits_{};
    PositionAdaptiveHuffmanWorkspace workspace_{};
    std::unique_ptr<std::byte[]> raw_,serialized_,payload_,staged_,finder_;
    std::unique_ptr<dictionary::internal::LzssTypedToken[]> tokens_;
    std::unique_ptr<entropy::internal::AdaptiveHuffmanNode[]> nodes_;
    std::unique_ptr<std::uint16_t[]> symbols_;
    std::array<std::byte,112> header_{};
    std::size_t collected_{},pending_{112},drained_{};
    std::uint64_t received_{},committed_{},sequence_{};
    State state_{State::header};core::StreamError error_{};bool end_seen_{};
};
class PositionAdaptiveHuffmanOwnedDecoder final : public core::Transform {
public:
    [[nodiscard]] static core::ErrorCode requirements(entropy::internal::PositionAdaptiveHuffmanProfile,
        std::uint32_t capacity,const core::DecoderLimits&,PositionAdaptiveHuffmanWorkspace&) noexcept;
    [[nodiscard]] static std::unique_ptr<PositionAdaptiveHuffmanOwnedDecoder> create(
        entropy::internal::PositionAdaptiveHuffmanProfile,std::uint32_t capacity,
        const core::DecoderLimits&,core::ErrorCode&) noexcept;
    PositionAdaptiveHuffmanOwnedDecoder(const PositionAdaptiveHuffmanOwnedDecoder&)=delete;
    PositionAdaptiveHuffmanOwnedDecoder& operator=(const PositionAdaptiveHuffmanOwnedDecoder&)=delete;
    [[nodiscard]] core::ProcessResult process(std::span<const std::byte>,std::span<std::byte>,std::uint32_t) noexcept override;
private:
    PositionAdaptiveHuffmanOwnedDecoder() noexcept=default;
    std::unique_ptr<std::byte[]> raw_,serialized_;
    std::unique_ptr<dictionary::internal::LzssTypedToken[]> tokens_;
    std::unique_ptr<entropy::internal::AdaptiveHuffmanNode[]> nodes_;
    std::unique_ptr<std::uint16_t[]> symbols_;
    std::optional<PositionAdaptiveHuffmanStreamDecoder> decoder_;
    core::StreamError error_{};bool ended_{};
};
} // namespace marc::frame::internal
#endif

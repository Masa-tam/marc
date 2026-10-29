#ifndef MARC_FRAME_LZSS_POSITION_BLOCKED_HUFFMAN_32M_OWNED_HPP
#define MARC_FRAME_LZSS_POSITION_BLOCKED_HUFFMAN_32M_OWNED_HPP
#include "frame/lzss_position_blocked_huffman_32m_stream_decoder.hpp"
#include <memory>
#include <optional>

namespace marc::frame::internal {
struct PositionBlockedHuffmanOwnedMechanics;
struct PositionBlockedHuffman32mWorkspace {
    std::size_t raw_bytes{},token_count{},serialized_bytes{},finder_bytes{},aggregate_bytes{};
    std::size_t token_bytes{},payload_bytes{},private_serialized_bytes{},table_bytes{};
    std::size_t owned_buffer_bytes{},owner_bytes{},fixed_working_bytes{},metadata_floor_bytes{};
    bool operator==(const PositionBlockedHuffman32mWorkspace&) const = default;
};
// All requirements are admitted before allocation; process calls never allocate.
class PositionBlockedHuffman32mOwnedEncoder final : public core::Transform {
public:
    [[nodiscard]] static core::ErrorCode requirements(const PositionBlockedHuffman32mStreamHeader&,
        const core::DecoderLimits&,PositionBlockedHuffman32mWorkspace&) noexcept;
    [[nodiscard]] static std::unique_ptr<PositionBlockedHuffman32mOwnedEncoder> create(
        const PositionBlockedHuffman32mStreamHeader&,const core::DecoderLimits&,core::ErrorCode&) noexcept;
    PositionBlockedHuffman32mOwnedEncoder(const PositionBlockedHuffman32mOwnedEncoder&)=delete;
    PositionBlockedHuffman32mOwnedEncoder& operator=(const PositionBlockedHuffman32mOwnedEncoder&)=delete;
    [[nodiscard]] core::ProcessResult process(std::span<const std::byte>,std::span<std::byte>,std::uint32_t) noexcept override;
private:
    friend struct PositionBlockedHuffmanOwnedMechanics;
    PositionBlockedHuffman32mOwnedEncoder() noexcept = default;
    enum class State { header,collecting,draining,awaiting_end,ended,error };
    [[nodiscard]] bool disjoint(std::span<const std::byte>,std::span<std::byte>) const noexcept;
    [[nodiscard]] core::ProcessResult fail(core::ErrorCode,std::size_t=0,std::size_t=0) noexcept;
    PositionBlockedHuffman32mStreamHeader stream_{};
    core::DecoderLimits limits_{};
    PositionBlockedHuffman32mWorkspace workspace_{};
    std::unique_ptr<std::byte[]> raw_,serialized_,finder_,payload_,private_serialized_;
    std::unique_ptr<entropy::internal::CanonicalHuffmanTable[]> tables_;
    std::unique_ptr<dictionary::internal::LzssTypedToken[]> tokens_;
    std::array<std::byte,112> header_{};
    std::size_t collected_{},pending_{112},drained_{};
    std::uint64_t received_{},committed_{},sequence_{};
    State state_{State::header};
    core::StreamError error_{};
    bool end_seen_{};
};
class PositionBlockedHuffman32mOwnedDecoder final : public core::Transform {
public:
    [[nodiscard]] static core::ErrorCode requirements(std::uint32_t,const core::DecoderLimits&,PositionBlockedHuffman32mWorkspace&) noexcept;
    [[nodiscard]] static std::unique_ptr<PositionBlockedHuffman32mOwnedDecoder> create(
        std::uint32_t,const core::DecoderLimits&,core::ErrorCode&) noexcept;
    PositionBlockedHuffman32mOwnedDecoder(const PositionBlockedHuffman32mOwnedDecoder&)=delete;
    PositionBlockedHuffman32mOwnedDecoder& operator=(const PositionBlockedHuffman32mOwnedDecoder&)=delete;
    [[nodiscard]] core::ProcessResult process(std::span<const std::byte>,std::span<std::byte>,std::uint32_t) noexcept override;
private:
    friend struct PositionBlockedHuffmanOwnedMechanics;
    PositionBlockedHuffman32mOwnedDecoder() noexcept = default;
    std::unique_ptr<std::byte[]> raw_,serialized_;
    std::unique_ptr<entropy::internal::HuffmanDecodeTable[]> tables_;
    std::unique_ptr<dictionary::internal::LzssTypedToken[]> tokens_;
    std::optional<PositionBlockedHuffman32mStreamDecoder> decoder_;
    core::StreamError error_{};
    bool ended_{};
};
} // namespace marc::frame::internal
#endif

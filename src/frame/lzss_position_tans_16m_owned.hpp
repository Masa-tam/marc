#ifndef MARC_FRAME_LZSS_POSITION_TANS_16M_OWNED_HPP
#define MARC_FRAME_LZSS_POSITION_TANS_16M_OWNED_HPP
#include "frame/lzss_position_tans_16m_stream_decoder.hpp"
#include <memory>
#include <optional>

namespace marc::frame::internal {
struct PositionTansOwnedMechanics;
struct PositionTans16mWorkspace {
    std::size_t raw_bytes{},token_count{},serialized_bytes{},finder_bytes{},aggregate_bytes{};
    std::size_t token_bytes{},payload_bytes{},private_serialized_bytes{},table_bytes{};
    std::size_t owned_buffer_bytes{},owner_bytes{},fixed_working_bytes{},metadata_floor_bytes{};
    bool operator==(const PositionTans16mWorkspace&) const = default;
};
// All requirements are admitted before allocation; process calls never allocate.
class PositionTans16mOwnedEncoder final : public core::Transform {
public:
    [[nodiscard]] static core::ErrorCode requirements(const PositionTans16mStreamHeader&,
        const core::DecoderLimits&,PositionTans16mWorkspace&) noexcept;
    [[nodiscard]] static std::unique_ptr<PositionTans16mOwnedEncoder> create(
        const PositionTans16mStreamHeader&,const core::DecoderLimits&,core::ErrorCode&) noexcept;
    PositionTans16mOwnedEncoder(const PositionTans16mOwnedEncoder&)=delete;
    PositionTans16mOwnedEncoder& operator=(const PositionTans16mOwnedEncoder&)=delete;
    [[nodiscard]] core::ProcessResult process(std::span<const std::byte>,std::span<std::byte>,std::uint32_t) noexcept override;
private:
    friend struct PositionTansOwnedMechanics;
    PositionTans16mOwnedEncoder() noexcept = default;
    enum class State { header,collecting,draining,awaiting_end,ended,error };
    [[nodiscard]] bool disjoint(std::span<const std::byte>,std::span<std::byte>) const noexcept;
    [[nodiscard]] core::ProcessResult fail(core::ErrorCode,std::size_t=0,std::size_t=0) noexcept;
    PositionTans16mStreamHeader stream_{};
    core::DecoderLimits limits_{};
    PositionTans16mWorkspace workspace_{};
    std::unique_ptr<std::byte[]> raw_,serialized_,finder_,payload_,private_serialized_;
    std::unique_ptr<std::uint16_t[]> tables_;
    std::unique_ptr<dictionary::internal::LzssTypedToken[]> tokens_;
    std::array<std::byte,112> header_{};
    std::size_t collected_{},pending_{112},drained_{};
    std::uint64_t received_{},committed_{},sequence_{};
    State state_{State::header};
    core::StreamError error_{};
    bool end_seen_{};
};
class PositionTans16mOwnedDecoder final : public core::Transform {
public:
    [[nodiscard]] static core::ErrorCode requirements(std::uint32_t,const core::DecoderLimits&,PositionTans16mWorkspace&) noexcept;
    [[nodiscard]] static std::unique_ptr<PositionTans16mOwnedDecoder> create(
        std::uint32_t,const core::DecoderLimits&,core::ErrorCode&) noexcept;
    PositionTans16mOwnedDecoder(const PositionTans16mOwnedDecoder&)=delete;
    PositionTans16mOwnedDecoder& operator=(const PositionTans16mOwnedDecoder&)=delete;
    [[nodiscard]] core::ProcessResult process(std::span<const std::byte>,std::span<std::byte>,std::uint32_t) noexcept override;
private:
    friend struct PositionTansOwnedMechanics;
    PositionTans16mOwnedDecoder() noexcept = default;
    std::unique_ptr<std::byte[]> raw_,serialized_;
    std::unique_ptr<entropy::internal::TansDecodeEntry[]> tables_;
    std::unique_ptr<dictionary::internal::LzssTypedToken[]> tokens_;
    std::optional<PositionTans16mStreamDecoder> decoder_;
    core::StreamError error_{};
    bool ended_{};
};
} // namespace marc::frame::internal
#endif





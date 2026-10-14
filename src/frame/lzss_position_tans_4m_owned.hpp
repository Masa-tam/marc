#ifndef MARC_FRAME_LZSS_POSITION_TANS_4M_OWNED_HPP
#define MARC_FRAME_LZSS_POSITION_TANS_4M_OWNED_HPP
#include "frame/lzss_position_tans_4m_stream_decoder.hpp"
#include <memory>
#include <optional>

namespace marc::frame::internal {
struct PositionTans4mWorkspace {
    std::size_t raw_bytes{},token_count{},serialized_bytes{},finder_bytes{},aggregate_bytes{};
    std::size_t token_bytes{},payload_bytes{},private_serialized_bytes{},table_bytes{};
    std::size_t owned_buffer_bytes{},owner_bytes{},fixed_working_bytes{},metadata_floor_bytes{};
    bool operator==(const PositionTans4mWorkspace&) const = default;
};
// All requirements are admitted before allocation; process calls never allocate.
class PositionTans4mOwnedEncoder final : public core::Transform {
public:
    [[nodiscard]] static core::ErrorCode requirements(const PositionTans4mStreamHeader&,
        const core::DecoderLimits&,PositionTans4mWorkspace&) noexcept;
    [[nodiscard]] static std::unique_ptr<PositionTans4mOwnedEncoder> create(
        const PositionTans4mStreamHeader&,const core::DecoderLimits&,core::ErrorCode&) noexcept;
    PositionTans4mOwnedEncoder(const PositionTans4mOwnedEncoder&)=delete;
    PositionTans4mOwnedEncoder& operator=(const PositionTans4mOwnedEncoder&)=delete;
    [[nodiscard]] core::ProcessResult process(std::span<const std::byte>,std::span<std::byte>,std::uint32_t) noexcept override;
private:
    PositionTans4mOwnedEncoder() noexcept = default;
    enum class State { header,collecting,draining,awaiting_end,ended,error };
    [[nodiscard]] bool disjoint(std::span<const std::byte>,std::span<std::byte>) const noexcept;
    [[nodiscard]] core::ProcessResult fail(core::ErrorCode,std::size_t=0,std::size_t=0) noexcept;
    PositionTans4mStreamHeader stream_{};
    core::DecoderLimits limits_{};
    PositionTans4mWorkspace workspace_{};
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
class PositionTans4mOwnedDecoder final : public core::Transform {
public:
    [[nodiscard]] static core::ErrorCode requirements(std::uint32_t,const core::DecoderLimits&,PositionTans4mWorkspace&) noexcept;
    [[nodiscard]] static std::unique_ptr<PositionTans4mOwnedDecoder> create(
        std::uint32_t,const core::DecoderLimits&,core::ErrorCode&) noexcept;
    PositionTans4mOwnedDecoder(const PositionTans4mOwnedDecoder&)=delete;
    PositionTans4mOwnedDecoder& operator=(const PositionTans4mOwnedDecoder&)=delete;
    [[nodiscard]] core::ProcessResult process(std::span<const std::byte>,std::span<std::byte>,std::uint32_t) noexcept override;
private:
    PositionTans4mOwnedDecoder() noexcept = default;
    std::unique_ptr<std::byte[]> raw_,serialized_;
    std::unique_ptr<entropy::internal::TansDecodeEntry[]> tables_;
    std::unique_ptr<dictionary::internal::LzssTypedToken[]> tokens_;
    std::optional<PositionTans4mStreamDecoder> decoder_;
    core::StreamError error_{};
    bool ended_{};
};
} // namespace marc::frame::internal
#endif



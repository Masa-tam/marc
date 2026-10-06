#ifndef MARC_FRAME_LZSS_POSITION_RANS_1M_OWNED_HPP
#define MARC_FRAME_LZSS_POSITION_RANS_1M_OWNED_HPP
#include "frame/lzss_position_rans_1m_stream_decoder.hpp"
#include <memory>
#include <optional>

namespace marc::frame::internal {
struct PositionRans1mWorkspace {
    std::size_t raw_bytes{},token_count{},serialized_bytes{},finder_bytes{},aggregate_bytes{};
    bool operator==(const PositionRans1mWorkspace&) const = default;
};
// All requirements are admitted before allocation; process calls never allocate.
class PositionRans1mOwnedEncoder final : public core::Transform {
public:
    [[nodiscard]] static core::ErrorCode requirements(const PositionRans1mStreamHeader&,
        const core::DecoderLimits&,PositionRans1mWorkspace&) noexcept;
    [[nodiscard]] static std::unique_ptr<PositionRans1mOwnedEncoder> create(
        const PositionRans1mStreamHeader&,const core::DecoderLimits&,core::ErrorCode&,std::uint32_t eligibility=5) noexcept;
    PositionRans1mOwnedEncoder(const PositionRans1mOwnedEncoder&)=delete;
    PositionRans1mOwnedEncoder& operator=(const PositionRans1mOwnedEncoder&)=delete;
    [[nodiscard]] core::ProcessResult process(std::span<const std::byte>,std::span<std::byte>,std::uint32_t) noexcept override;
private:
    PositionRans1mOwnedEncoder() noexcept = default;
    enum class State { header,collecting,draining,awaiting_end,ended,error };
    [[nodiscard]] bool disjoint(std::span<const std::byte>,std::span<std::byte>) const noexcept;
    [[nodiscard]] core::ProcessResult fail(core::ErrorCode,std::size_t=0,std::size_t=0) noexcept;
    PositionRans1mStreamHeader stream_{};
    core::DecoderLimits limits_{};
    PositionRans1mWorkspace workspace_{};
    std::uint32_t eligibility_{3};
    std::unique_ptr<std::byte[]> raw_,serialized_,finder_;
    std::unique_ptr<dictionary::internal::LzssTypedToken[]> tokens_;
    std::array<std::byte,112> header_{};
    std::size_t collected_{},pending_{112},drained_{};
    std::uint64_t received_{},committed_{},sequence_{};
    State state_{State::header};
    core::StreamError error_{};
    bool end_seen_{};
};
class PositionRans1mOwnedDecoder final : public core::Transform {
public:
    [[nodiscard]] static core::ErrorCode requirements(std::uint32_t,const core::DecoderLimits&,PositionRans1mWorkspace&) noexcept;
    [[nodiscard]] static std::unique_ptr<PositionRans1mOwnedDecoder> create(
        std::uint32_t,const core::DecoderLimits&,core::ErrorCode&) noexcept;
    PositionRans1mOwnedDecoder(const PositionRans1mOwnedDecoder&)=delete;
    PositionRans1mOwnedDecoder& operator=(const PositionRans1mOwnedDecoder&)=delete;
    [[nodiscard]] core::ProcessResult process(std::span<const std::byte>,std::span<std::byte>,std::uint32_t) noexcept override;
private:
    PositionRans1mOwnedDecoder() noexcept = default;
    std::unique_ptr<std::byte[]> raw_,serialized_;
    std::unique_ptr<dictionary::internal::LzssTypedToken[]> tokens_;
    std::optional<PositionRans1mStreamDecoder> decoder_;
    core::StreamError error_{};
    bool ended_{};
};
} // namespace marc::frame::internal
#endif

#ifndef MARC_FRAME_LZSS_POSITION_RANS_64K_OWNED_HPP
#define MARC_FRAME_LZSS_POSITION_RANS_64K_OWNED_HPP
#include "frame/lzss_position_rans_64k_stream_decoder.hpp"
#include <memory>
#include <optional>

namespace marc::frame::internal {
struct PositionRans64kWorkspace {
    std::size_t raw_bytes{},token_count{},serialized_bytes{},finder_bytes{},aggregate_bytes{};
    bool operator==(const PositionRans64kWorkspace&) const = default;
};
// All requirements are admitted before allocation; process calls never allocate.
class PositionRans64kOwnedEncoder final : public core::Transform {
public:
    [[nodiscard]] static core::ErrorCode requirements(const PositionRans64kStreamHeader&,
        const core::DecoderLimits&,PositionRans64kWorkspace&) noexcept;
    [[nodiscard]] static std::unique_ptr<PositionRans64kOwnedEncoder> create(
        const PositionRans64kStreamHeader&,const core::DecoderLimits&,core::ErrorCode&,std::uint32_t eligibility=5) noexcept;
    PositionRans64kOwnedEncoder(const PositionRans64kOwnedEncoder&)=delete;
    PositionRans64kOwnedEncoder& operator=(const PositionRans64kOwnedEncoder&)=delete;
    [[nodiscard]] core::ProcessResult process(std::span<const std::byte>,std::span<std::byte>,std::uint32_t) noexcept override;
private:
    PositionRans64kOwnedEncoder() noexcept = default;
    enum class State { header,collecting,draining,awaiting_end,ended,error };
    [[nodiscard]] bool disjoint(std::span<const std::byte>,std::span<std::byte>) const noexcept;
    [[nodiscard]] core::ProcessResult fail(core::ErrorCode,std::size_t=0,std::size_t=0) noexcept;
    PositionRans64kStreamHeader stream_{};
    core::DecoderLimits limits_{};
    PositionRans64kWorkspace workspace_{};
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
class PositionRans64kOwnedDecoder final : public core::Transform {
public:
    [[nodiscard]] static core::ErrorCode requirements(std::uint32_t,const core::DecoderLimits&,PositionRans64kWorkspace&) noexcept;
    [[nodiscard]] static std::unique_ptr<PositionRans64kOwnedDecoder> create(
        std::uint32_t,const core::DecoderLimits&,core::ErrorCode&) noexcept;
    PositionRans64kOwnedDecoder(const PositionRans64kOwnedDecoder&)=delete;
    PositionRans64kOwnedDecoder& operator=(const PositionRans64kOwnedDecoder&)=delete;
    [[nodiscard]] core::ProcessResult process(std::span<const std::byte>,std::span<std::byte>,std::uint32_t) noexcept override;
private:
    PositionRans64kOwnedDecoder() noexcept = default;
    std::unique_ptr<std::byte[]> raw_,serialized_;
    std::unique_ptr<dictionary::internal::LzssTypedToken[]> tokens_;
    std::optional<PositionRans64kStreamDecoder> decoder_;
    core::StreamError error_{};
    bool ended_{};
};
} // namespace marc::frame::internal
#endif

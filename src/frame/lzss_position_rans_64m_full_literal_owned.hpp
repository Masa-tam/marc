#ifndef MARC_FRAME_LZSS_POSITION_RANS_64M_FULL_LITERAL_OWNED_HPP
#define MARC_FRAME_LZSS_POSITION_RANS_64M_FULL_LITERAL_OWNED_HPP
#include "frame/lzss_position_rans_64m_full_literal_stream_decoder.hpp"
#include <memory>
#include <optional>

namespace marc::frame::internal {
struct PositionRans64mFullLiteralWorkspace {
    std::size_t raw_bytes{},token_count{},serialized_bytes{},finder_bytes{},aggregate_bytes{};
    bool operator==(const PositionRans64mFullLiteralWorkspace&) const = default;
};
// All requirements are admitted before allocation; process calls never allocate.
class PositionRans64mFullLiteralOwnedEncoder final : public core::Transform {
public:
    [[nodiscard]] static core::ErrorCode requirements(const PositionRans64mFullLiteralStreamHeader&,
        const core::DecoderLimits&,PositionRans64mFullLiteralWorkspace&) noexcept;
    [[nodiscard]] static std::unique_ptr<PositionRans64mFullLiteralOwnedEncoder> create(
        const PositionRans64mFullLiteralStreamHeader&,const core::DecoderLimits&,core::ErrorCode&,std::uint32_t eligibility=5) noexcept;
    PositionRans64mFullLiteralOwnedEncoder(const PositionRans64mFullLiteralOwnedEncoder&)=delete;
    PositionRans64mFullLiteralOwnedEncoder& operator=(const PositionRans64mFullLiteralOwnedEncoder&)=delete;
    [[nodiscard]] core::ProcessResult process(std::span<const std::byte>,std::span<std::byte>,std::uint32_t) noexcept override;
private:
    PositionRans64mFullLiteralOwnedEncoder() noexcept = default;
    enum class State { header,collecting,draining,awaiting_end,ended,error };
    [[nodiscard]] bool disjoint(std::span<const std::byte>,std::span<std::byte>) const noexcept;
    [[nodiscard]] core::ProcessResult fail(core::ErrorCode,std::size_t=0,std::size_t=0) noexcept;
    PositionRans64mFullLiteralStreamHeader stream_{};
    core::DecoderLimits limits_{};
    PositionRans64mFullLiteralWorkspace workspace_{};
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
class PositionRans64mFullLiteralOwnedDecoder final : public core::Transform {
public:
    [[nodiscard]] static core::ErrorCode requirements(std::uint32_t,const core::DecoderLimits&,PositionRans64mFullLiteralWorkspace&) noexcept;
    [[nodiscard]] static std::unique_ptr<PositionRans64mFullLiteralOwnedDecoder> create(
        std::uint32_t,const core::DecoderLimits&,core::ErrorCode&) noexcept;
    PositionRans64mFullLiteralOwnedDecoder(const PositionRans64mFullLiteralOwnedDecoder&)=delete;
    PositionRans64mFullLiteralOwnedDecoder& operator=(const PositionRans64mFullLiteralOwnedDecoder&)=delete;
    [[nodiscard]] core::ProcessResult process(std::span<const std::byte>,std::span<std::byte>,std::uint32_t) noexcept override;
private:
    PositionRans64mFullLiteralOwnedDecoder() noexcept = default;
    std::unique_ptr<std::byte[]> raw_,serialized_;
    std::unique_ptr<dictionary::internal::LzssTypedToken[]> tokens_;
    std::optional<PositionRans64mFullLiteralStreamDecoder> decoder_;
    core::StreamError error_{};
    bool ended_{};
};
} // namespace marc::frame::internal
#endif

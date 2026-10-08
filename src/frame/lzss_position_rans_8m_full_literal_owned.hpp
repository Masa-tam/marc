#ifndef MARC_FRAME_LZSS_POSITION_RANS_8M_FULL_LITERAL_OWNED_HPP
#define MARC_FRAME_LZSS_POSITION_RANS_8M_FULL_LITERAL_OWNED_HPP
#include "frame/lzss_position_rans_8m_full_literal_stream_decoder.hpp"
#include <memory>
#include <optional>

namespace marc::frame::internal {
struct PositionRans8mFullLiteralWorkspace {
    std::size_t raw_bytes{},token_count{},serialized_bytes{},finder_bytes{},aggregate_bytes{};
    bool operator==(const PositionRans8mFullLiteralWorkspace&) const = default;
};
// All requirements are admitted before allocation; process calls never allocate.
class PositionRans8mFullLiteralOwnedEncoder final : public core::Transform {
public:
    [[nodiscard]] static core::ErrorCode requirements(const PositionRans8mFullLiteralStreamHeader&,
        const core::DecoderLimits&,PositionRans8mFullLiteralWorkspace&) noexcept;
    [[nodiscard]] static std::unique_ptr<PositionRans8mFullLiteralOwnedEncoder> create(
        const PositionRans8mFullLiteralStreamHeader&,const core::DecoderLimits&,core::ErrorCode&,std::uint32_t eligibility=5) noexcept;
    PositionRans8mFullLiteralOwnedEncoder(const PositionRans8mFullLiteralOwnedEncoder&)=delete;
    PositionRans8mFullLiteralOwnedEncoder& operator=(const PositionRans8mFullLiteralOwnedEncoder&)=delete;
    [[nodiscard]] core::ProcessResult process(std::span<const std::byte>,std::span<std::byte>,std::uint32_t) noexcept override;
private:
    PositionRans8mFullLiteralOwnedEncoder() noexcept = default;
    enum class State { header,collecting,draining,awaiting_end,ended,error };
    [[nodiscard]] bool disjoint(std::span<const std::byte>,std::span<std::byte>) const noexcept;
    [[nodiscard]] core::ProcessResult fail(core::ErrorCode,std::size_t=0,std::size_t=0) noexcept;
    PositionRans8mFullLiteralStreamHeader stream_{};
    core::DecoderLimits limits_{};
    PositionRans8mFullLiteralWorkspace workspace_{};
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
class PositionRans8mFullLiteralOwnedDecoder final : public core::Transform {
public:
    [[nodiscard]] static core::ErrorCode requirements(std::uint32_t,const core::DecoderLimits&,PositionRans8mFullLiteralWorkspace&) noexcept;
    [[nodiscard]] static std::unique_ptr<PositionRans8mFullLiteralOwnedDecoder> create(
        std::uint32_t,const core::DecoderLimits&,core::ErrorCode&) noexcept;
    PositionRans8mFullLiteralOwnedDecoder(const PositionRans8mFullLiteralOwnedDecoder&)=delete;
    PositionRans8mFullLiteralOwnedDecoder& operator=(const PositionRans8mFullLiteralOwnedDecoder&)=delete;
    [[nodiscard]] core::ProcessResult process(std::span<const std::byte>,std::span<std::byte>,std::uint32_t) noexcept override;
private:
    PositionRans8mFullLiteralOwnedDecoder() noexcept = default;
    std::unique_ptr<std::byte[]> raw_,serialized_;
    std::unique_ptr<dictionary::internal::LzssTypedToken[]> tokens_;
    std::optional<PositionRans8mFullLiteralStreamDecoder> decoder_;
    core::StreamError error_{};
    bool ended_{};
};
} // namespace marc::frame::internal
#endif

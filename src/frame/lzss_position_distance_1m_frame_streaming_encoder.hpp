#ifndef MARC_FRAME_LZSS_POSITION_DISTANCE_1M_FRAME_STREAMING_ENCODER_HPP
#define MARC_FRAME_LZSS_POSITION_DISTANCE_1M_FRAME_STREAMING_ENCODER_HPP

#include "core/status.hpp"
#include "frame/lzss_position_distance_1m_encode_workspace.hpp"
#include "frame/lzss_position_distance_1m_raw_frame_encoder.hpp"
#include <array>

namespace marc::frame::internal {

[[nodiscard]] bool serialize_lzss_position_distance_1m_stream_header(
    const TypedContextStreamHeader&, const core::DecoderLimits&,
    std::array<std::byte,typed_context_stream_header_size>&) noexcept;

// Private fixed-policy transform. Query with sizeof(this concrete type).
// All three supplied spans remain disjoint and live throughout its lifetime.
// Flush preserves frames; ResetBlock is unsupported. Resubmit an unconsumed
// final suffix with EndInput. Earlier published frames cannot be rolled back.
class LzssPositionDistance1mFrameStreamingEncoder final : public core::Transform {
public:
    LzssPositionDistance1mFrameStreamingEncoder(
        TypedContextStreamHeader stream, core::DecoderLimits limits,
        std::span<std::byte> raw, std::span<std::byte> serialized,
        std::span<std::byte> aligned_views, std::uint32_t eligibility = 3,
        dictionary::internal::LzssPositionDistance1mSearch search = dictionary::internal::LzssPositionDistance1mSearch::indexed,
        std::size_t additional_owner_bytes = 0) noexcept;
    LzssPositionDistance1mFrameStreamingEncoder(const LzssPositionDistance1mFrameStreamingEncoder&) = delete;
    LzssPositionDistance1mFrameStreamingEncoder& operator=(const LzssPositionDistance1mFrameStreamingEncoder&) = delete;

    [[nodiscard]] core::ProcessResult process(std::span<const std::byte> input,
        std::span<std::byte> output, std::uint32_t flags) noexcept override;
    // Private diagnostic: counts direct raw-frame adapter calls, including a
    // failed attempt. Draining or input starvation must never repeat one.
    [[nodiscard]] std::uint64_t frame_preparation_count() const noexcept { return preparations_; }

private:
    enum class State { header, collecting, draining, awaiting_end, ended, error };
    [[nodiscard]] bool disjoint(std::span<const std::byte>, std::span<std::byte>) const noexcept;
    [[nodiscard]] core::ProcessResult fail(core::ErrorCode, std::uint64_t,
        std::size_t consumed = 0, std::size_t produced = 0) noexcept;
    TypedContextStreamHeader stream_{};
    core::DecoderLimits limits_{};
    std::span<std::byte> raw_storage_, serialized_storage_, aligned_storage_;
    LzssPositionDistanceWorkspaceViews views_{};
    std::array<std::byte, typed_context_stream_header_size> header_{};
    std::uint32_t eligibility_{};
    dictionary::internal::LzssPositionDistance1mSearch search_{};
    std::uint64_t received_{}, committed_{}, preparations_{};
    std::size_t collected_{}, pending_{}, drained_{};
    bool end_seen_{};
    State state_{State::header};
    core::StreamError error_{};
};
} // namespace marc::frame::internal
#endif

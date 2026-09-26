#ifndef MARC_FRAME_LZSS_POSITION_DISTANCE_FRAME_STREAMING_DECODER_HPP
#define MARC_FRAME_LZSS_POSITION_DISTANCE_FRAME_STREAMING_DECODER_HPP

#include "core/status.hpp"
#include "frame/lzss_position_distance_preflight.hpp"
#include "dictionary/lzss_typed_token.hpp"

#include <array>

namespace marc::frame::internal {

// Private exact 2/8 + 1/9 + 3/2 decoder. Publication is atomic per frame,
// not per stream. Caller-owned, disjoint workspaces live for the full lifetime.
// Flush preserves frames; ResetBlock is unsupported. EndInput must accompany
// any resubmitted final input suffix until consumed. Ended/error are sticky.
class LzssPositionDistanceFrameStreamingDecoder final : public core::Transform {
public:
    LzssPositionDistanceFrameStreamingDecoder(
        core::DecoderLimits limits,
        std::span<std::byte> serialized_frame_workspace,
        std::span<dictionary::internal::LzssTypedToken> token_workspace,
        std::span<std::byte> raw_frame_workspace) noexcept;

    [[nodiscard]] core::ProcessResult process(
        std::span<const std::byte> input, std::span<std::byte> output,
        std::uint32_t flags) noexcept override;

private:
    static constexpr std::size_t prefix_size = typed_context_frame_header_size
        + typed_context_range_descriptor_size;
    enum class State { stream_header, frame_prefix, payload, draining, awaiting_end, ended, error };
    [[nodiscard]] core::ProcessResult fail(core::ErrorCode code,
        std::uint64_t position, std::size_t consumed, std::size_t produced) noexcept;
    [[nodiscard]] bool disjoint(std::span<const std::byte> input,
        std::span<std::byte> output) const noexcept;

    core::DecoderLimits limits_{};
    std::span<std::byte> serialized_{};
    std::span<dictionary::internal::LzssTypedToken> tokens_{};
    std::span<std::byte> raw_{};
    std::size_t token_bytes_{};
    std::array<std::byte, typed_context_stream_header_size> header_{};
    std::array<std::byte, prefix_size> prefix_{};
    TypedContextStreamHeader stream_{};
    LzssShortMatchFrameRequirements needed_{};
    std::size_t collected_{};
    std::size_t drained_{};
    std::uint64_t input_position_{};
    std::uint64_t frame_start_{};
    std::uint64_t raw_validated_{};
    std::uint64_t sequence_{};
    bool end_seen_{};
    State state_{State::stream_header};
    core::StreamError error_{};
};

} // namespace marc::frame::internal
#endif

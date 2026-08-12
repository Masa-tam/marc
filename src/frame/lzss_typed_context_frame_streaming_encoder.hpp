#ifndef MARC_FRAME_LZSS_TYPED_CONTEXT_FRAME_STREAMING_ENCODER_HPP
#define MARC_FRAME_LZSS_TYPED_CONTEXT_FRAME_STREAMING_ENCODER_HPP

#include "core/status.hpp"
#include "frame/lzss_typed_context_frame_encoder.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame::internal {

class LzssTypedContextFrameStreamingEncoder final : public core::Transform {
public:
    LzssTypedContextFrameStreamingEncoder(
        TypedContextStreamHeader stream,
        core::DecoderLimits limits,
        std::span<std::byte> raw_frame_workspace,
        std::span<dictionary::internal::LzssTypedToken> token_workspace,
        std::span<context::internal::ModeledOperation> operation_workspace,
        std::span<std::byte> match_finder_workspace,
        std::span<std::byte> serialized_frame_workspace) noexcept;

    [[nodiscard]] core::ProcessResult process(
        std::span<const std::byte> input,
        std::span<std::byte> output,
        std::uint32_t flags) noexcept override;

private:
    enum class State : std::uint8_t {
        draining_header,
        collecting,
        draining_frame,
        awaiting_end,
        ended,
        error,
    };

    [[nodiscard]] core::ProcessResult fail(
        core::ErrorCode code,
        std::size_t consumed,
        std::size_t produced) noexcept;
    [[nodiscard]] bool prepare_frame() noexcept;
    [[nodiscard]] bool output_is_disjoint(
        std::span<std::byte> output) const noexcept;

    TypedContextStreamHeader stream_{};
    core::DecoderLimits limits_{};
    std::span<std::byte> raw_frame_workspace_{};
    std::span<dictionary::internal::LzssTypedToken> token_workspace_{};
    std::span<context::internal::ModeledOperation> operation_workspace_{};
    std::span<std::byte> match_finder_workspace_{};
    std::span<std::byte> serialized_frame_workspace_{};
    std::array<std::byte, typed_context_stream_header_size> stream_header_{};
    std::size_t raw_frame_size_{};
    std::size_t pending_size_{typed_context_stream_header_size};
    std::size_t pending_offset_{};
    std::uint64_t input_received_{};
    std::uint64_t input_committed_{};
    std::uint64_t frame_sequence_{};
    bool end_seen_{};
    State state_{State::draining_header};
    core::StreamError terminal_error_{};
    core::ErrorCode preparation_error_{core::ErrorCode::internal_error};
};

} // namespace marc::frame::internal

#endif

#ifndef MARC_FRAME_RANS_FRAME_STREAMING_ENCODER_HPP
#define MARC_FRAME_RANS_FRAME_STREAMING_ENCODER_HPP

#include "core/limits.hpp"
#include "core/status.hpp"
#include "frame/rans_frame.hpp"
#include "frame/stream_header.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

class RansFrameStreamingEncoder final : public core::Transform {
public:
    RansFrameStreamingEncoder(
        StreamHeader stream, core::DecoderLimits limits,
        std::span<std::byte> frame_input_storage,
        std::span<std::byte> frame_encoded_storage) noexcept;

    [[nodiscard]] core::ProcessResult process(
        std::span<const std::byte> input, std::span<std::byte> output,
        std::uint32_t flags) noexcept override;

private:
    enum class State : std::uint8_t {
        draining_header, collecting, draining_frame,
        awaiting_end, ended, error,
    };
    [[nodiscard]] core::ProcessResult fail(
        core::ErrorCode code, std::size_t consumed,
        std::size_t produced) noexcept;
    [[nodiscard]] bool prepare_frame() noexcept;

    StreamHeader stream_{};
    core::DecoderLimits limits_{};
    std::span<std::byte> frame_input_storage_{};
    std::span<std::byte> frame_encoded_storage_{};
    std::array<std::byte, stream_header_size> stream_header_{};
    std::size_t frame_input_size_{};
    std::size_t pending_size_{stream_header_size};
    std::size_t pending_offset_{};
    std::uint64_t input_received_{};
    std::uint64_t input_committed_{};
    std::uint64_t frame_sequence_{};
    bool end_seen_{};
    State state_{State::draining_header};
    core::StreamError terminal_error_{};
    core::ErrorCode preparation_error_{core::ErrorCode::internal_error};
};

} // namespace marc::frame

#endif

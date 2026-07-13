#ifndef MARC_FRAME_LZ78_STREAMING_ENCODER_HPP
#define MARC_FRAME_LZ78_STREAMING_ENCODER_HPP

#include "core/status.hpp"
#include "frame/lz78_stream.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

class Lz78FrameStreamingEncoder final : public core::Transform {
public:
    Lz78FrameStreamingEncoder(
        StreamHeader stream, dictionary::internal::Lz78Parameters parameters,
        core::DecoderLimits limits, std::span<std::byte> frame_input_storage,
        std::span<std::byte> frame_encoded_storage,
        std::span<dictionary::internal::Lz78EncoderEntry>
            dictionary_workspace) noexcept;

    [[nodiscard]] core::ProcessResult process(
        std::span<const std::byte> input, std::span<std::byte> output,
        std::uint32_t flags) noexcept override;

private:
    enum class State : std::uint8_t {
        draining_prefix,
        collecting,
        draining_frame,
        awaiting_end,
        ended,
        error,
    };

    [[nodiscard]] core::ProcessResult fail(core::ErrorCode code,
                                           std::size_t consumed,
                                           std::size_t produced) noexcept;
    [[nodiscard]] bool prepare_frame() noexcept;

    StreamHeader stream_{};
    dictionary::internal::Lz78Parameters parameters_{};
    core::DecoderLimits limits_{};
    std::span<std::byte> frame_input_storage_{};
    std::span<std::byte> frame_encoded_storage_{};
    std::span<dictionary::internal::Lz78EncoderEntry> dictionary_workspace_{};
    std::array<std::byte, lz78_stream_prefix_size> prefix_{};
    std::size_t frame_input_size_{};
    std::size_t pending_size_{lz78_stream_prefix_size};
    std::size_t pending_offset_{};
    std::uint64_t input_received_{};
    std::uint64_t input_committed_{};
    std::uint64_t frame_sequence_{};
    bool end_seen_{};
    State state_{State::draining_prefix};
    core::StreamError terminal_error_{};
    core::ErrorCode preparation_error_{core::ErrorCode::internal_error};
};

} // namespace marc::frame

#endif

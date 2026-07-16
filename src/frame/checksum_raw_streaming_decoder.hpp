#ifndef MARC_FRAME_CHECKSUM_RAW_STREAMING_DECODER_HPP
#define MARC_FRAME_CHECKSUM_RAW_STREAMING_DECODER_HPP

#include "core/status.hpp"
#include "frame/checksum_raw_stream.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

class ChecksumRawStreamingDecoder final : public core::Transform {
public:
    ChecksumRawStreamingDecoder(
        core::DecoderLimits limits,
        std::span<std::byte> serialized_frame_workspace) noexcept;

    [[nodiscard]] core::ProcessResult process(
        std::span<const std::byte> input, std::span<std::byte> output,
        std::uint32_t flags) noexcept override;

private:
    enum class State : std::uint8_t {
        collecting_prefix,
        collecting_frame_header,
        collecting_frame_body,
        draining_frame,
        awaiting_end,
        ended,
        error,
    };

    [[nodiscard]] core::ProcessResult fail(core::ErrorCode code,
                                           std::size_t consumed,
                                           std::size_t produced) noexcept;
    [[nodiscard]] bool parse_collected_prefix() noexcept;
    [[nodiscard]] bool parse_collected_frame_header() noexcept;
    [[nodiscard]] bool verify_collected_frame() noexcept;

    core::DecoderLimits limits_{};
    std::span<std::byte> workspace_{};
    std::array<std::byte, checksum_raw_stream_prefix_size> prefix_{};
    StreamHeader stream_{};
    std::array<HashDescriptor, 1> descriptors_{};
    FrameHeader frame_{};
    std::size_t collected_{};
    std::size_t frame_serialized_size_{};
    std::size_t output_offset_{};
    std::uint64_t input_position_{};
    std::uint64_t output_committed_{};
    std::uint64_t frame_sequence_{};
    bool end_seen_{};
    core::ErrorCode preparation_error_{core::ErrorCode::malformed_stream};
    State state_{State::collecting_prefix};
    core::StreamError terminal_error_{};
};

} // namespace marc::frame

#endif

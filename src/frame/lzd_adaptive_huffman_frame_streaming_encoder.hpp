#ifndef MARC_FRAME_LZD_ADAPTIVE_HUFFMAN_FRAME_STREAMING_ENCODER_HPP
#define MARC_FRAME_LZD_ADAPTIVE_HUFFMAN_FRAME_STREAMING_ENCODER_HPP

#include "core/status.hpp"
#include "frame/lzd_adaptive_huffman_frame.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

inline constexpr std::size_t lzd_adaptive_huffman_stream_prefix_size =
    stream_header_size + dictionary::internal::lzd_parameter_size;

class LzdAdaptiveHuffmanFrameStreamingEncoder final
    : public core::Transform {
public:
    LzdAdaptiveHuffmanFrameStreamingEncoder(
        StreamHeader stream,
        dictionary::internal::LzdParameters parameters,
        core::DecoderLimits limits,
        std::span<std::byte> frame_input_storage,
        std::span<std::byte> dictionary_staging,
        std::span<std::byte> frame_encoded_storage,
        std::span<dictionary::internal::LzdEncoderEntry>
            encoder_workspace) noexcept;

    [[nodiscard]] core::ProcessResult process(
        std::span<const std::byte> input,
        std::span<std::byte> output,
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

    [[nodiscard]] core::ProcessResult fail(
        core::ErrorCode code,
        std::size_t consumed,
        std::size_t produced) noexcept;
    [[nodiscard]] bool prepare_frame() noexcept;

    StreamHeader stream_{};
    dictionary::internal::LzdParameters parameters_{};
    core::DecoderLimits limits_{};
    std::span<std::byte> frame_input_storage_{};
    std::span<std::byte> dictionary_staging_{};
    std::span<std::byte> frame_encoded_storage_{};
    std::span<dictionary::internal::LzdEncoderEntry> encoder_workspace_{};
    std::array<std::byte, lzd_adaptive_huffman_stream_prefix_size> prefix_{};
    std::size_t frame_input_size_{};
    std::size_t pending_size_{lzd_adaptive_huffman_stream_prefix_size};
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

#ifndef MARC_FRAME_LZMW_ADAPTIVE_HUFFMAN_FRAME_STREAMING_DECODER_HPP
#define MARC_FRAME_LZMW_ADAPTIVE_HUFFMAN_FRAME_STREAMING_DECODER_HPP

#include "core/status.hpp"
#include "frame/lzmw_adaptive_huffman_frame_streaming_encoder.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

class LzmwAdaptiveHuffmanFrameStreamingDecoder final
    : public core::Transform {
public:
    LzmwAdaptiveHuffmanFrameStreamingDecoder(
        core::DecoderLimits limits,
        std::span<std::byte> frame_encoded_storage,
        std::span<std::byte> dictionary_staging,
        std::span<std::byte> frame_decoded_storage,
        std::span<dictionary::internal::LzmwPhraseEntry> phrase_workspace,
        std::span<std::uint32_t> expansion_workspace) noexcept;

    [[nodiscard]] core::ProcessResult process(
        std::span<const std::byte> input,
        std::span<std::byte> output,
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

    [[nodiscard]] core::ProcessResult fail(
        core::ErrorCode code,
        std::size_t consumed,
        std::size_t produced) noexcept;
    [[nodiscard]] bool parse_collected_prefix() noexcept;
    [[nodiscard]] bool parse_collected_frame_header() noexcept;
    [[nodiscard]] bool decode_collected_frame() noexcept;

    core::DecoderLimits limits_{};
    std::span<std::byte> frame_encoded_storage_{};
    std::span<std::byte> dictionary_staging_{};
    std::span<std::byte> frame_decoded_storage_{};
    std::span<dictionary::internal::LzmwPhraseEntry> phrase_workspace_{};
    std::span<std::uint32_t> expansion_workspace_{};
    std::array<std::byte, lzmw_adaptive_huffman_stream_prefix_size>
        prefix_bytes_{};
    std::array<std::byte, frame_header_size> frame_header_bytes_{};
    StreamHeader stream_{};
    dictionary::internal::LzmwParameters parameters_{};
    FrameHeader frame_{};
    std::size_t header_collected_{};
    std::size_t frame_serialized_size_{};
    std::size_t frame_collected_{};
    std::size_t decoded_size_{};
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

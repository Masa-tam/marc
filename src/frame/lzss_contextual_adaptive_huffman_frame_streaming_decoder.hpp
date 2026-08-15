#ifndef MARC_FRAME_LZSS_CONTEXTUAL_ADAPTIVE_HUFFMAN_FRAME_STREAMING_DECODER_HPP
#define MARC_FRAME_LZSS_CONTEXTUAL_ADAPTIVE_HUFFMAN_FRAME_STREAMING_DECODER_HPP

#include "core/status.hpp"
#include "frame/lzss_contextual_adaptive_huffman_frame_decoder.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame::internal {

enum class LzssContextualAdaptiveHuffmanStreamAdmission : std::uint8_t {
    any,
    field_context_64k,
    field_context_1m,
};

class LzssContextualAdaptiveHuffmanFrameStreamingDecoder final
    : public core::Transform {
public:
    LzssContextualAdaptiveHuffmanFrameStreamingDecoder(
        core::DecoderLimits limits,
        std::span<std::byte> serialized_frame_workspace,
        std::span<entropy::internal::AdaptiveHuffmanNode> node_workspace,
        std::span<std::uint16_t> symbol_workspace,
        std::span<dictionary::internal::LzssTypedToken> token_workspace,
        std::span<std::byte> raw_frame_workspace,
        LzssContextualAdaptiveHuffmanStreamAdmission admission =
            LzssContextualAdaptiveHuffmanStreamAdmission::any) noexcept;

    [[nodiscard]] core::ProcessResult process(
        std::span<const std::byte> input, std::span<std::byte> output,
        std::uint32_t flags) noexcept override;

private:
    enum class State : std::uint8_t {
        collecting_stream_header,
        collecting_frame_header,
        collecting_frame_body,
        draining_frame,
        awaiting_end,
        ended,
        error,
    };

    [[nodiscard]] core::ProcessResult fail(
        core::ErrorCode code, std::size_t consumed,
        std::size_t produced) noexcept;
    [[nodiscard]] bool parse_collected_stream_header() noexcept;
    [[nodiscard]] bool prepare_collected_frame() noexcept;
    [[nodiscard]] bool decode_collected_frame() noexcept;

    core::DecoderLimits limits_{};
    std::span<std::byte> serialized_frame_workspace_{};
    std::span<entropy::internal::AdaptiveHuffmanNode> node_workspace_{};
    std::span<std::uint16_t> symbol_workspace_{};
    std::span<dictionary::internal::LzssTypedToken> token_workspace_{};
    std::span<std::byte> raw_frame_workspace_{};
    LzssContextualAdaptiveHuffmanStreamAdmission admission_{
        LzssContextualAdaptiveHuffmanStreamAdmission::any};
    std::array<std::byte,
               lzss_contextual_adaptive_huffman_stream_header_size>
        stream_header_bytes_{};
    std::array<std::byte,
               lzss_contextual_adaptive_huffman_frame_header_size>
        frame_header_bytes_{};
    LzssContextualAdaptiveHuffmanStreamHeader stream_{};
    LzssContextualAdaptiveHuffmanFrameHeader frame_{};
    std::size_t header_collected_{};
    std::size_t frame_serialized_size_{};
    std::size_t frame_collected_{};
    std::size_t decoded_size_{};
    std::size_t output_offset_{};
    std::size_t selected_node_count_{};
    std::size_t selected_symbol_count_{};
    std::uint64_t input_position_{};
    std::uint64_t output_committed_{};
    std::uint64_t frame_sequence_{};
    bool end_seen_{};
    core::ErrorCode preparation_error_{core::ErrorCode::malformed_stream};
    State state_{State::collecting_stream_header};
    core::StreamError terminal_error_{};
};

} // namespace marc::frame::internal

#endif

#ifndef MARC_FRAME_BLOCKED_HUFFMAN_INCREMENTAL_DECODER_HPP
#define MARC_FRAME_BLOCKED_HUFFMAN_INCREMENTAL_DECODER_HPP

#include "core/limits.hpp"
#include "core/status.hpp"
#include "entropy/blocked_huffman_controller.hpp"
#include "frame/blocked_huffman_stream.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame {

class BlockedHuffmanIncrementalDecoder final : public core::Transform {
public:
    BlockedHuffmanIncrementalDecoder(
        core::DecoderLimits limits,
        std::span<std::byte> encoded_storage,
        std::span<std::byte> decoded_storage,
        std::span<entropy::internal::BlockedHuffmanBlockView> frame_views)
        noexcept;

    [[nodiscard]] core::ProcessResult process(
        std::span<const std::byte> input,
        std::span<std::byte> output,
        std::uint32_t flags) noexcept override;

    [[nodiscard]] std::size_t input_received() const noexcept {
        return input_received_;
    }
    [[nodiscard]] std::size_t decoded_size() const noexcept {
        return decoded_size_;
    }

private:
    enum class State : std::uint8_t {
        running,
        draining,
        ended,
        error,
    };

    [[nodiscard]] core::ProcessResult fail(core::ErrorCode code) noexcept;
    [[nodiscard]] core::ProcessResult drain(
        std::span<std::byte> output,
        std::size_t input_consumed) noexcept;

    core::DecoderLimits limits_{};
    std::span<std::byte> encoded_storage_{};
    std::span<std::byte> decoded_storage_{};
    std::span<entropy::internal::BlockedHuffmanBlockView> frame_views_{};
    std::size_t input_received_{};
    std::size_t decoded_size_{};
    std::size_t output_offset_{};
    State state_{State::running};
    core::StreamError terminal_error_{};
};

} // namespace marc::frame

#endif

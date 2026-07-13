#ifndef MARC_DICTIONARY_LZ77_STREAMING_ENCODER_HPP
#define MARC_DICTIONARY_LZ77_STREAMING_ENCODER_HPP

#include "core/status.hpp"
#include "dictionary/lz77_encoder.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

class Lz77StreamingEncoder final : public core::Transform {
public:
    Lz77StreamingEncoder(Lz77Parameters parameters,
                         std::uint64_t declared_frame_size,
                         core::DecoderLimits limits,
                         std::span<std::byte> raw_storage,
                         std::span<std::byte> encoded_storage) noexcept;

    [[nodiscard]] core::ProcessResult process(
        std::span<const std::byte> input, std::span<std::byte> output,
        std::uint32_t flags) noexcept override;

private:
    enum class State : std::uint8_t {
        collecting,
        draining,
        awaiting_end,
        ended,
        error,
    };

    [[nodiscard]] core::ProcessResult fail(core::ErrorCode code,
                                           std::size_t consumed,
                                           std::size_t produced) noexcept;
    [[nodiscard]] bool prepare_encoded_frame() noexcept;

    Lz77Parameters parameters_{};
    core::DecoderLimits limits_{};
    std::span<std::byte> raw_storage_{};
    std::span<std::byte> encoded_storage_{};
    std::uint64_t declared_frame_size_{};
    std::size_t raw_collected_{};
    std::size_t encoded_size_{};
    std::size_t output_offset_{};
    bool end_seen_{};
    core::ErrorCode preparation_error_{core::ErrorCode::internal_error};
    State state_{State::collecting};
    core::StreamError terminal_error_{};
};

} // namespace marc::dictionary::internal

#endif

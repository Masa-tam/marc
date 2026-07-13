#ifndef MARC_DICTIONARY_LZ77_STREAMING_DECODER_HPP
#define MARC_DICTIONARY_LZ77_STREAMING_DECODER_HPP

#include "core/status.hpp"
#include "dictionary/lz77_format.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

class Lz77StreamingDecoder final : public core::Transform {
public:
    Lz77StreamingDecoder(Lz77Parameters parameters,
                         std::uint64_t declared_frame_size,
                         core::DecoderLimits limits,
                         std::span<std::byte> history_storage) noexcept;

    [[nodiscard]] core::ProcessResult process(
        std::span<const std::byte> input, std::span<std::byte> output,
        std::uint32_t flags) noexcept override;

private:
    enum class State : std::uint8_t {
        collecting_token,
        draining_token,
        awaiting_end,
        ended,
        error,
    };

    [[nodiscard]] core::ProcessResult fail(core::ErrorCode code,
                                           std::size_t consumed,
                                           std::size_t produced) noexcept;

    Lz77Parameters parameters_{};
    core::DecoderLimits limits_{};
    std::span<std::byte> history_storage_{};
    std::uint64_t declared_frame_size_{};
    std::uint64_t output_committed_{};
    std::uint64_t input_position_{};
    std::array<std::byte, lz77_token_size> token_bytes_{};
    std::size_t token_collected_{};
    Lz77Token token_{};
    std::uint32_t match_copied_{};
    bool literal_pending_{};
    bool end_seen_{};
    State state_{State::collecting_token};
    core::StreamError terminal_error_{};
};

} // namespace marc::dictionary::internal

#endif

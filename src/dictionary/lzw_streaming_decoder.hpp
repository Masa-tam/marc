#ifndef MARC_DICTIONARY_LZW_STREAMING_DECODER_HPP
#define MARC_DICTIONARY_LZW_STREAMING_DECODER_HPP

#include "core/bit_io.hpp"
#include "core/status.hpp"
#include "dictionary/lzw_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

class LzwStreamingDecoder final : public core::Transform {
public:
    LzwStreamingDecoder(LzwParameters parameters,
                        std::uint64_t declared_frame_size,
                        core::DecoderLimits limits,
                        std::span<LzwPhraseEntry> dictionary_workspace) noexcept;

    [[nodiscard]] core::ProcessResult process(
        std::span<const std::byte> input, std::span<std::byte> output,
        std::uint32_t flags) noexcept override;

private:
    enum class State : std::uint8_t {
        collecting_code,
        draining_phrase,
        awaiting_end,
        ended,
        error,
    };

    [[nodiscard]] bool phrase_byte(std::uint32_t code,
                                   std::uint64_t forward_offset,
                                   std::byte& value) const noexcept;
    [[nodiscard]] core::ProcessResult fail(core::ErrorCode code,
                                           std::size_t consumed,
                                           std::size_t produced) noexcept;

    LzwParameters parameters_{};
    core::DecoderLimits limits_{};
    std::span<LzwPhraseEntry> dictionary_workspace_{};
    core::BitReader reader_{};
    std::uint64_t declared_frame_size_{};
    std::uint64_t output_committed_{};
    std::uint64_t input_position_{};
    std::uint32_t code_width_{lzw_minimum_code_width};
    std::uint32_t next_code_{lzw_first_free_code};
    std::uint32_t pending_code_{};
    std::uint32_t bits_collected_{};
    std::uint32_t previous_code_{};
    std::uint8_t previous_first_byte_{};
    std::uint64_t previous_length_{};
    std::uint32_t phrase_code_{};
    std::uint64_t phrase_length_{};
    std::uint64_t phrase_emitted_{};
    bool first_code_{true};
    bool end_seen_{};
    State state_{State::collecting_code};
    core::StreamError terminal_error_{};
};

} // namespace marc::dictionary::internal

#endif

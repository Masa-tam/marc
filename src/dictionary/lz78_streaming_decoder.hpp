#ifndef MARC_DICTIONARY_LZ78_STREAMING_DECODER_HPP
#define MARC_DICTIONARY_LZ78_STREAMING_DECODER_HPP

#include "core/status.hpp"
#include "dictionary/lz78_format.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

class Lz78StreamingDecoder final : public core::Transform {
public:
    Lz78StreamingDecoder(Lz78Parameters parameters,
                         std::uint64_t declared_frame_size,
                         core::DecoderLimits limits,
                         std::span<Lz78PhraseEntry> dictionary_workspace) noexcept;

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

    [[nodiscard]] bool phrase_byte(std::uint32_t phrase_index,
                                   std::uint64_t forward_offset,
                                   std::byte& value) const noexcept;
    [[nodiscard]] core::ProcessResult fail(core::ErrorCode code,
                                           std::size_t consumed,
                                           std::size_t produced) noexcept;

    Lz78Parameters parameters_{};
    core::DecoderLimits limits_{};
    std::span<Lz78PhraseEntry> dictionary_workspace_{};
    std::uint64_t declared_frame_size_{};
    std::uint64_t output_committed_{};
    std::uint64_t input_position_{};
    std::uint32_t dictionary_entries_{};
    std::array<std::byte, lz78_token_size> token_bytes_{};
    std::size_t token_collected_{};
    Lz78Token token_{};
    std::uint64_t phrase_length_{};
    std::uint64_t phrase_emitted_{};
    bool symbol_pending_{};
    bool end_seen_{};
    State state_{State::collecting_token};
    core::StreamError terminal_error_{};
};

} // namespace marc::dictionary::internal

#endif

#ifndef MARC_DICTIONARY_LZMW_STREAMING_DECODER_HPP
#define MARC_DICTIONARY_LZMW_STREAMING_DECODER_HPP

#include "core/status.hpp"
#include "dictionary/lzmw_decoder.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::dictionary::internal {

struct LzmwStreamingDecoderWorkspaceRequirements {
    std::size_t encoded_bytes{};
    std::size_t phrase_entries{};
    std::size_t expansion_entries{};
    std::size_t decoded_bytes{};
    bool supported{};
};

[[nodiscard]] LzmwStreamingDecoderWorkspaceRequirements
lzmw_streaming_decoder_workspace_requirements(
    std::uint64_t declared_frame_size,
    const LzmwParameters& parameters) noexcept;

class LzmwStreamingDecoder final : public core::Transform {
public:
    LzmwStreamingDecoder(
        LzmwParameters parameters, std::uint64_t declared_frame_size,
        core::DecoderLimits limits, std::span<std::byte> encoded_workspace,
        std::span<LzmwPhraseEntry> phrase_workspace,
        std::span<std::uint32_t> expansion_workspace,
        std::span<std::byte> decoded_workspace) noexcept;

    [[nodiscard]] core::ProcessResult process(
        std::span<const std::byte> input, std::span<std::byte> output,
        std::uint32_t flags) noexcept override;

private:
    enum class State : std::uint8_t {
        collecting,
        draining,
        ended,
        error,
    };

    [[nodiscard]] core::ProcessResult fail(core::ErrorCode code,
                                           std::size_t consumed,
                                           std::size_t produced) noexcept;

    LzmwParameters parameters_{};
    core::DecoderLimits limits_{};
    std::span<std::byte> encoded_workspace_{};
    std::span<LzmwPhraseEntry> phrase_workspace_{};
    std::span<std::uint32_t> expansion_workspace_{};
    std::span<std::byte> decoded_workspace_{};
    std::uint64_t declared_frame_size_{};
    std::size_t encoded_size_{};
    std::size_t output_offset_{};
    State state_{State::collecting};
    core::StreamError terminal_error_{};
};

} // namespace marc::dictionary::internal

#endif

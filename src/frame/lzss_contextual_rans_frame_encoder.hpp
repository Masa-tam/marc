#ifndef MARC_FRAME_LZSS_CONTEXTUAL_RANS_FRAME_ENCODER_HPP
#define MARC_FRAME_LZSS_CONTEXTUAL_RANS_FRAME_ENCODER_HPP

#include "context/lzss_contextual_rans_encoder.hpp"
#include "dictionary/lzss_typed_encoder.hpp"
#include "frame/lzss_contextual_rans_format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::frame::internal {

enum class LzssContextualRansFrameEncodeError : std::uint8_t {
    none,
    invalid_stream,
    input_size_mismatch,
    token_staging_too_small,
    serialized_output_too_small,
    overlapping_workspaces,
    workspace_limit,
    unsupported_match_finder_strategy,
    token_encode_error,
    entropy_encode_error,
    header_error,
    descriptor_error,
    arithmetic_overflow,
    internal_error,
};

struct LzssContextualRansFrameEncodeResult {
    std::size_t serialized_size{};
    std::size_t descriptor_size{};
    std::size_t token_count{};
    std::size_t event_count{};
    std::uint32_t decision_count{};
    std::size_t payload_size{};
    dictionary::internal::LzssTypedEncodeResult token_encode{};
    context::internal::LzssContextualRansEncodeResult entropy_encode{};
    LzssContextualRansFrameHeaderError header_error{
        LzssContextualRansFrameHeaderError::none};
    entropy::internal::ContextualRansFormatError descriptor_error{
        entropy::internal::ContextualRansFormatError::none};
    LzssContextualRansFrameEncodeError error{
        LzssContextualRansFrameEncodeError::none};
};

// Planning materializes private token staging but never writes serialized
// output.
[[nodiscard]] LzssContextualRansFrameEncodeResult
plan_lzss_contextual_rans_frame(
    const LzssContextualRansStreamHeader& stream,
    const core::DecoderLimits& limits,
    std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> raw_input,
    std::span<dictionary::internal::LzssTypedToken> private_tokens) noexcept;

[[nodiscard]] LzssContextualRansFrameEncodeResult
encode_lzss_contextual_rans_frame(
    const LzssContextualRansStreamHeader& stream,
    const core::DecoderLimits& limits,
    std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> raw_input,
    std::span<dictionary::internal::LzssTypedToken> private_tokens,
    std::span<std::byte> serialized_output) noexcept;

[[nodiscard]] LzssContextualRansFrameEncodeResult
plan_lzss_contextual_rans_frame_with_match_finder(
    const LzssContextualRansStreamHeader& stream,
    const core::DecoderLimits& limits,
    std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> raw_input,
    std::span<dictionary::internal::LzssTypedToken> private_tokens,
    dictionary::internal::LzssMatchFinderStrategy strategy,
    std::span<std::byte> match_finder_workspace,
    dictionary::internal::LzssMatchFinderStatistics* statistics = nullptr)
    noexcept;

[[nodiscard]] LzssContextualRansFrameEncodeResult
encode_lzss_contextual_rans_frame_with_match_finder(
    const LzssContextualRansStreamHeader& stream,
    const core::DecoderLimits& limits,
    std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> raw_input,
    std::span<dictionary::internal::LzssTypedToken> private_tokens,
    dictionary::internal::LzssMatchFinderStrategy strategy,
    std::span<std::byte> match_finder_workspace,
    std::span<std::byte> serialized_output,
    dictionary::internal::LzssMatchFinderStatistics* statistics = nullptr)
    noexcept;

[[nodiscard]] LzssContextualRansFrameEncodeResult
plan_lzss_contextual_rans_frame_hash_chain(
    const LzssContextualRansStreamHeader& stream,
    const core::DecoderLimits& limits,
    std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> raw_input,
    std::span<dictionary::internal::LzssTypedToken> private_tokens,
    std::span<std::byte> match_finder_workspace,
    dictionary::internal::LzssMatchFinderStatistics* statistics = nullptr)
    noexcept;

[[nodiscard]] LzssContextualRansFrameEncodeResult
encode_lzss_contextual_rans_frame_hash_chain(
    const LzssContextualRansStreamHeader& stream,
    const core::DecoderLimits& limits,
    std::uint64_t sequence,
    std::uint64_t output_already_committed,
    std::span<const std::byte> raw_input,
    std::span<dictionary::internal::LzssTypedToken> private_tokens,
    std::span<std::byte> match_finder_workspace,
    std::span<std::byte> serialized_output,
    dictionary::internal::LzssMatchFinderStatistics* statistics = nullptr)
    noexcept;

} // namespace marc::frame::internal

#endif

#include <marc/marc.h>

#include "core/status.hpp"
#include "dictionary/lzss_typed_token.hpp"
#include "entropy/contextual_rans_format.hpp"
#include "entropy/rans_decode_table.hpp"
#include "frame/lzss_contextual_rans_frame_decoder.hpp"
#include "frame/lzss_contextual_rans_format.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>

namespace {

using RansDecodeEntry = marc::entropy::internal::RansDecodeEntry;
using Token = marc::dictionary::internal::LzssTypedToken;

using PublicConfig = marc_lzss_contextual_rans_config;

constexpr std::size_t maximum_fuzz_input = 32768;
constexpr std::size_t maximum_total_output = 4096;
constexpr std::size_t maximum_frame = 1024;
constexpr std::size_t maximum_decisions = maximum_frame * 7;
constexpr std::size_t maximum_payload = maximum_frame * 14 + 8;
constexpr std::size_t maximum_encoded_frame =
    marc::frame::internal::lzss_contextual_rans_frame_header_size
    + marc::entropy::internal::contextual_rans_descriptor_capacity
    + maximum_payload;
constexpr std::size_t table_entries =
    marc::entropy::internal::contextual_rans_decode_table_entries;
constexpr std::size_t maximum_views_alignment =
    std::max(alignof(RansDecodeEntry), alignof(Token));
constexpr std::size_t maximum_views = table_entries * sizeof(RansDecodeEntry)
    + maximum_views_alignment - 1 + maximum_frame * sizeof(Token);
constexpr std::size_t maximum_internal =
    maximum_encoded_frame + maximum_views + maximum_frame;
constexpr std::size_t maximum_view_words =
    (maximum_views + sizeof(std::max_align_t) - 1)
    / sizeof(std::max_align_t);
static_assert(maximum_views_alignment <= alignof(std::max_align_t));

struct FuzzWorkspace {
    std::array<RansDecodeEntry, table_entries> private_tables{};
    std::array<Token, maximum_frame> private_tokens{};
    std::array<std::byte, maximum_frame> private_raw{};
    std::array<std::uint8_t, maximum_encoded_frame> primary{};
    std::array<std::uint8_t, maximum_frame> secondary{};
    std::array<std::max_align_t, maximum_view_words> views{};
    std::array<std::uint8_t, maximum_total_output> output{};
};

thread_local FuzzWorkspace workspace{};

[[nodiscard]] marc::core::DecoderLimits fuzz_limits() noexcept {
    marc::core::DecoderLimits limits{};
    limits.max_total_output_size = maximum_total_output;
    limits.max_frame_size = maximum_frame;
    limits.max_block_size = maximum_decisions;
    limits.max_compressed_payload_size = maximum_payload;
    limits.max_internal_buffered_bytes = maximum_internal;
    limits.max_lz_distance = UINT64_C(1) << 22;
    limits.max_lz_match_length = 258;
    limits.max_entropy_table_entries = table_entries;
    return limits;
}

[[nodiscard]] bool parse_stream_header(
    const std::span<const std::byte> input,
    const marc::core::DecoderLimits& limits,
    marc::frame::internal::LzssContextualRansStreamHeader& stream,
    std::size_t& consumed) noexcept {
    return marc::frame::internal::parse_lzss_contextual_rans_stream_header(
               input, limits, stream, consumed)
        == marc::frame::internal::LzssContextualRansStreamHeaderError::none;
}

void decode_complete_frame(
    const std::span<const std::byte> input,
    const marc::frame::internal::LzssContextualRansFrameValidationContext&
        context) noexcept {
    static_cast<void>(marc::frame::internal::decode_lzss_contextual_rans_frame(
        input, context, workspace.private_tables, workspace.private_tokens,
        workspace.private_raw));
}

[[nodiscard]] marc_status initialize_public_config(
    PublicConfig& config) noexcept {
    return marc_lzss_contextual_rans_config_init(
        MARC_DIRECTION_DECODE, &config);
}

[[nodiscard]] marc_status public_workspace_requirements(
    const PublicConfig& config,
    marc_workspace_requirements& requirements) noexcept {
    return marc_lzss_contextual_rans_workspace_requirements(
        &config, &requirements);
}

[[nodiscard]] marc_status create_public_decoder(
    const PublicConfig& config,
    const marc_buffer primary,
    const marc_buffer secondary,
    const marc_buffer views,
    marc_transform** decoder) noexcept {
    return marc_lzss_contextual_rans_create(
        &config, primary, secondary, views, decoder);
}

void exercise_complete_frame(const std::span<const std::byte> input) noexcept {
    if (input.size()
        < marc::frame::internal::lzss_contextual_rans_stream_header_size) {
        return;
    }
    const auto limits = fuzz_limits();
    marc::frame::internal::LzssContextualRansStreamHeader stream{};
    std::size_t consumed{};
    if (!parse_stream_header(input, limits, stream, consumed)
        || consumed
            != marc::frame::internal::lzss_contextual_rans_stream_header_size) {
        return;
    }
    const marc::frame::internal::LzssContextualRansFrameValidationContext
        context{stream, limits, 0, 0};
    decode_complete_frame(input.subspan(consumed), context);
}

void exercise_public_streaming(
    const std::span<const std::byte> input,
    const marc_lzss_contextual_window_profile window_profile) noexcept {
    PublicConfig config{};
    if (initialize_public_config(config) != MARC_STATUS_OK) {
        std::abort();
    }
    config.max_total_output_size = maximum_total_output;
    config.max_frame_size = maximum_frame;
    config.max_block_size = maximum_decisions;
    config.max_compressed_payload_size = maximum_payload;
    config.max_internal_buffered_bytes = maximum_internal;
    config.window_size = window_profile == MARC_LZSS_CONTEXTUAL_WINDOW_4M
        ? UINT32_C(1) << 22
        : window_profile == MARC_LZSS_CONTEXTUAL_WINDOW_1M
            ? UINT32_C(1) << 20 : UINT32_C(1) << 16;
    config.max_lz_distance = UINT64_C(1) << 22;
    config.max_lz_match_length = 258;
    config.max_entropy_table_entries = table_entries;
    config.window_profile = window_profile;

    marc_workspace_requirements requirements{};
    if (public_workspace_requirements(config, requirements) != MARC_STATUS_OK
        || requirements.primary_bytes > workspace.primary.size()
        || requirements.secondary_bytes > workspace.secondary.size()
        || requirements.views_bytes
            > workspace.views.size() * sizeof(std::max_align_t)
        || requirements.views_alignment > maximum_views_alignment) {
        std::abort();
    }

    marc_transform* decoder{};
    if (create_public_decoder(
            config,
            {workspace.primary.data(), requirements.primary_bytes},
            {workspace.secondary.data(), requirements.secondary_bytes},
            {reinterpret_cast<std::uint8_t*>(workspace.views.data()),
             requirements.views_bytes},
            &decoder)
        != MARC_STATUS_OK) {
        std::abort();
    }

    std::size_t input_offset{};
    std::size_t output_offset{};
    constexpr auto maximum_calls = maximum_fuzz_input
        + maximum_total_output + std::size_t{32};
    for (std::size_t call = 0; call < maximum_calls; ++call) {
        const auto remaining = input.size() - input_offset;
        const auto requested = remaining == 0 ? std::size_t{0}
            : std::size_t{1}
                + (std::to_integer<std::uint8_t>(input[input_offset]) % 17U);
        const auto chunk_size = std::min(remaining, requested);
        const auto output_capacity = workspace.output.size() - output_offset;
        const auto output_chunk_size = output_capacity == 0 ? std::size_t{0}
            : std::min<std::size_t>(
                  output_capacity,
                  std::size_t{1}
                      + (chunk_size == 0 ? 0U
                         : std::to_integer<std::uint8_t>(
                               input[input_offset])
                             % 19U));
        const auto flags = input_offset + chunk_size == input.size()
            ? MARC_PROCESS_END_INPUT : MARC_PROCESS_NONE;
        const auto* source_data = chunk_size == 0
            ? static_cast<const std::uint8_t*>(nullptr)
            : reinterpret_cast<const std::uint8_t*>(
                  input.data() + input_offset);
        auto* const output_data = output_chunk_size == 0
            ? static_cast<std::uint8_t*>(nullptr)
            : workspace.output.data() + output_offset;
        const auto result = marc_transform_process(
            decoder, {source_data, chunk_size},
            {output_data, output_chunk_size}, flags);
        if (result.input_consumed > chunk_size
            || result.output_produced > output_chunk_size) {
            marc_transform_destroy(decoder);
            std::abort();
        }
        input_offset += result.input_consumed;
        output_offset += result.output_produced;
        if (result.status >= MARC_STATUS_INVALID_ARGUMENT
            || result.status == MARC_STATUS_END_OF_STREAM) {
            marc_transform_destroy(decoder);
            return;
        }
        if (result.input_consumed == 0 && result.output_produced == 0
            && result.status != MARC_STATUS_NEED_INPUT
            && result.status != MARC_STATUS_NEED_OUTPUT) {
            marc_transform_destroy(decoder);
            std::abort();
        }
        if (input_offset == input.size()
            && result.status == MARC_STATUS_NEED_INPUT) {
            marc_transform_destroy(decoder);
            std::abort();
        }
        if (output_offset == workspace.output.size()
            && result.status == MARC_STATUS_NEED_OUTPUT) {
            marc_transform_destroy(decoder);
            return;
        }
    }
    marc_transform_destroy(decoder);
    std::abort();
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,
                                      const std::size_t size) {
    const auto bounded_size = std::min(size, maximum_fuzz_input);
    const std::span<const std::byte> input{
        reinterpret_cast<const std::byte*>(data), bounded_size};
    exercise_complete_frame(input);
    exercise_public_streaming(input, MARC_LZSS_CONTEXTUAL_WINDOW_64K);
    exercise_public_streaming(input, MARC_LZSS_CONTEXTUAL_WINDOW_1M);
    exercise_public_streaming(input, MARC_LZSS_CONTEXTUAL_WINDOW_4M);
    return 0;
}

#include <marc/marc.h>

#include "core/status.hpp"
#include "context/lzss_field_context_format.hpp"
#include "dictionary/lzss_typed_token.hpp"
#include "frame/lzss_typed_context_frame_decoder.hpp"
#include "frame/typed_context_format.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>

namespace {

using Token = marc::dictionary::internal::LzssTypedToken;

constexpr std::size_t maximum_fuzz_input = 8192;
constexpr std::size_t maximum_total_output = 4096;
constexpr std::size_t maximum_frame = 1024;
constexpr std::size_t maximum_payload = maximum_frame * 14 + 5;
constexpr std::size_t maximum_encoded_frame =
    marc::frame::internal::typed_context_frame_header_size
    + marc::frame::internal::typed_context_range_descriptor_size
    + maximum_payload;
constexpr std::size_t maximum_views = maximum_frame * sizeof(Token);
constexpr std::size_t maximum_views_alignment = alignof(Token);
constexpr std::size_t maximum_internal =
    maximum_encoded_frame + maximum_views + maximum_frame;

[[nodiscard]] marc::core::DecoderLimits fuzz_limits() noexcept {
    marc::core::DecoderLimits limits{};
    limits.max_total_output_size = maximum_total_output;
    limits.max_frame_size = maximum_frame;
    limits.max_block_size = maximum_frame;
    limits.max_compressed_payload_size = maximum_payload;
    limits.max_internal_buffered_bytes = maximum_internal;
    limits.max_lz_distance = UINT64_C(1) << 22;
    limits.max_lz_match_length = 258;
    limits.max_entropy_table_entries =
        marc::context::internal::lzss_field_context_frequency_entries_v3;
    limits.max_range_model_total =
        marc::frame::internal::typed_context_model_total;
    return limits;
}

void exercise_complete_frame(const std::span<const std::byte> input) noexcept {
    if (input.size()
        < marc::frame::internal::typed_context_stream_header_size) {
        return;
    }
    const auto limits = fuzz_limits();
    marc::frame::internal::TypedContextStreamHeader stream{};
    std::size_t consumed{};
    if (marc::frame::internal::parse_typed_context_stream_header(
            input, limits, stream, consumed)
            != marc::frame::internal::TypedContextStreamHeaderError::none
        || consumed
            != marc::frame::internal::typed_context_stream_header_size) {
        return;
    }
    std::array<Token, maximum_frame> tokens{};
    std::array<std::byte, maximum_frame> raw{};
    const marc::frame::internal::TypedContextFrameValidationContext context{
        stream, limits, 0, 0};
    static_cast<void>(
        marc::frame::internal::decode_lzss_typed_context_frame(
            input.subspan(consumed), context, tokens, raw));
}

void exercise_public_streaming(
    const std::span<const std::byte> input,
    const marc_lzss_contextual_profile profile) noexcept {
    marc_lzss_contextual_dynamic_range_config config{};
    if (marc_lzss_contextual_dynamic_range_config_init(
            MARC_DIRECTION_DECODE, &config)
        != MARC_STATUS_OK) {
        std::abort();
    }
    config.max_total_output_size = maximum_total_output;
    config.max_frame_size = maximum_frame;
    config.max_block_size = maximum_frame;
    config.max_compressed_payload_size = maximum_payload;
    config.max_internal_buffered_bytes = maximum_internal;
    config.max_lz_distance = UINT64_C(1) << 22;
    config.max_lz_match_length = 258;
    config.max_entropy_table_entries =
        marc::context::internal::lzss_field_context_frequency_entries_v3;
    config.max_range_model_total =
        marc::frame::internal::typed_context_model_total;
    config.profile = profile;

    marc_workspace_requirements requirements{};
    if (marc_lzss_contextual_dynamic_range_workspace_requirements(
            &config, &requirements)
            != MARC_STATUS_OK
        || requirements.primary_bytes > maximum_encoded_frame
        || requirements.secondary_bytes > maximum_frame
        || requirements.views_bytes > maximum_views
        || requirements.views_alignment > maximum_views_alignment) {
        std::abort();
    }

    std::array<std::uint8_t, maximum_encoded_frame> primary{};
    std::array<std::uint8_t, maximum_frame> secondary{};
    alignas(maximum_views_alignment)
        std::array<std::uint8_t, maximum_views> views{};
    marc_transform* decoder{};
    if (marc_lzss_contextual_dynamic_range_create(
            &config,
            {primary.data(), requirements.primary_bytes},
            {secondary.data(), requirements.secondary_bytes},
            {views.data(), requirements.views_bytes}, &decoder)
        != MARC_STATUS_OK) {
        std::abort();
    }

    std::array<std::uint8_t, maximum_total_output> output{};
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
        const auto output_capacity = output.size() - output_offset;
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
            : output.data() + output_offset;
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
        if (output_offset == output.size()
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
    exercise_public_streaming(input, MARC_LZSS_CONTEXTUAL_PROFILE_64K);
    exercise_public_streaming(input, MARC_LZSS_CONTEXTUAL_PROFILE_1M);
    exercise_public_streaming(input, MARC_LZSS_CONTEXTUAL_PROFILE_4M);
    return 0;
}

#include <marc/marc.h>

#include "dictionary/lzmw_format.hpp"
#include "frame/lzmw_rans_frame.hpp"
#include "frame/lzmw_rans_frame_streaming_encoder.hpp"
#include "frame/stream_header.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>

namespace {

using Phrase = marc::dictionary::internal::LzmwPhraseEntry;
using View = marc::entropy::internal::RansBlockView;

constexpr std::size_t maximum_fuzz_input = 8192;
constexpr std::size_t maximum_total_output = 4096;
constexpr std::size_t maximum_frame = 1024;
constexpr std::size_t maximum_payload = 16384;
constexpr std::size_t maximum_dictionary = 4096;
constexpr std::size_t maximum_phrases = maximum_frame - 1;
constexpr std::size_t maximum_expansion = maximum_phrases + 1;
constexpr std::size_t maximum_blocks = 8;
constexpr std::size_t maximum_encoded_frame =
    marc::frame::frame_header_size
    + maximum_blocks * marc::entropy::internal::rans_descriptor_size
    + maximum_payload;
constexpr std::size_t maximum_phrase_offset =
    maximum_blocks * sizeof(View) + alignof(Phrase) - 1;
constexpr std::size_t maximum_expansion_offset =
    maximum_phrase_offset + maximum_phrases * sizeof(Phrase)
    + alignof(std::uint32_t) - 1;
constexpr std::size_t maximum_views = maximum_expansion_offset
    + maximum_expansion * sizeof(std::uint32_t);
constexpr std::size_t maximum_views_alignment =
    std::max({alignof(View), alignof(Phrase), alignof(std::uint32_t)});
constexpr std::size_t maximum_internal = maximum_encoded_frame
    + maximum_dictionary + maximum_frame + maximum_views;
constexpr std::size_t maximum_primary =
    marc::frame::frame_header_size + maximum_internal;
constexpr std::size_t maximum_secondary =
    maximum_dictionary + maximum_frame;

[[nodiscard]] marc::core::DecoderLimits fuzz_limits() noexcept {
    marc::core::DecoderLimits limits{};
    limits.max_total_output_size = maximum_total_output;
    limits.max_frame_size = maximum_frame;
    limits.max_block_size = maximum_frame;
    limits.max_compressed_payload_size = maximum_payload;
    limits.max_dictionary_serialized_size = maximum_dictionary;
    limits.max_internal_buffered_bytes = maximum_internal;
    limits.max_dictionary_entries = maximum_phrases;
    limits.max_blocks_per_frame = maximum_blocks;
    return limits;
}

void exercise_complete_frame(const std::span<const std::byte> input) noexcept {
    if (input.size() < marc::frame::lzmw_rans_stream_prefix_size) {
        return;
    }
    const auto limits = fuzz_limits();
    marc::frame::StreamHeader stream{};
    const std::span<const std::byte, marc::frame::stream_header_size> header{
        input.data(), marc::frame::stream_header_size};
    if (marc::frame::parse_stream_header(header, limits, stream)
            != marc::frame::StreamHeaderError::none
        || stream.dictionary_algorithm
               != marc::frame::DictionaryAlgorithm::lzmw
        || stream.dictionary_variant != 1
        || stream.entropy_algorithm != marc::frame::EntropyAlgorithm::rans
        || stream.entropy_variant != 1
        || stream.dictionary_parameters_size
               != marc::dictionary::internal::lzmw_parameter_size
        || stream.entropy_parameters_size != 0) {
        return;
    }
    marc::dictionary::internal::LzmwParameters parameters{};
    const std::span<const std::byte,
                    marc::dictionary::internal::lzmw_parameter_size>
        parameter_bytes{
            input.data() + marc::frame::stream_header_size,
            marc::dictionary::internal::lzmw_parameter_size};
    if (marc::dictionary::internal::parse_lzmw_parameters(
            parameter_bytes, limits, parameters)
        != marc::dictionary::internal::LzmwFormatError::none) {
        return;
    }
    std::array<View, maximum_blocks> views{};
    std::array<std::byte, maximum_dictionary> dictionary{};
    std::array<Phrase, maximum_phrases> phrases{};
    std::array<std::uint32_t, maximum_expansion> expansion{};
    std::array<std::byte, maximum_frame> raw{};
    static_cast<void>(marc::frame::decode_lzmw_rans_frame_to_staging(
        stream, parameters, limits, 0, 0,
        input.subspan(marc::frame::lzmw_rans_stream_prefix_size),
        views, dictionary, phrases, expansion, raw));
}

void exercise_public_streaming(
    const std::span<const std::byte> input) noexcept {
    marc_lzmw_rans_config config{};
    if (marc_lzmw_rans_config_init(MARC_DIRECTION_DECODE, &config)
        != MARC_STATUS_OK) {
        std::abort();
    }
    config.max_total_output_size = maximum_total_output;
    config.max_frame_size = maximum_frame;
    config.max_block_size = maximum_frame;
    config.max_compressed_payload_size = maximum_payload;
    config.max_dictionary_serialized_size = maximum_dictionary;
    config.max_internal_buffered_bytes = maximum_internal;
    config.max_dictionary_entries = maximum_phrases;
    config.max_blocks_per_frame = maximum_blocks;

    marc_workspace_requirements requirements{};
    if (marc_lzmw_rans_workspace_requirements(&config, &requirements)
            != MARC_STATUS_OK
        || requirements.primary_bytes > maximum_primary
        || requirements.secondary_bytes > maximum_secondary
        || requirements.views_bytes > maximum_views
        || requirements.views_alignment > maximum_views_alignment) {
        std::abort();
    }

    std::array<std::uint8_t, maximum_primary> primary{};
    std::array<std::uint8_t, maximum_secondary> secondary{};
    alignas(maximum_views_alignment)
        std::array<std::uint8_t, maximum_views> views{};
    marc_transform* decoder{};
    if (marc_lzmw_rans_create(
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
            decoder,
            {source_data, chunk_size},
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
    exercise_public_streaming(input);
    return 0;
}

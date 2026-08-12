
#include "frame/lzss_tans_profile.hpp"

#include "core/checked_math.hpp"
#include "dictionary/lzss_hash_chain_match_finder.hpp"
#include "entropy/tans_format.hpp"

#include <algorithm>
#include <limits>

namespace marc::frame {
namespace {

inline constexpr std::uint64_t profile_max_frame_size = UINT64_C(1) << 20;
inline constexpr std::uint64_t dictionary_bytes_per_raw_byte = 2;

[[nodiscard]] bool to_size(
    const std::uint64_t value, std::size_t& result) noexcept {
    if (value > std::numeric_limits<std::size_t>::max()) {
        return false;
    }
    result = static_cast<std::size_t>(value);
    return true;
}

[[nodiscard]] bool block_payload_ceiling(
    const std::uint64_t symbol_count,
    std::uint64_t& payload_size) noexcept {
    std::uint64_t bits{};
    if (!core::checked_multiply(symbol_count, UINT64_C(12), bits)
        || !core::checked_add(bits, UINT64_C(7), bits)) {
        return false;
    }
    return core::checked_add(
        bits / 8,
        static_cast<std::uint64_t>(
            entropy::internal::tans_min_payload_size),
        payload_size);
}

[[nodiscard]] bool payload_ceiling(
    const std::uint64_t symbol_count,
    const std::uint64_t block_size,
    std::uint64_t& payload_size) noexcept {
    const auto full_blocks = symbol_count / block_size;
    const auto final_symbols = symbol_count % block_size;
    std::uint64_t full_payload{};
    std::uint64_t full_payloads{};
    std::uint64_t final_payload{};
    if (!block_payload_ceiling(block_size, full_payload)
        || !core::checked_multiply(
            full_blocks, full_payload, full_payloads)
        || (final_symbols != 0
            && !block_payload_ceiling(final_symbols, final_payload))) {
        return false;
    }
    return core::checked_add(
        full_payloads, final_payload, payload_size);
}

} // namespace

LzssTansProfileError make_lzss_tans_profile(
    const LzssTansProfileConfig& config,
    const core::DecoderLimits& limits,
    StreamHeader& stream,
    LzssTansEncoderWorkspaceRequirements& workspace) noexcept {
    stream = {};
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none
        || config.frame_size == 0 || config.entropy_block_size == 0) {
        return LzssTansProfileError::invalid_configuration;
    }
    const auto parameter_error =
        dictionary::internal::validate_lzss_parameters(
            config.parameters, limits);
    if (parameter_error != dictionary::internal::LzssFormatError::none) {
        return parameter_error
                   == dictionary::internal::LzssFormatError::limit_exceeded
            ? LzssTansProfileError::limit_exceeded
            : LzssTansProfileError::invalid_configuration;
    }
    if (config.original_size > limits.max_total_output_size
        || config.frame_size > limits.max_frame_size
        || config.frame_size > profile_max_frame_size
        || config.entropy_block_size > limits.max_block_size
        || config.entropy_block_size
               > entropy::internal::tans_max_block_size) {
        return LzssTansProfileError::limit_exceeded;
    }

    stream.dictionary_algorithm = DictionaryAlgorithm::lzss;
    stream.dictionary_variant = 1;
    stream.entropy_algorithm = EntropyAlgorithm::tans;
    stream.entropy_variant = 1;
    stream.frame_size = config.frame_size;
    stream.entropy_block_size = config.entropy_block_size;
    stream.dictionary_parameters_size =
        dictionary::internal::lzss_parameter_size;
    stream.original_size = config.original_size;
    if (validate_stream_header(stream, limits) != StreamHeaderError::none) {
        return LzssTansProfileError::unsupported;
    }

    const auto largest_frame = std::min<std::uint64_t>(
        config.original_size, config.frame_size);
    if (largest_frame == 0) {
        return LzssTansProfileError::none;
    }

    std::size_t largest_frame_size{};
    if (!to_size(largest_frame, largest_frame_size)) {
        return LzssTansProfileError::arithmetic_overflow;
    }
    const auto finder = dictionary::internal::
        calculate_lzss_hash_chain_workspace(
            largest_frame_size, config.parameters, limits);
    if (finder.error != dictionary::internal::LzssHashChainError::none) {
        return finder.error
                == dictionary::internal::LzssHashChainError::
                    workspace_limit_exceeded
            ? LzssTansProfileError::limit_exceeded
            : LzssTansProfileError::arithmetic_overflow;
    }

    std::uint64_t dictionary_bytes{};
    if (!core::checked_multiply(
            largest_frame, dictionary_bytes_per_raw_byte,
            dictionary_bytes)) {
        return LzssTansProfileError::arithmetic_overflow;
    }
    const auto block_count =
        UINT64_C(1)
        + (dictionary_bytes - 1) / config.entropy_block_size;
    if (block_count > limits.max_blocks_per_frame
        || block_count > std::numeric_limits<std::uint32_t>::max()) {
        return LzssTansProfileError::limit_exceeded;
    }

    std::uint64_t descriptor_bytes{};
    std::uint64_t payload_bytes{};
    std::uint64_t entropy_buffered_bytes{};
    std::uint64_t frame_encoded_bytes{frame_header_size};
    std::uint64_t aggregate_bytes{};
    if (!core::checked_multiply(
            block_count,
            static_cast<std::uint64_t>(
                entropy::internal::tans_descriptor_size),
            descriptor_bytes)
        || !payload_ceiling(
            dictionary_bytes, config.entropy_block_size, payload_bytes)
        || !core::checked_add(
            descriptor_bytes, payload_bytes, entropy_buffered_bytes)
        || !core::checked_add(
            frame_encoded_bytes, entropy_buffered_bytes,
            frame_encoded_bytes)
        || !core::checked_add(
            largest_frame, dictionary_bytes, aggregate_bytes)
        || !core::checked_add(
            aggregate_bytes,
            static_cast<std::uint64_t>(finder.workspace_size),
            aggregate_bytes)
        || !core::checked_add(
            aggregate_bytes, frame_encoded_bytes, aggregate_bytes)) {
        return LzssTansProfileError::arithmetic_overflow;
    }
    if (descriptor_bytes > std::numeric_limits<std::uint32_t>::max()
        || payload_bytes > std::numeric_limits<std::uint32_t>::max()
        || dictionary_bytes > std::numeric_limits<std::uint32_t>::max()
        || dictionary_bytes > limits.max_dictionary_serialized_size
        || payload_bytes > limits.max_compressed_payload_size
        || entropy_buffered_bytes > limits.max_internal_buffered_bytes
        || aggregate_bytes > limits.max_internal_buffered_bytes) {
        return LzssTansProfileError::limit_exceeded;
    }
    if (!to_size(largest_frame, workspace.frame_input_bytes)
        || !to_size(
            dictionary_bytes, workspace.dictionary_staging_bytes)
        || !to_size(frame_encoded_bytes, workspace.frame_encoded_bytes)) {
        workspace = {};
        return LzssTansProfileError::arithmetic_overflow;
    }
    workspace.match_finder_bytes = finder.workspace_size;
    workspace.match_finder_alignment = finder.workspace_size == 0
        ? 1 : finder.workspace_alignment;
    return LzssTansProfileError::none;
}

LzssTansProfileError calculate_lzss_tans_decoder_workspace(
    const core::DecoderLimits& limits,
    LzssTansDecoderWorkspaceRequirements& workspace) noexcept {
    workspace = {};
    if (core::validate_limits(limits) != core::LimitError::none) {
        return LzssTansProfileError::invalid_configuration;
    }
    const auto raw_bytes = std::min<std::uint64_t>(
        limits.max_frame_size, profile_max_frame_size);
    std::uint64_t profile_dictionary_bytes{};
    if (!core::checked_multiply(
            raw_bytes, dictionary_bytes_per_raw_byte,
            profile_dictionary_bytes)) {
        return LzssTansProfileError::arithmetic_overflow;
    }
    const auto dictionary_bytes = std::min<std::uint64_t>(
        limits.max_dictionary_serialized_size,
        profile_dictionary_bytes);
    std::uint64_t encoded_bytes{};
    if (!core::checked_add(
            static_cast<std::uint64_t>(frame_header_size),
            limits.max_internal_buffered_bytes, encoded_bytes)
        || !to_size(encoded_bytes, workspace.frame_encoded_bytes)
        || !to_size(
            dictionary_bytes, workspace.dictionary_staging_bytes)
        || !to_size(raw_bytes, workspace.frame_decoded_bytes)
        || !to_size(
            limits.max_blocks_per_frame, workspace.block_view_count)) {
        workspace = {};
        return LzssTansProfileError::arithmetic_overflow;
    }
    return LzssTansProfileError::none;
}

core::ErrorCode lzss_tans_profile_error_code(
    const LzssTansProfileError error) noexcept {
    switch (error) {
    case LzssTansProfileError::none:
        return core::ErrorCode::none;
    case LzssTansProfileError::invalid_configuration:
        return core::ErrorCode::invalid_argument;
    case LzssTansProfileError::unsupported:
        return core::ErrorCode::unsupported;
    case LzssTansProfileError::limit_exceeded:
    case LzssTansProfileError::arithmetic_overflow:
        return core::ErrorCode::limit_exceeded;
    }
    return core::ErrorCode::internal_error;
}

} // namespace marc::frame

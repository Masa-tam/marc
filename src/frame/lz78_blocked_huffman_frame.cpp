#include "frame/lz78_blocked_huffman_frame.hpp"

#include "core/checked_math.hpp"

namespace marc::frame {
namespace {

[[nodiscard]] bool supported_pipeline(const StreamHeader &stream) noexcept {
  return stream.dictionary_algorithm == DictionaryAlgorithm::lz78 &&
         stream.dictionary_variant == 1 &&
         stream.entropy_algorithm == EntropyAlgorithm::blocked_huffman &&
         stream.entropy_variant == 1 &&
         stream.dictionary_parameters_size ==
             dictionary::internal::lz78_parameter_size &&
         stream.entropy_parameters_size == 0;
}

} // namespace

Lz78BlockedHuffmanFrameValidationResult validate_lz78_blocked_huffman_frame(
    const StreamHeader &stream,
    const dictionary::internal::Lz78Parameters &parameters,
    const core::DecoderLimits &limits, const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<entropy::internal::BlockedHuffmanBlockView> views,
    const std::span<std::byte> dictionary_staging,
    const std::span<dictionary::internal::Lz78PhraseEntry>
        phrase_workspace) noexcept {
  Lz78BlockedHuffmanFrameValidationResult result{};
  if (validate_stream_header(stream, limits) != StreamHeaderError::none ||
      !supported_pipeline(stream) ||
      dictionary::internal::validate_lz78_parameters(parameters, limits) !=
          dictionary::internal::Lz78FormatError::none) {
    result.error = Lz78BlockedHuffmanFrameValidationError::unsupported_pipeline;
    return result;
  }
  if (input.size() < frame_header_size) {
    result.error = Lz78BlockedHuffmanFrameValidationError::truncated_frame;
    return result;
  }

  FrameHeader header{};
  const std::span<const std::byte, frame_header_size> encoded_header{
      input.data(), frame_header_size};
  const FrameValidationContext context{stream, limits, expected_sequence,
                                       output_already_committed};
  result.header_error = parse_frame_header(encoded_header, context, header);
  if (result.header_error != FrameHeaderError::none) {
    result.error = Lz78BlockedHuffmanFrameValidationError::header_error;
    return result;
  }

  result.dictionary_size = header.dictionary_serialized_size;
  result.raw_size = header.uncompressed_size;
  result.block_count = header.entropy_block_count;
  result.descriptor_size = header.block_descriptors_size;
  result.payload_size = header.compressed_payload_size;
  result.phrase_entries =
      dictionary::internal::lz78_validation_workspace_entries(
          result.dictionary_size, parameters);
  if (!core::checked_add(frame_header_size, result.descriptor_size,
                         result.serialized_size) ||
      !core::checked_add(result.serialized_size, result.payload_size,
                         result.serialized_size)) {
    result.error = Lz78BlockedHuffmanFrameValidationError::arithmetic_overflow;
    return result;
  }
  if (input.size() < result.serialized_size) {
    result.error = Lz78BlockedHuffmanFrameValidationError::truncated_frame;
    return result;
  }
  if (input.size() != result.serialized_size) {
    result.error = Lz78BlockedHuffmanFrameValidationError::trailing_frame_bytes;
    return result;
  }
  if (views.size() < result.block_count) {
    result.error =
        Lz78BlockedHuffmanFrameValidationError::view_output_too_small;
    return result;
  }
  if (dictionary_staging.size() < result.dictionary_size) {
    result.error =
        Lz78BlockedHuffmanFrameValidationError::dictionary_staging_too_small;
    return result;
  }
  if (phrase_workspace.size() < result.phrase_entries) {
    result.error =
        Lz78BlockedHuffmanFrameValidationError::phrase_workspace_too_small;
    return result;
  }

  std::uint64_t view_bytes{};
  std::uint64_t phrase_bytes{};
  std::uint64_t workspace_bytes{};
  if (!core::checked_multiply(static_cast<std::uint64_t>(result.block_count),
                              static_cast<std::uint64_t>(sizeof(
                                  entropy::internal::BlockedHuffmanBlockView)),
                              view_bytes) ||
      !core::checked_multiply(static_cast<std::uint64_t>(result.phrase_entries),
                              static_cast<std::uint64_t>(sizeof(
                                  dictionary::internal::Lz78PhraseEntry)),
                              phrase_bytes) ||
      !core::checked_add(static_cast<std::uint64_t>(result.descriptor_size),
                         static_cast<std::uint64_t>(result.payload_size),
                         workspace_bytes) ||
      !core::checked_add(workspace_bytes,
                         static_cast<std::uint64_t>(result.dictionary_size),
                         workspace_bytes) ||
      !core::checked_add(workspace_bytes, view_bytes, workspace_bytes) ||
      !core::checked_add(workspace_bytes, phrase_bytes, workspace_bytes)) {
    result.error = Lz78BlockedHuffmanFrameValidationError::arithmetic_overflow;
    return result;
  }
  if (workspace_bytes > limits.max_internal_buffered_bytes) {
    result.error = Lz78BlockedHuffmanFrameValidationError::workspace_limit;
    return result;
  }

  const auto descriptor_region =
      input.subspan(frame_header_size, result.descriptor_size);
  const auto payload_region = input.subspan(
      frame_header_size + result.descriptor_size, result.payload_size);
  const auto used_views = views.first(result.block_count);
  const auto controlled =
      entropy::internal::parse_blocked_huffman_descriptor_region(
          descriptor_region, header.dictionary_serialized_size,
          stream.entropy_block_size, header.entropy_block_count,
          header.compressed_payload_size, limits, used_views);
  if (controlled.error !=
      entropy::internal::BlockedHuffmanControllerError::none) {
    result.controller_error = controlled.error;
    result.error = Lz78BlockedHuffmanFrameValidationError::controller_error;
    return result;
  }

  const auto entropy_decoded = entropy::internal::decode_blocked_huffman_frame(
      descriptor_region, payload_region, used_views, limits,
      dictionary_staging.first(result.dictionary_size));
  if (entropy_decoded.error !=
      entropy::internal::BlockedHuffmanFrameDecodeError::none) {
    result.entropy_error = entropy_decoded.error;
    result.error = Lz78BlockedHuffmanFrameValidationError::entropy_decode_error;
    return result;
  }

  const auto dictionary_validated =
      dictionary::internal::validate_lz78_token_stream(
          dictionary_staging.first(result.dictionary_size), parameters,
          header.uncompressed_size, limits,
          phrase_workspace.first(result.phrase_entries));
  if (dictionary_validated.error !=
      dictionary::internal::Lz78ValidationError::none) {
    result.dictionary_error = dictionary_validated.error;
    result.dictionary_format_error = dictionary_validated.format_error;
    result.error =
        Lz78BlockedHuffmanFrameValidationError::dictionary_validation_error;
  }
  return result;
}

Lz78BlockedHuffmanFrameValidationResult decode_lz78_blocked_huffman_frame(
    const StreamHeader &stream,
    const dictionary::internal::Lz78Parameters &parameters,
    const core::DecoderLimits &limits, const std::uint64_t expected_sequence,
    const std::uint64_t output_already_committed,
    const std::span<const std::byte> input,
    const std::span<entropy::internal::BlockedHuffmanBlockView> views,
    const std::span<std::byte> dictionary_staging,
    const std::span<dictionary::internal::Lz78PhraseEntry> phrase_workspace,
    const std::span<std::byte> output) noexcept {
  auto result = validate_lz78_blocked_huffman_frame(
      stream, parameters, limits, expected_sequence, output_already_committed,
      input, views, dictionary_staging, phrase_workspace);
  if (result.error != Lz78BlockedHuffmanFrameValidationError::none)
    return result;
  if (output.size() < result.raw_size) {
    result.error = Lz78BlockedHuffmanFrameValidationError::raw_output_too_small;
    return result;
  }

  const auto decoded = dictionary::internal::decode_lz78_token_stream(
      dictionary_staging.first(result.dictionary_size), parameters,
      result.raw_size, limits, phrase_workspace.first(result.phrase_entries),
      output.first(result.raw_size));
  result.dictionary_decode_error = decoded.error;
  if (decoded.error != dictionary::internal::Lz78DecodeError::none) {
    result.dictionary_error = decoded.validation_error;
    result.dictionary_format_error = decoded.format_error;
    result.error =
        Lz78BlockedHuffmanFrameValidationError::dictionary_decode_error;
  }
  return result;
}

} // namespace marc::frame

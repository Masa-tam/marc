#include "frame/lzw_blocked_huffman_profile.hpp"

#include "core/checked_math.hpp"
#include "entropy/blocked_huffman_format.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace marc::frame {
namespace {

[[nodiscard]] bool to_size(const std::uint64_t value,
                           std::size_t &result) noexcept {
  if (value > std::numeric_limits<std::size_t>::max())
    return false;
  result = static_cast<std::size_t>(value);
  return true;
}

[[nodiscard]] bool align_up(const std::size_t value,
                            const std::size_t alignment,
                            std::size_t &result) noexcept {
  if (alignment == 0)
    return false;
  const auto remainder = value % alignment;
  const auto padding = remainder == 0 ? std::size_t{0} : alignment - remainder;
  return core::checked_add(value, padding, result);
}

[[nodiscard]] bool aligned(const void *const pointer,
                           const std::size_t alignment) noexcept {
  return alignment != 0 &&
         reinterpret_cast<std::uintptr_t>(pointer) % alignment == 0;
}

[[nodiscard]] std::uint64_t
maximum_supported_entries(const std::uint64_t local_limit) noexcept {
  std::uint64_t result{};
  for (std::uint32_t width = dictionary::internal::lzw_minimum_code_width;
       width <= dictionary::internal::lzw_maximum_code_width; ++width) {
    const auto capacity =
        (UINT64_C(1) << width) - dictionary::internal::lzw_first_free_code;
    if (capacity > local_limit)
      break;
    result = capacity;
  }
  return result;
}

[[nodiscard]] bool maximum_phrase_entries(const std::uint64_t payload_bytes,
                                          const std::uint64_t capacity,
                                          std::uint64_t &entries) noexcept {
  std::uint64_t whole_codes{};
  if (!core::checked_multiply(payload_bytes / 9, UINT64_C(8), whole_codes))
    return false;
  const auto partial_codes = (payload_bytes % 9 * 8) / 9;
  std::uint64_t maximum_codes{};
  if (!core::checked_add(whole_codes, partial_codes, maximum_codes))
    return false;
  entries = maximum_codes == 0 ? 0 : std::min(maximum_codes - 1, capacity);
  return true;
}

[[nodiscard]] bool decoder_views_layout(const std::size_t block_count,
                                        const std::size_t phrase_count,
                                        std::size_t &phrase_offset,
                                        std::size_t &total_bytes,
                                        std::size_t &alignment) noexcept {
  using Block = entropy::internal::BlockedHuffmanBlockView;
  using Phrase = dictionary::internal::LzwPhraseEntry;
  alignment = std::max(alignof(Block), alignof(Phrase));
  std::size_t block_bytes{};
  std::size_t phrase_bytes{};
  return core::checked_multiply(block_count, sizeof(Block), block_bytes) &&
         align_up(block_bytes, alignof(Phrase), phrase_offset) &&
         core::checked_multiply(phrase_count, sizeof(Phrase), phrase_bytes) &&
         core::checked_add(phrase_offset, phrase_bytes, total_bytes);
}

} // namespace

LzwBlockedHuffmanProfileError make_lzw_blocked_huffman_profile(
    const LzwBlockedHuffmanProfileConfig &config,
    const core::DecoderLimits &limits, StreamHeader &stream,
    LzwBlockedHuffmanEncoderWorkspaceRequirements &workspace) noexcept {
  stream = {};
  workspace = {};
  if (core::validate_limits(limits) != core::LimitError::none ||
      config.frame_size == 0 || config.entropy_block_size == 0) {
    return LzwBlockedHuffmanProfileError::invalid_configuration;
  }
  const auto parameter_error =
      dictionary::internal::validate_lzw_parameters(config.parameters, limits);
  if (parameter_error != dictionary::internal::LzwFormatError::none) {
    return parameter_error ==
                   dictionary::internal::LzwFormatError::limit_exceeded
               ? LzwBlockedHuffmanProfileError::limit_exceeded
               : LzwBlockedHuffmanProfileError::invalid_configuration;
  }
  if (config.original_size > limits.max_total_output_size ||
      config.frame_size > limits.max_frame_size ||
      config.entropy_block_size > limits.max_block_size) {
    return LzwBlockedHuffmanProfileError::limit_exceeded;
  }

  stream.dictionary_algorithm = DictionaryAlgorithm::lzw;
  stream.dictionary_variant = 1;
  stream.entropy_algorithm = EntropyAlgorithm::blocked_huffman;
  stream.entropy_variant = 1;
  stream.frame_size = config.frame_size;
  stream.entropy_block_size = config.entropy_block_size;
  stream.dictionary_parameters_size = dictionary::internal::lzw_parameter_size;
  stream.original_size = config.original_size;
  if (validate_stream_header(stream, limits) != StreamHeaderError::none)
    return LzwBlockedHuffmanProfileError::unsupported;

  const auto largest_frame =
      std::min<std::uint64_t>(config.original_size, config.frame_size);
  if (largest_frame == 0) {
    workspace.views_alignment = 1;
    return LzwBlockedHuffmanProfileError::none;
  }
  const auto entry_capacity = static_cast<std::uint64_t>(
      dictionary::internal::lzw_code_limit(config.parameters) -
      dictionary::internal::lzw_first_free_code);
  const auto encoder_entries = std::min(largest_frame - 1, entry_capacity);

  std::uint64_t dictionary_bits{};
  std::uint64_t rounded_bits{};
  std::uint64_t dictionary_bytes{};
  std::uint64_t entry_bytes{};
  if (!core::checked_multiply(
          largest_frame,
          static_cast<std::uint64_t>(config.parameters.maximum_code_width),
          dictionary_bits) ||
      !core::checked_add(dictionary_bits, UINT64_C(7), rounded_bits) ||
      !core::checked_multiply(encoder_entries,
                              static_cast<std::uint64_t>(sizeof(
                                  dictionary::internal::LzwEncoderEntry)),
                              entry_bytes)) {
    return LzwBlockedHuffmanProfileError::arithmetic_overflow;
  }
  dictionary_bytes = rounded_bits / 8;
  const auto block_count =
      UINT64_C(1) + (dictionary_bytes - 1) / config.entropy_block_size;
  if (block_count > limits.max_blocks_per_frame)
    return LzwBlockedHuffmanProfileError::limit_exceeded;

  std::uint64_t descriptor_bytes{};
  std::uint64_t entropy_bytes{};
  std::uint64_t encoded_bytes{frame_header_size};
  std::uint64_t aggregate_bytes{};
  if (!core::checked_multiply(
          block_count,
          static_cast<std::uint64_t>(
              entropy::internal::blocked_huffman_descriptor_size),
          descriptor_bytes) ||
      !core::checked_add(descriptor_bytes, dictionary_bytes, entropy_bytes) ||
      !core::checked_add(encoded_bytes, entropy_bytes, encoded_bytes) ||
      !core::checked_add(largest_frame, dictionary_bytes, aggregate_bytes) ||
      !core::checked_add(aggregate_bytes, encoded_bytes, aggregate_bytes) ||
      !core::checked_add(aggregate_bytes, entry_bytes, aggregate_bytes)) {
    return LzwBlockedHuffmanProfileError::arithmetic_overflow;
  }
  if (dictionary_bytes > limits.max_dictionary_serialized_size ||
      dictionary_bytes > limits.max_compressed_payload_size ||
      entropy_bytes > limits.max_internal_buffered_bytes ||
      aggregate_bytes > limits.max_internal_buffered_bytes) {
    return LzwBlockedHuffmanProfileError::limit_exceeded;
  }
  if (!to_size(largest_frame, workspace.frame_input_bytes) ||
      !to_size(dictionary_bytes, workspace.dictionary_staging_bytes) ||
      !to_size(encoded_bytes, workspace.frame_encoded_bytes) ||
      !to_size(encoder_entries, workspace.encoder_entry_count) ||
      !to_size(entry_bytes, workspace.views_bytes)) {
    workspace = {};
    return LzwBlockedHuffmanProfileError::arithmetic_overflow;
  }
  workspace.views_alignment = encoder_entries == 0
                                  ? 1
                                  : alignof(
                                        dictionary::internal::LzwEncoderEntry);
  return LzwBlockedHuffmanProfileError::none;
}

LzwBlockedHuffmanProfileError calculate_lzw_blocked_huffman_decoder_workspace(
    const core::DecoderLimits &limits,
    LzwBlockedHuffmanDecoderWorkspaceRequirements &workspace) noexcept {
  workspace = {};
  if (core::validate_limits(limits) != core::LimitError::none)
    return LzwBlockedHuffmanProfileError::invalid_configuration;
  const auto maximum_entries =
      maximum_supported_entries(limits.max_dictionary_entries);
  if (maximum_entries == 0)
    return LzwBlockedHuffmanProfileError::limit_exceeded;
  std::uint64_t phrase_entries{};
  std::uint64_t encoded_bytes{};
  if (!maximum_phrase_entries(limits.max_dictionary_serialized_size,
                              maximum_entries, phrase_entries) ||
      !core::checked_add(static_cast<std::uint64_t>(frame_header_size),
                         limits.max_internal_buffered_bytes, encoded_bytes) ||
      !to_size(encoded_bytes, workspace.frame_encoded_bytes) ||
      !to_size(limits.max_dictionary_serialized_size,
               workspace.dictionary_staging_bytes) ||
      !to_size(limits.max_frame_size, workspace.frame_decoded_bytes) ||
      !to_size(limits.max_blocks_per_frame, workspace.block_view_count) ||
      !to_size(phrase_entries, workspace.phrase_entry_count) ||
      !decoder_views_layout(workspace.block_view_count,
                            workspace.phrase_entry_count,
                            workspace.phrase_offset, workspace.views_bytes,
                            workspace.views_alignment)) {
    workspace = {};
    return LzwBlockedHuffmanProfileError::arithmetic_overflow;
  }
  return LzwBlockedHuffmanProfileError::none;
}

LzwBlockedHuffmanWorkspaceError partition_lzw_blocked_huffman_encoder_views(
    const LzwBlockedHuffmanEncoderWorkspaceRequirements &requirements,
    const std::span<std::byte> storage,
    LzwBlockedHuffmanEncoderViews &views) noexcept {
  views = {};
  std::size_t expected_bytes{};
  if (!core::checked_multiply(requirements.encoder_entry_count,
                              sizeof(dictionary::internal::LzwEncoderEntry),
                              expected_bytes)) {
    return LzwBlockedHuffmanWorkspaceError::arithmetic_overflow;
  }
  if (expected_bytes == 0) {
    return requirements.views_bytes == 0 && requirements.views_alignment == 1
               ? LzwBlockedHuffmanWorkspaceError::none
               : LzwBlockedHuffmanWorkspaceError::invalid_requirements;
  }
  if (expected_bytes != requirements.views_bytes ||
      requirements.views_alignment !=
          alignof(dictionary::internal::LzwEncoderEntry)) {
    return LzwBlockedHuffmanWorkspaceError::invalid_requirements;
  }
  if (storage.size() < expected_bytes)
    return LzwBlockedHuffmanWorkspaceError::too_small;
  if (!aligned(storage.data(), requirements.views_alignment))
    return LzwBlockedHuffmanWorkspaceError::misaligned;
  views.entries = {
      reinterpret_cast<dictionary::internal::LzwEncoderEntry *>(storage.data()),
      requirements.encoder_entry_count};
  return LzwBlockedHuffmanWorkspaceError::none;
}

LzwBlockedHuffmanWorkspaceError partition_lzw_blocked_huffman_decoder_views(
    const LzwBlockedHuffmanDecoderWorkspaceRequirements &requirements,
    const std::span<std::byte> storage,
    LzwBlockedHuffmanDecoderViews &views) noexcept {
  views = {};
  std::size_t phrase_offset{};
  std::size_t expected_bytes{};
  std::size_t expected_alignment{};
  if (!decoder_views_layout(requirements.block_view_count,
                            requirements.phrase_entry_count, phrase_offset,
                            expected_bytes, expected_alignment)) {
    return LzwBlockedHuffmanWorkspaceError::arithmetic_overflow;
  }
  if (phrase_offset != requirements.phrase_offset ||
      expected_bytes != requirements.views_bytes ||
      expected_alignment != requirements.views_alignment) {
    return LzwBlockedHuffmanWorkspaceError::invalid_requirements;
  }
  if (storage.size() < expected_bytes)
    return LzwBlockedHuffmanWorkspaceError::too_small;
  if (expected_bytes != 0 && !aligned(storage.data(), expected_alignment))
    return LzwBlockedHuffmanWorkspaceError::misaligned;
  auto *const bytes = storage.data();
  views.blocks = {
      reinterpret_cast<entropy::internal::BlockedHuffmanBlockView *>(bytes),
      requirements.block_view_count};
  views.phrases = {reinterpret_cast<dictionary::internal::LzwPhraseEntry *>(
                       bytes + phrase_offset),
                   requirements.phrase_entry_count};
  return LzwBlockedHuffmanWorkspaceError::none;
}

core::ErrorCode lzw_blocked_huffman_profile_error_code(
    const LzwBlockedHuffmanProfileError error) noexcept {
  switch (error) {
  case LzwBlockedHuffmanProfileError::none:
    return core::ErrorCode::none;
  case LzwBlockedHuffmanProfileError::invalid_configuration:
    return core::ErrorCode::invalid_argument;
  case LzwBlockedHuffmanProfileError::unsupported:
    return core::ErrorCode::unsupported;
  case LzwBlockedHuffmanProfileError::limit_exceeded:
  case LzwBlockedHuffmanProfileError::arithmetic_overflow:
    return core::ErrorCode::limit_exceeded;
  }
  return core::ErrorCode::internal_error;
}

} // namespace marc::frame

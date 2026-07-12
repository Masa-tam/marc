#include "frame/blocked_huffman_incremental_encoder.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace marc::frame {
namespace {

constexpr std::uint32_t known_flags =
    core::flag_value(core::ProcessFlags::flush)
    | core::flag_value(core::ProcessFlags::end_input)
    | core::flag_value(core::ProcessFlags::reset_block);

} // namespace

BlockedHuffmanIncrementalEncoder::BlockedHuffmanIncrementalEncoder(
    const StreamHeader stream,
    const core::DecoderLimits limits,
    const std::span<std::byte> input_storage,
    const std::span<std::byte> encoded_storage) noexcept
    : stream_(stream),
      limits_(limits),
      input_storage_(input_storage),
      encoded_storage_(encoded_storage) {
    if (validate_stream_header(stream_, limits_) != StreamHeaderError::none
        || stream_.dictionary_algorithm != DictionaryAlgorithm::none
        || stream_.entropy_algorithm != EntropyAlgorithm::blocked_huffman
        || stream_.entropy_variant != 1
        || stream_.original_size > input_storage_.size()
        || stream_.original_size > std::numeric_limits<std::size_t>::max()) {
        state_ = State::error;
        terminal_error_ = {core::ErrorCode::invalid_argument, 0, 0};
    }
}

core::ProcessResult BlockedHuffmanIncrementalEncoder::fail(
    const core::ErrorCode code) noexcept {
    state_ = State::error;
    terminal_error_ = {
        code, static_cast<std::uint64_t>(input_received_), 0};
    return {0, 0, core::StreamStatus::error, terminal_error_};
}

core::ProcessResult BlockedHuffmanIncrementalEncoder::drain(
    const std::span<std::byte> output,
    const std::size_t input_consumed) noexcept {
    const auto remaining = encoded_size_ - output_offset_;
    const auto produced = std::min(remaining, output.size());
    if (produced != 0) {
        std::memmove(output.data(), encoded_storage_.data() + output_offset_,
                     produced);
        output_offset_ += produced;
    }
    if (output_offset_ == encoded_size_) {
        state_ = State::ended;
        return {input_consumed, produced,
                core::StreamStatus::end_of_stream, {}};
    }
    return {input_consumed, produced,
            output.size() == produced
                ? core::StreamStatus::need_output
                : core::StreamStatus::progress,
            {}};
}

core::ProcessResult BlockedHuffmanIncrementalEncoder::process(
    const std::span<const std::byte> input,
    const std::span<std::byte> output,
    const std::uint32_t flags) noexcept {
    if (state_ == State::error) {
        return {0, 0, core::StreamStatus::error, terminal_error_};
    }
    if (state_ == State::ended) {
        return {0, 0, core::StreamStatus::end_of_stream, {}};
    }
    if ((flags & ~known_flags) != 0
        || (flags & core::flag_value(core::ProcessFlags::reset_block)) != 0) {
        return fail(core::ErrorCode::unsupported);
    }
    if (state_ == State::draining) {
        if (!input.empty()) {
            return fail(core::ErrorCode::invalid_argument);
        }
        return drain(output, 0);
    }

    const auto original_size = static_cast<std::size_t>(stream_.original_size);
    const auto remaining_input = original_size - input_received_;
    if (input.size() > remaining_input) {
        return fail(core::ErrorCode::invalid_argument);
    }
    if (!input.empty()) {
        std::memmove(input_storage_.data() + input_received_,
                     input.data(), input.size());
        input_received_ += input.size();
    }
    const bool end_input =
        (flags & core::flag_value(core::ProcessFlags::end_input)) != 0;
    if (!end_input) {
        return {input.size(), 0,
                input.empty() ? core::StreamStatus::need_input
                              : core::StreamStatus::progress,
                {}};
    }
    if (input_received_ != original_size) {
        auto result = fail(core::ErrorCode::invalid_argument);
        result.input_consumed = input.size();
        return result;
    }

    const auto plan = plan_blocked_huffman_stream(
        stream_, limits_, input_storage_.first(input_received_));
    if (plan.error != BlockedHuffmanStreamCodecError::none) {
        auto result = fail(core::ErrorCode::internal_error);
        result.input_consumed = input.size();
        return result;
    }
    if (encoded_storage_.size() < plan.serialized_size) {
        auto result = fail(core::ErrorCode::out_of_memory);
        result.input_consumed = input.size();
        return result;
    }
    const auto encoded = encode_blocked_huffman_stream(
        stream_, limits_, input_storage_.first(input_received_),
        encoded_storage_.first(plan.serialized_size));
    if (encoded.error != BlockedHuffmanStreamCodecError::none) {
        auto result = fail(core::ErrorCode::internal_error);
        result.input_consumed = input.size();
        return result;
    }
    encoded_size_ = plan.serialized_size;
    state_ = State::draining;
    return drain(output, input.size());
}

} // namespace marc::frame

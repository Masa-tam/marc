#include "frame/blocked_huffman_incremental_decoder.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace marc::frame {
namespace {

constexpr std::uint32_t known_flags =
    core::flag_value(core::ProcessFlags::flush)
    | core::flag_value(core::ProcessFlags::end_input)
    | core::flag_value(core::ProcessFlags::reset_block);

} // namespace

BlockedHuffmanIncrementalDecoder::BlockedHuffmanIncrementalDecoder(
    const core::DecoderLimits limits,
    const std::span<std::byte> encoded_storage,
    const std::span<std::byte> decoded_storage,
    const std::span<entropy::internal::BlockedHuffmanBlockView> frame_views)
    noexcept
    : limits_(limits),
      encoded_storage_(encoded_storage),
      decoded_storage_(decoded_storage),
      frame_views_(frame_views) {
    if (core::validate_limits(limits_) != core::LimitError::none) {
        state_ = State::error;
        terminal_error_ = {core::ErrorCode::invalid_argument, 0, 0};
    }
}

core::ProcessResult BlockedHuffmanIncrementalDecoder::fail(
    const core::ErrorCode code) noexcept {
    state_ = State::error;
    terminal_error_ = {
        code, static_cast<std::uint64_t>(input_received_), 0};
    return {0, 0, core::StreamStatus::error, terminal_error_};
}

core::ProcessResult BlockedHuffmanIncrementalDecoder::drain(
    const std::span<std::byte> output,
    const std::size_t input_consumed) noexcept {
    const auto remaining = decoded_size_ - output_offset_;
    const auto produced = std::min(remaining, output.size());
    if (produced != 0) {
        std::memmove(output.data(), decoded_storage_.data() + output_offset_,
                     produced);
        output_offset_ += produced;
    }
    if (output_offset_ == decoded_size_) {
        state_ = State::ended;
        return {input_consumed, produced,
                core::StreamStatus::end_of_stream, {}};
    }
    return {input_consumed, produced, core::StreamStatus::need_output, {}};
}

core::ProcessResult BlockedHuffmanIncrementalDecoder::process(
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
    if (input.size() > encoded_storage_.size() - input_received_) {
        return fail(core::ErrorCode::out_of_memory);
    }
    if (!input.empty()) {
        std::memmove(encoded_storage_.data() + input_received_,
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

    if (input_received_ < stream_header_size) {
        auto result = fail(core::ErrorCode::malformed_stream);
        result.input_consumed = input.size();
        return result;
    }
    const std::span<const std::byte, stream_header_size> header_bytes{
        encoded_storage_.data(), stream_header_size};
    StreamHeader parsed{};
    if (parse_stream_header(header_bytes, limits_, parsed)
        != StreamHeaderError::none) {
        auto result = fail(core::ErrorCode::malformed_stream);
        result.input_consumed = input.size();
        return result;
    }
    if (parsed.original_size > decoded_storage_.size()) {
        auto result = fail(core::ErrorCode::out_of_memory);
        result.input_consumed = input.size();
        return result;
    }

    StreamHeader decoded_stream{};
    const auto decoded = decode_blocked_huffman_stream(
        encoded_storage_.first(input_received_), limits_, frame_views_,
        decoded_storage_.first(static_cast<std::size_t>(parsed.original_size)),
        decoded_stream);
    if (decoded.error != BlockedHuffmanStreamCodecError::none) {
        const auto code =
            decoded.error == BlockedHuffmanStreamCodecError::view_output_too_small
                || decoded.error == BlockedHuffmanStreamCodecError::output_too_small
            ? core::ErrorCode::out_of_memory
            : core::ErrorCode::malformed_stream;
        auto result = fail(code);
        result.input_consumed = input.size();
        return result;
    }
    decoded_size_ = static_cast<std::size_t>(parsed.original_size);
    state_ = State::draining;
    return drain(output, input.size());
}

} // namespace marc::frame

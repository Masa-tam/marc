#include "dictionary/lzd_streaming_decoder.hpp"

#include "core/checked_math.hpp"

#include <algorithm>
#include <cstring>
#include <limits>

namespace marc::dictionary::internal {
namespace {

constexpr std::uint32_t known_flags =
    core::flag_value(core::ProcessFlags::flush)
    | core::flag_value(core::ProcessFlags::end_input)
    | core::flag_value(core::ProcessFlags::reset_block);

[[nodiscard]] core::ErrorCode decode_error_code(
    const LzdDecodeError error) noexcept {
    switch (error) {
    case LzdDecodeError::none:
        return core::ErrorCode::none;
    case LzdDecodeError::invalid_token_stream:
        return core::ErrorCode::malformed_stream;
    case LzdDecodeError::limit_exceeded:
    case LzdDecodeError::output_size_unsupported:
        return core::ErrorCode::limit_exceeded;
    case LzdDecodeError::output_too_small:
    case LzdDecodeError::expansion_workspace_too_small:
        return core::ErrorCode::out_of_memory;
    case LzdDecodeError::internal_error:
        return core::ErrorCode::internal_error;
    }
    return core::ErrorCode::internal_error;
}

} // namespace

LzdStreamingDecoderWorkspaceRequirements
lzd_streaming_decoder_workspace_requirements(
    const std::uint64_t declared_frame_size,
    const LzdParameters& parameters) noexcept {
    LzdStreamingDecoderWorkspaceRequirements result{};
    if (declared_frame_size
        > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
        return result;
    std::uint64_t token_count = declared_frame_size / 2;
    if ((declared_frame_size & 1U) != 0
        && !core::checked_add(token_count, UINT64_C(1), token_count))
        return result;
    std::uint64_t encoded_bytes{};
    if (!core::checked_multiply(
            token_count, static_cast<std::uint64_t>(lzd_token_size),
            encoded_bytes)
        || encoded_bytes
            > static_cast<std::uint64_t>(
                std::numeric_limits<std::size_t>::max()))
        return result;

    result.encoded_bytes = static_cast<std::size_t>(encoded_bytes);
    result.phrase_entries = std::min(
        static_cast<std::size_t>(token_count),
        static_cast<std::size_t>(parameters.maximum_entries));
    result.expansion_entries = lzd_expansion_workspace_entries(
        result.phrase_entries, declared_frame_size != 0);
    result.decoded_bytes = static_cast<std::size_t>(declared_frame_size);
    result.supported = true;
    return result;
}

LzdStreamingDecoder::LzdStreamingDecoder(
    const LzdParameters parameters,
    const std::uint64_t declared_frame_size,
    const core::DecoderLimits limits,
    const std::span<std::byte> encoded_workspace,
    const std::span<LzdPhraseEntry> phrase_workspace,
    const std::span<std::uint32_t> expansion_workspace,
    const std::span<std::byte> decoded_workspace) noexcept
    : parameters_(parameters), limits_(limits),
      encoded_workspace_(encoded_workspace),
      phrase_workspace_(phrase_workspace),
      expansion_workspace_(expansion_workspace),
      decoded_workspace_(decoded_workspace),
      declared_frame_size_(declared_frame_size) {
    if (core::validate_limits(limits_) != core::LimitError::none
        || validate_lzd_parameters(parameters_, limits_)
               != LzdFormatError::none) {
        state_ = State::error;
        terminal_error_ = {core::ErrorCode::invalid_argument, 0, 0};
        return;
    }
    const auto required = lzd_streaming_decoder_workspace_requirements(
        declared_frame_size_, parameters_);
    if (!required.supported
        || declared_frame_size_ > limits_.max_frame_size
        || declared_frame_size_ > limits_.max_total_output_size
        || required.encoded_bytes
            > limits_.max_dictionary_serialized_size) {
        state_ = State::error;
        terminal_error_ = {core::ErrorCode::limit_exceeded, 0, 0};
        return;
    }

    std::uint64_t phrase_bytes{};
    std::uint64_t expansion_bytes{};
    std::uint64_t aggregate{};
    if (!core::checked_multiply(
            static_cast<std::uint64_t>(required.phrase_entries),
            static_cast<std::uint64_t>(sizeof(LzdPhraseEntry)), phrase_bytes)
        || !core::checked_multiply(
            static_cast<std::uint64_t>(required.expansion_entries),
            static_cast<std::uint64_t>(sizeof(std::uint32_t)),
            expansion_bytes)
        || !core::checked_add(
            static_cast<std::uint64_t>(required.encoded_bytes), phrase_bytes,
            aggregate)
        || !core::checked_add(aggregate, expansion_bytes, aggregate)
        || !core::checked_add(
            aggregate, static_cast<std::uint64_t>(required.decoded_bytes),
            aggregate)
        || aggregate > limits_.max_internal_buffered_bytes) {
        state_ = State::error;
        terminal_error_ = {core::ErrorCode::limit_exceeded, 0, 0};
        return;
    }
    if (encoded_workspace_.size() < required.encoded_bytes
        || phrase_workspace_.size() < required.phrase_entries
        || expansion_workspace_.size() < required.expansion_entries
        || decoded_workspace_.size() < required.decoded_bytes) {
        state_ = State::error;
        terminal_error_ = {core::ErrorCode::out_of_memory, 0, 0};
        return;
    }
}

core::ProcessResult LzdStreamingDecoder::fail(
    const core::ErrorCode code, const std::size_t consumed,
    const std::size_t produced) noexcept {
    state_ = State::error;
    terminal_error_ = {code, encoded_size_, 0};
    return {consumed, produced, core::StreamStatus::error, terminal_error_};
}

core::ProcessResult LzdStreamingDecoder::process(
    const std::span<const std::byte> input,
    const std::span<std::byte> output,
    const std::uint32_t flags) noexcept {
    if (state_ == State::error)
        return {0, 0, core::StreamStatus::error, terminal_error_};
    if (state_ == State::ended)
        return {0, 0, core::StreamStatus::end_of_stream, {}};
    if ((flags & ~known_flags) != 0
        || (flags & core::flag_value(core::ProcessFlags::reset_block)) != 0)
        return fail(core::ErrorCode::unsupported, 0, 0);
    const bool end_requested =
        (flags & core::flag_value(core::ProcessFlags::end_input)) != 0;
    std::size_t consumed{};
    std::size_t produced{};

    if (state_ == State::collecting) {
        if (input.size() > encoded_workspace_.size() - encoded_size_)
            return fail(core::ErrorCode::malformed_stream, 0, 0);
        if (!input.empty()) {
            std::memmove(encoded_workspace_.data() + encoded_size_,
                         input.data(), input.size());
            encoded_size_ += input.size();
            consumed = input.size();
        }
        if (!end_requested) {
            return {consumed, 0,
                    consumed != 0 ? core::StreamStatus::progress
                                  : core::StreamStatus::need_input,
                    {}};
        }

        const auto decoded = decode_lzd_token_stream(
            encoded_workspace_.first(encoded_size_), parameters_,
            declared_frame_size_, limits_, phrase_workspace_,
            expansion_workspace_,
            decoded_workspace_.first(
                static_cast<std::size_t>(declared_frame_size_)));
        if (decoded.error != LzdDecodeError::none) {
            state_ = State::error;
            const auto code = decode_error_code(decoded.error);
            const auto position =
                decoded.error == LzdDecodeError::invalid_token_stream
                ? static_cast<std::uint64_t>(decoded.input_offset)
                : static_cast<std::uint64_t>(encoded_size_);
            terminal_error_ = {code, position, 0};
            return {consumed, 0, core::StreamStatus::error,
                    terminal_error_};
        }
        state_ = State::draining;
    } else if (!input.empty()) {
        return fail(core::ErrorCode::malformed_stream, 0, 0);
    }

    const auto decoded_size = static_cast<std::size_t>(declared_frame_size_);
    const auto count = std::min(
        decoded_size - output_offset_, output.size());
    if (count != 0) {
        std::memmove(output.data(),
                     decoded_workspace_.data() + output_offset_, count);
        output_offset_ += count;
        produced = count;
    }
    if (output_offset_ == decoded_size) {
        state_ = State::ended;
        return {consumed, produced, core::StreamStatus::end_of_stream, {}};
    }
    return {consumed, produced, core::StreamStatus::need_output, {}};
}

} // namespace marc::dictionary::internal

#include "dictionary/lzmw_streaming_encoder.hpp"

#include "core/checked_math.hpp"

#include <algorithm>
#include <cstring>

namespace marc::dictionary::internal {
namespace {

constexpr std::uint32_t known_flags =
    core::flag_value(core::ProcessFlags::flush)
    | core::flag_value(core::ProcessFlags::end_input)
    | core::flag_value(core::ProcessFlags::reset_block);

} // namespace

LzmwStreamingEncoderWorkspaceRequirements
lzmw_streaming_encoder_workspace_requirements(
    const std::uint64_t declared_frame_size,
    const LzmwParameters& parameters) noexcept {
    LzmwStreamingEncoderWorkspaceRequirements result{};
    if (!lzmw_maximum_token_stream_size(
            declared_frame_size, result.encoded_bytes))
        return result;
    result.raw_bytes = static_cast<std::size_t>(declared_frame_size);
    result.dictionary_entries = lzmw_encoder_workspace_entries(
        result.raw_bytes, parameters);
    result.supported = true;
    return result;
}

LzmwStreamingEncoder::LzmwStreamingEncoder(
    const LzmwParameters parameters,
    const std::uint64_t declared_frame_size,
    const core::DecoderLimits limits,
    const std::span<std::byte> raw_storage,
    const std::span<std::byte> encoded_storage,
    const std::span<LzmwEncoderEntry> dictionary_workspace) noexcept
    : parameters_(parameters), limits_(limits), raw_storage_(raw_storage),
      encoded_storage_(encoded_storage),
      dictionary_workspace_(dictionary_workspace),
      declared_frame_size_(declared_frame_size) {
    if (core::validate_limits(limits_) != core::LimitError::none
        || validate_lzmw_parameters(parameters_, limits_)
               != LzmwFormatError::none) {
        state_ = State::error;
        terminal_error_ = {core::ErrorCode::invalid_argument, 0, 0};
        return;
    }
    const auto required = lzmw_streaming_encoder_workspace_requirements(
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

    std::uint64_t dictionary_bytes{};
    std::uint64_t aggregate{};
    if (!core::checked_multiply(
            static_cast<std::uint64_t>(required.dictionary_entries),
            static_cast<std::uint64_t>(sizeof(LzmwEncoderEntry)),
            dictionary_bytes)
        || !core::checked_add(
            static_cast<std::uint64_t>(required.raw_bytes),
            static_cast<std::uint64_t>(required.encoded_bytes), aggregate)
        || !core::checked_add(aggregate, dictionary_bytes, aggregate)
        || aggregate > limits_.max_internal_buffered_bytes) {
        state_ = State::error;
        terminal_error_ = {core::ErrorCode::limit_exceeded, 0, 0};
        return;
    }
    if (raw_storage_.size() < required.raw_bytes
        || encoded_storage_.size() < required.encoded_bytes
        || dictionary_workspace_.size() < required.dictionary_entries) {
        state_ = State::error;
        terminal_error_ = {core::ErrorCode::out_of_memory, 0, 0};
        return;
    }
    if (declared_frame_size_ == 0) state_ = State::awaiting_end;
}

core::ProcessResult LzmwStreamingEncoder::fail(
    const core::ErrorCode code, const std::size_t consumed,
    const std::size_t produced) noexcept {
    state_ = State::error;
    terminal_error_ = {code, raw_collected_, 0};
    return {consumed, produced, core::StreamStatus::error, terminal_error_};
}

bool LzmwStreamingEncoder::prepare_encoded_frame() noexcept {
    preparation_error_ = core::ErrorCode::internal_error;
    const auto raw = std::span<const std::byte>{raw_storage_}.first(
        raw_collected_);
    const auto planned = plan_lzmw_token_stream(
        raw, parameters_, limits_, dictionary_workspace_);
    if (planned.error != LzmwEncodeError::none) {
        switch (planned.error) {
        case LzmwEncodeError::input_limit_exceeded:
        case LzmwEncodeError::serialized_limit_exceeded:
        case LzmwEncodeError::workspace_limit_exceeded:
            preparation_error_ = core::ErrorCode::limit_exceeded;
            break;
        case LzmwEncodeError::workspace_too_small:
        case LzmwEncodeError::output_too_small:
            preparation_error_ = core::ErrorCode::out_of_memory;
            break;
        default:
            preparation_error_ = core::ErrorCode::internal_error;
            break;
        }
        return false;
    }
    if (planned.output_size > encoded_storage_.size()) {
        preparation_error_ = core::ErrorCode::out_of_memory;
        return false;
    }
    const auto encoded = encode_lzmw_token_stream(
        raw, parameters_, limits_, dictionary_workspace_,
        encoded_storage_.first(planned.output_size));
    if (encoded.error != LzmwEncodeError::none
        || encoded.output_size != planned.output_size
        || encoded.token_count != planned.token_count
        || encoded.dictionary_entries != planned.dictionary_entries)
        return false;
    encoded_size_ = encoded.output_size;
    output_offset_ = 0;
    state_ = encoded_size_ == 0 ? State::awaiting_end : State::draining;
    return true;
}

core::ProcessResult LzmwStreamingEncoder::process(
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

    while (true) {
        if (state_ == State::draining) {
            if (consumed != input.size())
                return fail(core::ErrorCode::invalid_argument, consumed,
                            produced);
            const auto remaining = encoded_size_ - output_offset_;
            const auto count = std::min(
                remaining, output.size() - produced);
            if (count != 0) {
                std::memmove(output.data() + produced,
                             encoded_storage_.data() + output_offset_, count);
                output_offset_ += count;
                produced += count;
            }
            if (output_offset_ != encoded_size_) {
                if (end_requested && consumed == input.size()) end_seen_ = true;
                return {consumed, produced, core::StreamStatus::need_output,
                        {}};
            }
            state_ = State::awaiting_end;
            continue;
        }

        if (state_ == State::awaiting_end) {
            if (consumed != input.size())
                return fail(core::ErrorCode::invalid_argument, consumed,
                            produced);
            if (end_seen_ || end_requested) {
                state_ = State::ended;
                return {consumed, produced,
                        core::StreamStatus::end_of_stream, {}};
            }
            return {consumed, produced,
                    (consumed != 0 || produced != 0)
                        ? core::StreamStatus::progress
                        : core::StreamStatus::need_input,
                    {}};
        }

        const auto frame_size = static_cast<std::size_t>(
            declared_frame_size_);
        const auto needed = frame_size - raw_collected_;
        const auto count = std::min(needed, input.size() - consumed);
        if (count != 0) {
            std::memmove(raw_storage_.data() + raw_collected_,
                         input.data() + consumed, count);
            raw_collected_ += count;
            consumed += count;
        }
        if (raw_collected_ != frame_size) {
            if (end_requested && consumed == input.size())
                return fail(core::ErrorCode::invalid_argument, consumed,
                            produced);
            return {consumed, produced,
                    (consumed != 0 || produced != 0)
                        ? core::StreamStatus::progress
                        : core::StreamStatus::need_input,
                    {}};
        }
        if (consumed != input.size())
            return fail(core::ErrorCode::invalid_argument, consumed,
                        produced);
        if (end_requested) end_seen_ = true;
        if (!prepare_encoded_frame())
            return fail(preparation_error_, consumed, produced);
    }
}

} // namespace marc::dictionary::internal

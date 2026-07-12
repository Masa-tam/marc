#include "frame/dynamic_range_frame_streaming_decoder.hpp"
#include "core/checked_math.hpp"
#include <algorithm>
#include <cstring>
namespace marc::frame {
namespace {
constexpr std::uint32_t flags_known =
    core::flag_value(core::ProcessFlags::flush) |
    core::flag_value(core::ProcessFlags::end_input) |
    core::flag_value(core::ProcessFlags::reset_block);
}
DynamicRangeFrameStreamingDecoder::DynamicRangeFrameStreamingDecoder(
    core::DecoderLimits l, std::span<std::byte> e,
    std::span<std::byte> d) noexcept
    : limits_(l), encoded_(e), decoded_(d) {
  if (core::validate_limits(l) != core::LimitError::none) {
    state_ = State::error;
    terminal_ = {core::ErrorCode::invalid_argument, 0, 0};
  }
}
core::ProcessResult
DynamicRangeFrameStreamingDecoder::fail(core::ErrorCode c, std::size_t i,
                                           std::size_t o) noexcept {
  state_ = State::error;
  terminal_ = {c, input_pos_, 0};
  return {i, o, core::StreamStatus::error, terminal_};
}
bool DynamicRangeFrameStreamingDecoder::parse_stream() noexcept {
  return parse_stream_header(stream_bytes_, limits_, stream_) ==
             StreamHeaderError::none &&
         stream_.dictionary_algorithm == DictionaryAlgorithm::none &&
         stream_.dictionary_variant == 0 &&
         stream_.entropy_algorithm == EntropyAlgorithm::dynamic_range &&
         stream_.entropy_variant == 1 && stream_.entropy_block_size == 0 &&
         stream_.dictionary_parameters_size == 0 &&
         stream_.entropy_parameters_size == 0;
}
bool DynamicRangeFrameStreamingDecoder::prepare_frame() noexcept {
  prep_error_ = core::ErrorCode::malformed_stream;
  FrameValidationContext c{stream_, limits_, sequence_, output_committed_};
  if (parse_frame_header(frame_bytes_, c, frame_) != FrameHeaderError::none)
    return false;
  if (frame_.dictionary_serialized_size > decoded_.size()) {
    prep_error_ = core::ErrorCode::out_of_memory;
    return false;
  }
  std::size_t n{};
  if (!core::checked_add(frame_header_size,
                         (std::size_t)frame_.block_descriptors_size, n) ||
      !core::checked_add(n, (std::size_t)frame_.compressed_payload_size, n) ||
      n > encoded_.size()) {
    prep_error_ = core::ErrorCode::out_of_memory;
    return false;
  }
  frame_size_ = n;
  std::memmove(encoded_.data(), frame_bytes_.data(), frame_header_size);
  frame_n_ = frame_header_size;
  return true;
}
bool DynamicRangeFrameStreamingDecoder::decode_frame() noexcept {
  auto r = decode_dynamic_range_frame(
      stream_, limits_, sequence_, output_committed_,
      encoded_.first(frame_size_),
      decoded_.first(frame_.dictionary_serialized_size));
  if (r.error != DynamicRangeFrameCodecError::none)
    return false;
  decoded_n_ = frame_.dictionary_serialized_size;
  output_n_ = 0;
  output_committed_ += decoded_n_;
  ++sequence_;
  header_n_ = frame_n_ = 0;
  state_ = State::draining;
  return true;
}
core::ProcessResult
DynamicRangeFrameStreamingDecoder::process(std::span<const std::byte> in,
                                              std::span<std::byte> out,
                                              std::uint32_t f) noexcept {
  if (state_ == State::error)
    return {0, 0, core::StreamStatus::error, terminal_};
  if (state_ == State::ended)
    return {0, 0, core::StreamStatus::end_of_stream, {}};
  if ((f & ~flags_known) ||
      (f & core::flag_value(core::ProcessFlags::reset_block)))
    return fail(core::ErrorCode::unsupported, 0, 0);
  bool end = f & core::flag_value(core::ProcessFlags::end_input);
  std::size_t used{}, made{};
  while (true) {
    if (state_ == State::draining) {
      auto n = std::min(decoded_n_ - output_n_, out.size() - made);
      if (n) {
        std::memmove(out.data() + made, decoded_.data() + output_n_, n);
        output_n_ += n;
        made += n;
      }
      if (output_n_ != decoded_n_)
        return {used, made, core::StreamStatus::need_output, {}};
      decoded_n_ = output_n_ = 0;
      state_ = output_committed_ == stream_.original_size ? State::awaiting_end
                                                          : State::frame_header;
      continue;
    }
    if (state_ == State::awaiting_end) {
      if (used != in.size())
        return fail(core::ErrorCode::malformed_stream, used, made);
      if (end_seen_ || end) {
        state_ = State::ended;
        return {used, made, core::StreamStatus::end_of_stream, {}};
      }
      return {used,
              made,
              (used || made) ? core::StreamStatus::progress
                             : core::StreamStatus::need_input,
              {}};
    }
    if (state_ == State::stream_header) {
      auto n = std::min(stream_header_size - header_n_, in.size() - used);
      if (n) {
        std::memmove(stream_bytes_.data() + header_n_, in.data() + used, n);
        header_n_ += n;
        used += n;
        input_pos_ += n;
      }
      if (header_n_ != stream_header_size) {
        if (end && used == in.size())
          return fail(core::ErrorCode::malformed_stream, used, made);
        return {used,
                made,
                used ? core::StreamStatus::progress
                     : core::StreamStatus::need_input,
                {}};
      }
      if (!parse_stream())
        return fail(core::ErrorCode::malformed_stream, used, made);
      header_n_ = 0;
      state_ =
          stream_.original_size ? State::frame_header : State::awaiting_end;
      if (state_ == State::awaiting_end && used != in.size())
        return fail(core::ErrorCode::malformed_stream, used, made);
      if (state_ == State::awaiting_end && end)
        end_seen_ = true;
      continue;
    }
    if (state_ == State::frame_header) {
      auto n = std::min(frame_header_size - header_n_, in.size() - used);
      if (n) {
        std::memmove(frame_bytes_.data() + header_n_, in.data() + used, n);
        header_n_ += n;
        used += n;
        input_pos_ += n;
      }
      if (header_n_ != frame_header_size) {
        if (end && used == in.size())
          return fail(core::ErrorCode::malformed_stream, used, made);
        return {used,
                made,
                (used || made) ? core::StreamStatus::progress
                               : core::StreamStatus::need_input,
                {}};
      }
      if (!prepare_frame())
        return fail(prep_error_, used, made);
      state_ = State::frame_body;
      continue;
    }
    auto n = std::min(frame_size_ - frame_n_, in.size() - used);
    if (n) {
      std::memmove(encoded_.data() + frame_n_, in.data() + used, n);
      frame_n_ += n;
      used += n;
      input_pos_ += n;
    }
    if (frame_n_ != frame_size_) {
      if (end && used == in.size())
        return fail(core::ErrorCode::malformed_stream, used, made);
      return {used,
              made,
              (used || made) ? core::StreamStatus::progress
                             : core::StreamStatus::need_input,
              {}};
    }
    bool final = output_committed_ + frame_.dictionary_serialized_size ==
                 stream_.original_size;
    if (final && used != in.size())
      return fail(core::ErrorCode::malformed_stream, used, made);
    if (!decode_frame())
      return fail(core::ErrorCode::malformed_stream, used, made);
    if (final && end && used == in.size())
      end_seen_ = true;
  }
}
} // namespace marc::frame

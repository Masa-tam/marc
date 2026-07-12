#ifndef MARC_FRAME_DYNAMIC_RANGE_FRAME_STREAMING_DECODER_HPP
#define MARC_FRAME_DYNAMIC_RANGE_FRAME_STREAMING_DECODER_HPP
#include "core/status.hpp"
#include "frame/dynamic_range_frame.hpp"
#include <array>
namespace marc::frame {
class DynamicRangeFrameStreamingDecoder final : public core::Transform {
public:
  DynamicRangeFrameStreamingDecoder(core::DecoderLimits,
                                       std::span<std::byte>,
                                       std::span<std::byte>) noexcept;
  core::ProcessResult process(std::span<const std::byte>, std::span<std::byte>,
                              std::uint32_t) noexcept override;

private:
  enum class State : std::uint8_t {
    stream_header,
    frame_header,
    frame_body,
    draining,
    awaiting_end,
    ended,
    error
  };
  core::ProcessResult fail(core::ErrorCode, std::size_t, std::size_t) noexcept;
  bool parse_stream() noexcept;
  bool prepare_frame() noexcept;
  bool decode_frame() noexcept;
  core::DecoderLimits limits_{};
  std::span<std::byte> encoded_{}, decoded_{};
  std::array<std::byte, stream_header_size> stream_bytes_{};
  std::array<std::byte, frame_header_size> frame_bytes_{};
  StreamHeader stream_{};
  FrameHeader frame_{};
  std::size_t header_n_{}, frame_size_{}, frame_n_{}, decoded_n_{}, output_n_{};
  std::uint64_t input_pos_{}, output_committed_{}, sequence_{};
  bool end_seen_{};
  core::ErrorCode prep_error_{core::ErrorCode::malformed_stream};
  State state_{State::stream_header};
  core::StreamError terminal_{};
};
} // namespace marc::frame
#endif

#ifndef MARC_FRAME_LZSS_POSITION_DISTANCE_8M_STREAM_ENCODER_HPP
#define MARC_FRAME_LZSS_POSITION_DISTANCE_8M_STREAM_ENCODER_HPP
#include "core/status.hpp"
#include "frame/lzss_position_distance_8m_frame_encoder.hpp"
#include <array>
namespace marc::frame::internal {
struct LzssPositionDistance8mStreamEncodeCapacities {
  std::size_t raw_bytes{}, publication_bytes{}, tokens{}, token_scratch{},
      index_entries{}, operations{}, operation_scratch{}, frame_bytes{},
      payload_bytes{}, input_bytes{}, output_bytes{};
};
struct LzssPositionDistance8mStreamEncodeRequirements {
  std::size_t aggregate_bytes{}, buffer_bytes{}, owner_bytes{}, control_bytes{},
      helper_bytes{};
  core::ErrorCode error{core::ErrorCode::none};
};
// Full numeric capacities, concrete owner/control/helper and external retained.
// Per-call input/output extents also count; this is not physical RSS/stack.
[[nodiscard]] LzssPositionDistance8mStreamEncodeRequirements
query_lzss_position_distance_8m_stream_encode_workspace(
    const core::DecoderLimits &,
    const LzssPositionDistance8mStreamEncodeCapacities &,
    std::size_t external_retained_bytes = 0) noexcept;
// Private known-size Encode transform. Borrow fixed full disjoint workspaces
// for its lifetime; no allocation. A frame is completely validated in a private
// publication slot before any fragment drains. Failed frames publish no bytes;
// previous valid output is retained. Flush neutral; ResetBlock unsupported.
// Repeat EndInput on an unconsumed final suffix. Ended/error states sticky.
class LzssPositionDistance8mStreamEncoder final : public core::Transform {
public:
  LzssPositionDistance8mStreamEncoder(
      TypedContextStreamHeader, core::DecoderLimits, std::span<std::byte> raw,
      std::span<std::byte> publication,
      LzssPositionDistance8mFrameEncodeWorkspace,
      std::size_t external_retained_bytes = 0) noexcept;
  LzssPositionDistance8mStreamEncoder(
      const LzssPositionDistance8mStreamEncoder &) = delete;
  LzssPositionDistance8mStreamEncoder &
  operator=(const LzssPositionDistance8mStreamEncoder &) = delete;
  [[nodiscard]] core::ProcessResult
  process(std::span<const std::byte>, std::span<std::byte>,
          std::uint32_t flags) noexcept override;

private:
  enum class State {
    header_drain,
    collecting,
    frame_drain,
    awaiting_end,
    ended,
    error
  };
  bool disjoint(std::span<const std::byte>,
                std::span<std::byte>) const noexcept;
  core::ProcessResult fail(core::ErrorCode, std::uint64_t, std::size_t = 0,
                           std::size_t = 0) noexcept;
  TypedContextStreamHeader stream_{};
  core::DecoderLimits limits_{};
  std::span<std::byte> raw_, publication_;
  LzssPositionDistance8mFrameEncodeWorkspace workspace_{};
  LzssPositionDistance8mStreamEncodeRequirements base_{};
  std::array<std::size_t, 5> typed_bytes_{};
  std::array<std::byte, 112> header_{};
  std::size_t external_{}, header_written_{}, collected_{}, drained_{},
      frame_written_{};
  std::uint64_t accepted_{}, validated_{}, sequence_{};
  TypedContextFrameLayout layout_{};
  LzssPositionDistance8mFrameEncodeResult frame_result_{};
  core::StreamError error_{};
  bool end_seen_{};
  State state_{State::header_drain};
};
} // namespace marc::frame::internal
#endif

#ifndef MARC_FRAME_LZSS_POSITION_DISTANCE_32M_STREAM_DECODER_HPP
#define MARC_FRAME_LZSS_POSITION_DISTANCE_32M_STREAM_DECODER_HPP
#include "core/status.hpp"
#include "frame/lzss_position_distance_32m_compact_frame_decoder.hpp"
#include <array>
namespace marc::frame::internal {
struct LzssPositionDistance32mStreamRequirements {
  std::size_t aggregate_bytes{}, owner_bytes{}, control_bytes{}, helper_bytes{};
  core::ErrorCode error{core::ErrorCode::none};
};
// Numeric full-capacity query. Includes concrete owner and conservatively
// reserved parser/control objects. Not payload validation or stack/RSS.
[[nodiscard]] LzssPositionDistance32mStreamRequirements
query_lzss_position_distance_32m_stream_workspace(
    const core::DecoderLimits &, std::size_t serialized_capacity,
    std::size_t token_capacity, std::size_t scratch_token_capacity,
    std::size_t raw_capacity, std::size_t scratch_raw_capacity,
    std::size_t additional_owner_bytes = 0) noexcept;
// Private reserved profile only. Borrow full-capacity disjoint workspaces for
// the whole lifetime. No allocation. EndInput must accompany a final suffix
// until consumed. Failure publishes no bytes of that frame; prior frames stay
// committed. Flush is neutral; ResetBlock unsupported; terminal states sticky.
class LzssPositionDistance32mStreamDecoder final : public core::Transform {
public:
  LzssPositionDistance32mStreamDecoder(
      core::DecoderLimits, std::span<std::byte> serialized,
      std::span<std::byte> tokens, std::span<std::byte> token_scratch,
      std::span<std::byte> raw, std::span<std::byte> raw_scratch,
      std::size_t additional_owner_bytes = 0) noexcept;
  LzssPositionDistance32mStreamDecoder(
      const LzssPositionDistance32mStreamDecoder &) = delete;
  LzssPositionDistance32mStreamDecoder &
  operator=(const LzssPositionDistance32mStreamDecoder &) = delete;
  [[nodiscard]] core::ProcessResult
  process(std::span<const std::byte>, std::span<std::byte>,
          std::uint32_t flags) noexcept override;

private:
  enum class State {
    header,
    prefix,
    payload,
    draining,
    awaiting_end,
    ended,
    error
  };
  bool disjoint(std::span<const std::byte>,
                std::span<std::byte>) const noexcept;
  core::ProcessResult fail(core::ErrorCode, std::uint64_t, std::size_t,
                           std::size_t) noexcept;
  core::DecoderLimits limits_{};
  std::span<std::byte> serialized_, raw_, raw_scratch_;
  std::span<std::byte> tokens_, token_scratch_;
  std::size_t token_bytes_{}, scratch_token_bytes_{}, retained_{};
  std::array<std::byte, 112> header_{};
  std::array<std::byte, 80> prefix_{};
  TypedContextStreamHeader stream_{};
  TypedContextFrameLayout layout_{};
  LzssPositionDistance32mFrameRequirements needed_{};
  std::size_t collected_{}, drained_{};
  std::uint64_t input_position_{}, frame_start_{}, validated_{}, sequence_{};
  bool end_seen_{};
  State state_{State::header};
  core::StreamError error_{};
  LzssPositionDistance32mCompactFrameDecodeResult frame_detail_{};
};
} // namespace marc::frame::internal
#endif

#ifndef MARC_FRAME_LZSS_POSITION_DISTANCE_8M_PREPARED_STREAM_ENCODER_HPP
#define MARC_FRAME_LZSS_POSITION_DISTANCE_8M_PREPARED_STREAM_ENCODER_HPP
#include "frame/lzss_position_distance_8m_owned_stream_encoder.hpp"
#include "frame/lzss_position_distance_8m_prepared_storage_owner.hpp"
#include <array>
namespace marc::frame::internal {
// Private known-size operation-free transform. Allocator outlives this object.
// External full owners, observer controls and call-view tails count in extra.
// Flush neutral; ResetBlock unsupported. Repeat EndInput on unconsumed suffix.
// Failed frames never drain; counts can include earlier valid bytes in the same
// failing call. Error and EndOfStream are sticky. Not a public codec.
class LzssPositionDistance8mPreparedStreamEncoder final : public core::Transform {
public:
  LzssPositionDistance8mPreparedStreamEncoder(
      TypedContextStreamHeader, core::DecoderLimits,
      LzssPositionDistance8mStreamAllocator &,
      std::size_t external_retained_bytes = 0) noexcept;
  ~LzssPositionDistance8mPreparedStreamEncoder();
  LzssPositionDistance8mPreparedStreamEncoder(
      const LzssPositionDistance8mPreparedStreamEncoder &) = delete;
  LzssPositionDistance8mPreparedStreamEncoder &
  operator=(const LzssPositionDistance8mPreparedStreamEncoder &) = delete;
  core::ProcessResult process(std::span<const std::byte>, std::span<std::byte>,
                              std::uint32_t) noexcept override;
  // Persistent coordinator plus conservative sequential call/helper controls;
  // excludes allocations, underlying allocator, call views and external owners.
  static std::size_t working_bytes() noexcept;

private:
  struct Region {
    const void *data{};
    std::size_t bytes{};
  };
  // Fixed receipts for the five current and five candidate blocks. This
  // bridge counts full capacities even after publication has drained.
  class Bridge final : public LzssPositionDistance8mBlockAllocator {
  public:
    explicit Bridge(LzssPositionDistance8mStreamAllocator &a) noexcept
        : source(a) {}
    LzssPositionDistance8mAllocatorControls controls() const noexcept override;
    LzssPositionDistance8mOwnedTokens tokens(std::size_t) noexcept override;
    LzssPositionDistance8mOwnedBytes bytes(std::size_t) noexcept override;
    void release(LzssPositionDistance8mOwnedTokens &) noexcept override;
    void release(LzssPositionDistance8mOwnedBytes &) noexcept override;
    bool bytes_live(std::size_t &) const noexcept;
    LzssPositionDistance8mStreamAllocator &source;
    std::array<Region, 10> regions{};

  private:
    void remember(const void *, std::size_t) noexcept;
    void forget(const void *) noexcept;
  };
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
  bool budget(std::size_t, std::size_t, std::size_t &) const noexcept;
  core::ProcessResult fail(core::ErrorCode, std::uint64_t, std::size_t = 0,
                           std::size_t = 0) noexcept;
  TypedContextStreamHeader stream_{};
  core::DecoderLimits limits_{};
  Bridge bridge_;
  LzssPositionDistance8mPreparedStorageOwner owner_;
  LzssPositionDistance8mOwnedBytes raw_{};
  LzssPositionDistance8mOwnedIndex index_{};
  LzssPositionDistance8mAllocatorControls allocator_controls_{};
  std::array<std::byte, 112> header_{};
  std::size_t external_{}, index_bytes_{}, header_written_{}, collected_{},
      drained_{};
  std::uint64_t accepted_{}, validated_{}, sequence_{};
  core::StreamError error_{};
  bool end_seen_{};
  State state_{State::header_drain};
};
} // namespace marc::frame::internal
#endif

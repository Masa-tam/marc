#ifndef MARC_FRAME_POSITION_DISTANCE16M_OWNED_STREAM_ENCODER_HPP
#define MARC_FRAME_POSITION_DISTANCE16M_OWNED_STREAM_ENCODER_HPP
#include "frame/lzss_position_distance_16m_compact_owner.hpp"
#include <array>
namespace marc::frame::internal {
class LzssPositionDistance16mStreamAllocator
    : public LzssPositionDistance16mCompactAllocator {
public:
  using LzssPositionDistance16mCompactAllocator::release;
  virtual LzssPositionDistance16mOwnedIndex indices(std::size_t) noexcept = 0;
  virtual void release(LzssPositionDistance16mOwnedIndex &) noexcept = 0;
};
class LzssPositionDistance16mExactStreamAllocator final
    : public LzssPositionDistance16mStreamAllocator {
public:
  LzssPositionDistance16mAllocatorControls controls() const noexcept override;
  LzssPositionDistance16mOwnedBytes bytes(std::size_t) noexcept override;
  LzssPositionDistance16mOwnedIndex indices(std::size_t) noexcept override;
  void release(LzssPositionDistance16mOwnedBytes &) noexcept override;
  void release(LzssPositionDistance16mOwnedIndex &) noexcept override;
};
// Known-size private owning transform. Allocator outlives encoder; full input,
// output and external retained capacities charged. Flush neutral; ResetBlock
// unsupported; EndInput repeated on final suffix until consumed. Sticky
// terminal states. Failed frames never drain; prior valid bytes may appear in
// an Error call.
class LzssPositionDistance16mOwnedStreamEncoder final : public core::Transform {
public:
  LzssPositionDistance16mOwnedStreamEncoder(
      TypedContextStreamHeader, core::DecoderLimits,
      LzssPositionDistance16mStreamAllocator &,
      std::size_t external_retained_bytes = 0) noexcept;
  ~LzssPositionDistance16mOwnedStreamEncoder();
  LzssPositionDistance16mOwnedStreamEncoder(
      const LzssPositionDistance16mOwnedStreamEncoder &) = delete;
  LzssPositionDistance16mOwnedStreamEncoder &
  operator=(const LzssPositionDistance16mOwnedStreamEncoder &) = delete;
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
  // Fixed receipts for the one current and four candidate blocks. This
  // bridge counts full capacities even after publication has drained.
  class Bridge final : public LzssPositionDistance16mCompactAllocator {
  public:
    explicit Bridge(LzssPositionDistance16mStreamAllocator &a) noexcept
        : source(a) {}
    LzssPositionDistance16mAllocatorControls controls() const noexcept override;
    LzssPositionDistance16mOwnedBytes bytes(std::size_t) noexcept override;
    void release(LzssPositionDistance16mOwnedBytes &) noexcept override;
    bool bytes_live(std::size_t &) const noexcept;
    LzssPositionDistance16mStreamAllocator &source;
    std::array<Region, 5> regions{};

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
  LzssPositionDistance16mCompactOwner owner_;
  LzssPositionDistance16mOwnedBytes raw_{};
  LzssPositionDistance16mOwnedIndex index_{};
  LzssPositionDistance16mAllocatorControls allocator_controls_{};
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

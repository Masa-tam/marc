#ifndef MARC_FRAME_POSITION_DISTANCE32M_OWNED_STREAM_ENCODER_HPP
#define MARC_FRAME_POSITION_DISTANCE32M_OWNED_STREAM_ENCODER_HPP
#include "frame/lzss_position_distance_32m_compact_owner.hpp"
#include <array>
namespace marc::frame::internal {
class LzssPositionDistance32mStreamAllocator
    : public LzssPositionDistance32mCompactAllocator {
public:
  using LzssPositionDistance32mCompactAllocator::release;
  virtual LzssPositionDistance32mOwnedIndex indices(std::size_t) noexcept = 0;
  virtual void release(LzssPositionDistance32mOwnedIndex &) noexcept = 0;
};
class LzssPositionDistance32mExactStreamAllocator final
    : public LzssPositionDistance32mStreamAllocator {
public:
  LzssPositionDistance32mAllocatorControls controls() const noexcept override;
  LzssPositionDistance32mOwnedBytes bytes(std::size_t) noexcept override;
  LzssPositionDistance32mOwnedIndex indices(std::size_t) noexcept override;
  void release(LzssPositionDistance32mOwnedBytes &) noexcept override;
  void release(LzssPositionDistance32mOwnedIndex &) noexcept override;
};
// Known-size private owning transform. Allocator outlives encoder; full input,
// output and external retained capacities charged. Flush neutral; ResetBlock
// unsupported; EndInput repeated on final suffix until consumed. Sticky
// terminal states. Failed frames never drain; prior valid bytes may appear in
// an Error call.
class LzssPositionDistance32mOwnedStreamEncoder final : public core::Transform {
public:
  LzssPositionDistance32mOwnedStreamEncoder(
      TypedContextStreamHeader, core::DecoderLimits,
      LzssPositionDistance32mStreamAllocator &,
      std::size_t external_retained_bytes = 0) noexcept;
  ~LzssPositionDistance32mOwnedStreamEncoder();
  LzssPositionDistance32mOwnedStreamEncoder(
      const LzssPositionDistance32mOwnedStreamEncoder &) = delete;
  LzssPositionDistance32mOwnedStreamEncoder &
  operator=(const LzssPositionDistance32mOwnedStreamEncoder &) = delete;
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
  class Bridge final : public LzssPositionDistance32mCompactAllocator {
  public:
    explicit Bridge(LzssPositionDistance32mStreamAllocator &a) noexcept
        : source(a) {}
    LzssPositionDistance32mAllocatorControls controls() const noexcept override;
    LzssPositionDistance32mOwnedBytes bytes(std::size_t) noexcept override;
    void release(LzssPositionDistance32mOwnedBytes &) noexcept override;
    bool bytes_live(std::size_t &) const noexcept;
    LzssPositionDistance32mStreamAllocator &source;
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
  LzssPositionDistance32mCompactOwner owner_;
  LzssPositionDistance32mOwnedBytes raw_{};
  LzssPositionDistance32mOwnedIndex index_{};
  LzssPositionDistance32mAllocatorControls allocator_controls_{};
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

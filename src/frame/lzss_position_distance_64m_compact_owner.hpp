#ifndef MARC_FRAME_POSITION_DISTANCE64M_COMPACT_OWNER_HPP
#define MARC_FRAME_POSITION_DISTANCE64M_COMPACT_OWNER_HPP
#include "core/status.hpp"
#include "frame/lzss_position_distance_64m_compact_frame_encoder.hpp"
namespace marc::frame::internal {
template <class T> struct LzssPositionDistance64mOwnedBlock {
  T *data{};
  std::size_t capacity{};
  std::span<T> view() const noexcept { return {data, capacity}; }
};
using LzssPositionDistance64mOwnedBytes =
    LzssPositionDistance64mOwnedBlock<std::byte>;
using LzssPositionDistance64mOwnedIndex =
    LzssPositionDistance64mOwnedBlock<std::uint32_t>;
struct LzssPositionDistance64mAllocatorControls {
  const void *data{};
  std::size_t bytes{}, working_bytes{};
};
// Trusted non-reentrant noexcept allocator, stable full control extent.
// Unique disjoint live byte arrays, EXACT admitted bound before allocation.
// Failure returns {}; release actually destroys the array before clearing.
class LzssPositionDistance64mCompactAllocator {
public:
  virtual ~LzssPositionDistance64mCompactAllocator() = default;
  virtual LzssPositionDistance64mAllocatorControls
  controls() const noexcept = 0;
  virtual LzssPositionDistance64mOwnedBytes bytes(std::size_t) noexcept = 0;
  virtual void release(LzssPositionDistance64mOwnedBytes &) noexcept = 0;
};
struct LzssPositionDistance64mOwnerResult {
  core::ErrorCode error{core::ErrorCode::none};
  std::size_t aggregate_bytes{}, bytes_validated{};
};
class LzssPositionDistance64mCompactOwner final {
public:
  explicit LzssPositionDistance64mCompactOwner(
      LzssPositionDistance64mCompactAllocator &) noexcept;
  ~LzssPositionDistance64mCompactOwner();
  LzssPositionDistance64mCompactOwner(
      const LzssPositionDistance64mCompactOwner &) = delete;
  LzssPositionDistance64mCompactOwner &
  operator=(const LzssPositionDistance64mCompactOwner &) = delete;
  // Raw/config/index stable throughout allocator callbacks. Full borrowed
  // owners outside views, observer/allocator extras belong in retained.
  // Prepare compact records once; finish without reparsing raw input.
  // Fresh candidate on every call. All failures preserve the prior full
  // publication/layout/length/pending flag. Index/private candidate
  // discardable. Temporary blocks and old publication released only after
  // complete success.
  LzssPositionDistance64mOwnerResult
  encode(std::span<const std::byte>, const TypedContextFrameValidationContext &,
         std::span<std::uint32_t>,
         std::size_t retained_owner_bytes = 0) noexcept;
  std::span<const std::byte> publication() const noexcept {
    return publication_.view().first(written_);
  }
  const TypedContextFrameLayout &layout() const noexcept { return layout_; }
  bool pending() const noexcept { return pending_; }
  void acknowledge_drained() noexcept { pending_ = false; }
  std::size_t publication_capacity() const noexcept {
    return publication_.capacity;
  }
  static std::size_t working_bytes() noexcept;

private:
  LzssPositionDistance64mCompactAllocator &allocator_;
  LzssPositionDistance64mOwnedBytes publication_{};
  TypedContextFrameLayout layout_{};
  std::size_t written_{};
  bool pending_{};
};
} // namespace marc::frame::internal
#endif

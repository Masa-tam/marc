#ifndef MARC_FRAME_POSITION_DISTANCE32M_COMPACT_OWNER_HPP
#define MARC_FRAME_POSITION_DISTANCE32M_COMPACT_OWNER_HPP
#include "core/status.hpp"
#include "frame/lzss_position_distance_32m_compact_frame_encoder.hpp"
namespace marc::frame::internal {
template <class T> struct LzssPositionDistance32mOwnedBlock {
  T *data{};
  std::size_t capacity{};
  std::span<T> view() const noexcept { return {data, capacity}; }
};
using LzssPositionDistance32mOwnedBytes =
    LzssPositionDistance32mOwnedBlock<std::byte>;
using LzssPositionDistance32mOwnedIndex =
    LzssPositionDistance32mOwnedBlock<std::uint32_t>;
struct LzssPositionDistance32mAllocatorControls {
  const void *data{};
  std::size_t bytes{}, working_bytes{};
};
// Trusted non-reentrant noexcept allocator, stable full control extent.
// Unique disjoint live byte arrays, EXACT admitted bound before allocation.
// Failure returns {}; release actually destroys the array before clearing.
class LzssPositionDistance32mCompactAllocator {
public:
  virtual ~LzssPositionDistance32mCompactAllocator() = default;
  virtual LzssPositionDistance32mAllocatorControls
  controls() const noexcept = 0;
  virtual LzssPositionDistance32mOwnedBytes bytes(std::size_t) noexcept = 0;
  virtual void release(LzssPositionDistance32mOwnedBytes &) noexcept = 0;
};
struct LzssPositionDistance32mOwnerResult {
  core::ErrorCode error{core::ErrorCode::none};
  std::size_t aggregate_bytes{}, bytes_validated{};
};
class LzssPositionDistance32mCompactOwner final {
public:
  explicit LzssPositionDistance32mCompactOwner(
      LzssPositionDistance32mCompactAllocator &) noexcept;
  ~LzssPositionDistance32mCompactOwner();
  LzssPositionDistance32mCompactOwner(
      const LzssPositionDistance32mCompactOwner &) = delete;
  LzssPositionDistance32mCompactOwner &
  operator=(const LzssPositionDistance32mCompactOwner &) = delete;
  // Raw/config/index stable throughout allocator callbacks. Full borrowed
  // owners outside views, observer/allocator extras belong in retained.
  // Prepare compact records once; finish without reparsing raw input.
  // Fresh candidate on every call. All failures preserve the prior full
  // publication/layout/length/pending flag. Index/private candidate
  // discardable. Temporary blocks and old publication released only after
  // complete success.
  LzssPositionDistance32mOwnerResult
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
  LzssPositionDistance32mCompactAllocator &allocator_;
  LzssPositionDistance32mOwnedBytes publication_{};
  TypedContextFrameLayout layout_{};
  std::size_t written_{};
  bool pending_{};
};
} // namespace marc::frame::internal
#endif

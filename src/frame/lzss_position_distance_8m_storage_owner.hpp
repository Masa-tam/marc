#ifndef MARC_FRAME_LZSS_POSITION_DISTANCE_8M_STORAGE_OWNER_HPP
#define MARC_FRAME_LZSS_POSITION_DISTANCE_8M_STORAGE_OWNER_HPP
#include "core/status.hpp"
#include "frame/lzss_position_distance_8m_storage_adapter.hpp"
namespace marc::frame::internal {
template <class T> struct LzssPositionDistance8mOwnedBlock {
  T *data{};
  std::size_t capacity{};
  std::span<T> view() const noexcept { return {data, capacity}; }
};
using LzssPositionDistance8mOwnedTokens =
    LzssPositionDistance8mOwnedBlock<dictionary::internal::LzssTypedToken>;
using LzssPositionDistance8mOwnedBytes =
    LzssPositionDistance8mOwnedBlock<std::byte>;
struct LzssPositionDistance8mAllocatorControls {
  const void *data;
  std::size_t bytes;
  std::size_t working_bytes;
};
// Non-reentrant, noexcept allocator; controls report the complete live object
// and fixed instrumentation. Successful blocks own unique disjoint live T
// arrays. Allocation must obey the admitted exact element bound BEFORE
// allocation. Failure returns {}; release actually destroys storage before
// clearing receipt.
class LzssPositionDistance8mBlockAllocator {
public:
  virtual ~LzssPositionDistance8mBlockAllocator() = default;
  virtual LzssPositionDistance8mAllocatorControls controls() const noexcept = 0;
  virtual LzssPositionDistance8mOwnedTokens tokens(std::size_t) noexcept = 0;
  virtual LzssPositionDistance8mOwnedBytes bytes(std::size_t) noexcept = 0;
  virtual void release(LzssPositionDistance8mOwnedTokens &) noexcept = 0;
  virtual void release(LzssPositionDistance8mOwnedBytes &) noexcept = 0;
};
class LzssPositionDistance8mExactAllocator final
    : public LzssPositionDistance8mBlockAllocator {
public:
  LzssPositionDistance8mAllocatorControls controls() const noexcept override;
  LzssPositionDistance8mOwnedTokens tokens(std::size_t) noexcept override;
  LzssPositionDistance8mOwnedBytes bytes(std::size_t) noexcept override;
  void release(LzssPositionDistance8mOwnedTokens &) noexcept override;
  void release(LzssPositionDistance8mOwnedBytes &) noexcept override;
};
struct LzssPositionDistance8mOwnerResult {
  core::ErrorCode error{core::ErrorCode::none};
  std::size_t aggregate_bytes{}, bytes_validated{};
};
class LzssPositionDistance8mStorageOwner final {
public:
  explicit LzssPositionDistance8mStorageOwner(
      LzssPositionDistance8mBlockAllocator &) noexcept;
  ~LzssPositionDistance8mStorageOwner();
  LzssPositionDistance8mStorageOwner(
      const LzssPositionDistance8mStorageOwner &) = delete;
  LzssPositionDistance8mStorageOwner &
  operator=(const LzssPositionDistance8mStorageOwner &) = delete;
  // Fresh candidate generation on every call; no implicit reuse/release/retry.
  // Allocator must outlive owner. Raw/index full owners and call/observer
  // spares outside supplied views must be included in retained bytes. Index
  // private. Failure preserves current generation/layout/length/pending state.
  // Success only exposes a complete validated private frame; no stream bytes
  // are drained.
  LzssPositionDistance8mOwnerResult
  encode(std::span<const std::byte>, const TypedContextFrameValidationContext &,
         std::span<std::uint32_t> index,
         std::size_t retained_bytes = 0) noexcept;
  std::span<const std::byte> publication() const noexcept;
  const TypedContextFrameLayout &layout() const noexcept { return layout_; }
  bool pending() const noexcept { return pending_; }
  // Coordinator must call only after every publication byte has drained.
  void acknowledge_drained() noexcept { pending_ = false; }
  static std::size_t working_bytes() noexcept;

private:
  struct Generation {
    LzssPositionDistance8mBlockAllocator *allocator;
    LzssPositionDistance8mOwnedTokens tokens{}, scratch{};
    LzssPositionDistance8mOwnedBytes frame{}, payload{}, publication{};
    ~Generation();
    LzssPositionDistance8mGenerationCapacities capacities() const noexcept;
    void swap(Generation &) noexcept;
  };
  LzssPositionDistance8mBlockAllocator &allocator_;
  Generation current_;
  TypedContextFrameLayout layout_{};
  std::size_t written_{};
  bool pending_{};
};
} // namespace marc::frame::internal
#endif

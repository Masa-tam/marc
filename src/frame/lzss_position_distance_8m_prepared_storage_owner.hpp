#ifndef MARC_FRAME_LZSS_POSITION_DISTANCE_8M_PREPARED_STORAGE_OWNER_HPP
#define MARC_FRAME_LZSS_POSITION_DISTANCE_8M_PREPARED_STORAGE_OWNER_HPP
#include "frame/lzss_position_distance_8m_storage_adapter.hpp"
#include "frame/lzss_position_distance_8m_storage_owner.hpp"
namespace marc::frame::internal {
class LzssPositionDistance8mPreparedStorageOwner final {
public:
  explicit LzssPositionDistance8mPreparedStorageOwner(
      LzssPositionDistance8mBlockAllocator &) noexcept;
  ~LzssPositionDistance8mPreparedStorageOwner();
  LzssPositionDistance8mPreparedStorageOwner(
      const LzssPositionDistance8mPreparedStorageOwner &) = delete;
  LzssPositionDistance8mPreparedStorageOwner &
  operator=(const LzssPositionDistance8mPreparedStorageOwner &) = delete;
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
  class Prepared;
  struct Generation {
    LzssPositionDistance8mBlockAllocator *allocator;
    LzssPositionDistance8mOwnedTokens tokens{}, scratch{};
    LzssPositionDistance8mOwnedBytes frame{}, payload{}, publication{};
    ~Generation();
    LzssPositionDistance8mGenerationCapacities capacities() const noexcept;
    void swap(Generation &) noexcept;
  };
  static std::size_t continuation_working_bytes() noexcept;
  static LzssPositionDistance8mTokenFrameResult
  continue_frame(Prepared &, Generation &, std::size_t aggregate,
                 TypedContextFrameLayout &, std::size_t &) noexcept;
  LzssPositionDistance8mBlockAllocator &allocator_;
  Generation current_;
  TypedContextFrameLayout layout_{};
  std::size_t written_{};
  bool pending_{};
};
} // namespace marc::frame::internal
#endif

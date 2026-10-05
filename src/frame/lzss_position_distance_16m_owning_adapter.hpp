#ifndef MARC_FRAME_LZSS_POSITION_DISTANCE_16M_OWNING_ADAPTER_HPP
#define MARC_FRAME_LZSS_POSITION_DISTANCE_16M_OWNING_ADAPTER_HPP
#include "frame/lzss_position_distance_16m_owned_stream_encoder.hpp"
#include <optional>
namespace marc::frame::internal {
struct LzssPositionDistance16mOwningConfig {
  TypedContextStreamHeader stream{};
  core::DecoderLimits limits{};
  std::size_t external{}, input_capacity{}, output_capacity{};
};
struct LzssPositionDistance16mOwningRequirements {
  std::size_t external_charge{}, fixed_bytes{}, raw_bytes{}, index_entries{},
      initial_bytes{};
  core::ErrorCode error{core::ErrorCode::none};
  // Success admits initial storage only; each frame is admitted separately.
};
// Private concrete delegate boundary, isolated only in standalone tests.
LzssPositionDistance16mOwnedBytes
owning16m_bytes(LzssPositionDistance16mExactStreamAllocator &,
               std::size_t) noexcept;
LzssPositionDistance16mOwnedIndex
owning16m_indices(LzssPositionDistance16mExactStreamAllocator &,
                 std::size_t) noexcept;
void owning16m_release_bytes(LzssPositionDistance16mExactStreamAllocator &,
                            LzssPositionDistance16mOwnedBytes &) noexcept;
void owning16m_release_indices(LzssPositionDistance16mExactStreamAllocator &,
                              LzssPositionDistance16mOwnedIndex &) noexcept;
class LzssPositionDistance16mOwningAdapter final : public core::Transform {
public:
  explicit LzssPositionDistance16mOwningAdapter(
      const LzssPositionDistance16mOwningConfig &) noexcept;
  ~LzssPositionDistance16mOwningAdapter();
  LzssPositionDistance16mOwningAdapter(
      const LzssPositionDistance16mOwningAdapter &) = delete;
  LzssPositionDistance16mOwningAdapter &
  operator=(const LzssPositionDistance16mOwningAdapter &) = delete;
  LzssPositionDistance16mOwningAdapter(LzssPositionDistance16mOwningAdapter &&) =
      delete;
  LzssPositionDistance16mOwningAdapter &
  operator=(LzssPositionDistance16mOwningAdapter &&) = delete;
  static LzssPositionDistance16mOwningRequirements
  query(const LzssPositionDistance16mOwningConfig &) noexcept;
  core::ProcessResult process(std::span<const std::byte>, std::span<std::byte>,
                              std::uint32_t) noexcept override;
  struct Ledger {
    std::size_t live{}, peak{}, calls{}, released{}, blocks{};
  };
  Ledger ledger() const noexcept;

private:
  class Allocator final : public LzssPositionDistance16mStreamAllocator {
  public:
    struct Receipt {
      void *data{};
      std::size_t count{}, bytes{};
      unsigned kind{};
    };
    std::array<Receipt, 12> receipts{};
    LzssPositionDistance16mExactStreamAllocator exact;
    std::size_t reserve{}, limit{};
    Ledger state{};
    LzssPositionDistance16mAllocatorControls controls() const noexcept override;
    static std::size_t working() noexcept;
    LzssPositionDistance16mOwnedBytes bytes(std::size_t) noexcept override;
    LzssPositionDistance16mOwnedIndex indices(std::size_t) noexcept override;
    void release(LzssPositionDistance16mOwnedBytes &) noexcept override;
    void release(LzssPositionDistance16mOwnedIndex &) noexcept override;
    template <class T>
    LzssPositionDistance16mOwnedBlock<T> obtain(std::size_t) noexcept;
    template <class T>
    void drop(LzssPositionDistance16mOwnedBlock<T> &) noexcept;
    ~Allocator();
  } allocator_; // Constructed first, destroyed after coordinator_.
  std::optional<LzssPositionDistance16mOwnedStreamEncoder> coordinator_;
  std::size_t input_capacity_{}, output_capacity_{};
  std::uint64_t accepted_{};
  core::ProcessResult terminal_{};
  bool ended_{};
  static std::size_t controls_bytes() noexcept;
  core::ProcessResult fail(core::ErrorCode) noexcept;
};
} // namespace marc::frame::internal
#endif

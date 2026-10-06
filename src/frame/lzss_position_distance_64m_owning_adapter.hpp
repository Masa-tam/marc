#ifndef MARC_FRAME_LZSS_POSITION_DISTANCE_64M_OWNING_ADAPTER_HPP
#define MARC_FRAME_LZSS_POSITION_DISTANCE_64M_OWNING_ADAPTER_HPP
#include "frame/lzss_position_distance_64m_owned_stream_encoder.hpp"
#include <optional>
namespace marc::frame::internal {
struct LzssPositionDistance64mOwningConfig {
  TypedContextStreamHeader stream{};
  core::DecoderLimits limits{};
  std::size_t external{}, input_capacity{}, output_capacity{};
};
struct LzssPositionDistance64mOwningRequirements {
  std::size_t external_charge{}, fixed_bytes{}, raw_bytes{}, index_entries{},
      initial_bytes{};
  core::ErrorCode error{core::ErrorCode::none};
  // Success admits initial storage only; each frame is admitted separately.
};
// Private concrete delegate boundary, isolated only in standalone tests.
LzssPositionDistance64mOwnedBytes
owning64m_bytes(LzssPositionDistance64mExactStreamAllocator &,
                std::size_t) noexcept;
LzssPositionDistance64mOwnedIndex
owning64m_indices(LzssPositionDistance64mExactStreamAllocator &,
                  std::size_t) noexcept;
void owning64m_release_bytes(LzssPositionDistance64mExactStreamAllocator &,
                             LzssPositionDistance64mOwnedBytes &) noexcept;
void owning64m_release_indices(LzssPositionDistance64mExactStreamAllocator &,
                               LzssPositionDistance64mOwnedIndex &) noexcept;
class LzssPositionDistance64mOwningAdapter final : public core::Transform {
public:
  explicit LzssPositionDistance64mOwningAdapter(
      const LzssPositionDistance64mOwningConfig &) noexcept;
  ~LzssPositionDistance64mOwningAdapter();
  LzssPositionDistance64mOwningAdapter(
      const LzssPositionDistance64mOwningAdapter &) = delete;
  LzssPositionDistance64mOwningAdapter &
  operator=(const LzssPositionDistance64mOwningAdapter &) = delete;
  LzssPositionDistance64mOwningAdapter(
      LzssPositionDistance64mOwningAdapter &&) = delete;
  LzssPositionDistance64mOwningAdapter &
  operator=(LzssPositionDistance64mOwningAdapter &&) = delete;
  static LzssPositionDistance64mOwningRequirements
  query(const LzssPositionDistance64mOwningConfig &) noexcept;
  core::ProcessResult process(std::span<const std::byte>, std::span<std::byte>,
                              std::uint32_t) noexcept override;
  struct Ledger {
    std::size_t live{}, peak{}, calls{}, released{}, blocks{};
  };
  Ledger ledger() const noexcept;

private:
  class Allocator final : public LzssPositionDistance64mStreamAllocator {
  public:
    struct Receipt {
      void *data{};
      std::size_t count{}, bytes{};
      unsigned kind{};
    };
    std::array<Receipt, 12> receipts{};
    LzssPositionDistance64mExactStreamAllocator exact;
    std::size_t reserve{}, limit{};
    Ledger state{};
    LzssPositionDistance64mAllocatorControls controls() const noexcept override;
    static std::size_t working() noexcept;
    LzssPositionDistance64mOwnedBytes bytes(std::size_t) noexcept override;
    LzssPositionDistance64mOwnedIndex indices(std::size_t) noexcept override;
    void release(LzssPositionDistance64mOwnedBytes &) noexcept override;
    void release(LzssPositionDistance64mOwnedIndex &) noexcept override;
    template <class T>
    LzssPositionDistance64mOwnedBlock<T> obtain(std::size_t) noexcept;
    template <class T>
    void drop(LzssPositionDistance64mOwnedBlock<T> &) noexcept;
    ~Allocator();
  } allocator_; // Constructed first, destroyed after coordinator_.
  std::optional<LzssPositionDistance64mOwnedStreamEncoder> coordinator_;
  std::size_t input_capacity_{}, output_capacity_{};
  std::uint64_t accepted_{};
  core::ProcessResult terminal_{};
  bool ended_{};
  static std::size_t controls_bytes() noexcept;
  core::ProcessResult fail(core::ErrorCode) noexcept;
};
} // namespace marc::frame::internal
#endif

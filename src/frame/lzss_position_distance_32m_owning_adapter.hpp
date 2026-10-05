#ifndef MARC_FRAME_LZSS_POSITION_DISTANCE_32M_OWNING_ADAPTER_HPP
#define MARC_FRAME_LZSS_POSITION_DISTANCE_32M_OWNING_ADAPTER_HPP
#include "frame/lzss_position_distance_32m_owned_stream_encoder.hpp"
#include <optional>
namespace marc::frame::internal {
struct LzssPositionDistance32mOwningConfig {
  TypedContextStreamHeader stream{};
  core::DecoderLimits limits{};
  std::size_t external{}, input_capacity{}, output_capacity{};
};
struct LzssPositionDistance32mOwningRequirements {
  std::size_t external_charge{}, fixed_bytes{}, raw_bytes{}, index_entries{},
      initial_bytes{};
  core::ErrorCode error{core::ErrorCode::none};
  // Success admits initial storage only; each frame is admitted separately.
};
// Private concrete delegate boundary, isolated only in standalone tests.
LzssPositionDistance32mOwnedBytes
owning32m_bytes(LzssPositionDistance32mExactStreamAllocator &,
                std::size_t) noexcept;
LzssPositionDistance32mOwnedIndex
owning32m_indices(LzssPositionDistance32mExactStreamAllocator &,
                  std::size_t) noexcept;
void owning32m_release_bytes(LzssPositionDistance32mExactStreamAllocator &,
                             LzssPositionDistance32mOwnedBytes &) noexcept;
void owning32m_release_indices(LzssPositionDistance32mExactStreamAllocator &,
                               LzssPositionDistance32mOwnedIndex &) noexcept;
class LzssPositionDistance32mOwningAdapter final : public core::Transform {
public:
  explicit LzssPositionDistance32mOwningAdapter(
      const LzssPositionDistance32mOwningConfig &) noexcept;
  ~LzssPositionDistance32mOwningAdapter();
  LzssPositionDistance32mOwningAdapter(
      const LzssPositionDistance32mOwningAdapter &) = delete;
  LzssPositionDistance32mOwningAdapter &
  operator=(const LzssPositionDistance32mOwningAdapter &) = delete;
  LzssPositionDistance32mOwningAdapter(
      LzssPositionDistance32mOwningAdapter &&) = delete;
  LzssPositionDistance32mOwningAdapter &
  operator=(LzssPositionDistance32mOwningAdapter &&) = delete;
  static LzssPositionDistance32mOwningRequirements
  query(const LzssPositionDistance32mOwningConfig &) noexcept;
  core::ProcessResult process(std::span<const std::byte>, std::span<std::byte>,
                              std::uint32_t) noexcept override;
  struct Ledger {
    std::size_t live{}, peak{}, calls{}, released{}, blocks{};
  };
  Ledger ledger() const noexcept;

private:
  class Allocator final : public LzssPositionDistance32mStreamAllocator {
  public:
    struct Receipt {
      void *data{};
      std::size_t count{}, bytes{};
      unsigned kind{};
    };
    std::array<Receipt, 12> receipts{};
    LzssPositionDistance32mExactStreamAllocator exact;
    std::size_t reserve{}, limit{};
    Ledger state{};
    LzssPositionDistance32mAllocatorControls controls() const noexcept override;
    static std::size_t working() noexcept;
    LzssPositionDistance32mOwnedBytes bytes(std::size_t) noexcept override;
    LzssPositionDistance32mOwnedIndex indices(std::size_t) noexcept override;
    void release(LzssPositionDistance32mOwnedBytes &) noexcept override;
    void release(LzssPositionDistance32mOwnedIndex &) noexcept override;
    template <class T>
    LzssPositionDistance32mOwnedBlock<T> obtain(std::size_t) noexcept;
    template <class T>
    void drop(LzssPositionDistance32mOwnedBlock<T> &) noexcept;
    ~Allocator();
  } allocator_; // Constructed first, destroyed after coordinator_.
  std::optional<LzssPositionDistance32mOwnedStreamEncoder> coordinator_;
  std::size_t input_capacity_{}, output_capacity_{};
  std::uint64_t accepted_{};
  core::ProcessResult terminal_{};
  bool ended_{};
  static std::size_t controls_bytes() noexcept;
  core::ProcessResult fail(core::ErrorCode) noexcept;
};
} // namespace marc::frame::internal
#endif

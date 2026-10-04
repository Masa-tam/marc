#ifndef MARC_FRAME_LZSS_POSITION_DISTANCE_8M_OWNING_ADAPTER_HPP
#define MARC_FRAME_LZSS_POSITION_DISTANCE_8M_OWNING_ADAPTER_HPP
#include "frame/lzss_position_distance_8m_prepared_stream_encoder.hpp"
#include <optional>
namespace marc::frame::internal {
struct LzssPositionDistance8mOwningConfig {
  TypedContextStreamHeader stream{};
  core::DecoderLimits limits{};
  std::size_t external{}, input_capacity{}, output_capacity{};
};
struct LzssPositionDistance8mOwningRequirements {
  std::size_t external_charge{}, fixed_bytes{}, raw_bytes{}, index_entries{},
      initial_bytes{};
  core::ErrorCode error{core::ErrorCode::none};
  // Success admits initial storage only; each frame is admitted separately.
};
// Private concrete delegate boundary, isolated only in standalone tests.
LzssPositionDistance8mOwnedTokens
owning8m_tokens(LzssPositionDistance8mExactStreamAllocator &,
                std::size_t) noexcept;
LzssPositionDistance8mOwnedBytes
owning8m_bytes(LzssPositionDistance8mExactStreamAllocator &,
               std::size_t) noexcept;
LzssPositionDistance8mOwnedIndex
owning8m_indices(LzssPositionDistance8mExactStreamAllocator &,
                 std::size_t) noexcept;
void owning8m_release_tokens(LzssPositionDistance8mExactStreamAllocator &,
                             LzssPositionDistance8mOwnedTokens &) noexcept;
void owning8m_release_bytes(LzssPositionDistance8mExactStreamAllocator &,
                            LzssPositionDistance8mOwnedBytes &) noexcept;
void owning8m_release_indices(LzssPositionDistance8mExactStreamAllocator &,
                              LzssPositionDistance8mOwnedIndex &) noexcept;
class LzssPositionDistance8mOwningAdapter final : public core::Transform {
public:
  explicit LzssPositionDistance8mOwningAdapter(
      const LzssPositionDistance8mOwningConfig &) noexcept;
  ~LzssPositionDistance8mOwningAdapter();
  LzssPositionDistance8mOwningAdapter(
      const LzssPositionDistance8mOwningAdapter &) = delete;
  LzssPositionDistance8mOwningAdapter &
  operator=(const LzssPositionDistance8mOwningAdapter &) = delete;
  LzssPositionDistance8mOwningAdapter(LzssPositionDistance8mOwningAdapter &&) =
      delete;
  LzssPositionDistance8mOwningAdapter &
  operator=(LzssPositionDistance8mOwningAdapter &&) = delete;
  static LzssPositionDistance8mOwningRequirements
  query(const LzssPositionDistance8mOwningConfig &) noexcept;
  core::ProcessResult process(std::span<const std::byte>, std::span<std::byte>,
                              std::uint32_t) noexcept override;
  struct Ledger {
    std::size_t live{}, peak{}, calls{}, released{}, blocks{};
  };
  Ledger ledger() const noexcept;

private:
  class Allocator final : public LzssPositionDistance8mStreamAllocator {
  public:
    struct Receipt {
      void *data{};
      std::size_t count{}, bytes{};
      unsigned kind{};
    };
    std::array<Receipt, 12> receipts{};
    LzssPositionDistance8mExactStreamAllocator exact;
    std::size_t reserve{}, limit{};
    Ledger state{};
    LzssPositionDistance8mAllocatorControls controls() const noexcept override;
    static std::size_t working() noexcept;
    LzssPositionDistance8mOwnedTokens tokens(std::size_t) noexcept override;
    LzssPositionDistance8mOwnedBytes bytes(std::size_t) noexcept override;
    LzssPositionDistance8mOwnedIndex indices(std::size_t) noexcept override;
    void release(LzssPositionDistance8mOwnedTokens &) noexcept override;
    void release(LzssPositionDistance8mOwnedBytes &) noexcept override;
    void release(LzssPositionDistance8mOwnedIndex &) noexcept override;
    template <class T>
    LzssPositionDistance8mOwnedBlock<T> obtain(std::size_t) noexcept;
    template <class T>
    void drop(LzssPositionDistance8mOwnedBlock<T> &) noexcept;
    ~Allocator();
  } allocator_; // Constructed first, destroyed after coordinator_.
  std::optional<LzssPositionDistance8mPreparedStreamEncoder> coordinator_;
  std::size_t input_capacity_{}, output_capacity_{};
  std::uint64_t accepted_{};
  core::ProcessResult terminal_{};
  bool ended_{};
  static std::size_t controls_bytes() noexcept;
  core::ProcessResult fail(core::ErrorCode) noexcept;
};
} // namespace marc::frame::internal
#endif

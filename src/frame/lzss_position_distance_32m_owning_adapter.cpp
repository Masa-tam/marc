#include "frame/lzss_position_distance_32m_owning_adapter.hpp"
#include "frame/position_range_owning_adapter_common.hpp"
namespace marc::frame::internal {
namespace {
using Adapter = LzssPositionDistance32mOwningAdapter;
using Code = core::ErrorCode;
template <class T>
auto allocate(LzssPositionDistance32mExactStreamAllocator &a,
              std::size_t n) noexcept {
  if constexpr (std::is_same_v<T, std::byte>)
    return owning32m_bytes(a, n);
  else {
    static_assert(std::is_same_v<T, std::uint32_t>);
    return owning32m_indices(a, n);
  }
}
template <class T>
void delegate_release(LzssPositionDistance32mExactStreamAllocator &a,
                      LzssPositionDistance32mOwnedBlock<T> &b) noexcept {
  if constexpr (std::is_same_v<T, std::byte>)
    owning32m_release_bytes(a, b);
  else {
    static_assert(std::is_same_v<T, std::uint32_t>);
    owning32m_release_indices(a, b);
  }
}
struct Profile {
  using OwningConfig = LzssPositionDistance32mOwningConfig;
  using OwningRequirements = LzssPositionDistance32mOwningRequirements;
  using PreflightError = LzssPositionDistance32mPreflightError;
  using OwnedStreamEncoder = LzssPositionDistance32mOwnedStreamEncoder;
  using CompactOwner = LzssPositionDistance32mCompactOwner;
  using AllocatorControls = LzssPositionDistance32mAllocatorControls;
  using ExactStreamAllocator = LzssPositionDistance32mExactStreamAllocator;
  template <class T> using OwnedBlock = LzssPositionDistance32mOwnedBlock<T>;
  static constexpr auto validate_stream =
      validate_lzss_position_distance_32m_stream_semantics;
  template <class T>
  static auto allocate_block(ExactStreamAllocator &a, std::size_t n) noexcept {
    return allocate<T>(a, n);
  }
  template <class T>
  static void release_block(ExactStreamAllocator &a,
                            OwnedBlock<T> &b) noexcept {
    delegate_release<T>(a, b);
  }
};
using Mechanics = PositionRangeOwningAdapterMechanics;
} // namespace
std::size_t Adapter::Allocator::working() noexcept {
  return Mechanics::allocator_working<Allocator>();
}
LzssPositionDistance32mAllocatorControls
Adapter::Allocator::controls() const noexcept {
  return Mechanics::allocator_controls<Profile>(*this);
}
template <class T>
LzssPositionDistance32mOwnedBlock<T>
Adapter::Allocator::obtain(std::size_t n) noexcept {
  return Mechanics::obtain<Profile, T>(*this, n);
}
template <class T>
void Adapter::Allocator::drop(
    LzssPositionDistance32mOwnedBlock<T> &b) noexcept {
  Mechanics::drop<Profile, T>(*this, b);
}
Adapter::Allocator::~Allocator() {
  if (state.live || state.blocks)
    std::terminate();
}
LzssPositionDistance32mOwnedBytes
Adapter::Allocator::bytes(std::size_t n) noexcept {
  return obtain<std::byte>(n);
}
LzssPositionDistance32mOwnedIndex
Adapter::Allocator::indices(std::size_t n) noexcept {
  return obtain<std::uint32_t>(n);
}
void Adapter::Allocator::release(
    LzssPositionDistance32mOwnedBytes &b) noexcept {
  drop(b);
}
void Adapter::Allocator::release(
    LzssPositionDistance32mOwnedIndex &b) noexcept {
  drop(b);
}
std::size_t Adapter::controls_bytes() noexcept {
  return Mechanics::controls_bytes<Profile>();
}
LzssPositionDistance32mOwningRequirements
Adapter::query(const LzssPositionDistance32mOwningConfig &c) noexcept {
  return Mechanics::query<Profile, Adapter>(c);
}
Adapter::LzssPositionDistance32mOwningAdapter(
    const LzssPositionDistance32mOwningConfig &c) noexcept
    : input_capacity_(c.input_capacity), output_capacity_(c.output_capacity) {
  Mechanics::initialize<Profile>(*this, c);
}
Adapter::~LzssPositionDistance32mOwningAdapter() { Mechanics::destroy(*this); }
Adapter::Ledger Adapter::ledger() const noexcept { return allocator_.state; }
core::ProcessResult Adapter::fail(Code e) noexcept {
  return Mechanics::fail(*this, e);
}
core::ProcessResult Adapter::process(std::span<const std::byte> in,
                                     std::span<std::byte> out,
                                     std::uint32_t flags) noexcept {
  return Mechanics::process(*this, in, out, flags);
}
} // namespace marc::frame::internal

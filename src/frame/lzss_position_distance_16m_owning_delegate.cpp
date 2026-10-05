#include "frame/lzss_position_distance_16m_owning_adapter.hpp"
namespace marc::frame::internal {
LzssPositionDistance16mOwnedBytes
owning16m_bytes(LzssPositionDistance16mExactStreamAllocator &a,
               std::size_t n) noexcept {
  return a.bytes(n);
}
LzssPositionDistance16mOwnedIndex
owning16m_indices(LzssPositionDistance16mExactStreamAllocator &a,
                 std::size_t n) noexcept {
  return a.indices(n);
}
void owning16m_release_bytes(LzssPositionDistance16mExactStreamAllocator &a,
                            LzssPositionDistance16mOwnedBytes &b) noexcept {
  a.release(b);
}
void owning16m_release_indices(LzssPositionDistance16mExactStreamAllocator &a,
                              LzssPositionDistance16mOwnedIndex &b) noexcept {
  a.release(b);
}
} // namespace marc::frame::internal

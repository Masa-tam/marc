#include "frame/lzss_position_distance_32m_owning_adapter.hpp"
namespace marc::frame::internal {
LzssPositionDistance32mOwnedBytes
owning32m_bytes(LzssPositionDistance32mExactStreamAllocator &a,
                std::size_t n) noexcept {
  return a.bytes(n);
}
LzssPositionDistance32mOwnedIndex
owning32m_indices(LzssPositionDistance32mExactStreamAllocator &a,
                  std::size_t n) noexcept {
  return a.indices(n);
}
void owning32m_release_bytes(LzssPositionDistance32mExactStreamAllocator &a,
                             LzssPositionDistance32mOwnedBytes &b) noexcept {
  a.release(b);
}
void owning32m_release_indices(LzssPositionDistance32mExactStreamAllocator &a,
                               LzssPositionDistance32mOwnedIndex &b) noexcept {
  a.release(b);
}
} // namespace marc::frame::internal

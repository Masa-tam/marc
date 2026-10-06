#include "frame/lzss_position_distance_64m_owning_adapter.hpp"
namespace marc::frame::internal {
LzssPositionDistance64mOwnedBytes
owning64m_bytes(LzssPositionDistance64mExactStreamAllocator &a,
                std::size_t n) noexcept {
  return a.bytes(n);
}
LzssPositionDistance64mOwnedIndex
owning64m_indices(LzssPositionDistance64mExactStreamAllocator &a,
                  std::size_t n) noexcept {
  return a.indices(n);
}
void owning64m_release_bytes(LzssPositionDistance64mExactStreamAllocator &a,
                             LzssPositionDistance64mOwnedBytes &b) noexcept {
  a.release(b);
}
void owning64m_release_indices(LzssPositionDistance64mExactStreamAllocator &a,
                               LzssPositionDistance64mOwnedIndex &b) noexcept {
  a.release(b);
}
} // namespace marc::frame::internal

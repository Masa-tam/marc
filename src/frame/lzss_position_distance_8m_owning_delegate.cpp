#include "frame/lzss_position_distance_8m_owning_adapter.hpp"
namespace marc::frame::internal {
LzssPositionDistance8mOwnedTokens
owning8m_tokens(LzssPositionDistance8mExactStreamAllocator &a,
                std::size_t n) noexcept {
  return a.tokens(n);
}
LzssPositionDistance8mOwnedBytes
owning8m_bytes(LzssPositionDistance8mExactStreamAllocator &a,
               std::size_t n) noexcept {
  return a.bytes(n);
}
LzssPositionDistance8mOwnedIndex
owning8m_indices(LzssPositionDistance8mExactStreamAllocator &a,
                 std::size_t n) noexcept {
  return a.indices(n);
}
void owning8m_release_tokens(LzssPositionDistance8mExactStreamAllocator &a,
                             LzssPositionDistance8mOwnedTokens &b) noexcept {
  a.release(b);
}
void owning8m_release_bytes(LzssPositionDistance8mExactStreamAllocator &a,
                            LzssPositionDistance8mOwnedBytes &b) noexcept {
  a.release(b);
}
void owning8m_release_indices(LzssPositionDistance8mExactStreamAllocator &a,
                              LzssPositionDistance8mOwnedIndex &b) noexcept {
  a.release(b);
}
} // namespace marc::frame::internal

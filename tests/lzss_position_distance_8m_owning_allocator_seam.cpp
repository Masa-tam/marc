#include "lzss_position_distance_8m_owning_allocator_seam.hpp"
#include <algorithm>
#include <exception>
#include <type_traits>
namespace {
using namespace marc::frame::internal;
thread_local marc::test::owning8m::Controller *active{};
template <class T>
auto obtain(LzssPositionDistance8mExactStreamAllocator &a,
            std::size_t n) noexcept {
  if (active && ++active->calls == active->fail_at) {
    ++active->injections;
    return LzssPositionDistance8mOwnedBlock<T>{};
  }
  if (active && active->calls == active->oversize_at) {
    ++n;
    ++active->injections;
  }
  auto b = [&] {
    if constexpr (std::is_same_v<T, std::byte>)
      return owning8m_bytes(a, n);
    else if constexpr (std::is_same_v<T, std::uint32_t>)
      return owning8m_indices(a, n);
    else
      return owning8m_tokens(a, n);
  }();
  if (active && b.data) {
    auto slot = std::find_if(active->records.begin(), active->records.end(),
                             [](auto &r) { return !r.p; });
    if (slot == active->records.end())
      std::terminate();
    *slot = {b.data, b.capacity * sizeof(T)};
    active->live += slot->bytes;
    active->peak = std::max(active->peak, active->live);
  }
  return b;
}
template <class T>
void drop(LzssPositionDistance8mExactStreamAllocator &a,
          LzssPositionDistance8mOwnedBlock<T> &b) noexcept {
  const auto p = b.data;
  if constexpr (std::is_same_v<T, std::byte>)
    owning8m_release_bytes(a, b);
  else if constexpr (std::is_same_v<T, std::uint32_t>)
    owning8m_release_indices(a, b);
  else
    owning8m_release_tokens(a, b);
  if (active && p) {
    auto slot = std::find_if(active->records.begin(), active->records.end(),
                             [&](auto &r) { return r.p == p; });
    if (slot == active->records.end())
      std::terminate();
    active->valid = active->valid && !b.data && !b.capacity;
    active->live -= slot->bytes;
    *slot = {};
    ++active->deleted;
  }
}
} // namespace
namespace marc::test::owning8m {
Scope::Scope(Controller &c) noexcept : old_(active) { active = &c; }
Scope::~Scope() { active = old_; }
} // namespace marc::test::owning8m
namespace marc::frame::internal {
LzssPositionDistance8mOwnedTokens
test_owning8m_tokens(LzssPositionDistance8mExactStreamAllocator &a,
                     std::size_t n) noexcept {
  return obtain<dictionary::internal::LzssTypedToken>(a, n);
}
LzssPositionDistance8mOwnedBytes
test_owning8m_bytes(LzssPositionDistance8mExactStreamAllocator &a,
                    std::size_t n) noexcept {
  return obtain<std::byte>(a, n);
}
LzssPositionDistance8mOwnedIndex
test_owning8m_indices(LzssPositionDistance8mExactStreamAllocator &a,
                      std::size_t n) noexcept {
  return obtain<std::uint32_t>(a, n);
}
void test_owning8m_release_tokens(
    LzssPositionDistance8mExactStreamAllocator &a,
    LzssPositionDistance8mOwnedTokens &b) noexcept {
  drop(a, b);
}
void test_owning8m_release_bytes(LzssPositionDistance8mExactStreamAllocator &a,
                                 LzssPositionDistance8mOwnedBytes &b) noexcept {
  drop(a, b);
}
void test_owning8m_release_indices(
    LzssPositionDistance8mExactStreamAllocator &a,
    LzssPositionDistance8mOwnedIndex &b) noexcept {
  drop(a, b);
}
} // namespace marc::frame::internal

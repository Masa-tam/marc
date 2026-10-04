#ifndef MARC_TEST_MAX8M_FAULT_SEAM_HPP
#define MARC_TEST_MAX8M_FAULT_SEAM_HPP
#include "entropy/lzss_position_distance_8m_token_range_encoder.hpp"
#include "frame/lzss_position_distance_8m_owning_adapter.hpp"
#include "frame/lzss_position_distance_8m_serializer.hpp"
#include <array>
#include <vector>
namespace marc::test::max8m {
using Token = dictionary::internal::LzssTypedToken;
inline constexpr std::byte sentinel{0xa7};
struct Late {
  unsigned mode{};
  std::uint64_t prior{}, sequence{};
  std::size_t raw{}, frame{}, range{}, prefix{}, reparse{}, injections{};
  std::size_t range_bytes{}, prefix_bytes{};
  std::uint32_t payload{};
  std::uint64_t parsed_sequence{};
  bool valid{true};
};
class LateScope {
  Late *previous_;

public:
  explicit LateScope(Late &) noexcept;
  ~LateScope();
};
std::size_t working_bytes() noexcept;
struct Observer {
  struct Record {
    void *p{};
    std::size_t count{}, bytes{}, id{};
    unsigned kind{};
  };
  struct Snapshot {
    Record record{};
    std::vector<Token> tokens;
    std::vector<std::byte> bytes;
  };
  std::array<Record, 12> records{};
  std::array<Snapshot, 5> snapshots{};
  std::size_t calls{}, deleted{}, live{}, peak{}, base{}, snapshot_at{},
      snapshot_count{}, before_live{}, before_deleted{}, candidate_bytes{};
  bool valid{true}, fault{}, pristine{true}, dirty{};
  void reserve(std::size_t, std::size_t);
  std::size_t snapshot_bytes() const;
  void capture() noexcept;
  void arrived() noexcept;
  void obtained(void *, std::size_t, std::size_t, unsigned) noexcept;
  void before_release(void *, std::size_t, unsigned) noexcept;
  void released(void *) noexcept;
  bool preserved() const noexcept;
  bool zero() const noexcept;
};
class ObserveScope {
  Observer *previous_;

public:
  explicit ObserveScope(Observer &) noexcept;
  ~ObserveScope();
};
} // namespace marc::test::max8m
#endif

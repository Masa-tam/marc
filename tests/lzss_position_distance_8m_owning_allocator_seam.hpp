#ifndef MARC_TEST_OWNING8M_SEAM_HPP
#define MARC_TEST_OWNING8M_SEAM_HPP
#include "frame/lzss_position_distance_8m_owning_adapter.hpp"
namespace marc::test::owning8m {
struct Controller {
  struct Record {
    void *p{};
    std::size_t bytes{};
  };
  std::array<Record, 12> records{};
  std::size_t calls{}, fail_at{}, oversize_at{}, live{}, peak{}, deleted{},
      injections{};
  bool valid{true};
};
class Scope {
public:
  explicit Scope(Controller &) noexcept;
  ~Scope();

private:
  Controller *old_;
};
} // namespace marc::test::owning8m
#endif

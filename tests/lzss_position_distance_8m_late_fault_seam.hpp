#ifndef MARC_TEST_8M_LATE_FAULT_SEAM_HPP
#define MARC_TEST_8M_LATE_FAULT_SEAM_HPP
#include "entropy/lzss_position_distance_8m_token_range_encoder.hpp"
#include "frame/lzss_position_distance_8m_serializer.hpp"
namespace marc::test::late8m {
struct Controller {
  unsigned mode{};
  std::uint64_t prior{};
  std::size_t raw{}, range{}, prefix{}, reparse{}, injections{};
  bool valid{true};
  frame::internal::LzssPositionDistance8mPreflightError parsed_error{};
};
class Scope {
public:
  explicit Scope(Controller &) noexcept;
  ~Scope();
  Scope(const Scope &) = delete;
  Scope &operator=(const Scope &) = delete;

private:
  Controller *previous_;
};
std::size_t working_bytes() noexcept;
} // namespace marc::test::late8m
#endif

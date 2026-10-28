#ifndef MARC_C_API_OWNING_ADAPTER_BOUNDARY_HPP
#define MARC_C_API_OWNING_ADAPTER_BOUNDARY_HPP

#include "c_api/buffer_validation.hpp"
#include "c_api/transform_handle.hpp"
#include "core/checked_math.hpp"
#include "core/status.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace marc::c_api::detail {

struct OwningAdapterRegion {
  const void *data;
  std::size_t size;
};
inline bool disjoint(OwningAdapterRegion a, OwningAdapterRegion b) noexcept {
  return disjoint_buffers(a.data, a.size, b.data, b.size);
}

template <class Adapter, class Config>
class OwningAdapterBoundary final : public core::Transform {
public:
  explicit OwningAdapterBoundary(const Config &c) noexcept
      : adapter_(c) {}
  core::ProcessResult ready() noexcept { return adapter_.process({}, {}, 0); }
  void handle(marc_transform *h) noexcept { handle_ = h; }
  core::ProcessResult process(std::span<const std::byte> in,
                              std::span<std::byte> out,
                              std::uint32_t flags) noexcept override {
    if (terminal_)
      return {0, 0, last_.status, last_.error};
    const std::array regions{
        OwningAdapterRegion{in.data(), in.size()}, OwningAdapterRegion{out.data(), out.size()},
        OwningAdapterRegion{this, sizeof(*this)}, OwningAdapterRegion{handle_, sizeof(*handle_)}};
    for (std::size_t i = 0; i < regions.size(); ++i)
      for (std::size_t j = i + 1; j < regions.size(); ++j)
        if (!disjoint(regions[i], regions[j])) {
          last_ = {0,
                   0,
                   core::StreamStatus::error,
                   {core::ErrorCode::invalid_argument, accepted_, 0}};
          terminal_ = true;
          return last_;
        }
    last_ = adapter_.process(in, out, flags);
    // The unchanged coordinator proves bounded consumption and total size.
    if (!core::checked_add(accepted_,
                           static_cast<std::uint64_t>(last_.input_consumed),
                           accepted_)) {
      last_.status = core::StreamStatus::error;
      last_.error = {core::ErrorCode::internal_error, accepted_, 0};
    }
    terminal_ = last_.status == core::StreamStatus::error ||
                last_.status == core::StreamStatus::end_of_stream;
    return last_;
  }

private:
  Adapter adapter_;
  marc_transform *handle_{};
  std::uint64_t accepted_{};
  core::ProcessResult last_{};
  bool terminal_{};
};

} // namespace marc::c_api::detail

#endif

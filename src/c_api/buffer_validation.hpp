#ifndef MARC_C_API_BUFFER_VALIDATION_HPP
#define MARC_C_API_BUFFER_VALIDATION_HPP

#include "core/buffer_overlap.hpp"

namespace marc::c_api {

[[nodiscard]] inline bool disjoint_buffers(const void* a, std::size_t an, const void* b,
                                           std::size_t bn) noexcept {
    return core::check_buffer_overlap(a, an, b, bn) == core::BufferOverlap::disjoint;
}

} // namespace marc::c_api

#endif

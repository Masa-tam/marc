#ifndef MARC_C_API_TRANSFORM_HANDLE_HPP
#define MARC_C_API_TRANSFORM_HANDLE_HPP

#include <type_traits>

namespace marc::core { class Transform; }

// Private completion of the public opaque C type. Keep its layout stable:
// factories charge this object and reject overlap with the entire handle.
struct marc_transform {
    marc::core::Transform* implementation;
};

static_assert(std::is_standard_layout_v<marc_transform>);
static_assert(std::is_aggregate_v<marc_transform>);
static_assert(sizeof(marc_transform) == sizeof(marc::core::Transform*));
static_assert(alignof(marc_transform) == alignof(marc::core::Transform*));

#endif

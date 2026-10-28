#include "c_api/status.hpp"
#include "c_api/owning_adapter_boundary.hpp"
#include "c_api/transform_handle.hpp"
#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"
#include "frame/lzss_position_distance_8m_owning_adapter.hpp"
#include "marc/marc.h"
#include <array>
#include <limits>
#include <memory>
#include <new>


namespace {
using marc::c_api::factory_status;

using namespace marc;
using namespace frame::internal;
using Config = marc_lzss_position_distance_dynamic_range_8m_config;
using Resources = marc_lzss_position_distance_dynamic_range_8m_resources;
using Region = marc::c_api::detail::OwningAdapterRegion;
using marc::c_api::detail::disjoint;
using Boundary = marc::c_api::detail::OwningAdapterBoundary<
    LzssPositionDistance8mOwningAdapter, LzssPositionDistance8mOwningConfig>;
constexpr std::size_t public_controls =
    sizeof(Config) + sizeof(Resources) +
    2 * sizeof(LzssPositionDistance8mOwningConfig) +
    2 * sizeof(LzssPositionDistance8mOwningRequirements) +
    4 * sizeof(core::ProcessResult) + 12 * sizeof(Region) +
    16 * sizeof(std::size_t) + 8 * sizeof(void *);

marc_status prepare(const Config *c, LzssPositionDistance8mOwningConfig &p,
                    LzssPositionDistance8mOwningRequirements &r) noexcept {
  if (!c || c->struct_size != sizeof(*c) ||
      c->abi_version != MARC_ABI_VERSION || c->reserved || c->reserved2 ||
      c->encoder_strategy != MARC_LZSS_POSITION_DISTANCE_8M_PREPARED_OWNING ||
      !c->frame_size || c->frame_size > 8388608)
    return MARC_STATUS_INVALID_ARGUMENT;
  const auto maximum = std::numeric_limits<std::size_t>::max();
  if (c->external_retained_bytes > maximum ||
      c->input_capacity_bytes > maximum || c->output_capacity_bytes > maximum)
    return MARC_STATUS_LIMIT_EXCEEDED;
  p = {};
  p.stream = {c->frame_size,
              c->original_size,
              {8388608, 3, 258, 0},
              32768,
              47,
              11,
              1,
              12};
  auto &l = p.limits;
  l.max_total_output_size = c->max_total_output_size;
  l.max_frame_size = c->max_frame_size;
  l.max_block_size = c->max_block_size;
  l.max_compressed_payload_size = c->max_compressed_payload_size;
  l.max_internal_buffered_bytes = c->max_internal_buffered_bytes;
  l.max_lz_distance = c->max_lz_distance;
  l.max_lz_match_length = c->max_lz_match_length;
  l.max_entropy_table_entries = c->max_entropy_table_entries;
  l.max_range_model_total = c->max_range_model_total;
  l.max_expansion_ratio = c->max_expansion_ratio;
  l.expansion_slack = c->expansion_slack;
  if (core::validate_limits(l) != core::LimitError::none)
    return MARC_STATUS_INVALID_ARGUMENT;
  p.external = static_cast<std::size_t>(c->external_retained_bytes);
  for (auto n : {sizeof(marc_transform), sizeof(Boundary), public_controls})
    if (!core::checked_add(p.external, n, p.external))
      return MARC_STATUS_LIMIT_EXCEEDED;
  p.input_capacity = static_cast<std::size_t>(c->input_capacity_bytes);
  p.output_capacity = static_cast<std::size_t>(c->output_capacity_bytes);
  r = LzssPositionDistance8mOwningAdapter::query(p);
  return factory_status(r.error);
}
} // namespace
marc_status
marc_lzss_position_distance_dynamic_range_8m_config_init(Config *c) noexcept {
  if (!c)
    return MARC_STATUS_INVALID_ARGUMENT;
  Config v{};
  v.struct_size = sizeof(v);
  v.abi_version = MARC_ABI_VERSION;
  v.encoder_strategy = MARC_LZSS_POSITION_DISTANCE_8M_PREPARED_OWNING;
  v.frame_size = 8388608;
  v.max_frame_size = 8388608;
  v.max_block_size = 8388608;
  v.max_lz_distance = 8388608;
  v.max_lz_match_length = 258;
  v.max_range_model_total = 32768;
  *c = v;
  return MARC_STATUS_OK;
}
marc_status marc_lzss_position_distance_dynamic_range_8m_resource_requirements(
    const Config *c, Resources *out) noexcept {
  if (!c || !out || !disjoint({c, sizeof(*c)}, {out, sizeof(*out)}))
    return MARC_STATUS_INVALID_ARGUMENT;
  LzssPositionDistance8mOwningConfig p{};
  LzssPositionDistance8mOwningRequirements r{};
  const auto s = prepare(c, p, r);
  if (s != MARC_STATUS_OK)
    return s;
  *out = {sizeof(*out),
          MARC_ABI_VERSION,
          r.external_charge,
          r.fixed_bytes,
          r.raw_bytes,
          r.index_entries,
          r.initial_bytes,
          MARC_LZSS_POSITION_DISTANCE_8M_INITIAL_ONLY,
          0};
  return MARC_STATUS_OK;
}
marc_status marc_lzss_position_distance_dynamic_range_8m_create_encoder(
    const Config *c, marc_transform **out) noexcept {
  if (!out || (c && !disjoint({c, sizeof(*c)}, {out, sizeof(*out)})))
    return MARC_STATUS_INVALID_ARGUMENT;
  *out = nullptr;
  LzssPositionDistance8mOwningConfig p{};
  LzssPositionDistance8mOwningRequirements r{};
  const auto s = prepare(c, p, r);
  if (s != MARC_STATUS_OK)
    return s;
  auto guard = std::unique_ptr<Boundary>(new (std::nothrow) Boundary(p));
  if (!guard)
    return MARC_STATUS_OUT_OF_MEMORY;
  // Zero extents, no flags: detects constructor allocation failure without
  // consuming input or exposing even a stream-header byte.
  const auto ready = guard->ready();
  if (ready.status == core::StreamStatus::error)
    return factory_status(ready.error.code);
  if (ready.status != core::StreamStatus::need_output || ready.input_consumed ||
      ready.output_produced)
    return MARC_STATUS_INTERNAL_ERROR;
  auto *h = new (std::nothrow) marc_transform{guard.get()};
  if (!h)
    return MARC_STATUS_OUT_OF_MEMORY;
  guard->handle(h);
  guard.release();
  *out = h;
  return MARC_STATUS_OK;
}

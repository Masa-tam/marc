#ifndef MARC_TOOLS_POSITION_DISTANCE_8M_CLI_HPP
#define MARC_TOOLS_POSITION_DISTANCE_8M_CLI_HPP

#include "support/file_pump.hpp"
#include "support/borrowed_storage.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <marc/marc.h>
#include <memory>
#include <new>

namespace marc_cli_8m {
constexpr std::size_t io_size = 65536;
constexpr std::uint64_t frame = 8388608, policy = UINT64_C(1) << 30;

using Storage = marc::cli::Storage;
struct Destroy {
  void operator()(marc_transform *p) const noexcept {
    marc_transform_destroy(p);
  }
};
using Handle = std::unique_ptr<marc_transform, Destroy>;
struct Context {
  Storage serialized, tokens, token_scratch, raw, raw_scratch, input, output;
  marc_lzss_position_distance_dynamic_range_8m_config encoder{};
  marc_lzss_position_distance_dynamic_range_8m_resources resources{};
  marc_lzss_position_distance_dynamic_range_8m_decoder_config decoder{};
  marc_lzss_position_distance_dynamic_range_8m_decoder_requirements
      requirements{};
  marc_lzss_position_distance_dynamic_range_8m_decoder_buffers buffers{};
};
using Loop = marc::cli::FilePumpState;
inline bool add(std::uint64_t &n, std::uint64_t v) noexcept {
  if (v > std::numeric_limits<std::uint64_t>::max() - n)
    return false;
  n += v;
  return true;
}
inline bool external_charge(std::uint64_t &n) noexcept {
  n = 0;
  // Complete retained objects plus a conservative reserve for helper locals.
  // File-library internals and allocator overhead are not a codec RSS limit.
  for (auto size :
       {sizeof(Context), sizeof(Loop), sizeof(Handle), sizeof(std::ifstream),
        sizeof(std::ofstream), 32 * sizeof(std::size_t), 8 * sizeof(void *),
        4 * sizeof(marc_status), sizeof(marc_transform *),
        sizeof(marc_direction), sizeof(std::uint64_t)})
    if (!add(n, size))
      return false;
  return true;
}
template <class C> inline void limits(C &c, std::uint64_t external) noexcept {
  c.max_total_output_size = UINT64_C(1) << 40;
  c.max_frame_size = c.max_block_size = c.max_lz_distance = frame;
  c.max_lz_match_length = 258;
  c.max_compressed_payload_size = 18 * frame + 5;
  c.max_internal_buffered_bytes = policy;
  c.max_entropy_table_entries = 2599;
  c.max_range_model_total = 32768;
  c.max_expansion_ratio = 1024;
  c.expansion_slack = UINT64_C(1) << 20;
  c.external_retained_bytes = external;
  c.input_capacity_bytes = c.output_capacity_bytes = io_size;
}
inline marc_status create(Context &c, marc_direction direction,
                          std::uint64_t size, marc_transform **out) noexcept {
  std::uint64_t external{};
  if (!external_charge(external))
    return MARC_STATUS_LIMIT_EXCEEDED;
  if (direction == MARC_DIRECTION_ENCODE) {
    auto s =
        marc_lzss_position_distance_dynamic_range_8m_config_init(&c.encoder);
    if (s != MARC_STATUS_OK)
      return s;
    limits(c.encoder, external);
    c.encoder.original_size = size;
    c.resources.struct_size = sizeof(c.resources);
    c.resources.abi_version = MARC_ABI_VERSION;
    s = marc_lzss_position_distance_dynamic_range_8m_resource_requirements(
        &c.encoder, &c.resources);
    if (s != MARC_STATUS_OK)
      return s;
    if (c.resources.admission_scope !=
        MARC_LZSS_POSITION_DISTANCE_8M_INITIAL_ONLY)
      return MARC_STATUS_INTERNAL_ERROR;
    return marc_lzss_position_distance_dynamic_range_8m_create_encoder(
        &c.encoder, out);
  }
  if (direction != MARC_DIRECTION_DECODE)
    return MARC_STATUS_INVALID_ARGUMENT;
  auto s = marc_lzss_position_distance_dynamic_range_8m_decoder_config_init(
      &c.decoder);
  if (s != MARC_STATUS_OK)
    return s;
  limits(c.decoder, external);
  auto &q = c.requirements;
  q.struct_size = sizeof(q);
  q.abi_version = MARC_ABI_VERSION;
  s = marc_lzss_position_distance_dynamic_range_8m_decoder_workspace_requirements(
      &c.decoder, &q);
  if (s != MARC_STATUS_OK)
    return s;
  if (q.admission_scope != MARC_LZSS_POSITION_DISTANCE_8M_CAPACITY_ONLY)
    return MARC_STATUS_INTERNAL_ERROR;
  std::uint64_t total{};
  const std::array bytes{q.serialized_bytes, q.token_bytes,
                         q.token_scratch_bytes, q.raw_bytes,
                         q.raw_scratch_bytes};
  const std::array alignment{std::uint64_t{alignof(std::max_align_t)},
                             q.token_alignment, q.token_alignment,
                             std::uint64_t{alignof(std::max_align_t)},
                             std::uint64_t{alignof(std::max_align_t)}};
  // Validate every extent before the first workspace allocation.
  for (std::size_t i = 0; i < bytes.size(); ++i)
    if (!bytes[i] || bytes[i] > std::numeric_limits<std::size_t>::max() ||
        !alignment[i] || (alignment[i] & (alignment[i] - 1)) ||
        alignment[i] > std::numeric_limits<std::size_t>::max() ||
        !add(total, bytes[i]))
      return MARC_STATUS_LIMIT_EXCEEDED;
  if (total > policy || q.minimum_aggregate_bytes > policy)
    return MARC_STATUS_LIMIT_EXCEEDED;
  const std::array<Storage *, 5> owners{
      &c.serialized, &c.tokens, &c.token_scratch, &c.raw, &c.raw_scratch};
  for (std::size_t i = 0; i < owners.size(); ++i) {
    s = owners[i]->allocate(bytes[i], alignment[i]);
    if (s != MARC_STATUS_OK)
      return s;
  }
  c.buffers = {sizeof(c.buffers),
               MARC_ABI_VERSION,
               0,
               0,
               c.serialized.view(),
               c.tokens.view(),
               c.token_scratch.view(),
               c.raw.view(),
               c.raw_scratch.view()};
  return marc_lzss_position_distance_dynamic_range_8m_create_decoder(
      &c.decoder, &c.buffers, out);
}
inline bool fail(const char *message, marc_status status) {
  std::cerr << "marc: " << message << ": " << marc_status_name(status) << '\n';
  return false;
}
inline bool process_file(marc_direction direction, std::uint64_t size,
                         std::ifstream &source, std::ofstream &sink) {
  Context c; // All borrowed storage outlives the handle on every return.
  Loop loop;
  marc_transform *p{};
  auto s = create(c, direction, size, &p);
  Handle handle{p};
  if (s != MARC_STATUS_OK)
    return fail("transform creation failed", s);
  s = c.input.allocate(io_size, alignof(std::max_align_t));
  if (s == MARC_STATUS_OK)
    s = c.output.allocate(io_size, alignof(std::max_align_t));
  if (s != MARC_STATUS_OK)
    return fail("I/O buffer allocation failed", s);
  return marc::cli::pump_workspace_profile<marc::cli::FilePumpPolicy::BorrowedRange>(
      handle.get(), size, source, sink, c.input.view(), c.output.view(), loop);
}
} // namespace marc_cli_8m
#endif

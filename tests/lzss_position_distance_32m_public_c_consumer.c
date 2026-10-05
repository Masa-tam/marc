#include "marc/marc.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void) {
  marc_lzss_position_distance_dynamic_range_32m_config c;
  marc_lzss_position_distance_dynamic_range_32m_resources q;
  marc_transform *h = NULL;
  uint8_t input[1] = {0x7f}, output[4096];
  marc_process_result r, sticky;
  if (marc_lzss_position_distance_dynamic_range_32m_config_init(&c) !=
      MARC_STATUS_OK)
    return 1;
  c.original_size = 1;
  c.frame_size = 64;
  c.max_frame_size = c.max_block_size = 64;
  c.max_total_output_size = 1048576;
  c.max_compressed_payload_size = 65536;
  c.max_internal_buffered_bytes = 8 * 1048576;
  c.max_entropy_table_entries = 2621;
  c.max_expansion_ratio = 1024;
  c.expansion_slack = 1048576;
  c.external_retained_bytes = sizeof(c) + sizeof(q) + sizeof(input) +
                              sizeof(output) + sizeof(h) + sizeof(r) +
                              sizeof(sticky) + 4096;
  c.input_capacity_bytes = sizeof(input);
  c.output_capacity_bytes = sizeof(output);
  if (marc_lzss_position_distance_dynamic_range_32m_resource_requirements(
          &c, &q) != MARC_STATUS_OK)
    return 2;
  if (q.admission_scope != MARC_LZSS_POSITION_DISTANCE_32M_INITIAL_ONLY ||
      q.initial_raw_bytes != 1 || q.initial_index_entries != 1048577)
    return 3;
  if (marc_lzss_position_distance_dynamic_range_32m_create_encoder(&c, &h) !=
          MARC_STATUS_OK ||
      h == NULL)
    return 4;
  memset(&c, 0, sizeof(c)); /* configuration is no longer borrowed */
  memset(output, 0xa5, sizeof(output));
  r = marc_transform_process(h, (marc_const_buffer){input, 1},
                             (marc_buffer){output, sizeof(output)},
                             MARC_PROCESS_END_INPUT);
  if (r.status != MARC_STATUS_END_OF_STREAM || r.input_consumed != 1 ||
      r.output_produced <= 192)
    return 5;
  if (memcmp(output, "MARC", 4) != 0 || output[14] != 13 || output[98] != 14)
    return 6;
  for (size_t i = r.output_produced; i < sizeof(output); ++i)
    if (output[i] != 0xa5)
      return 7;
  sticky = marc_transform_process(h, (marc_const_buffer){input, 1},
                                  (marc_buffer){output, 1}, UINT32_MAX);
  if (sticky.status != MARC_STATUS_END_OF_STREAM || sticky.input_consumed ||
      sticky.output_produced)
    return 8;
  marc_transform_destroy(h);
  {
    marc_lzss_position_distance_dynamic_range_32m_decoder_config d;
    marc_lzss_position_distance_dynamic_range_32m_decoder_requirements
        requirements = {0};
    marc_lzss_position_distance_dynamic_range_32m_decoder_buffers buffers = {0};
    marc_transform *decoder = NULL;
    uint8_t *allocations[5] = {NULL};
    uint8_t decoded[16];
    marc_buffer *views[5] = {&buffers.serialized, &buffers.tokens,
                             &buffers.token_scratch, &buffers.raw,
                             &buffers.raw_scratch};
    uint64_t capacities[5];
    int failed = 1;
    if (marc_lzss_position_distance_dynamic_range_32m_decoder_config_init(&d) !=
        MARC_STATUS_OK)
      return 9;
    d.max_total_output_size = 1048576;
    d.max_frame_size = d.max_block_size = 64;
    d.max_compressed_payload_size = 65536;
    d.max_internal_buffered_bytes = 8 * 1048576;
    d.max_entropy_table_entries = 2621;
    d.max_expansion_ratio = 1024;
    d.expansion_slack = 1048576;
    d.external_retained_bytes = 65536;
    d.input_capacity_bytes = sizeof(output);
    d.output_capacity_bytes = sizeof(decoded);
    requirements.struct_size = sizeof(requirements);
    requirements.abi_version = MARC_ABI_VERSION;
    if (marc_lzss_position_distance_dynamic_range_32m_decoder_workspace_requirements(
            &d, &requirements) != MARC_STATUS_OK ||
        requirements.token_alignment != 1 ||
        requirements.record_capacity_bytes != 192)
      return 10;
    capacities[0] = requirements.serialized_bytes;
    capacities[1] = requirements.token_bytes;
    capacities[2] = requirements.token_scratch_bytes;
    capacities[3] = requirements.raw_bytes;
    capacities[4] = requirements.raw_scratch_bytes;
    buffers.struct_size = sizeof(buffers);
    buffers.abi_version = MARC_ABI_VERSION;
    for (size_t i = 0; i < 5; ++i) {
      allocations[i] = (uint8_t *)malloc((size_t)capacities[i] + 1);
      if (!allocations[i])
        goto cleanup;
      allocations[i][0] = 0xa5;
      *views[i] = (marc_buffer){allocations[i] + 1, (size_t)capacities[i]};
    }
    if (marc_lzss_position_distance_dynamic_range_32m_create_decoder(
            &d, &buffers, &decoder) != MARC_STATUS_OK ||
        !decoder)
      goto cleanup;
    memset(&d, 0, sizeof(d));
    memset(&buffers, 0,
           sizeof(buffers)); /* both metadata objects were copied */
    memset(decoded, 0xa5, sizeof(decoded));
    sticky = marc_transform_process(
        decoder, (marc_const_buffer){output, r.output_produced},
        (marc_buffer){decoded, sizeof(decoded)}, MARC_PROCESS_END_INPUT);
    if (sticky.status != MARC_STATUS_END_OF_STREAM ||
        sticky.input_consumed != r.output_produced ||
        sticky.output_produced != 1 || decoded[0] != input[0])
      goto cleanup;
    for (size_t i = 1; i < sizeof(decoded); ++i)
      if (decoded[i] != 0xa5)
        goto cleanup;
    for (size_t i = 0; i < 5; ++i)
      if (allocations[i][0] != 0xa5)
        goto cleanup;
    failed = 0;
  cleanup:
    marc_transform_destroy(decoder);
    for (size_t i = 0; i < 5; ++i)
      free(allocations[i]);
    if (failed)
      return 11;
  }
  puts("PASS thirty-two-MiB public C consumer");
  return 0;
}

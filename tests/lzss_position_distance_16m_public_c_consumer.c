#include "marc/marc.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
int main(void) {
  marc_lzss_position_distance_dynamic_range_16m_config c;
  marc_lzss_position_distance_dynamic_range_16m_resources q;
  marc_transform *h = NULL;
  uint8_t input[1] = {0x7f}, output[4096];
  marc_process_result r, sticky;
  if (marc_lzss_position_distance_dynamic_range_16m_config_init(&c) !=
      MARC_STATUS_OK)
    return 1;
  c.original_size = 1;
  c.frame_size = 64;
  c.max_frame_size = c.max_block_size = 64;
  c.max_total_output_size = 1048576;
  c.max_compressed_payload_size = 65536;
  c.max_internal_buffered_bytes = 8 * 1048576;
  c.max_entropy_table_entries = 2610;
  c.max_expansion_ratio = 1024;
  c.expansion_slack = 1048576;
  c.external_retained_bytes = sizeof(c) + sizeof(q) + sizeof(input) +
                              sizeof(output) + sizeof(h) + sizeof(r) +
                              sizeof(sticky) + 4096;
  c.input_capacity_bytes = sizeof(input);
  c.output_capacity_bytes = sizeof(output);
  if (marc_lzss_position_distance_dynamic_range_16m_resource_requirements(
          &c, &q) != MARC_STATUS_OK)
    return 2;
  if (q.admission_scope != MARC_LZSS_POSITION_DISTANCE_16M_INITIAL_ONLY ||
      q.initial_raw_bytes != 1 || q.initial_index_entries != 1048577)
    return 3;
  if (marc_lzss_position_distance_dynamic_range_16m_create_encoder(&c, &h) !=
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
  if (memcmp(output, "MARC", 4) != 0 || output[14] != 12 || output[98] != 13)
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
  puts("PASS sixteen-MiB public C consumer");
  return 0;
}

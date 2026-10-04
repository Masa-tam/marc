#include "marc/marc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void) {
  marc_lzss_position_distance_dynamic_range_8m_config e;
  marc_lzss_position_distance_dynamic_range_8m_decoder_config c;
  marc_lzss_position_distance_dynamic_range_8m_decoder_requirements q = {0};
  marc_lzss_position_distance_dynamic_range_8m_decoder_buffers b = {0};
  marc_transform *encoder = NULL, *decoder = NULL;
  uint8_t raw[160], wire[4096], out[176];
  if (marc_lzss_position_distance_dynamic_range_8m_config_init(&e) !=
          MARC_STATUS_OK ||
      marc_lzss_position_distance_dynamic_range_8m_decoder_config_init(&c) !=
          MARC_STATUS_OK)
    return 1;
  for (size_t i = 0; i < 160; ++i)
    raw[i] = (uint8_t)((i % 64) % 7);
  e.original_size = 160;
  e.frame_size = 64;
  e.max_frame_size = e.max_block_size = 64;
  e.max_total_output_size = 1048576;
  e.max_compressed_payload_size = 65536;
  e.max_internal_buffered_bytes = 4194304;
  e.max_entropy_table_entries = 2599;
  e.max_expansion_ratio = 1024;
  e.expansion_slack = 1048576;
  e.input_capacity_bytes = 160;
  e.output_capacity_bytes = 4096;
  if (marc_lzss_position_distance_dynamic_range_8m_create_encoder(
          &e, &encoder) != MARC_STATUS_OK)
    return 2;
  marc_process_result r =
      marc_transform_process(encoder, (marc_const_buffer){raw, 160},
                             (marc_buffer){wire, 4096}, MARC_PROCESS_END_INPUT);
  if (r.status != MARC_STATUS_END_OF_STREAM)
    return 3;
  const size_t wire_size = r.output_produced;
  marc_transform_destroy(encoder);
  c.max_total_output_size = 1048576;
  c.max_frame_size = c.max_block_size = 64;
  c.max_compressed_payload_size = 65536;
  c.max_internal_buffered_bytes = 4194304;
  c.max_entropy_table_entries = 2599;
  c.max_expansion_ratio = 1024;
  c.expansion_slack = 1048576;
  c.input_capacity_bytes = 4096;
  c.output_capacity_bytes = 176;
  c.external_retained_bytes = 65536;
  q.struct_size = sizeof(q);
  q.abi_version = MARC_ABI_VERSION;
  if (marc_lzss_position_distance_dynamic_range_8m_decoder_workspace_requirements(
          &c, &q) != MARC_STATUS_OK)
    return 4;
  if (q.admission_scope != MARC_LZSS_POSITION_DISTANCE_8M_CAPACITY_ONLY ||
      q.raw_bytes != 64)
    return 5;
  b.struct_size = sizeof(b);
  b.abi_version = MARC_ABI_VERSION;
  b.serialized = (marc_buffer){malloc((size_t)q.serialized_bytes),
                               (size_t)q.serialized_bytes};
  b.tokens =
      (marc_buffer){malloc((size_t)q.token_bytes), (size_t)q.token_bytes};
  b.token_scratch = (marc_buffer){malloc((size_t)q.token_scratch_bytes),
                                  (size_t)q.token_scratch_bytes};
  b.raw = (marc_buffer){malloc((size_t)q.raw_bytes), (size_t)q.raw_bytes};
  b.raw_scratch = (marc_buffer){malloc((size_t)q.raw_scratch_bytes),
                                (size_t)q.raw_scratch_bytes};
  if (!b.serialized.data || !b.tokens.data || !b.token_scratch.data ||
      !b.raw.data || !b.raw_scratch.data)
    return 6;
  if (marc_lzss_position_distance_dynamic_range_8m_create_decoder(
          &c, &b, &decoder) != MARC_STATUS_OK)
    return 7;
  memset(&c, 0, sizeof(c)); /* caller configuration may expire */
  memset(out, 0xa5, sizeof(out));
  size_t used = 0, made = 0;
  for (size_t iterations = 0; iterations < 4096; ++iterations) {
    const size_t n = used < wire_size ? 1 : 0;
    r = marc_transform_process(decoder, (marc_const_buffer){wire + used, n},
                               (marc_buffer){out + made, 1},
                               used + n == wire_size ? MARC_PROCESS_END_INPUT
                                                     : MARC_PROCESS_FLUSH);
    used += r.input_consumed;
    made += r.output_produced;
    if (r.status == MARC_STATUS_END_OF_STREAM)
      break;
    if (r.status >= MARC_STATUS_INVALID_ARGUMENT)
      return 8;
  }
  if (r.status != MARC_STATUS_END_OF_STREAM || made != 160 ||
      memcmp(out, raw, 160))
    return 9;
  for (size_t i = 160; i < sizeof(out); ++i)
    if (out[i] != 0xa5)
      return 10;
  r = marc_transform_process(decoder, (marc_const_buffer){NULL, 0},
                             (marc_buffer){NULL, 0}, 0);
  if (r.status != MARC_STATUS_END_OF_STREAM || r.input_consumed ||
      r.output_produced)
    return 11;
  marc_transform_destroy(decoder);
  free(b.serialized.data);
  free(b.tokens.data);
  free(b.token_scratch.data);
  free(b.raw.data);
  free(b.raw_scratch.data);
  puts("PASS five-buffer public decoder C consumer");
  return 0;
}

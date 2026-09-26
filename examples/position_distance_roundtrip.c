#include <marc/marc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Configuration defaults suffice: this family has no apply_profile helper.
 * Use small frames here to demonstrate multiple frames in a short example. */
static int transform_bytes(marc_direction direction,
    const uint8_t* input, size_t input_size,
    uint8_t* output, size_t output_capacity, size_t* output_size) {
    marc_lzss_position_distance_dynamic_range_config config;
    marc_workspace_requirements needed;
    marc_transform* transform = NULL;
    marc_buffer primary = {NULL, 0}, secondary = {NULL, 0}, views = {NULL, 0};
    size_t consumed = 0, produced = 0, calls;
    int ok = 0;
    marc_status status = marc_lzss_position_distance_dynamic_range_config_init(direction, &config);
    if (status != MARC_STATUS_OK) goto cleanup;
    config.original_size = input_size; /* Ignored when decoding. */
    config.frame_size = 32;            /* Also ignored when decoding. */
    config.max_frame_size = 32;
    config.max_block_size = 32;
    config.max_compressed_payload_size = 18 * 32 + 5;
    status = marc_lzss_position_distance_dynamic_range_workspace_requirements(&config, &needed);
    if (status != MARC_STATUS_OK) goto cleanup;
    primary.data = (uint8_t*)malloc(needed.primary_bytes); primary.size = needed.primary_bytes;
    secondary.data = (uint8_t*)malloc(needed.secondary_bytes); secondary.size = needed.secondary_bytes;
    views.data = (uint8_t*)malloc(needed.views_bytes); views.size = needed.views_bytes;
    if (!primary.data || !secondary.data || !views.data) {
        status = MARC_STATUS_OUT_OF_MEMORY; goto cleanup;
    }
    /* malloc satisfies the fundamental alignment used by this codec. */
    if ((uintptr_t)views.data % needed.views_alignment != 0) {
        status = MARC_STATUS_INVALID_ARGUMENT; goto cleanup;
    }
    status = marc_lzss_position_distance_dynamic_range_create(&config,
        primary, secondary, views, &transform);
    if (status != MARC_STATUS_OK) goto cleanup;
    for (calls = 0; calls < 100000; ++calls) {
        size_t n = consumed < input_size ? 1 : 0;
        size_t m = produced < output_capacity ? 1 : 0;
        marc_const_buffer source = {n ? input + consumed : NULL, n};
        marc_buffer sink = {m ? output + produced : NULL, m};
        marc_process_flags flags = consumed + n == input_size ? MARC_PROCESS_END_INPUT : MARC_PROCESS_NONE;
        marc_process_result result = marc_transform_process(transform, source, sink, flags);
        status = result.status;
        if (result.input_consumed > n || result.output_produced > m) goto cleanup;
        if (status < MARC_STATUS_PROGRESS || status > MARC_STATUS_END_OF_STREAM) goto cleanup;
        if (status == MARC_STATUS_PROGRESS && !result.input_consumed && !result.output_produced) goto cleanup;
        consumed += result.input_consumed; produced += result.output_produced;
        if (status == MARC_STATUS_END_OF_STREAM) {
            ok = consumed == input_size;
            if (ok) *output_size = produced;
            break;
        }
        if (status == MARC_STATUS_NEED_OUTPUT && produced == output_capacity) goto cleanup;
    }
cleanup:
    marc_transform_destroy(transform); /* Destroy before freeing retained storage. */
    free(views.data); free(secondary.data); free(primary.data);
    if (!ok) fprintf(stderr, "position-distance transform failed: %s\n", marc_status_name(status));
    return ok;
}

int main(void) {
    static const uint8_t input[] =
        "marc position-distance streaming example: "
        "repeated text, repeated text, repeated text.";
    uint8_t encoded[4096], decoded[sizeof(input)];
    size_t encoded_size = 0, decoded_size = 0;
    if (!transform_bytes(MARC_DIRECTION_ENCODE, input, sizeof(input), encoded,
            sizeof(encoded), &encoded_size)
        || !transform_bytes(MARC_DIRECTION_DECODE, encoded, encoded_size, decoded,
            sizeof(decoded), &decoded_size)) return 1;
    if (decoded_size != sizeof(input) || memcmp(input, decoded, sizeof(input)) != 0) return 1;
    puts("position-distance installed-package round trip passed");
    return 0;
}

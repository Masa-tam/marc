#include <marc/marc.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static marc_buffer allocate_workspace(size_t size) {
    marc_buffer result = {NULL, size};
    if (size != 0) result.data = (uint8_t*)malloc(size);
    return result;
}

static int workspace_ok(marc_buffer workspace) {
    return workspace.size == 0 || workspace.data != NULL;
}

int main(void) {
    static const uint8_t input[] = "marc C API round trip";
    uint8_t encoded[1024];
    uint8_t decoded[sizeof(input)];
    marc_blocked_huffman_config config;
    marc_workspace_requirements needed;
    marc_transform* transform = NULL;

    if (marc_blocked_huffman_config_init(MARC_DIRECTION_ENCODE, &config)
            != MARC_STATUS_OK) return 1;
    config.original_size = sizeof(input);
    config.frame_size = 64;
    config.block_size = 32;
    if (marc_blocked_huffman_workspace_requirements(&config, &needed)
            != MARC_STATUS_OK) return 1;
    marc_buffer primary = allocate_workspace(needed.primary_bytes);
    marc_buffer secondary = allocate_workspace(needed.secondary_bytes);
    marc_buffer views = {NULL, 0};
    if (!workspace_ok(primary) || !workspace_ok(secondary)
        || marc_blocked_huffman_create(
            &config, primary, secondary, views, &transform)
            != MARC_STATUS_OK) return 1;
    const marc_const_buffer raw = {input, sizeof(input)};
    const marc_buffer encoded_output = {encoded, sizeof(encoded)};
    marc_process_result result = marc_transform_process(
        transform, raw, encoded_output, MARC_PROCESS_END_INPUT);
    if (result.status != MARC_STATUS_END_OF_STREAM) return 1;
    const size_t encoded_size = result.output_produced;
    marc_transform_destroy(transform);
    free(primary.data);
    free(secondary.data);

    if (marc_blocked_huffman_config_init(MARC_DIRECTION_DECODE, &config)
            != MARC_STATUS_OK) return 1;
    config.max_total_output_size = 1024;
    config.max_frame_size = 1024;
    config.max_block_size = 1024;
    config.max_compressed_payload_size = 4096;
    config.max_internal_buffered_bytes = 8192;
    config.max_blocks_per_frame = 64;
    if (marc_blocked_huffman_workspace_requirements(&config, &needed)
            != MARC_STATUS_OK) return 1;
    primary = allocate_workspace(needed.primary_bytes);
    secondary = allocate_workspace(needed.secondary_bytes);
    views = allocate_workspace(needed.views_bytes);
    transform = NULL;
    if (!workspace_ok(primary) || !workspace_ok(secondary)
        || !workspace_ok(views)
        || marc_blocked_huffman_create(
            &config, primary, secondary, views, &transform)
            != MARC_STATUS_OK) return 1;
    const marc_const_buffer compressed = {encoded, encoded_size};
    const marc_buffer decoded_output = {decoded, sizeof(decoded)};
    result = marc_transform_process(
        transform, compressed, decoded_output, MARC_PROCESS_END_INPUT);
    const int success = result.status == MARC_STATUS_END_OF_STREAM
        && result.output_produced == sizeof(input)
        && memcmp(input, decoded, sizeof(input)) == 0;
    marc_transform_destroy(transform);
    free(primary.data);
    free(secondary.data);
    free(views.data);
    if (!success) return 1;
    puts("marc C API round trip succeeded");
    return 0;
}

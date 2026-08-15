#include <marc/marc.h>

#include "test_assert.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static marc_buffer allocate(size_t size) {
    marc_buffer result = {size == 0 ? NULL : (uint8_t*)malloc(size), size};
    assert(size == 0 || result.data != NULL);
    return result;
}

static void release(marc_buffer buffer) {
    free(buffer.data);
}

static void set_small_limits(
    marc_lzss_contextual_blocked_huffman_config* config) {
    config->max_total_output_size = 1024;
    config->max_frame_size = 2;
    config->max_block_size = 4096;
    config->max_compressed_payload_size = 4096;
    config->max_internal_buffered_bytes = UINT64_C(2) << 20;
    config->max_lz_distance = 65536;
    config->max_lz_match_length = 258;
    config->max_entropy_table_entries = UINT64_C(1) << 20;
}

static void set_extended_limits(
    marc_lzss_contextual_blocked_huffman_config* config,
    size_t raw_size) {
    config->max_total_output_size = raw_size;
    config->max_frame_size = raw_size;
    config->max_block_size = raw_size;
    config->max_compressed_payload_size = UINT64_C(1) << 20;
    config->max_internal_buffered_bytes = UINT64_C(64) << 20;
    config->max_lz_distance = UINT64_C(1) << 20;
    config->max_lz_match_length = 258;
    config->max_entropy_table_entries = UINT64_C(1) << 20;
}

static void run_extended_profile(
    const uint8_t* baseline_encoded,
    size_t baseline_encoded_size) {
    const size_t gap = 65536;
    const size_t raw_size = 5 + gap + 5;
    static const uint8_t marker[] = {'A', 'B', 'C', 'D', 'E'};
    uint8_t* raw = (uint8_t*)malloc(raw_size);
    uint8_t* decoded = (uint8_t*)malloc(raw_size);
    assert(raw != NULL && decoded != NULL);
    memset(raw, 'Z', raw_size);
    memcpy(raw, marker, sizeof(marker));
    memcpy(raw + raw_size - sizeof(marker), marker, sizeof(marker));

    marc_lzss_contextual_blocked_huffman_config config;
    marc_workspace_requirements needed;
    marc_transform* transform = NULL;
    assert(marc_lzss_contextual_blocked_huffman_config_init(
               MARC_DIRECTION_ENCODE, &config) == MARC_STATUS_OK);
    assert(config.window_profile == MARC_LZSS_CONTEXTUAL_WINDOW_64K);
    config.original_size = raw_size;
    config.frame_size = (uint32_t)raw_size;
    config.window_size = UINT32_C(1) << 20;
    config.window_profile = MARC_LZSS_CONTEXTUAL_WINDOW_1M;
    set_extended_limits(&config, raw_size);
    assert(marc_lzss_contextual_blocked_huffman_workspace_requirements(
               &config, &needed) == MARC_STATUS_OK);
    marc_buffer primary = allocate(needed.primary_bytes);
    marc_buffer secondary = allocate(needed.secondary_bytes);
    marc_buffer views = allocate(needed.views_bytes);
    marc_buffer encoded = allocate(needed.secondary_bytes + 112);
    assert(marc_lzss_contextual_blocked_huffman_create(
               &config, primary, secondary, views, &transform)
           == MARC_STATUS_OK);
    marc_process_result result = marc_transform_process(
        transform, (marc_const_buffer){raw, raw_size}, encoded,
        MARC_PROCESS_END_INPUT);
    assert(result.status == MARC_STATUS_END_OF_STREAM);
    assert(result.input_consumed == raw_size);
    assert(result.output_produced > 112);
    assert(encoded.data[14] == 3 && encoded.data[15] == 0);
    assert(encoded.data[16] == 2 && encoded.data[17] == 0);
    assert(encoded.data[98] == 2 && encoded.data[99] == 0);
    const size_t encoded_size = result.output_produced;
    result = marc_transform_process(
        transform, (marc_const_buffer){NULL, 0},
        (marc_buffer){NULL, 0}, MARC_PROCESS_NONE);
    assert(result.status == MARC_STATUS_END_OF_STREAM);
    marc_transform_destroy(transform);
    release(primary);
    release(secondary);
    release(views);

    assert(marc_lzss_contextual_blocked_huffman_config_init(
               MARC_DIRECTION_DECODE, &config) == MARC_STATUS_OK);
    set_extended_limits(&config, raw_size);
    marc_workspace_requirements legacy_needed;
    assert(marc_lzss_contextual_blocked_huffman_workspace_requirements(
               &config, &legacy_needed) == MARC_STATUS_OK);
    config.window_profile = MARC_LZSS_CONTEXTUAL_WINDOW_1M;
    config.window_size = UINT32_C(1) << 20;
    assert(marc_lzss_contextual_blocked_huffman_workspace_requirements(
               &config, &needed) == MARC_STATUS_OK);
    assert(needed.primary_bytes == legacy_needed.primary_bytes + 18);

    primary = allocate(legacy_needed.primary_bytes);
    secondary = allocate(legacy_needed.secondary_bytes);
    views = allocate(legacy_needed.views_bytes);
    config.window_profile = MARC_LZSS_CONTEXTUAL_WINDOW_64K;
    config.window_size = UINT32_C(1) << 16;
    assert(marc_lzss_contextual_blocked_huffman_create(
               &config, primary, secondary, views, &transform)
           == MARC_STATUS_OK);
    memset(decoded, 0xcc, raw_size);
    result = marc_transform_process(
        transform, (marc_const_buffer){encoded.data, encoded_size},
        (marc_buffer){decoded, raw_size}, MARC_PROCESS_END_INPUT);
    assert(result.status == MARC_STATUS_MALFORMED_STREAM);
    assert(result.output_produced == 0 && decoded[0] == 0xcc);
    marc_transform_destroy(transform);
    release(primary);
    release(secondary);
    release(views);

    config.window_profile = MARC_LZSS_CONTEXTUAL_WINDOW_1M;
    config.window_size = UINT32_C(1) << 20;
    primary = allocate(needed.primary_bytes);
    secondary = allocate(needed.secondary_bytes);
    views = allocate(needed.views_bytes);
    assert(marc_lzss_contextual_blocked_huffman_create(
               &config, primary, secondary, views, &transform)
           == MARC_STATUS_OK);
    result = marc_transform_process(
        transform, (marc_const_buffer){encoded.data, encoded_size},
        (marc_buffer){decoded, raw_size}, MARC_PROCESS_END_INPUT);
    assert(result.status == MARC_STATUS_END_OF_STREAM);
    assert(result.output_produced == raw_size);
    assert(memcmp(decoded, raw, raw_size) == 0);
    marc_transform_destroy(transform);

    assert(marc_lzss_contextual_blocked_huffman_create(
               &config, primary, secondary, views, &transform)
           == MARC_STATUS_OK);
    memset(decoded, 0xcc, raw_size);
    result = marc_transform_process(
        transform,
        (marc_const_buffer){baseline_encoded, baseline_encoded_size},
        (marc_buffer){decoded, raw_size}, MARC_PROCESS_END_INPUT);
    assert(result.status == MARC_STATUS_MALFORMED_STREAM);
    assert(result.output_produced == 0 && decoded[0] == 0xcc);
    marc_transform_destroy(transform);

    config.window_profile = UINT32_C(2);
    assert(marc_lzss_contextual_blocked_huffman_workspace_requirements(
               &config, &needed) == MARC_STATUS_INVALID_ARGUMENT);
    assert(needed.primary_bytes == 0 && needed.secondary_bytes == 0
           && needed.views_bytes == 0);
    assert(marc_lzss_contextual_blocked_huffman_config_init(
               MARC_DIRECTION_ENCODE, &config) == MARC_STATUS_OK);
    config.original_size = 1;
    config.frame_size = 1;
    config.window_profile = MARC_LZSS_CONTEXTUAL_WINDOW_64K;
    config.window_size = UINT32_C(1) << 20;
    assert(marc_lzss_contextual_blocked_huffman_workspace_requirements(
               &config, &needed) == MARC_STATUS_UNSUPPORTED);
    config.window_profile = MARC_LZSS_CONTEXTUAL_WINDOW_1M;
    config.reserved2 = 1;
    assert(marc_lzss_contextual_blocked_huffman_workspace_requirements(
               &config, &needed) == MARC_STATUS_INVALID_ARGUMENT);

    release(primary);
    release(secondary);
    release(views);
    release(encoded);
    free(raw);
    free(decoded);
}

int main(void) {
    static const uint8_t input[] = {0x41, 0x42, 0x41, 0x42, 0x58};
    uint8_t encoded[20000];
    uint8_t decoded[sizeof(input)];
    marc_lzss_contextual_blocked_huffman_config config;
    marc_workspace_requirements needed;
    marc_transform* transform = NULL;

    assert(marc_lzss_contextual_blocked_huffman_config_init(
               MARC_DIRECTION_ENCODE, &config) == MARC_STATUS_OK);
    assert(config.struct_size == sizeof(config));
    assert(config.abi_version == MARC_ABI_VERSION);
    assert(config.frame_size == 65536);
    assert(config.window_size == 65536);
    assert(config.min_match_length == 5);
    assert(config.max_match_length == 258);
    assert(config.window_profile == MARC_LZSS_CONTEXTUAL_WINDOW_64K);
    assert(config.reserved2 == 0);
    assert(sizeof(config) == 112);
    config.original_size = sizeof(input);
    config.frame_size = 2;
    set_small_limits(&config);
    assert(marc_lzss_contextual_blocked_huffman_workspace_requirements(
               &config, &needed) == MARC_STATUS_OK);
    assert(needed.primary_bytes == 2);
    assert(needed.secondary_bytes >= 2648);
    assert(needed.views_bytes != 0 && needed.views_alignment != 0);

    marc_buffer primary = allocate(needed.primary_bytes);
    marc_buffer secondary = allocate(needed.secondary_bytes);
    marc_buffer views = allocate(needed.views_bytes);
    assert(marc_lzss_contextual_blocked_huffman_create(
               &config, primary, secondary, views, &transform)
           == MARC_STATUS_OK);
    marc_process_result result = marc_transform_process(
        transform, (marc_const_buffer){input, sizeof(input)},
        (marc_buffer){encoded, sizeof(encoded)}, MARC_PROCESS_END_INPUT);
    assert(result.status == MARC_STATUS_END_OF_STREAM);
    assert(result.input_consumed == sizeof(input));
    assert(result.output_produced > 112);
    assert(encoded[0] == 0x4d && encoded[1] == 0x41
           && encoded[2] == 0x52 && encoded[3] == 0x43);
    assert(encoded[4] == 2 && encoded[5] == 0);
    assert(encoded[14] == 2 && encoded[15] == 0);
    assert(encoded[16] == 2 && encoded[17] == 0);
    assert(encoded[18] == 2 && encoded[19] == 0);
    const size_t encoded_size = result.output_produced;
    marc_transform_destroy(transform);
    release(primary);
    release(secondary);
    release(views);

    assert(marc_lzss_contextual_blocked_huffman_config_init(
               MARC_DIRECTION_DECODE, &config) == MARC_STATUS_OK);
    set_small_limits(&config);
    assert(marc_lzss_contextual_blocked_huffman_workspace_requirements(
               &config, &needed) == MARC_STATUS_OK);
    assert(needed.primary_bytes >= 2648);
    assert(needed.secondary_bytes == 2);
    assert(needed.views_bytes != 0 && needed.views_alignment != 0);
    primary = allocate(needed.primary_bytes);
    secondary = allocate(needed.secondary_bytes);
    views = allocate(needed.views_bytes);
    assert(marc_lzss_contextual_blocked_huffman_create(
               &config, primary, secondary, views, &transform)
           == MARC_STATUS_OK);
    result = marc_transform_process(
        transform, (marc_const_buffer){encoded, encoded_size},
        (marc_buffer){decoded, sizeof(decoded)}, MARC_PROCESS_END_INPUT);
    assert(result.status == MARC_STATUS_END_OF_STREAM);
    assert(result.input_consumed == encoded_size);
    assert(result.output_produced == sizeof(decoded));
    assert(memcmp(decoded, input, sizeof(input)) == 0);
    marc_transform_destroy(transform);

    assert(marc_lzss_contextual_blocked_huffman_create(
               &config,
               (marc_buffer){primary.data, needed.primary_bytes - 1},
               secondary, views, &transform) == MARC_STATUS_INVALID_ARGUMENT);
    assert(transform == NULL);
    assert(marc_lzss_contextual_blocked_huffman_create(
               &config, primary,
               (marc_buffer){secondary.data, needed.secondary_bytes - 1},
               views, &transform) == MARC_STATUS_INVALID_ARGUMENT);
    assert(transform == NULL);
    assert(marc_lzss_contextual_blocked_huffman_create(
               &config, primary, secondary,
               (marc_buffer){views.data, needed.views_bytes - 1},
               &transform) == MARC_STATUS_INVALID_ARGUMENT);
    assert(transform == NULL);
    assert(marc_lzss_contextual_blocked_huffman_create(
               &config, primary, secondary,
               (marc_buffer){primary.data, needed.views_bytes},
               &transform) == MARC_STATUS_INVALID_ARGUMENT);
    assert(transform == NULL);
    if (needed.views_alignment > 1) {
        marc_buffer storage = allocate(needed.views_bytes + 1);
        const marc_buffer misaligned = {
            storage.data + 1, needed.views_bytes};
        assert(marc_lzss_contextual_blocked_huffman_create(
                   &config, primary, secondary, misaligned, &transform)
               == MARC_STATUS_INVALID_ARGUMENT);
        assert(transform == NULL);
        release(storage);
    }
    assert(marc_lzss_contextual_blocked_huffman_create(
               &config, primary, secondary, views, NULL)
           == MARC_STATUS_INVALID_ARGUMENT);

    config.reserved = 1;
    assert(marc_lzss_contextual_blocked_huffman_workspace_requirements(
               &config, &needed) == MARC_STATUS_INVALID_ARGUMENT);
    config.reserved = 0;
    config.reserved2 = 1;
    assert(marc_lzss_contextual_blocked_huffman_workspace_requirements(
               &config, &needed) == MARC_STATUS_INVALID_ARGUMENT);
    config.reserved2 = 0;
    --config.struct_size;
    assert(marc_lzss_contextual_blocked_huffman_workspace_requirements(
               &config, &needed) == MARC_STATUS_INVALID_ARGUMENT);
    ++config.struct_size;
    ++config.abi_version;
    assert(marc_lzss_contextual_blocked_huffman_workspace_requirements(
               &config, &needed) == MARC_STATUS_INVALID_ARGUMENT);
    --config.abi_version;
    assert(marc_lzss_contextual_blocked_huffman_workspace_requirements(
               NULL, &needed) == MARC_STATUS_INVALID_ARGUMENT);
    assert(marc_lzss_contextual_blocked_huffman_workspace_requirements(
               &config, NULL) == MARC_STATUS_INVALID_ARGUMENT);
    config.direction = 99;
    assert(marc_lzss_contextual_blocked_huffman_workspace_requirements(
               &config, &needed) == MARC_STATUS_INVALID_ARGUMENT);
    assert(marc_lzss_contextual_blocked_huffman_config_init(0, &config)
           == MARC_STATUS_INVALID_ARGUMENT);
    assert(marc_lzss_contextual_blocked_huffman_config_init(
               MARC_DIRECTION_ENCODE, NULL) == MARC_STATUS_INVALID_ARGUMENT);

    release(primary);
    release(secondary);
    release(views);
    run_extended_profile(encoded, encoded_size);
    return 0;
}

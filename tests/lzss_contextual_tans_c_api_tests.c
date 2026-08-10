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

static void set_small_limits(marc_lzss_contextual_tans_config* config) {
    config->max_total_output_size = 1024;
    config->max_frame_size = 2;
    config->max_block_size = 4096;
    config->max_compressed_payload_size = 64;
    config->max_internal_buffered_bytes = UINT64_C(2) << 20;
    config->max_lz_distance = 65536;
    config->max_lz_match_length = 258;
    config->max_entropy_table_entries = UINT64_C(1) << 20;
}

int main(void) {
    static const uint8_t input[] = {0x41, 0x42, 0x41, 0x42, 0x58};
    uint8_t encoded[40000];
    uint8_t decoded[sizeof(input)];
    marc_lzss_contextual_tans_config config;
    marc_workspace_requirements needed;
    marc_transform* transform = NULL;

    assert(marc_lzss_contextual_tans_config_init(
               MARC_DIRECTION_ENCODE, &config) == MARC_STATUS_OK);
    assert(config.struct_size == sizeof(config));
    assert(config.abi_version == MARC_ABI_VERSION);
    assert(config.frame_size == 65536);
    assert(config.window_size == 65536);
    assert(config.min_match_length == 5);
    assert(config.max_match_length == 258);
    config.original_size = sizeof(input);
    config.frame_size = 2;
    set_small_limits(&config);
    assert(marc_lzss_contextual_tans_workspace_requirements(
               &config, &needed) == MARC_STATUS_OK);
    assert(needed.primary_bytes == 2);
    assert(needed.secondary_bytes > 9000);
    assert(needed.views_bytes != 0 && needed.views_alignment != 0);

    marc_buffer primary = allocate(needed.primary_bytes);
    marc_buffer secondary = allocate(needed.secondary_bytes);
    marc_buffer views = allocate(needed.views_bytes);
    assert(marc_lzss_contextual_tans_create(
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
    assert(encoded[16] == 5 && encoded[17] == 0);
    assert(encoded[18] == 2 && encoded[19] == 0);
    const size_t encoded_size = result.output_produced;
    marc_transform_destroy(transform);
    release(primary);
    release(secondary);
    release(views);

    assert(marc_lzss_contextual_tans_config_init(
               MARC_DIRECTION_DECODE, &config) == MARC_STATUS_OK);
    set_small_limits(&config);
    assert(marc_lzss_contextual_tans_workspace_requirements(
               &config, &needed) == MARC_STATUS_OK);
    assert(needed.primary_bytes > 9000);
    assert(needed.secondary_bytes == 2);
    assert(needed.views_bytes != 0 && needed.views_alignment != 0);
    primary = allocate(needed.primary_bytes);
    secondary = allocate(needed.secondary_bytes);
    views = allocate(needed.views_bytes);
    assert(marc_lzss_contextual_tans_create(
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

    assert(marc_lzss_contextual_tans_create(
               &config,
               (marc_buffer){primary.data, needed.primary_bytes - 1},
               secondary, views, &transform) == MARC_STATUS_INVALID_ARGUMENT);
    assert(transform == NULL);
    assert(marc_lzss_contextual_tans_create(
               &config, primary,
               (marc_buffer){secondary.data, needed.secondary_bytes - 1},
               views, &transform) == MARC_STATUS_INVALID_ARGUMENT);
    assert(transform == NULL);
    assert(marc_lzss_contextual_tans_create(
               &config, primary, secondary,
               (marc_buffer){views.data, needed.views_bytes - 1},
               &transform) == MARC_STATUS_INVALID_ARGUMENT);
    assert(transform == NULL);
    assert(marc_lzss_contextual_tans_create(
               &config, primary, secondary,
               (marc_buffer){primary.data, needed.views_bytes},
               &transform) == MARC_STATUS_INVALID_ARGUMENT);
    assert(transform == NULL);
    if (needed.views_alignment > 1) {
        marc_buffer storage = allocate(needed.views_bytes + 1);
        const marc_buffer misaligned = {
            storage.data + 1, needed.views_bytes};
        assert(marc_lzss_contextual_tans_create(
                   &config, primary, secondary, misaligned, &transform)
               == MARC_STATUS_INVALID_ARGUMENT);
        assert(transform == NULL);
        release(storage);
    }
    assert(marc_lzss_contextual_tans_create(
               &config, primary, secondary, views, NULL)
           == MARC_STATUS_INVALID_ARGUMENT);

    config.reserved = 1;
    assert(marc_lzss_contextual_tans_workspace_requirements(
               &config, &needed) == MARC_STATUS_INVALID_ARGUMENT);
    config.reserved = 0;
    config.reserved2 = 1;
    assert(marc_lzss_contextual_tans_workspace_requirements(
               &config, &needed) == MARC_STATUS_INVALID_ARGUMENT);
    config.reserved2 = 0;
    --config.struct_size;
    assert(marc_lzss_contextual_tans_workspace_requirements(
               &config, &needed) == MARC_STATUS_INVALID_ARGUMENT);
    ++config.struct_size;
    ++config.abi_version;
    assert(marc_lzss_contextual_tans_workspace_requirements(
               &config, &needed) == MARC_STATUS_INVALID_ARGUMENT);
    --config.abi_version;
    assert(marc_lzss_contextual_tans_workspace_requirements(
               NULL, &needed) == MARC_STATUS_INVALID_ARGUMENT);
    assert(marc_lzss_contextual_tans_workspace_requirements(
               &config, NULL) == MARC_STATUS_INVALID_ARGUMENT);
    config.direction = 99;
    assert(marc_lzss_contextual_tans_workspace_requirements(
               &config, &needed) == MARC_STATUS_INVALID_ARGUMENT);
    assert(marc_lzss_contextual_tans_config_init(0, &config)
           == MARC_STATUS_INVALID_ARGUMENT);
    assert(marc_lzss_contextual_tans_config_init(
               MARC_DIRECTION_ENCODE, NULL) == MARC_STATUS_INVALID_ARGUMENT);

    release(primary);
    release(secondary);
    release(views);
    return 0;
}

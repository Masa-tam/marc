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

static void set_small_limits(marc_lzmw_blocked_huffman_config* config) {
    config->max_total_output_size = 1024;
    config->max_frame_size = 2;
    config->max_block_size = 4;
    config->max_compressed_payload_size = 8;
    config->max_dictionary_serialized_size = 8;
    config->max_internal_buffered_bytes = 512;
    config->max_dictionary_entries = 65536;
    config->max_blocks_per_frame = 2;
}

int main(void) {
    static const uint8_t input[] = {0x41, 0x42, 0x41, 0x42, 0x58};
    uint8_t encoded[348];
    uint8_t decoded[sizeof(input)];
    marc_lzmw_blocked_huffman_config config;
    marc_workspace_requirements needed;
    marc_transform* transform = NULL;

    assert(marc_lzmw_blocked_huffman_config_init(
               MARC_DIRECTION_ENCODE, &config)
           == MARC_STATUS_OK);
    assert(config.struct_size == sizeof(config));
    assert(config.entropy_block_size == 65536);
    assert(config.maximum_entries == 65536);
    config.original_size = sizeof(input);
    config.frame_size = 2;
    config.entropy_block_size = 4;
    set_small_limits(&config);
    assert(marc_lzmw_blocked_huffman_workspace_requirements(
               &config, &needed)
           == MARC_STATUS_OK);
    assert(needed.primary_bytes == 2);
    assert(needed.secondary_bytes == 104);
    assert(needed.views_bytes != 0 && needed.views_alignment != 0);

    marc_buffer primary = allocate(needed.primary_bytes);
    marc_buffer secondary = allocate(needed.secondary_bytes);
    marc_buffer views = allocate(needed.views_bytes);
    assert(marc_lzmw_blocked_huffman_create(
               &config, primary, secondary, views, &transform)
           == MARC_STATUS_OK);
    marc_process_result result = marc_transform_process(
        transform, (marc_const_buffer){input, sizeof(input)},
        (marc_buffer){encoded, sizeof(encoded)}, MARC_PROCESS_END_INPUT);
    assert(result.status == MARC_STATUS_END_OF_STREAM);
    assert(result.input_consumed == sizeof(input));
    assert(result.output_produced == sizeof(encoded));
    marc_transform_destroy(transform);
    release(primary);
    release(secondary);
    release(views);

    assert(marc_lzmw_blocked_huffman_config_init(
               MARC_DIRECTION_DECODE, &config)
           == MARC_STATUS_OK);
    set_small_limits(&config);
    assert(marc_lzmw_blocked_huffman_workspace_requirements(
               &config, &needed)
           == MARC_STATUS_OK);
    assert(needed.primary_bytes == 568);
    assert(needed.secondary_bytes == 10);
    assert(needed.views_bytes != 0 && needed.views_alignment != 0);

    primary = allocate(needed.primary_bytes);
    secondary = allocate(needed.secondary_bytes);
    views = allocate(needed.views_bytes);
    assert(marc_lzmw_blocked_huffman_create(
               &config, primary, secondary, views, &transform)
           == MARC_STATUS_OK);
    result = marc_transform_process(
        transform, (marc_const_buffer){encoded, sizeof(encoded)},
        (marc_buffer){decoded, sizeof(decoded)}, MARC_PROCESS_END_INPUT);
    assert(result.status == MARC_STATUS_END_OF_STREAM);
    assert(result.output_produced == sizeof(decoded));
    assert(memcmp(input, decoded, sizeof(input)) == 0);
    marc_transform_destroy(transform);

    assert(marc_lzmw_blocked_huffman_create(
               &config, primary,
               (marc_buffer){secondary.data, needed.secondary_bytes - 1},
               views, &transform)
           == MARC_STATUS_INVALID_ARGUMENT);
    if (needed.views_alignment > 1) {
        marc_buffer misaligned_storage = allocate(needed.views_bytes + 1);
        marc_buffer misaligned = {
            misaligned_storage.data + 1, needed.views_bytes};
        assert(marc_lzmw_blocked_huffman_create(
                   &config, primary, secondary, misaligned, &transform)
               == MARC_STATUS_INVALID_ARGUMENT);
        release(misaligned_storage);
    }
    config.reserved2 = 1;
    assert(marc_lzmw_blocked_huffman_workspace_requirements(
               &config, &needed)
           == MARC_STATUS_INVALID_ARGUMENT);
    release(primary);
    release(secondary);
    release(views);
    return 0;
}

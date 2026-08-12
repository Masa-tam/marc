#include <marc/marc.h>

#include "test_assert.h"
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

int main(void) {
    static const uint8_t input[] = {
        0x41, 0x42, 0x43, 0x44, 0x45, 0x31,
        0x41, 0x42, 0x43, 0x44, 0x45, 0x32};
    uint8_t encoded[8192];
    uint8_t decoded[sizeof(input)];
    marc_lzss_tans_config config;
    marc_workspace_requirements needed;
    marc_transform* transform = NULL;

    assert(marc_lzss_tans_config_init(MARC_DIRECTION_ENCODE, &config)
           == MARC_STATUS_OK);
    assert(config.struct_size == sizeof(config));
    assert(config.entropy_block_size == 65536);
    assert(config.window_size == 65536);
    assert(config.min_match_length == 5);
    config.original_size = sizeof(input);
    config.frame_size = sizeof(input);
    config.entropy_block_size = 16;
    assert(marc_lzss_tans_workspace_requirements(&config, &needed)
           == MARC_STATUS_OK);
    assert(needed.primary_bytes == sizeof(input));
    assert(needed.secondary_bytes > 650);
    assert(needed.views_bytes == 0 && needed.views_alignment == 1);

    marc_buffer primary = allocate(needed.primary_bytes);
    marc_buffer secondary = allocate(needed.secondary_bytes);
    marc_buffer views = allocate(needed.views_bytes);
    marc_buffer short_secondary = secondary;
    --short_secondary.size;
    assert(marc_lzss_tans_create(
               &config, primary, short_secondary, views, &transform)
           == MARC_STATUS_INVALID_ARGUMENT);
    assert(transform == NULL);
    assert(marc_lzss_tans_create(
               &config, primary, secondary, views, &transform)
           == MARC_STATUS_OK);
    marc_const_buffer source = {input, sizeof(input)};
    marc_buffer sink = {encoded, sizeof(encoded)};
    marc_process_result result = marc_transform_process(
        transform, source, sink, MARC_PROCESS_END_INPUT);
    assert(result.status == MARC_STATUS_END_OF_STREAM);
    const size_t encoded_size = result.output_produced;
    assert(encoded_size > 80 && encoded_size <= sizeof(encoded));
    marc_transform_destroy(transform);
    release(primary);
    release(secondary);
    release(views);

    assert(marc_lzss_tans_config_init(MARC_DIRECTION_DECODE, &config)
           == MARC_STATUS_OK);
    config.max_total_output_size = 4096;
    config.max_frame_size = 4096;
    config.max_block_size = 4096;
    config.max_compressed_payload_size = 512;
    config.max_dictionary_serialized_size = 6000;
    config.max_internal_buffered_bytes = 8192;
    config.max_blocks_per_frame = 7;
    config.max_lz_distance = 65536;
    config.max_lz_match_length = 258;
    assert(marc_lzss_tans_workspace_requirements(&config, &needed)
           == MARC_STATUS_OK);
    assert(needed.primary_bytes == 8248);
    assert(needed.secondary_bytes == 10096);
    assert(needed.views_bytes != 0 && needed.views_alignment != 0);

    primary = allocate(needed.primary_bytes);
    secondary = allocate(needed.secondary_bytes);
    views = allocate(needed.views_bytes);
    assert(marc_lzss_tans_create(
               &config, primary, secondary, views, &transform)
           == MARC_STATUS_OK);
    source.data = encoded;
    source.size = encoded_size;
    sink.data = decoded;
    sink.size = sizeof(decoded);
    result = marc_transform_process(
        transform, source, sink, MARC_PROCESS_END_INPUT);
    assert(result.status == MARC_STATUS_END_OF_STREAM);
    assert(result.output_produced == sizeof(decoded));
    assert(memcmp(input, decoded, sizeof(input)) == 0);
    marc_transform_destroy(transform);

    assert(marc_lzss_tans_create(
               &config, primary, secondary,
               (marc_buffer){views.data, needed.views_bytes - 1},
               &transform)
           == MARC_STATUS_INVALID_ARGUMENT);
    assert(transform == NULL);
    config.reserved2 = 1;
    assert(marc_lzss_tans_workspace_requirements(&config, &needed)
           == MARC_STATUS_INVALID_ARGUMENT);
    release(primary);
    release(secondary);
    release(views);
    return 0;
}

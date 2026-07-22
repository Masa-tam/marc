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
        0x41, 0x42, 0x41, 0x42, 0x41, 0x42, 0x58};
    uint8_t encoded[4096];
    uint8_t decoded[sizeof(input)];
    marc_lz77_dynamic_range_config config;
    marc_workspace_requirements needed;
    marc_transform* transform = NULL;

    assert(marc_lz77_dynamic_range_config_init(
               MARC_DIRECTION_ENCODE, &config)
           == MARC_STATUS_OK);
    assert(config.struct_size == sizeof(config));
    assert(config.frame_size == 65536);
    assert(config.window_size == 65536);
    config.original_size = sizeof(input);
    config.frame_size = sizeof(input);
    assert(marc_lz77_dynamic_range_workspace_requirements(
               &config, &needed)
           == MARC_STATUS_OK);
    assert(needed.primary_bytes == sizeof(input));
    assert(needed.secondary_bytes > sizeof(input) * 16);
    assert(needed.views_bytes == 0 && needed.views_alignment == 1);

    marc_buffer primary = allocate(needed.primary_bytes);
    marc_buffer secondary = allocate(needed.secondary_bytes);
    assert(marc_lz77_dynamic_range_create(
               &config, primary, secondary, &transform)
           == MARC_STATUS_OK);
    marc_const_buffer source = {input, sizeof(input)};
    marc_buffer sink = {encoded, sizeof(encoded)};
    marc_process_result result = marc_transform_process(
        transform, source, sink, MARC_PROCESS_END_INPUT);
    assert(result.status == MARC_STATUS_END_OF_STREAM);
    const size_t encoded_size = result.output_produced;
    assert(encoded_size > 64 && encoded_size <= sizeof(encoded));
    marc_transform_destroy(transform);
    release(primary);
    release(secondary);

    assert(marc_lz77_dynamic_range_config_init(
               MARC_DIRECTION_DECODE, &config)
           == MARC_STATUS_OK);
    config.max_total_output_size = 1024;
    config.max_frame_size = sizeof(input);
    config.max_compressed_payload_size = 2048;
    config.max_dictionary_serialized_size = sizeof(input) * 16;
    config.max_internal_buffered_bytes = 4096;
    config.max_lz_distance = 65536;
    config.max_lz_match_length = 258;
    assert(marc_lz77_dynamic_range_workspace_requirements(
               &config, &needed)
           == MARC_STATUS_OK);
    assert(needed.primary_bytes > encoded_size);
    assert(needed.secondary_bytes == sizeof(input) * 17);
    assert(needed.views_bytes == 0 && needed.views_alignment == 1);

    primary = allocate(needed.primary_bytes);
    secondary = allocate(needed.secondary_bytes);
    assert(marc_lz77_dynamic_range_create(
               &config, primary, secondary, &transform)
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

    assert(marc_lz77_dynamic_range_create(
               &config, primary,
               (marc_buffer){secondary.data, needed.secondary_bytes - 1},
               &transform)
           == MARC_STATUS_INVALID_ARGUMENT);
    config.reserved2 = 1;
    assert(marc_lz77_dynamic_range_workspace_requirements(
               &config, &needed)
           == MARC_STATUS_INVALID_ARGUMENT);
    release(primary);
    release(secondary);
    return 0;
}

#include <marc/marc.h>

#include "test_assert.h"
#include <stdlib.h>
#include <string.h>

static marc_buffer allocate(size_t size) {
    marc_buffer result = {size == 0 ? NULL : (uint8_t*)malloc(size), size};
    assert(size == 0 || result.data != NULL);
    return result;
}

int main(void) {
    static const uint8_t input[] = {
        0x41, 0x41, 0x41, 0x41, 0x41, 0x41};
    uint8_t encoded[1024];
    uint8_t decoded[sizeof(input)];
    marc_lzw_config config;
    marc_workspace_requirements needed;
    marc_transform* transform = NULL;

    assert(marc_lzw_config_init(MARC_DIRECTION_ENCODE, &config)
           == MARC_STATUS_OK);
    assert(config.struct_size == sizeof(config));
    assert(config.maximum_code_width == 16);
    config.original_size = sizeof(input);
    config.frame_size = 3;
    assert(marc_lzw_workspace_requirements(&config, &needed)
           == MARC_STATUS_OK);
    assert(needed.primary_bytes == 3 && needed.secondary_bytes == 62);
    assert(needed.views_bytes != 0 && needed.views_alignment != 0);
    marc_buffer primary = allocate(needed.primary_bytes);
    marc_buffer secondary = allocate(needed.secondary_bytes);
    marc_buffer views = allocate(needed.views_bytes);

    if (needed.views_alignment > 1) {
        marc_buffer misaligned_storage = allocate(needed.views_bytes + 1);
        marc_buffer misaligned = {
            misaligned_storage.data + 1, needed.views_bytes};
        assert(marc_lzw_create(
                   &config, primary, secondary, misaligned, &transform)
               == MARC_STATUS_INVALID_ARGUMENT);
        free(misaligned_storage.data);
    }

    assert(marc_lzw_create(
               &config, primary, secondary, views, &transform)
           == MARC_STATUS_OK);
    marc_const_buffer source = {input, sizeof(input)};
    marc_buffer sink = {encoded, sizeof(encoded)};
    marc_process_result result = marc_transform_process(
        transform, source, sink, MARC_PROCESS_END_INPUT);
    assert(result.status == MARC_STATUS_END_OF_STREAM);
    const size_t encoded_size = result.output_produced;
    assert(encoded_size == 198);
    marc_transform_destroy(transform);
    free(primary.data);
    free(secondary.data);
    free(views.data);

    assert(marc_lzw_config_init(MARC_DIRECTION_DECODE, &config)
           == MARC_STATUS_OK);
    config.max_total_output_size = 1024;
    config.max_frame_size = 64;
    config.max_compressed_payload_size = 4096;
    config.max_dictionary_serialized_size = 4096;
    config.max_internal_buffered_bytes = 16384;
    config.max_dictionary_entries = 65536;
    assert(marc_lzw_workspace_requirements(&config, &needed)
           == MARC_STATUS_OK);
    assert(needed.primary_bytes >= 59 && needed.primary_bytes <= 56 + 4096);
    assert(needed.secondary_bytes == 64);
    assert(needed.views_bytes != 0 && needed.views_alignment != 0);
    primary = allocate(needed.primary_bytes);
    secondary = allocate(needed.secondary_bytes);
    views = allocate(needed.views_bytes);
    assert(marc_lzw_create(
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
    free(primary.data);
    free(secondary.data);
    free(views.data);

    config.reserved2 = 1;
    assert(marc_lzw_workspace_requirements(&config, &needed)
           == MARC_STATUS_INVALID_ARGUMENT);
    config.reserved2 = 0;
    config.max_dictionary_entries = 0;
    assert(marc_lzw_workspace_requirements(&config, &needed)
           == MARC_STATUS_INVALID_ARGUMENT);

    assert(marc_lzw_config_init(MARC_DIRECTION_ENCODE, &config)
           == MARC_STATUS_OK);
    config.maximum_code_width = 8;
    assert(marc_lzw_workspace_requirements(&config, &needed)
           == MARC_STATUS_INVALID_ARGUMENT);
    return 0;
}

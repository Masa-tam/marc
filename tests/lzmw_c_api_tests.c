#include <marc/marc.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static marc_buffer allocate(size_t size) {
    marc_buffer result = {size == 0 ? NULL : (uint8_t*)malloc(size), size};
    assert(size == 0 || result.data != NULL);
    return result;
}
int main(void) {
    static const uint8_t input[] = {0x41, 0x42, 0x41, 0x42};
    uint8_t encoded[1024];
    uint8_t decoded[sizeof(input)];
    marc_lzmw_config config;
    marc_workspace_requirements needed;
    marc_transform* transform = NULL;

    assert(marc_lzmw_config_init(MARC_DIRECTION_ENCODE, &config)
           == MARC_STATUS_OK);
    assert(config.struct_size == sizeof(config));
    assert(config.maximum_entries == 65536);
    config.original_size = sizeof(input);
    config.frame_size = 2;
    assert(marc_lzmw_workspace_requirements(&config, &needed)
           == MARC_STATUS_OK);
    assert(needed.primary_bytes == 2 && needed.secondary_bytes == 64);
    assert(needed.views_bytes != 0 && needed.views_alignment != 0);
    marc_buffer primary = allocate(needed.primary_bytes);
    marc_buffer secondary = allocate(needed.secondary_bytes);
    marc_buffer views = allocate(needed.views_bytes);

    if (needed.views_alignment > 1) {
        marc_buffer storage = allocate(needed.views_bytes + 1);
        marc_buffer misaligned = {storage.data + 1, needed.views_bytes};
        assert(marc_lzmw_create(
                   &config, primary, secondary, misaligned, &transform)
               == MARC_STATUS_INVALID_ARGUMENT);
        free(storage.data);
    }

    assert(marc_lzmw_create(&config, primary, secondary, views, &transform)
           == MARC_STATUS_OK);
    marc_process_result result = marc_transform_process(
        transform, (marc_const_buffer){input, sizeof(input)},
        (marc_buffer){encoded, sizeof(encoded)}, MARC_PROCESS_END_INPUT);
    assert(result.status == MARC_STATUS_END_OF_STREAM);
    assert(result.input_consumed == sizeof(input));
    assert(result.output_produced == 208);
    const size_t encoded_size = result.output_produced;
    marc_transform_destroy(transform);
    free(primary.data);
    free(secondary.data);
    free(views.data);

    assert(marc_lzmw_config_init(MARC_DIRECTION_DECODE, &config)
           == MARC_STATUS_OK);
    config.max_total_output_size = 1024;
    config.max_frame_size = 64;
    config.max_compressed_payload_size = 4096;
    config.max_dictionary_serialized_size = 4096;
    config.max_internal_buffered_bytes = 16384;
    config.max_dictionary_entries = 65536;
    assert(marc_lzmw_workspace_requirements(&config, &needed)
           == MARC_STATUS_OK);
    assert(needed.primary_bytes >= 64 && needed.primary_bytes <= 56 + 4096);
    assert(needed.secondary_bytes == 64);
    assert(needed.views_bytes != 0 && needed.views_alignment != 0);
    primary = allocate(needed.primary_bytes);
    secondary = allocate(needed.secondary_bytes);
    views = allocate(needed.views_bytes);
    assert(marc_lzmw_create(&config, primary, secondary, views, &transform)
           == MARC_STATUS_OK);
    result = marc_transform_process(
        transform, (marc_const_buffer){encoded, encoded_size},
        (marc_buffer){decoded, sizeof(decoded)}, MARC_PROCESS_END_INPUT);
    assert(result.status == MARC_STATUS_END_OF_STREAM);
    assert(result.output_produced == sizeof(decoded));
    assert(memcmp(input, decoded, sizeof(input)) == 0);
    marc_transform_destroy(transform);
    free(primary.data);
    free(secondary.data);
    free(views.data);

    config.reserved2 = 1;
    assert(marc_lzmw_workspace_requirements(&config, &needed)
           == MARC_STATUS_INVALID_ARGUMENT);
    config.reserved2 = 0;
    config.max_dictionary_entries = 0;
    assert(marc_lzmw_workspace_requirements(&config, &needed)
           == MARC_STATUS_INVALID_ARGUMENT);

    assert(marc_lzmw_config_init(MARC_DIRECTION_ENCODE, &config)
           == MARC_STATUS_OK);
    config.maximum_entries = 0;
    assert(marc_lzmw_workspace_requirements(&config, &needed)
           == MARC_STATUS_INVALID_ARGUMENT);
    return 0;
}

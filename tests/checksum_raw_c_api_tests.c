#include <marc/marc.h>

#include "test_assert.h"

#include <stdlib.h>
#include <string.h>

static marc_buffer allocate(size_t size) {
    marc_buffer result = {size == 0 ? NULL : (uint8_t*)malloc(size), size};
    assert(size == 0 || result.data != NULL);
    return result;
}

static size_t run_one_byte_chunks(
    marc_transform* transform,
    const uint8_t* input,
    size_t input_size,
    uint8_t* output,
    size_t output_capacity) {
    size_t input_offset = 0;
    size_t output_offset = 0;
    marc_status status = MARC_STATUS_PROGRESS;
    size_t calls = 0;
    while (status != MARC_STATUS_END_OF_STREAM) {
        const size_t input_count = input_offset < input_size ? 1 : 0;
        const marc_process_flags flags =
            input_offset + input_count == input_size
                ? MARC_PROCESS_END_INPUT : MARC_PROCESS_NONE;
        const marc_const_buffer source = {
            input_count == 0 ? NULL : input + input_offset, input_count};
        const marc_buffer sink = {
            output_offset == output_capacity ? NULL : output + output_offset,
            output_offset == output_capacity ? 0 : 1};
        const marc_process_result result = marc_transform_process(
            transform, source, sink, flags);
        assert(result.input_consumed <= source.size);
        assert(result.output_produced <= sink.size);
        assert(result.status == MARC_STATUS_PROGRESS
            || result.status == MARC_STATUS_NEED_INPUT
            || result.status == MARC_STATUS_NEED_OUTPUT
            || result.status == MARC_STATUS_END_OF_STREAM);
        assert(result.status != MARC_STATUS_PROGRESS
            || result.input_consumed != 0 || result.output_produced != 0);
        input_offset += result.input_consumed;
        output_offset += result.output_produced;
        status = result.status;
        assert(output_offset <= output_capacity);
        assert(++calls < 10000);
    }
    assert(input_offset == input_size);
    return output_offset;
}

int main(void) {
    static const uint8_t input[] = {
        0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47};
    uint8_t encoded[207];
    uint8_t encoded_chunked[207];
    uint8_t decoded[sizeof(input)];
    marc_checksum_raw_config config;
    marc_workspace_requirements needed;
    marc_transform* transform = NULL;

    assert(marc_checksum_raw_config_init(
        MARC_DIRECTION_ENCODE, &config) == MARC_STATUS_OK);
    config.original_size = sizeof(input);
    config.frame_size = 4;
    assert(marc_checksum_raw_workspace_requirements(
        &config, &needed) == MARC_STATUS_OK);
    assert(needed.primary_bytes == 64);
    assert(needed.secondary_bytes == 0);
    assert(needed.views_bytes == 0);
    assert(needed.views_alignment == 1);
    marc_buffer workspace = allocate(needed.primary_bytes);
    assert(marc_checksum_raw_create(
        &config, workspace, &transform) == MARC_STATUS_OK);
    marc_process_result result = marc_transform_process(
        transform,
        (marc_const_buffer){input, sizeof(input)},
        (marc_buffer){encoded, sizeof(encoded)},
        MARC_PROCESS_END_INPUT);
    assert(result.status == MARC_STATUS_END_OF_STREAM);
    assert(result.input_consumed == sizeof(input));
    assert(result.output_produced == sizeof(encoded));
    assert(encoded[6] == 1);  /* format minor version 1 */
    assert(encoded[64] == 1); /* CRC-32C algorithm ID */
    marc_transform_destroy(transform);

    transform = NULL;
    assert(marc_checksum_raw_create(
        &config, workspace, &transform) == MARC_STATUS_OK);
    const size_t chunked_size = run_one_byte_chunks(
        transform, input, sizeof(input), encoded_chunked,
        sizeof(encoded_chunked));
    assert(chunked_size == sizeof(encoded));
    assert(memcmp(encoded, encoded_chunked, sizeof(encoded)) == 0);
    marc_transform_destroy(transform);
    free(workspace.data);

    assert(marc_checksum_raw_config_init(
        MARC_DIRECTION_DECODE, &config) == MARC_STATUS_OK);
    config.max_total_output_size = 1024;
    config.max_frame_size = 64;
    config.max_compressed_payload_size = 64;
    config.max_dictionary_serialized_size = 64;
    config.max_internal_buffered_bytes = 124;
    assert(marc_checksum_raw_workspace_requirements(
        &config, &needed) == MARC_STATUS_OK);
    assert(needed.primary_bytes == 124);
    workspace = allocate(needed.primary_bytes);
    transform = NULL;
    assert(marc_checksum_raw_create(
        &config, workspace, &transform) == MARC_STATUS_OK);
    result = marc_transform_process(
        transform,
        (marc_const_buffer){encoded, sizeof(encoded)},
        (marc_buffer){decoded, sizeof(decoded)},
        MARC_PROCESS_END_INPUT);
    assert(result.status == MARC_STATUS_END_OF_STREAM);
    assert(result.input_consumed == sizeof(encoded));
    assert(result.output_produced == sizeof(decoded));
    assert(memcmp(decoded, input, sizeof(input)) == 0);
    marc_transform_destroy(transform);

    transform = NULL;
    assert(marc_checksum_raw_create(
        &config, workspace, &transform) == MARC_STATUS_OK);
    memset(decoded, 0, sizeof(decoded));
    const size_t decoded_chunked_size = run_one_byte_chunks(
        transform, encoded, sizeof(encoded), decoded, sizeof(decoded));
    assert(decoded_chunked_size == sizeof(decoded));
    assert(memcmp(decoded, input, sizeof(input)) == 0);
    marc_transform_destroy(transform);

    transform = NULL;
    assert(marc_checksum_raw_create(
        &config, workspace, &transform) == MARC_STATUS_OK);
    memset(decoded, 0x5a, sizeof(decoded));
    encoded[sizeof(encoded) - 1] ^= 1;
    result = marc_transform_process(
        transform,
        (marc_const_buffer){encoded, sizeof(encoded)},
        (marc_buffer){decoded, sizeof(decoded)},
        MARC_PROCESS_END_INPUT);
    assert(result.status == MARC_STATUS_MALFORMED_STREAM);
    assert(result.output_produced == 4);
    assert(memcmp(decoded, input, 4) == 0);
    assert(decoded[4] == 0x5a && decoded[5] == 0x5a
        && decoded[6] == 0x5a);
    marc_transform_destroy(transform);

    marc_checksum_raw_config empty_config;
    assert(marc_checksum_raw_config_init(
        MARC_DIRECTION_ENCODE, &empty_config) == MARC_STATUS_OK);
    assert(marc_checksum_raw_workspace_requirements(
        &empty_config, &needed) == MARC_STATUS_OK);
    assert(needed.primary_bytes == 0);
    const marc_buffer no_workspace = {NULL, 0};
    transform = NULL;
    assert(marc_checksum_raw_create(
        &empty_config, no_workspace, &transform) == MARC_STATUS_OK);
    uint8_t empty_encoded[80];
    result = marc_transform_process(
        transform, (marc_const_buffer){NULL, 0},
        (marc_buffer){empty_encoded, sizeof(empty_encoded)},
        MARC_PROCESS_END_INPUT);
    assert(result.status == MARC_STATUS_END_OF_STREAM);
    assert(result.output_produced == sizeof(empty_encoded));
    marc_transform_destroy(transform);

    marc_checksum_raw_config invalid = config;
    invalid.reserved3 = 1;
    assert(marc_checksum_raw_workspace_requirements(
        &invalid, &needed) == MARC_STATUS_INVALID_ARGUMENT);
    invalid = config;
    ++invalid.abi_version;
    assert(marc_checksum_raw_workspace_requirements(
        &invalid, &needed) == MARC_STATUS_INVALID_ARGUMENT);
    marc_buffer short_workspace = workspace;
    --short_workspace.size;
    transform = (marc_transform*)1;
    assert(marc_checksum_raw_create(
        &config, short_workspace, &transform)
        == MARC_STATUS_INVALID_ARGUMENT);
    assert(transform == NULL);
    assert(marc_checksum_raw_config_init(
        99, &invalid) == MARC_STATUS_INVALID_ARGUMENT);
    assert(marc_checksum_raw_config_init(
        MARC_DIRECTION_ENCODE, NULL) == MARC_STATUS_INVALID_ARGUMENT);

    free(workspace.data);
    return 0;
}

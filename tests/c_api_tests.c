#include "marc/marc.h"

#include "test_assert.h"
#include <stdlib.h>
#include <string.h>

static marc_buffer allocate_buffer(size_t size) {
    marc_buffer buffer;
    buffer.data = size == 0 ? NULL : (uint8_t*)malloc(size);
    buffer.size = size;
    assert(size == 0 || buffer.data != NULL);
    return buffer;
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
        const size_t input_chunk = input_offset < input_size ? 1 : 0;
        const marc_process_flags flags =
            input_offset + input_chunk == input_size
                ? MARC_PROCESS_END_INPUT
                : MARC_PROCESS_NONE;
        marc_const_buffer source = {
            input_chunk == 0 ? NULL : input + input_offset, input_chunk};
        marc_buffer sink = {
            output_capacity == output_offset ? NULL : output + output_offset,
            output_capacity == output_offset ? 0 : 1};
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
        assert(++calls < 100000);
    }
    assert(input_offset == input_size);
    return output_offset;
}

int main(void) {
    marc_process_result result = {0};
    result.status = MARC_STATUS_NEED_INPUT;

    assert(sizeof(marc_status) == sizeof(uint32_t));
    assert(sizeof(marc_direction) == sizeof(uint32_t));
    assert(sizeof(marc_process_flags) == sizeof(uint32_t));
    assert(result.status == MARC_STATUS_NEED_INPUT);
    assert(marc_abi_version() == MARC_ABI_VERSION);
    assert(strcmp(marc_status_name(result.status), "need_input") == 0);

    marc_blocked_huffman_config encoder_config;
    assert(marc_blocked_huffman_config_init(
        MARC_DIRECTION_ENCODE, &encoder_config) == MARC_STATUS_OK);
    encoder_config.original_size = 200;
    encoder_config.frame_size = 64;
    encoder_config.block_size = 32;
    marc_workspace_requirements encoder_requirements;
    assert(marc_blocked_huffman_workspace_requirements(
        &encoder_config, &encoder_requirements) == MARC_STATUS_OK);
    assert(encoder_requirements.primary_bytes == 64);
    assert(encoder_requirements.views_bytes == 0);

    marc_buffer encoder_primary =
        allocate_buffer(encoder_requirements.primary_bytes);
    marc_buffer encoder_secondary =
        allocate_buffer(encoder_requirements.secondary_bytes);
    marc_buffer no_views = {NULL, 0};
    marc_transform* encoder = NULL;
    assert(marc_blocked_huffman_create(
        &encoder_config, encoder_primary, encoder_secondary, no_views,
        &encoder) == MARC_STATUS_OK);

    uint8_t input[200];
    uint8_t encoded[2048];
    memset(input, 0x5a, sizeof(input));
    marc_const_buffer encoder_input = {input, sizeof(input)};
    marc_buffer encoder_output = {encoded, sizeof(encoded)};
    result = marc_transform_process(
        encoder, encoder_input, encoder_output, MARC_PROCESS_END_INPUT);
    assert(result.status == MARC_STATUS_END_OF_STREAM);
    assert(result.input_consumed == sizeof(input));
    const size_t encoded_size = result.output_produced;
    assert(encoded_size != 0);
    marc_transform_destroy(encoder);

    marc_blocked_huffman_config decoder_config;
    assert(marc_blocked_huffman_config_init(
        MARC_DIRECTION_DECODE, &decoder_config) == MARC_STATUS_OK);
    decoder_config.max_total_output_size = 1024;
    decoder_config.max_frame_size = 1024;
    decoder_config.max_block_size = 1024;
    decoder_config.max_compressed_payload_size = 4096;
    decoder_config.max_internal_buffered_bytes = 8192;
    decoder_config.max_blocks_per_frame = 64;
    marc_workspace_requirements decoder_requirements;
    assert(marc_blocked_huffman_workspace_requirements(
        &decoder_config, &decoder_requirements) == MARC_STATUS_OK);
    assert(decoder_requirements.views_bytes != 0);

    marc_buffer decoder_primary =
        allocate_buffer(decoder_requirements.primary_bytes);
    marc_buffer decoder_secondary =
        allocate_buffer(decoder_requirements.secondary_bytes);
    marc_buffer decoder_views =
        allocate_buffer(decoder_requirements.views_bytes);
    marc_transform* decoder = NULL;
    assert(marc_blocked_huffman_create(
        &decoder_config, decoder_primary, decoder_secondary, decoder_views,
        &decoder) == MARC_STATUS_OK);

    uint8_t decoded[200];
    marc_const_buffer decoder_input = {encoded, encoded_size};
    marc_buffer decoder_output = {decoded, sizeof(decoded)};
    result = marc_transform_process(
        decoder, decoder_input, decoder_output, MARC_PROCESS_END_INPUT);
    assert(result.status == MARC_STATUS_END_OF_STREAM);
    assert(result.input_consumed == encoded_size);
    assert(result.output_produced == sizeof(decoded));
    assert(memcmp(decoded, input, sizeof(input)) == 0);
    marc_transform_destroy(decoder);

    /* Arbitrary ABI chunking must not change the encoded representation. */
    encoder = NULL;
    assert(marc_blocked_huffman_create(
        &encoder_config, encoder_primary, encoder_secondary, no_views,
        &encoder) == MARC_STATUS_OK);
    uint8_t encoded_chunked[2048];
    const size_t encoded_chunked_size = run_one_byte_chunks(
        encoder, input, sizeof(input), encoded_chunked,
        sizeof(encoded_chunked));
    assert(encoded_chunked_size == encoded_size);
    assert(memcmp(encoded_chunked, encoded, encoded_size) == 0);
    marc_const_buffer empty_input = {NULL, 0};
    marc_buffer empty_output = {NULL, 0};
    result = marc_transform_process(
        encoder, empty_input, empty_output, MARC_PROCESS_END_INPUT);
    assert(result.status == MARC_STATUS_END_OF_STREAM);
    marc_transform_destroy(encoder);

    decoder = NULL;
    assert(marc_blocked_huffman_create(
        &decoder_config, decoder_primary, decoder_secondary, decoder_views,
        &decoder) == MARC_STATUS_OK);
    uint8_t decoded_chunked[200];
    const size_t decoded_chunked_size = run_one_byte_chunks(
        decoder, encoded_chunked, encoded_chunked_size, decoded_chunked,
        sizeof(decoded_chunked));
    assert(decoded_chunked_size == sizeof(input));
    assert(memcmp(decoded_chunked, input, sizeof(input)) == 0);
    result = marc_transform_process(
        decoder, empty_input, empty_output, MARC_PROCESS_END_INPUT);
    assert(result.status == MARC_STATUS_END_OF_STREAM);
    marc_transform_destroy(decoder);

    /* Configuration tags, reserved fields, and capacities are strict. */
    marc_blocked_huffman_config invalid_config = encoder_config;
    invalid_config.abi_version += 1;
    assert(marc_blocked_huffman_workspace_requirements(
        &invalid_config, &encoder_requirements)
        == MARC_STATUS_INVALID_ARGUMENT);
    invalid_config = encoder_config;
    invalid_config.reserved = 1;
    assert(marc_blocked_huffman_workspace_requirements(
        &invalid_config, &encoder_requirements)
        == MARC_STATUS_INVALID_ARGUMENT);
    marc_buffer short_primary = encoder_primary;
    short_primary.size -= 1;
    encoder = (marc_transform*)1;
    assert(marc_blocked_huffman_create(
        &encoder_config, short_primary, encoder_secondary, no_views,
        &encoder) == MARC_STATUS_INVALID_ARGUMENT);
    assert(encoder == NULL);
    assert(marc_blocked_huffman_config_init(
        99, &invalid_config) == MARC_STATUS_INVALID_ARGUMENT);
    assert(marc_blocked_huffman_config_init(
        MARC_DIRECTION_ENCODE, NULL) == MARC_STATUS_INVALID_ARGUMENT);
    marc_transform_destroy(NULL);

    /* A malformed stream reports the stable public category and no output. */
    uint8_t malformed[2048];
    memcpy(malformed, encoded, encoded_size);
    malformed[0] ^= 1;
    decoder = NULL;
    assert(marc_blocked_huffman_create(
        &decoder_config, decoder_primary, decoder_secondary, decoder_views,
        &decoder) == MARC_STATUS_OK);
    decoder_input.data = malformed;
    decoder_input.size = encoded_size;
    result = marc_transform_process(
        decoder, decoder_input, decoder_output, MARC_PROCESS_END_INPUT);
    assert(result.status == MARC_STATUS_MALFORMED_STREAM);
    assert(result.output_produced == 0);
    marc_transform_destroy(decoder);

    result = marc_transform_process(
        NULL, empty_input, empty_output, MARC_PROCESS_NONE);
    assert(result.status == MARC_STATUS_INVALID_ARGUMENT);

    free(encoder_primary.data);
    free(encoder_secondary.data);
    free(decoder_primary.data);
    free(decoder_secondary.data);
    free(decoder_views.data);
    return 0;
}

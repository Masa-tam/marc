#include "marc/marc.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static marc_buffer allocate_buffer(size_t size) {
    marc_buffer buffer;
    buffer.data = size == 0 ? NULL : (uint8_t*)malloc(size);
    buffer.size = size;
    assert(size == 0 || buffer.data != NULL);
    return buffer;
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

    free(encoder_primary.data);
    free(encoder_secondary.data);
    free(decoder_primary.data);
    free(decoder_secondary.data);
    free(decoder_views.data);
    return 0;
}

#include "marc/marc.h"
#include "test_assert.h"
#include <stdlib.h>
#include <string.h>

static marc_buffer allocate(size_t size) {
    marc_buffer b = {(uint8_t*)malloc(size), size};
    assert(b.data != NULL);
    return b;
}

static size_t run(marc_direction direction, const uint8_t* input, size_t size,
                  uint8_t* output, size_t capacity, size_t chunk) {
    marc_lzss_position_distance_dynamic_range_config c;
    marc_workspace_requirements r;
    marc_transform* t = NULL;
    marc_buffer p, s, v;
    size_t consumed = 0, produced = 0, calls = 0;
    marc_status status = MARC_STATUS_PROGRESS;
    assert(marc_lzss_position_distance_dynamic_range_config_init(direction, &c) == MARC_STATUS_OK);
    c.original_size = direction == MARC_DIRECTION_ENCODE ? size : UINT64_MAX;
    c.frame_size = direction == MARC_DIRECTION_ENCODE ? 33 : 0;
    c.max_frame_size = 33; c.max_block_size = 33;
    c.max_compressed_payload_size = 18 * 33 + 5;
    assert(marc_lzss_position_distance_dynamic_range_workspace_requirements(&c, &r) == MARC_STATUS_OK);
    assert(r.struct_size == sizeof(r) && r.abi_version == MARC_ABI_VERSION);
    p = allocate(r.primary_bytes); s = allocate(r.secondary_bytes); v = allocate(r.views_bytes);
    assert((uintptr_t)v.data % r.views_alignment == 0);
    assert(marc_lzss_position_distance_dynamic_range_create(&c, p, s, v, &t) == MARC_STATUS_OK);
    assert(t != NULL);
    while (status != MARC_STATUS_END_OF_STREAM) {
        size_t n = size - consumed < chunk ? size - consumed : chunk;
        size_t m = capacity - produced < chunk ? capacity - produced : chunk;
        marc_const_buffer source = {n ? input + consumed : NULL, n};
        marc_buffer sink = {m ? output + produced : NULL, m};
        marc_process_flags flags = consumed + n == size ? MARC_PROCESS_END_INPUT : MARC_PROCESS_NONE;
        marc_process_result result;
        if (calls % 2) flags |= MARC_PROCESS_FLUSH;
        result = marc_transform_process(t, source, sink, flags);
        assert(result.input_consumed <= n && result.output_produced <= m);
        assert(result.status >= MARC_STATUS_PROGRESS && result.status <= MARC_STATUS_END_OF_STREAM);
        assert(result.status != MARC_STATUS_PROGRESS || result.input_consumed || result.output_produced);
        consumed += result.input_consumed; produced += result.output_produced;
        status = result.status;
        assert(++calls < 100000);
    }
    assert(consumed == size);
    {
        marc_const_buffer empty_input = {NULL, 0};
        marc_buffer empty_output = {NULL, 0};
        marc_process_result ended = marc_transform_process(t, empty_input, empty_output, MARC_PROCESS_END_INPUT);
        assert(ended.status == MARC_STATUS_END_OF_STREAM);
        assert(ended.input_consumed == 0 && ended.output_produced == 0);
    }
    marc_transform_destroy(t);
    free(v.data); free(s.data); free(p.data);
    return produced;
}

int main(void) {
    uint8_t input[800], encoded[20000], alternate[20000], decoded[800];
    const size_t sizes[] = {0, 1, 32, 33, 34, 256, 800};
    size_t i, j;
    assert(marc_abi_version() == MARC_ABI_VERSION);
    for (i = 0; i < sizeof(input); ++i) input[i] = (uint8_t)(i < 256 ? i : i % 7);
    for (j = 0; j < sizeof(sizes)/sizeof(sizes[0]); ++j) {
        size_t n = run(MARC_DIRECTION_ENCODE, input, sizes[j], encoded, sizeof(encoded), 1);
        size_t m = run(MARC_DIRECTION_ENCODE, input, sizes[j], alternate, sizeof(alternate), 17);
        assert(n == m && memcmp(encoded, alternate, n) == 0);
        memset(decoded, 0xcd, sizeof(decoded));
        assert(run(MARC_DIRECTION_DECODE, encoded, n, decoded, sizeof(decoded), 1) == sizes[j]);
        assert(memcmp(input, decoded, sizes[j]) == 0);
        for (i = sizes[j]; i < sizeof(decoded); ++i) assert(decoded[i] == 0xcd);
        assert(run(MARC_DIRECTION_DECODE, encoded, n, decoded, sizeof(decoded), 17) == sizes[j]);
        assert(memcmp(input, decoded, sizes[j]) == 0);
    }
    marc_transform_destroy(NULL);
    return 0;
}

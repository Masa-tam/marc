#include <marc/marc.h>
#include <string.h>
int main(void) {
    marc_lzss_position_distance_blocked_huffman_config config;
    marc_lzss_position_distance_blocked_huffman_resources resources;
    marc_transform* transform = NULL;
    unsigned char wire[112];
    size_t produced = 0;
    if (marc_lzss_position_distance_blocked_huffman_config_init(MARC_DIRECTION_ENCODE, &config) != MARC_STATUS_OK) return 1;
    config.frame_size = 1;
    config.input_capacity_bytes = config.output_capacity_bytes = 1;
    if (marc_lzss_position_distance_blocked_huffman_resource_requirements(&config, &resources) != MARC_STATUS_OK) return 2;
    if (resources.minimum_aggregate_bytes > config.max_internal_buffered_bytes) return 3;
    if (marc_lzss_position_distance_blocked_huffman_create(&config, &transform) != MARC_STATUS_OK) return 4;
    while (produced < sizeof(wire)) {
        unsigned char guarded[3] = {0xa5, 0xa5, 0xa5};
        marc_const_buffer input = {NULL, 0};
        marc_buffer output = {guarded + 1, 1};
        marc_process_result result = marc_transform_process(transform, input, output, MARC_PROCESS_END_INPUT);
        if (result.input_consumed || result.output_produced != 1 || guarded[0] != 0xa5 || guarded[2] != 0xa5) return 5;
        wire[produced++] = guarded[1];
        if (result.status == MARC_STATUS_END_OF_STREAM && produced != sizeof(wire)) return 6;
        if (result.status >= 100) return 7;
    }
    marc_transform_destroy(transform);
    if (memcmp(wire, "MARC", 4) || wire[12] != 2 || wire[14] != 8 || wire[16] != 2 || wire[18] != 3 || wire[96] != 1 || wire[98] != 28) return 8;
    {
        const unsigned char raw[3] = {0, 255, 65};
        unsigned char encoded[1024], restored[3];
        size_t used = 0, count = 0, read = 0, written = 0;
        size_t calls;
        if (marc_lzss_position_distance_blocked_huffman_config_init(MARC_DIRECTION_ENCODE, &config)) return 9;
        config.frame_size = 1; config.original_size = sizeof(raw);
        config.input_capacity_bytes = config.output_capacity_bytes = 1;
        if (marc_lzss_position_distance_blocked_huffman_create(&config, &transform)) return 10;
        for (calls = 0; calls < 2048; ++calls) {
            const size_t size = used < sizeof(raw) ? 1 : 0;
            marc_const_buffer input = {raw + used, size};
            marc_buffer output = {encoded + count, 1};
            marc_process_result result = marc_transform_process(transform, input, output,
                used + size == sizeof(raw) ? MARC_PROCESS_END_INPUT : 0);
            if (result.status >= 100 || result.input_consumed > size || result.output_produced > 1) return 11;
            used += result.input_consumed; count += result.output_produced;
            if (result.status == MARC_STATUS_END_OF_STREAM) break;
            if (count == sizeof(encoded)) return 12;
        }
        marc_transform_destroy(transform);
        if (used != sizeof(raw) || calls == 2048) return 13;
        if (marc_lzss_position_distance_blocked_huffman_config_init(MARC_DIRECTION_DECODE, &config)) return 14;
        config.frame_size = 1; config.input_capacity_bytes = config.output_capacity_bytes = 1;
        if (marc_lzss_position_distance_blocked_huffman_create(&config, &transform)) return 15;
        for (calls = 0; calls < 2048; ++calls) {
            unsigned char guarded[3] = {0xa5, 0xa5, 0xa5};
            const size_t size = read < count ? 1 : 0;
            marc_const_buffer input = {encoded + read, size};
            marc_buffer output = {guarded + 1, 1};
            marc_process_result result = marc_transform_process(transform, input, output,
                read + size == count ? MARC_PROCESS_END_INPUT : 0);
            if (result.status >= 100 || result.input_consumed > size || result.output_produced > 1
                || guarded[0] != 0xa5 || guarded[2] != 0xa5) return 16;
            read += result.input_consumed;
            if (result.output_produced) {
                if (written >= sizeof(restored)) return 17;
                restored[written++] = guarded[1];
            }
            if (result.status == MARC_STATUS_END_OF_STREAM) break;
        }
        marc_transform_destroy(transform);
        if (read != count || written != sizeof(raw) || calls == 2048 || memcmp(raw, restored, sizeof(raw))) return 18;
    }
    return 0;
}

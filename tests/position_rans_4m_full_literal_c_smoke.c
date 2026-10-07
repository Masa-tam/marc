#include <marc/marc.h>
#include <string.h>
int main(void) {
    marc_lzss_position_distance_rans_4m_config config;
    marc_lzss_position_distance_rans_4m_resources resources;
    marc_transform* transform = NULL;
    unsigned char wire[112];
    size_t produced = 0;
    if (marc_lzss_position_distance_rans_4m_config_init(MARC_DIRECTION_ENCODE, &config) != MARC_STATUS_OK) return 1;
    config.frame_size = 1;
    config.input_capacity_bytes = config.output_capacity_bytes = 1;
    if (marc_lzss_position_distance_rans_4m_resource_requirements(&config, &resources) != MARC_STATUS_OK) return 2;
    if (resources.minimum_aggregate_bytes > config.max_internal_buffered_bytes) return 3;
    if (marc_lzss_position_distance_rans_4m_create(&config, &transform) != MARC_STATUS_OK) return 4;
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
    if (memcmp(wire, "MARC", 4) || wire[12] != 2 || wire[14] != 10 || wire[16] != 4 || wire[18] != 7) return 8;
    return 0;
}

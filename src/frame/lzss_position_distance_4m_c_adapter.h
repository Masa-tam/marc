#ifndef MARC_PRIVATE_POSITION_DISTANCE_4M_C_ADAPTER_H
#define MARC_PRIVATE_POSITION_DISTANCE_4M_C_ADAPTER_H
#include "marc/marc.h"
/* Private prototype: this header is neither installed nor exported.
 * Workspaces are borrowed until destroy; creation charges their full capacities.
 * Query failures preserve the result. A disjoint creation output is null on
 * failure; aliased metadata outputs are rejected without writing through them.
 * Use marc_transform_process/destroy with the returned opaque handle. */
#ifdef __cplusplus
#define MARC_PRIVATE_4M_NOEXCEPT noexcept
extern "C" {
#else
#define MARC_PRIVATE_4M_NOEXCEPT
#endif
typedef struct marc_private_position_distance_4m_config {
    uint32_t struct_size;
    uint32_t abi_version;
    marc_direction direction;
    uint32_t reserved;
    uint64_t original_size;
    uint32_t frame_size;
    uint32_t reserved2;
    uint64_t max_total_output_size;
    uint64_t max_frame_size;
    uint64_t max_block_size;
    uint64_t max_compressed_payload_size;
    uint64_t max_internal_buffered_bytes;
    uint64_t max_lz_distance;
    uint64_t max_lz_match_length;
    uint64_t max_entropy_table_entries;
    uint64_t max_range_model_total;
    uint64_t max_expansion_ratio;
    uint64_t expansion_slack;
} marc_private_position_distance_4m_config;
marc_status marc_private_position_distance_4m_config_init(marc_direction,marc_private_position_distance_4m_config*) MARC_PRIVATE_4M_NOEXCEPT;
marc_status marc_private_position_distance_4m_workspace_requirements(const marc_private_position_distance_4m_config*,marc_workspace_requirements*) MARC_PRIVATE_4M_NOEXCEPT;
marc_status marc_private_position_distance_4m_create(const marc_private_position_distance_4m_config*,marc_buffer,marc_buffer,marc_buffer,marc_transform**) MARC_PRIVATE_4M_NOEXCEPT;
#ifdef __cplusplus
}
#endif
#undef MARC_PRIVATE_4M_NOEXCEPT
#endif

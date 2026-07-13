#ifndef MARC_MARC_H
#define MARC_MARC_H

#include <stddef.h>
#include <stdint.h>

#include "marc/export.h"

#ifdef __cplusplus
#define MARC_NOEXCEPT noexcept
extern "C" {
#else
#define MARC_NOEXCEPT
#endif

#define MARC_ABI_VERSION UINT32_C(1)

typedef uint32_t marc_status;
#define MARC_STATUS_OK UINT32_C(0)
#define MARC_STATUS_PROGRESS UINT32_C(1)
#define MARC_STATUS_NEED_INPUT UINT32_C(2)
#define MARC_STATUS_NEED_OUTPUT UINT32_C(3)
#define MARC_STATUS_END_OF_STREAM UINT32_C(4)
#define MARC_STATUS_INVALID_ARGUMENT UINT32_C(100)
#define MARC_STATUS_UNSUPPORTED UINT32_C(101)
#define MARC_STATUS_LIMIT_EXCEEDED UINT32_C(102)
#define MARC_STATUS_OUT_OF_MEMORY UINT32_C(103)
#define MARC_STATUS_MALFORMED_STREAM UINT32_C(104)
#define MARC_STATUS_INTERNAL_ERROR UINT32_C(105)

typedef uint32_t marc_direction;
#define MARC_DIRECTION_ENCODE UINT32_C(1)
#define MARC_DIRECTION_DECODE UINT32_C(2)

typedef uint32_t marc_process_flags;
#define MARC_PROCESS_NONE UINT32_C(0)
#define MARC_PROCESS_FLUSH (UINT32_C(1) << 0)
#define MARC_PROCESS_END_INPUT (UINT32_C(1) << 1)
#define MARC_PROCESS_RESET_BLOCK (UINT32_C(1) << 2)

typedef struct marc_buffer {
    uint8_t* data;
    size_t size;
} marc_buffer;

typedef struct marc_const_buffer {
    const uint8_t* data;
    size_t size;
} marc_const_buffer;

typedef struct marc_process_result {
    size_t input_consumed;
    size_t output_produced;
    marc_status status;
    uint64_t error_byte_position;
    uint8_t error_bit_position;
    uint8_t reserved[7];
} marc_process_result;

typedef struct marc_transform marc_transform;

typedef struct marc_blocked_huffman_config {
    uint32_t struct_size;
    uint32_t abi_version;
    marc_direction direction;
    uint32_t reserved;
    uint64_t original_size;
    uint32_t frame_size;
    uint32_t block_size;
    uint64_t max_total_output_size;
    uint64_t max_frame_size;
    uint64_t max_block_size;
    uint64_t max_compressed_payload_size;
    uint64_t max_internal_buffered_bytes;
    uint32_t max_blocks_per_frame;
    uint32_t reserved2;
} marc_blocked_huffman_config;

typedef struct marc_adaptive_huffman_config {
    uint32_t struct_size;
    uint32_t abi_version;
    marc_direction direction;
    uint32_t reserved;
    uint64_t original_size;
    uint32_t frame_size;
    uint32_t reserved2;
    uint64_t max_total_output_size;
    uint64_t max_frame_size;
    uint64_t max_compressed_payload_size;
    uint64_t max_internal_buffered_bytes;
} marc_adaptive_huffman_config;

typedef struct marc_dynamic_range_config {
    uint32_t struct_size;
    uint32_t abi_version;
    marc_direction direction;
    uint32_t reserved;
    uint64_t original_size;
    uint32_t frame_size;
    uint32_t reserved2;
    uint64_t max_total_output_size;
    uint64_t max_frame_size;
    uint64_t max_compressed_payload_size;
    uint64_t max_internal_buffered_bytes;
    uint64_t max_range_model_total;
} marc_dynamic_range_config;

typedef struct marc_rans_config {
    uint32_t struct_size;
    uint32_t abi_version;
    marc_direction direction;
    uint32_t reserved;
    uint64_t original_size;
    uint32_t frame_size;
    uint32_t block_size;
    uint64_t max_total_output_size;
    uint64_t max_frame_size;
    uint64_t max_block_size;
    uint64_t max_compressed_payload_size;
    uint64_t max_internal_buffered_bytes;
    uint32_t max_blocks_per_frame;
    uint32_t reserved2;
} marc_rans_config;

typedef struct marc_tans_config {
    uint32_t struct_size;
    uint32_t abi_version;
    marc_direction direction;
    uint32_t reserved;
    uint64_t original_size;
    uint32_t frame_size;
    uint32_t block_size;
    uint64_t max_total_output_size;
    uint64_t max_frame_size;
    uint64_t max_block_size;
    uint64_t max_compressed_payload_size;
    uint64_t max_internal_buffered_bytes;
    uint32_t max_blocks_per_frame;
    uint32_t reserved2;
} marc_tans_config;

typedef struct marc_workspace_requirements {
    uint32_t struct_size;
    uint32_t abi_version;
    size_t primary_bytes;
    size_t secondary_bytes;
    size_t views_bytes;
    size_t views_alignment;
} marc_workspace_requirements;

/* ABI-safe library metadata. These functions never throw across the C ABI. */
MARC_API uint32_t marc_abi_version(void) MARC_NOEXCEPT;
MARC_API const char* marc_version_string(void) MARC_NOEXCEPT;
MARC_API const char* marc_status_name(marc_status status) MARC_NOEXCEPT;

MARC_API marc_status marc_blocked_huffman_config_init(
    marc_direction direction, marc_blocked_huffman_config* config)
    MARC_NOEXCEPT;
MARC_API marc_status marc_blocked_huffman_workspace_requirements(
    const marc_blocked_huffman_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;
/*
 * primary/secondary meanings follow direction: encoder input/encoded-frame,
 * decoder encoded-frame/decoded-frame. views_workspace is decoder-only and
 * its address must satisfy views_alignment. All workspaces remain caller-owned
 * and must outlive the transform.
 */
MARC_API marc_status marc_blocked_huffman_create(
    const marc_blocked_huffman_config* config,
    marc_buffer primary_workspace,
    marc_buffer secondary_workspace,
    marc_buffer views_workspace,
    marc_transform** transform) MARC_NOEXCEPT;
MARC_API marc_status marc_adaptive_huffman_config_init(
    marc_direction direction, marc_adaptive_huffman_config* config)
    MARC_NOEXCEPT;
MARC_API marc_status marc_adaptive_huffman_workspace_requirements(
    const marc_adaptive_huffman_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;
/* Adaptive Huffman does not use views_workspace. */
MARC_API marc_status marc_adaptive_huffman_create(
    const marc_adaptive_huffman_config* config,
    marc_buffer primary_workspace,
    marc_buffer secondary_workspace,
    marc_transform** transform) MARC_NOEXCEPT;
MARC_API marc_status marc_dynamic_range_config_init(
    marc_direction direction, marc_dynamic_range_config* config)
    MARC_NOEXCEPT;
MARC_API marc_status marc_dynamic_range_workspace_requirements(
    const marc_dynamic_range_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;
/* Dynamic Range Coder does not use views_workspace. */
MARC_API marc_status marc_dynamic_range_create(
    const marc_dynamic_range_config* config,
    marc_buffer primary_workspace,
    marc_buffer secondary_workspace,
    marc_transform** transform) MARC_NOEXCEPT;
MARC_API marc_status marc_rans_config_init(
    marc_direction direction, marc_rans_config* config) MARC_NOEXCEPT;
MARC_API marc_status marc_rans_workspace_requirements(
    const marc_rans_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;
MARC_API marc_status marc_rans_create(
    const marc_rans_config* config,
    marc_buffer primary_workspace,
    marc_buffer secondary_workspace,
    marc_buffer views_workspace,
    marc_transform** transform) MARC_NOEXCEPT;
MARC_API marc_status marc_tans_config_init(
    marc_direction direction, marc_tans_config* config) MARC_NOEXCEPT;
MARC_API marc_status marc_tans_workspace_requirements(
    const marc_tans_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;
MARC_API marc_status marc_tans_create(
    const marc_tans_config* config,
    marc_buffer primary_workspace,
    marc_buffer secondary_workspace,
    marc_buffer views_workspace,
    marc_transform** transform) MARC_NOEXCEPT;
MARC_API void marc_transform_destroy(marc_transform* transform) MARC_NOEXCEPT;
MARC_API marc_process_result marc_transform_process(
    marc_transform* transform, marc_const_buffer input, marc_buffer output,
    marc_process_flags flags) MARC_NOEXCEPT;

#ifdef __cplusplus
}
#endif

#undef MARC_NOEXCEPT

#endif

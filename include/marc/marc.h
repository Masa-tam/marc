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

/* Selects an exact dictionary/context stream identity; it is not inferred. */
typedef uint32_t marc_lzss_contextual_profile;
#define MARC_LZSS_CONTEXTUAL_PROFILE_64K UINT32_C(0)
#define MARC_LZSS_CONTEXTUAL_PROFILE_1M UINT32_C(1)
#define MARC_LZSS_CONTEXTUAL_PROFILE_4M UINT32_C(2)
#define MARC_LZSS_CONTEXTUAL_PROFILE_16M UINT32_C(3)
#define MARC_LZSS_CONTEXTUAL_PROFILE_64M UINT32_C(4)

/* Selects an encoder-only exact LZSS match-finding strategy. */
typedef uint32_t marc_lzss_match_finder_strategy;
#define MARC_LZSS_MATCH_FINDER_HASH_CHAIN_EXACT UINT32_C(0)
#define MARC_LZSS_MATCH_FINDER_BINARY_TREE_EXACT UINT32_C(1)

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

typedef struct marc_checksum_raw_config {
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
    uint64_t max_dictionary_serialized_size;
    uint64_t max_internal_buffered_bytes;
    uint64_t reserved3;
} marc_checksum_raw_config;

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

typedef struct marc_lz77_config {
    uint32_t struct_size;
    uint32_t abi_version;
    marc_direction direction;
    uint32_t reserved;
    uint64_t original_size;
    uint32_t frame_size;
    uint32_t window_size;
    uint32_t min_match_length;
    uint32_t max_match_length;
    uint64_t max_total_output_size;
    uint64_t max_frame_size;
    uint64_t max_compressed_payload_size;
    uint64_t max_dictionary_serialized_size;
    uint64_t max_internal_buffered_bytes;
    uint64_t max_lz_distance;
    uint64_t max_lz_match_length;
    uint64_t reserved2;
} marc_lz77_config;

typedef struct marc_lz77_blocked_huffman_config {
    uint32_t struct_size;
    uint32_t abi_version;
    marc_direction direction;
    uint32_t reserved;
    uint64_t original_size;
    uint32_t frame_size;
    uint32_t entropy_block_size;
    uint32_t window_size;
    uint32_t min_match_length;
    uint32_t max_match_length;
    uint32_t max_blocks_per_frame;
    uint64_t max_total_output_size;
    uint64_t max_frame_size;
    uint64_t max_block_size;
    uint64_t max_compressed_payload_size;
    uint64_t max_dictionary_serialized_size;
    uint64_t max_internal_buffered_bytes;
    uint64_t max_lz_distance;
    uint64_t max_lz_match_length;
    uint64_t reserved2;
} marc_lz77_blocked_huffman_config;

typedef struct marc_lz77_adaptive_huffman_config {
    uint32_t struct_size;
    uint32_t abi_version;
    marc_direction direction;
    uint32_t reserved;
    uint64_t original_size;
    uint32_t frame_size;
    uint32_t window_size;
    uint32_t min_match_length;
    uint32_t max_match_length;
    uint64_t max_total_output_size;
    uint64_t max_frame_size;
    uint64_t max_compressed_payload_size;
    uint64_t max_dictionary_serialized_size;
    uint64_t max_internal_buffered_bytes;
    uint64_t max_lz_distance;
    uint64_t max_lz_match_length;
    uint64_t reserved2;
} marc_lz77_adaptive_huffman_config;

typedef struct marc_lz77_dynamic_range_config {
    uint32_t struct_size;
    uint32_t abi_version;
    marc_direction direction;
    uint32_t reserved;
    uint64_t original_size;
    uint32_t frame_size;
    uint32_t window_size;
    uint32_t min_match_length;
    uint32_t max_match_length;
    uint64_t max_total_output_size;
    uint64_t max_frame_size;
    uint64_t max_compressed_payload_size;
    uint64_t max_dictionary_serialized_size;
    uint64_t max_internal_buffered_bytes;
    uint64_t max_lz_distance;
    uint64_t max_lz_match_length;
    uint64_t reserved2;
} marc_lz77_dynamic_range_config;

typedef struct marc_lz77_rans_config {
    uint32_t struct_size;
    uint32_t abi_version;
    marc_direction direction;
    uint32_t reserved;
    uint64_t original_size;
    uint32_t frame_size;
    uint32_t entropy_block_size;
    uint32_t window_size;
    uint32_t min_match_length;
    uint32_t max_match_length;
    uint32_t max_blocks_per_frame;
    uint64_t max_total_output_size;
    uint64_t max_frame_size;
    uint64_t max_block_size;
    uint64_t max_compressed_payload_size;
    uint64_t max_dictionary_serialized_size;
    uint64_t max_internal_buffered_bytes;
    uint64_t max_lz_distance;
    uint64_t max_lz_match_length;
    uint64_t reserved2;
} marc_lz77_rans_config;

typedef struct marc_lz77_tans_config {
    uint32_t struct_size;
    uint32_t abi_version;
    marc_direction direction;
    uint32_t reserved;
    uint64_t original_size;
    uint32_t frame_size;
    uint32_t entropy_block_size;
    uint32_t window_size;
    uint32_t min_match_length;
    uint32_t max_match_length;
    uint32_t max_blocks_per_frame;
    uint64_t max_total_output_size;
    uint64_t max_frame_size;
    uint64_t max_block_size;
    uint64_t max_compressed_payload_size;
    uint64_t max_dictionary_serialized_size;
    uint64_t max_internal_buffered_bytes;
    uint64_t max_lz_distance;
    uint64_t max_lz_match_length;
    uint64_t reserved2;
} marc_lz77_tans_config;

typedef struct marc_lzss_config {
    uint32_t struct_size;
    uint32_t abi_version;
    marc_direction direction;
    uint32_t reserved;
    uint64_t original_size;
    uint32_t frame_size;
    uint32_t window_size;
    uint32_t min_match_length;
    uint32_t max_match_length;
    uint64_t max_total_output_size;
    uint64_t max_frame_size;
    uint64_t max_compressed_payload_size;
    uint64_t max_dictionary_serialized_size;
    uint64_t max_internal_buffered_bytes;
    uint64_t max_lz_distance;
    uint64_t max_lz_match_length;
    uint64_t reserved2;
} marc_lzss_config;

typedef struct marc_lzss_blocked_huffman_config {
    uint32_t struct_size;
    uint32_t abi_version;
    marc_direction direction;
    uint32_t reserved;
    uint64_t original_size;
    uint32_t frame_size;
    uint32_t entropy_block_size;
    uint32_t window_size;
    uint32_t min_match_length;
    uint32_t max_match_length;
    uint32_t max_blocks_per_frame;
    uint64_t max_total_output_size;
    uint64_t max_frame_size;
    uint64_t max_block_size;
    uint64_t max_compressed_payload_size;
    uint64_t max_dictionary_serialized_size;
    uint64_t max_internal_buffered_bytes;
    uint64_t max_lz_distance;
    uint64_t max_lz_match_length;
    uint64_t reserved2;
} marc_lzss_blocked_huffman_config;

typedef struct marc_lzss_adaptive_huffman_config {
    uint32_t struct_size;
    uint32_t abi_version;
    marc_direction direction;
    uint32_t reserved;
    uint64_t original_size;
    uint32_t frame_size;
    uint32_t window_size;
    uint32_t min_match_length;
    uint32_t max_match_length;
    uint64_t max_total_output_size;
    uint64_t max_frame_size;
    uint64_t max_compressed_payload_size;
    uint64_t max_dictionary_serialized_size;
    uint64_t max_internal_buffered_bytes;
    uint64_t max_lz_distance;
    uint64_t max_lz_match_length;
    uint64_t reserved2;
} marc_lzss_adaptive_huffman_config;

typedef struct marc_lzss_dynamic_range_config {
    uint32_t struct_size;
    uint32_t abi_version;
    marc_direction direction;
    uint32_t reserved;
    uint64_t original_size;
    uint32_t frame_size;
    uint32_t window_size;
    uint32_t min_match_length;
    uint32_t max_match_length;
    uint64_t max_total_output_size;
    uint64_t max_frame_size;
    uint64_t max_compressed_payload_size;
    uint64_t max_dictionary_serialized_size;
    uint64_t max_internal_buffered_bytes;
    uint64_t max_lz_distance;
    uint64_t max_lz_match_length;
    uint64_t reserved2;
} marc_lzss_dynamic_range_config;

/* Position-distance configuration: fixed 64 KiB window, matches 3..258.
 * No profile helper is needed. This family is staged pending admission. */
typedef struct marc_lzss_position_distance_dynamic_range_config {
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
} marc_lzss_position_distance_dynamic_range_config;

/* Separate 1 MiB window, matches 3..258; distinct wire identity. */
typedef struct marc_lzss_position_distance_dynamic_range_1m_config {
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
} marc_lzss_position_distance_dynamic_range_1m_config;

/* Distinct four-MiB identity; no generic or one-MiB defaults are changed. */
typedef struct marc_lzss_position_distance_dynamic_range_4m_config {
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
} marc_lzss_position_distance_dynamic_range_4m_config;

/* Explicit known-size eight-MiB encoder config; no generic profile alias.
 * init produces a template: remaining zero limits must be configured explicitly.
 * Capacity fields bound full process extents, including unused tails. */
#define MARC_LZSS_POSITION_DISTANCE_8M_PREPARED_OWNING UINT32_C(1)
#define MARC_LZSS_POSITION_DISTANCE_8M_INITIAL_ONLY UINT32_C(1)
typedef struct marc_lzss_position_distance_dynamic_range_8m_config {
    uint32_t struct_size, abi_version, encoder_strategy, reserved;
    uint64_t original_size;
    uint32_t frame_size, reserved2;
    uint64_t max_total_output_size, max_frame_size, max_block_size;
    uint64_t max_compressed_payload_size, max_internal_buffered_bytes;
    uint64_t max_lz_distance, max_lz_match_length;
    uint64_t max_entropy_table_entries, max_range_model_total;
    uint64_t max_expansion_ratio, expansion_slack;
    uint64_t external_retained_bytes, input_capacity_bytes, output_capacity_bytes;
} marc_lzss_position_distance_dynamic_range_8m_config;
typedef struct marc_lzss_position_distance_dynamic_range_8m_resources {
    uint32_t struct_size, abi_version;
    uint64_t external_charge_bytes, fixed_bytes, initial_raw_bytes;
    uint64_t initial_index_entries, initial_bytes;
    uint32_t admission_scope, reserved;
} marc_lzss_position_distance_dynamic_range_8m_resources;

/* Five borrowed workspaces; opaque token storage, never a wire representation.
 * This decoder has its own configuration and immutable decode direction. */
#define MARC_LZSS_POSITION_DISTANCE_8M_CAPACITY_ONLY UINT32_C(1)
typedef struct marc_lzss_position_distance_dynamic_range_8m_decoder_config {
    uint32_t struct_size, abi_version, reserved, reserved2;
    uint64_t max_total_output_size, max_frame_size, max_block_size;
    uint64_t max_compressed_payload_size, max_internal_buffered_bytes;
    uint64_t max_lz_distance, max_lz_match_length;
    uint64_t max_entropy_table_entries, max_range_model_total;
    uint64_t max_expansion_ratio, expansion_slack;
    uint64_t external_retained_bytes, input_capacity_bytes, output_capacity_bytes;
} marc_lzss_position_distance_dynamic_range_8m_decoder_config;
typedef struct marc_lzss_position_distance_dynamic_range_8m_decoder_requirements {
    uint32_t struct_size, abi_version, admission_scope, reserved;
    uint64_t serialized_bytes, token_bytes, token_scratch_bytes;
    uint64_t raw_bytes, raw_scratch_bytes, token_alignment, token_elements;
    uint64_t minimum_aggregate_bytes;
} marc_lzss_position_distance_dynamic_range_8m_decoder_requirements;
typedef struct marc_lzss_position_distance_dynamic_range_8m_decoder_buffers {
    uint32_t struct_size, abi_version, reserved, reserved2;
    marc_buffer serialized, tokens, token_scratch, raw, raw_scratch;
} marc_lzss_position_distance_dynamic_range_8m_decoder_buffers;

typedef struct marc_lzss_contextual_dynamic_range_config {
    uint32_t struct_size;
    uint32_t abi_version;
    marc_direction direction;
    marc_lzss_match_finder_strategy match_finder_strategy;
    uint64_t original_size;
    uint32_t frame_size;
    uint32_t window_size;
    uint32_t min_match_length;
    uint32_t max_match_length;
    uint64_t max_total_output_size;
    uint64_t max_frame_size;
    uint64_t max_block_size;
    uint64_t max_compressed_payload_size;
    uint64_t max_internal_buffered_bytes;
    uint64_t max_lz_distance;
    uint64_t max_lz_match_length;
    uint64_t max_entropy_table_entries;
    uint64_t max_range_model_total;
    marc_lzss_contextual_profile profile;
    uint32_t reserved2;
} marc_lzss_contextual_dynamic_range_config;

typedef struct marc_lzss_contextual_rans_config {
    uint32_t struct_size;
    uint32_t abi_version;
    marc_direction direction;
    marc_lzss_match_finder_strategy match_finder_strategy;
    uint64_t original_size;
    uint32_t frame_size;
    uint32_t window_size;
    uint32_t min_match_length;
    uint32_t max_match_length;
    uint64_t max_total_output_size;
    uint64_t max_frame_size;
    uint64_t max_block_size;
    uint64_t max_compressed_payload_size;
    uint64_t max_internal_buffered_bytes;
    uint64_t max_lz_distance;
    uint64_t max_lz_match_length;
    uint64_t max_entropy_table_entries;
    marc_lzss_contextual_profile profile;
    uint32_t reserved2;
} marc_lzss_contextual_rans_config;

typedef struct marc_lzss_contextual_tans_config {
    uint32_t struct_size;
    uint32_t abi_version;
    marc_direction direction;
    marc_lzss_match_finder_strategy match_finder_strategy;
    uint64_t original_size;
    uint32_t frame_size;
    uint32_t window_size;
    uint32_t min_match_length;
    uint32_t max_match_length;
    uint64_t max_total_output_size;
    uint64_t max_frame_size;
    uint64_t max_block_size;
    uint64_t max_compressed_payload_size;
    uint64_t max_internal_buffered_bytes;
    uint64_t max_lz_distance;
    uint64_t max_lz_match_length;
    uint64_t max_entropy_table_entries;
    marc_lzss_contextual_profile profile;
    uint32_t reserved2;
} marc_lzss_contextual_tans_config;

typedef struct marc_lzss_contextual_adaptive_huffman_config {
    uint32_t struct_size;
    uint32_t abi_version;
    marc_direction direction;
    marc_lzss_match_finder_strategy match_finder_strategy;
    uint64_t original_size;
    uint32_t frame_size;
    uint32_t window_size;
    uint32_t min_match_length;
    uint32_t max_match_length;
    uint64_t max_total_output_size;
    uint64_t max_frame_size;
    uint64_t max_block_size;
    uint64_t max_compressed_payload_size;
    uint64_t max_internal_buffered_bytes;
    uint64_t max_lz_distance;
    uint64_t max_lz_match_length;
    uint64_t max_entropy_table_entries;
    marc_lzss_contextual_profile profile;
    uint32_t reserved2;
} marc_lzss_contextual_adaptive_huffman_config;

typedef struct marc_lzss_contextual_blocked_huffman_config {
    uint32_t struct_size;
    uint32_t abi_version;
    marc_direction direction;
    marc_lzss_match_finder_strategy match_finder_strategy;
    uint64_t original_size;
    uint32_t frame_size;
    uint32_t window_size;
    uint32_t min_match_length;
    uint32_t max_match_length;
    uint64_t max_total_output_size;
    uint64_t max_frame_size;
    uint64_t max_block_size;
    uint64_t max_compressed_payload_size;
    uint64_t max_internal_buffered_bytes;
    uint64_t max_lz_distance;
    uint64_t max_lz_match_length;
    uint64_t max_entropy_table_entries;
    marc_lzss_contextual_profile profile;
    uint32_t reserved2;
} marc_lzss_contextual_blocked_huffman_config;

typedef struct marc_lzss_rans_config {
    uint32_t struct_size;
    uint32_t abi_version;
    marc_direction direction;
    uint32_t reserved;
    uint64_t original_size;
    uint32_t frame_size;
    uint32_t entropy_block_size;
    uint32_t window_size;
    uint32_t min_match_length;
    uint32_t max_match_length;
    uint32_t max_blocks_per_frame;
    uint64_t max_total_output_size;
    uint64_t max_frame_size;
    uint64_t max_block_size;
    uint64_t max_compressed_payload_size;
    uint64_t max_dictionary_serialized_size;
    uint64_t max_internal_buffered_bytes;
    uint64_t max_lz_distance;
    uint64_t max_lz_match_length;
    uint64_t reserved2;
} marc_lzss_rans_config;

typedef struct marc_lzss_tans_config {
    uint32_t struct_size;
    uint32_t abi_version;
    marc_direction direction;
    uint32_t reserved;
    uint64_t original_size;
    uint32_t frame_size;
    uint32_t entropy_block_size;
    uint32_t window_size;
    uint32_t min_match_length;
    uint32_t max_match_length;
    uint32_t max_blocks_per_frame;
    uint64_t max_total_output_size;
    uint64_t max_frame_size;
    uint64_t max_block_size;
    uint64_t max_compressed_payload_size;
    uint64_t max_dictionary_serialized_size;
    uint64_t max_internal_buffered_bytes;
    uint64_t max_lz_distance;
    uint64_t max_lz_match_length;
    uint64_t reserved2;
} marc_lzss_tans_config;

typedef struct marc_lz78_config {
    uint32_t struct_size;
    uint32_t abi_version;
    marc_direction direction;
    uint32_t reserved;
    uint64_t original_size;
    uint32_t frame_size;
    uint32_t maximum_entries;
    uint64_t max_total_output_size;
    uint64_t max_frame_size;
    uint64_t max_compressed_payload_size;
    uint64_t max_dictionary_serialized_size;
    uint64_t max_internal_buffered_bytes;
    uint64_t max_dictionary_entries;
    uint64_t reserved2;
} marc_lz78_config;

typedef struct marc_lz78_blocked_huffman_config {
    uint32_t struct_size;
    uint32_t abi_version;
    marc_direction direction;
    uint32_t reserved;
    uint64_t original_size;
    uint32_t frame_size;
    uint32_t entropy_block_size;
    uint32_t maximum_entries;
    uint32_t max_blocks_per_frame;
    uint64_t max_total_output_size;
    uint64_t max_frame_size;
    uint64_t max_block_size;
    uint64_t max_compressed_payload_size;
    uint64_t max_dictionary_serialized_size;
    uint64_t max_internal_buffered_bytes;
    uint64_t max_dictionary_entries;
    uint64_t reserved2;
} marc_lz78_blocked_huffman_config;

typedef struct marc_lz78_adaptive_huffman_config {
    uint32_t struct_size;
    uint32_t abi_version;
    marc_direction direction;
    uint32_t reserved;
    uint64_t original_size;
    uint32_t frame_size;
    uint32_t maximum_entries;
    uint64_t max_total_output_size;
    uint64_t max_frame_size;
    uint64_t max_compressed_payload_size;
    uint64_t max_dictionary_serialized_size;
    uint64_t max_internal_buffered_bytes;
    uint64_t max_dictionary_entries;
    uint64_t reserved2;
} marc_lz78_adaptive_huffman_config;

typedef struct marc_lz78_dynamic_range_config {
    uint32_t struct_size;
    uint32_t abi_version;
    marc_direction direction;
    uint32_t reserved;
    uint64_t original_size;
    uint32_t frame_size;
    uint32_t maximum_entries;
    uint64_t max_total_output_size;
    uint64_t max_frame_size;
    uint64_t max_compressed_payload_size;
    uint64_t max_dictionary_serialized_size;
    uint64_t max_internal_buffered_bytes;
    uint64_t max_dictionary_entries;
    uint64_t reserved2;
} marc_lz78_dynamic_range_config;

typedef struct marc_lz78_rans_config {
    uint32_t struct_size;
    uint32_t abi_version;
    marc_direction direction;
    uint32_t reserved;
    uint64_t original_size;
    uint32_t frame_size;
    uint32_t entropy_block_size;
    uint32_t maximum_entries;
    uint32_t max_blocks_per_frame;
    uint64_t max_total_output_size;
    uint64_t max_frame_size;
    uint64_t max_block_size;
    uint64_t max_compressed_payload_size;
    uint64_t max_dictionary_serialized_size;
    uint64_t max_internal_buffered_bytes;
    uint64_t max_dictionary_entries;
    uint64_t reserved2;
} marc_lz78_rans_config;

typedef struct marc_lz78_tans_config {
    uint32_t struct_size;
    uint32_t abi_version;
    marc_direction direction;
    uint32_t reserved;
    uint64_t original_size;
    uint32_t frame_size;
    uint32_t entropy_block_size;
    uint32_t maximum_entries;
    uint32_t max_blocks_per_frame;
    uint64_t max_total_output_size;
    uint64_t max_frame_size;
    uint64_t max_block_size;
    uint64_t max_compressed_payload_size;
    uint64_t max_dictionary_serialized_size;
    uint64_t max_internal_buffered_bytes;
    uint64_t max_dictionary_entries;
    uint64_t reserved2;
} marc_lz78_tans_config;

typedef struct marc_lzw_config {
    uint32_t struct_size;
    uint32_t abi_version;
    marc_direction direction;
    uint32_t reserved;
    uint64_t original_size;
    uint32_t frame_size;
    uint32_t maximum_code_width;
    uint64_t max_total_output_size;
    uint64_t max_frame_size;
    uint64_t max_compressed_payload_size;
    uint64_t max_dictionary_serialized_size;
    uint64_t max_internal_buffered_bytes;
    uint64_t max_dictionary_entries;
    uint64_t reserved2;
} marc_lzw_config;

typedef struct marc_lzw_blocked_huffman_config {
    uint32_t struct_size;
    uint32_t abi_version;
    marc_direction direction;
    uint32_t reserved;
    uint64_t original_size;
    uint32_t frame_size;
    uint32_t entropy_block_size;
    uint32_t maximum_code_width;
    uint32_t max_blocks_per_frame;
    uint64_t max_total_output_size;
    uint64_t max_frame_size;
    uint64_t max_block_size;
    uint64_t max_compressed_payload_size;
    uint64_t max_dictionary_serialized_size;
    uint64_t max_internal_buffered_bytes;
    uint64_t max_dictionary_entries;
    uint64_t reserved2;
} marc_lzw_blocked_huffman_config;

typedef struct marc_lzw_adaptive_huffman_config {
    uint32_t struct_size;
    uint32_t abi_version;
    marc_direction direction;
    uint32_t reserved;
    uint64_t original_size;
    uint32_t frame_size;
    uint32_t maximum_code_width;
    uint64_t max_total_output_size;
    uint64_t max_frame_size;
    uint64_t max_compressed_payload_size;
    uint64_t max_dictionary_serialized_size;
    uint64_t max_internal_buffered_bytes;
    uint64_t max_dictionary_entries;
    uint64_t reserved2;
} marc_lzw_adaptive_huffman_config;

typedef struct marc_lzw_dynamic_range_config {
    uint32_t struct_size;
    uint32_t abi_version;
    marc_direction direction;
    uint32_t reserved;
    uint64_t original_size;
    uint32_t frame_size;
    uint32_t maximum_code_width;
    uint64_t max_total_output_size;
    uint64_t max_frame_size;
    uint64_t max_compressed_payload_size;
    uint64_t max_dictionary_serialized_size;
    uint64_t max_internal_buffered_bytes;
    uint64_t max_dictionary_entries;
    uint64_t reserved2;
} marc_lzw_dynamic_range_config;

typedef struct marc_lzw_rans_config {
    uint32_t struct_size;
    uint32_t abi_version;
    marc_direction direction;
    uint32_t reserved;
    uint64_t original_size;
    uint32_t frame_size;
    uint32_t entropy_block_size;
    uint32_t maximum_code_width;
    uint32_t max_blocks_per_frame;
    uint64_t max_total_output_size;
    uint64_t max_frame_size;
    uint64_t max_block_size;
    uint64_t max_compressed_payload_size;
    uint64_t max_dictionary_serialized_size;
    uint64_t max_internal_buffered_bytes;
    uint64_t max_dictionary_entries;
    uint64_t reserved2;
} marc_lzw_rans_config;

typedef struct marc_lzw_tans_config {
    uint32_t struct_size;
    uint32_t abi_version;
    marc_direction direction;
    uint32_t reserved;
    uint64_t original_size;
    uint32_t frame_size;
    uint32_t entropy_block_size;
    uint32_t maximum_code_width;
    uint32_t max_blocks_per_frame;
    uint64_t max_total_output_size;
    uint64_t max_frame_size;
    uint64_t max_block_size;
    uint64_t max_compressed_payload_size;
    uint64_t max_dictionary_serialized_size;
    uint64_t max_internal_buffered_bytes;
    uint64_t max_dictionary_entries;
    uint64_t reserved2;
} marc_lzw_tans_config;

typedef struct marc_lzd_config {
    uint32_t struct_size;
    uint32_t abi_version;
    marc_direction direction;
    uint32_t reserved;
    uint64_t original_size;
    uint32_t frame_size;
    uint32_t maximum_entries;
    uint64_t max_total_output_size;
    uint64_t max_frame_size;
    uint64_t max_compressed_payload_size;
    uint64_t max_dictionary_serialized_size;
    uint64_t max_internal_buffered_bytes;
    uint64_t max_dictionary_entries;
    uint64_t reserved2;
} marc_lzd_config;

typedef struct marc_lzd_blocked_huffman_config {
    uint32_t struct_size;
    uint32_t abi_version;
    marc_direction direction;
    uint32_t reserved;
    uint64_t original_size;
    uint32_t frame_size;
    uint32_t entropy_block_size;
    uint32_t maximum_entries;
    uint32_t max_blocks_per_frame;
    uint64_t max_total_output_size;
    uint64_t max_frame_size;
    uint64_t max_block_size;
    uint64_t max_compressed_payload_size;
    uint64_t max_dictionary_serialized_size;
    uint64_t max_internal_buffered_bytes;
    uint64_t max_dictionary_entries;
    uint64_t reserved2;
} marc_lzd_blocked_huffman_config;

typedef struct marc_lzd_adaptive_huffman_config {
    uint32_t struct_size;
    uint32_t abi_version;
    marc_direction direction;
    uint32_t reserved;
    uint64_t original_size;
    uint32_t frame_size;
    uint32_t maximum_entries;
    uint64_t max_total_output_size;
    uint64_t max_frame_size;
    uint64_t max_compressed_payload_size;
    uint64_t max_dictionary_serialized_size;
    uint64_t max_internal_buffered_bytes;
    uint64_t max_dictionary_entries;
    uint64_t reserved2;
} marc_lzd_adaptive_huffman_config;

typedef struct marc_lzd_dynamic_range_config {
    uint32_t struct_size;
    uint32_t abi_version;
    marc_direction direction;
    uint32_t reserved;
    uint64_t original_size;
    uint32_t frame_size;
    uint32_t maximum_entries;
    uint64_t max_total_output_size;
    uint64_t max_frame_size;
    uint64_t max_compressed_payload_size;
    uint64_t max_dictionary_serialized_size;
    uint64_t max_internal_buffered_bytes;
    uint64_t max_dictionary_entries;
    uint64_t reserved2;
} marc_lzd_dynamic_range_config;

typedef struct marc_lzd_rans_config {
    uint32_t struct_size;
    uint32_t abi_version;
    marc_direction direction;
    uint32_t reserved;
    uint64_t original_size;
    uint32_t frame_size;
    uint32_t entropy_block_size;
    uint32_t maximum_entries;
    uint32_t max_blocks_per_frame;
    uint64_t max_total_output_size;
    uint64_t max_frame_size;
    uint64_t max_block_size;
    uint64_t max_compressed_payload_size;
    uint64_t max_dictionary_serialized_size;
    uint64_t max_internal_buffered_bytes;
    uint64_t max_dictionary_entries;
    uint64_t reserved2;
} marc_lzd_rans_config;

typedef struct marc_lzd_tans_config {
    uint32_t struct_size;
    uint32_t abi_version;
    marc_direction direction;
    uint32_t reserved;
    uint64_t original_size;
    uint32_t frame_size;
    uint32_t entropy_block_size;
    uint32_t maximum_entries;
    uint32_t max_blocks_per_frame;
    uint64_t max_total_output_size;
    uint64_t max_frame_size;
    uint64_t max_block_size;
    uint64_t max_compressed_payload_size;
    uint64_t max_dictionary_serialized_size;
    uint64_t max_internal_buffered_bytes;
    uint64_t max_dictionary_entries;
    uint64_t reserved2;
} marc_lzd_tans_config;

typedef struct marc_lzmw_rans_config {
    uint32_t struct_size;
    uint32_t abi_version;
    marc_direction direction;
    uint32_t reserved;
    uint64_t original_size;
    uint32_t frame_size;
    uint32_t entropy_block_size;
    uint32_t maximum_entries;
    uint32_t max_blocks_per_frame;
    uint64_t max_total_output_size;
    uint64_t max_frame_size;
    uint64_t max_block_size;
    uint64_t max_compressed_payload_size;
    uint64_t max_dictionary_serialized_size;
    uint64_t max_internal_buffered_bytes;
    uint64_t max_dictionary_entries;
    uint64_t reserved2;
} marc_lzmw_rans_config;

typedef struct marc_lzmw_tans_config {
    uint32_t struct_size;
    uint32_t abi_version;
    marc_direction direction;
    uint32_t reserved;
    uint64_t original_size;
    uint32_t frame_size;
    uint32_t entropy_block_size;
    uint32_t maximum_entries;
    uint32_t max_blocks_per_frame;
    uint64_t max_total_output_size;
    uint64_t max_frame_size;
    uint64_t max_block_size;
    uint64_t max_compressed_payload_size;
    uint64_t max_dictionary_serialized_size;
    uint64_t max_internal_buffered_bytes;
    uint64_t max_dictionary_entries;
    uint64_t reserved2;
} marc_lzmw_tans_config;

typedef struct marc_lzmw_config {
    uint32_t struct_size;
    uint32_t abi_version;
    marc_direction direction;
    uint32_t reserved;
    uint64_t original_size;
    uint32_t frame_size;
    uint32_t maximum_entries;
    uint64_t max_total_output_size;
    uint64_t max_frame_size;
    uint64_t max_compressed_payload_size;
    uint64_t max_dictionary_serialized_size;
    uint64_t max_internal_buffered_bytes;
    uint64_t max_dictionary_entries;
    uint64_t reserved2;
} marc_lzmw_config;

typedef struct marc_lzmw_blocked_huffman_config {
    uint32_t struct_size;
    uint32_t abi_version;
    marc_direction direction;
    uint32_t reserved;
    uint64_t original_size;
    uint32_t frame_size;
    uint32_t entropy_block_size;
    uint32_t maximum_entries;
    uint32_t max_blocks_per_frame;
    uint64_t max_total_output_size;
    uint64_t max_frame_size;
    uint64_t max_block_size;
    uint64_t max_compressed_payload_size;
    uint64_t max_dictionary_serialized_size;
    uint64_t max_internal_buffered_bytes;
    uint64_t max_dictionary_entries;
    uint64_t reserved2;
} marc_lzmw_blocked_huffman_config;

typedef struct marc_lzmw_adaptive_huffman_config {
    uint32_t struct_size;
    uint32_t abi_version;
    marc_direction direction;
    uint32_t reserved;
    uint64_t original_size;
    uint32_t frame_size;
    uint32_t maximum_entries;
    uint64_t max_total_output_size;
    uint64_t max_frame_size;
    uint64_t max_compressed_payload_size;
    uint64_t max_dictionary_serialized_size;
    uint64_t max_internal_buffered_bytes;
    uint64_t max_dictionary_entries;
    uint64_t reserved2;
} marc_lzmw_adaptive_huffman_config;

typedef struct marc_lzmw_dynamic_range_config {
    uint32_t struct_size;
    uint32_t abi_version;
    marc_direction direction;
    uint32_t reserved;
    uint64_t original_size;
    uint32_t frame_size;
    uint32_t maximum_entries;
    uint64_t max_total_output_size;
    uint64_t max_frame_size;
    uint64_t max_compressed_payload_size;
    uint64_t max_dictionary_serialized_size;
    uint64_t max_internal_buffered_bytes;
    uint64_t max_dictionary_entries;
    uint64_t reserved2;
} marc_lzmw_dynamic_range_config;

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

/*
 * Version 1.1 None/None framing with one fixed per-frame CRC-32C over raw
 * bytes. The sole primary workspace remains caller-owned for the transform's
 * lifetime; secondary and views workspaces are not used.
 */
MARC_API marc_status marc_checksum_raw_config_init(
    marc_direction direction, marc_checksum_raw_config* config)
    MARC_NOEXCEPT;
MARC_API marc_status marc_checksum_raw_workspace_requirements(
    const marc_checksum_raw_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;
MARC_API marc_status marc_checksum_raw_create(
    const marc_checksum_raw_config* config,
    marc_buffer primary_workspace,
    marc_transform** transform) MARC_NOEXCEPT;

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
MARC_API marc_status marc_lz77_config_init(
    marc_direction direction, marc_lz77_config* config) MARC_NOEXCEPT;
MARC_API marc_status marc_lz77_workspace_requirements(
    const marc_lz77_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;
/* LZ77 does not use views_workspace. */
MARC_API marc_status marc_lz77_create(
    const marc_lz77_config* config,
    marc_buffer primary_workspace,
    marc_buffer secondary_workspace,
    marc_transform** transform) MARC_NOEXCEPT;
MARC_API marc_status marc_lz77_blocked_huffman_config_init(
    marc_direction direction, marc_lz77_blocked_huffman_config* config)
    MARC_NOEXCEPT;
MARC_API marc_status marc_lz77_blocked_huffman_workspace_requirements(
    const marc_lz77_blocked_huffman_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;
/*
 * secondary_workspace is partitioned internally into dictionary staging and
 * frame storage. Decoding uses aligned views_workspace for entropy blocks.
 */
MARC_API marc_status marc_lz77_blocked_huffman_create(
    const marc_lz77_blocked_huffman_config* config,
    marc_buffer primary_workspace,
    marc_buffer secondary_workspace,
    marc_buffer views_workspace,
    marc_transform** transform) MARC_NOEXCEPT;
MARC_API marc_status marc_lz77_adaptive_huffman_config_init(
    marc_direction direction, marc_lz77_adaptive_huffman_config* config)
    MARC_NOEXCEPT;
MARC_API marc_status marc_lz77_adaptive_huffman_workspace_requirements(
    const marc_lz77_adaptive_huffman_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;
/* LZ77 plus Adaptive Huffman does not use views_workspace. */
MARC_API marc_status marc_lz77_adaptive_huffman_create(
    const marc_lz77_adaptive_huffman_config* config,
    marc_buffer primary_workspace,
    marc_buffer secondary_workspace,
    marc_transform** transform) MARC_NOEXCEPT;
MARC_API marc_status marc_lz77_dynamic_range_config_init(
    marc_direction direction, marc_lz77_dynamic_range_config* config)
    MARC_NOEXCEPT;
MARC_API marc_status marc_lz77_dynamic_range_workspace_requirements(
    const marc_lz77_dynamic_range_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;
/* LZ77 plus Dynamic Range does not use views_workspace. */
MARC_API marc_status marc_lz77_dynamic_range_create(
    const marc_lz77_dynamic_range_config* config,
    marc_buffer primary_workspace,
    marc_buffer secondary_workspace,
    marc_transform** transform) MARC_NOEXCEPT;
MARC_API marc_status marc_lz77_rans_config_init(
    marc_direction direction, marc_lz77_rans_config* config) MARC_NOEXCEPT;
MARC_API marc_status marc_lz77_rans_workspace_requirements(
    const marc_lz77_rans_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;
/*
 * secondary_workspace is partitioned internally into dictionary staging and
 * frame storage. Decoding uses aligned views_workspace for rANS blocks.
 */
MARC_API marc_status marc_lz77_rans_create(
    const marc_lz77_rans_config* config,
    marc_buffer primary_workspace,
    marc_buffer secondary_workspace,
    marc_buffer views_workspace,
    marc_transform** transform) MARC_NOEXCEPT;
MARC_API marc_status marc_lz77_tans_config_init(
    marc_direction direction, marc_lz77_tans_config* config) MARC_NOEXCEPT;
MARC_API marc_status marc_lz77_tans_workspace_requirements(
    const marc_lz77_tans_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;
/*
 * secondary_workspace is partitioned internally into dictionary staging and
 * frame storage. Decoding uses aligned views_workspace for tANS blocks.
 */
MARC_API marc_status marc_lz77_tans_create(
    const marc_lz77_tans_config* config,
    marc_buffer primary_workspace,
    marc_buffer secondary_workspace,
    marc_buffer views_workspace,
    marc_transform** transform) MARC_NOEXCEPT;
MARC_API marc_status marc_lzss_config_init(
    marc_direction direction, marc_lzss_config* config) MARC_NOEXCEPT;
MARC_API marc_status marc_lzss_workspace_requirements(
    const marc_lzss_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;
/* LZSS does not use views_workspace. */
MARC_API marc_status marc_lzss_create(
    const marc_lzss_config* config,
    marc_buffer primary_workspace,
    marc_buffer secondary_workspace,
    marc_transform** transform) MARC_NOEXCEPT;
MARC_API marc_status marc_lzss_blocked_huffman_config_init(
    marc_direction direction, marc_lzss_blocked_huffman_config* config)
    MARC_NOEXCEPT;
MARC_API marc_status marc_lzss_blocked_huffman_workspace_requirements(
    const marc_lzss_blocked_huffman_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;
/*
 * secondary_workspace is partitioned internally into LZSS token staging and
 * frame storage. Decoding uses aligned views_workspace for entropy blocks.
 */
MARC_API marc_status marc_lzss_blocked_huffman_create(
    const marc_lzss_blocked_huffman_config* config,
    marc_buffer primary_workspace,
    marc_buffer secondary_workspace,
    marc_buffer views_workspace,
    marc_transform** transform) MARC_NOEXCEPT;
MARC_API marc_status marc_lzss_adaptive_huffman_config_init(
    marc_direction direction, marc_lzss_adaptive_huffman_config* config)
    MARC_NOEXCEPT;
MARC_API marc_status marc_lzss_adaptive_huffman_workspace_requirements(
    const marc_lzss_adaptive_huffman_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;
/* LZSS plus Adaptive Huffman does not use views_workspace. */
MARC_API marc_status marc_lzss_adaptive_huffman_create(
    const marc_lzss_adaptive_huffman_config* config,
    marc_buffer primary_workspace,
    marc_buffer secondary_workspace,
    marc_transform** transform) MARC_NOEXCEPT;
MARC_API marc_status marc_lzss_dynamic_range_config_init(
    marc_direction direction, marc_lzss_dynamic_range_config* config)
    MARC_NOEXCEPT;
MARC_API marc_status marc_lzss_dynamic_range_workspace_requirements(
    const marc_lzss_dynamic_range_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;
/* LZSS plus Dynamic Range does not use views_workspace. */
MARC_API marc_status marc_lzss_dynamic_range_create(
    const marc_lzss_dynamic_range_config* config,
    marc_buffer primary_workspace,
    marc_buffer secondary_workspace,
    marc_transform** transform) MARC_NOEXCEPT;
/* Defaults suffice for the sole supported range. Invalid direction/null
 * leaves the configuration unchanged. Decode ignores original_size/frame_size. */
MARC_API marc_status marc_lzss_position_distance_dynamic_range_config_init(
    marc_direction direction,
    marc_lzss_position_distance_dynamic_range_config* config) MARC_NOEXCEPT;

/* Encode: primary=raw, secondary=serialized; decode reverses these roles.
 * Views holds aligned typed storage. Includes handle/state in aggregate checks,
 * but reports caller storage only. Failure (including metadata overlap) leaves
 * requirements unchanged. Local limits never enlarge the wire envelope. */
MARC_API marc_status marc_lzss_position_distance_dynamic_range_workspace_requirements(
    const marc_lzss_position_distance_dynamic_range_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;

/* Retains only queried prefixes for the handle lifetime. Retained prefixes
 * must be disjoint from one another and config; handle output must not overlap config or
 * supplied workspace. A disjoint handle output is null on failure. Flush
 * preserves frames; ResetBlock is unsupported. Ended/error states are sticky.
 * Decoder publication is atomic per frame, not for the whole stream. */
MARC_API marc_status marc_lzss_position_distance_dynamic_range_create(
    const marc_lzss_position_distance_dynamic_range_config* config,
    marc_buffer primary_workspace, marc_buffer secondary_workspace,
    marc_buffer views_workspace, marc_transform** transform) MARC_NOEXCEPT;

/* 1 MiB counterpart: the same prefix ownership and process contracts apply. */
MARC_API marc_status marc_lzss_position_distance_dynamic_range_1m_config_init(
    marc_direction direction,
    marc_lzss_position_distance_dynamic_range_1m_config* config) MARC_NOEXCEPT;

/* Encode: primary=raw, secondary=serialized; decode reverses these roles.
 * Views holds aligned typed storage. Includes handle/state in aggregate checks,
 * but reports caller storage only. Failure (including metadata overlap) leaves
 * requirements unchanged. Local limits never enlarge the wire envelope. */
MARC_API marc_status marc_lzss_position_distance_dynamic_range_1m_workspace_requirements(
    const marc_lzss_position_distance_dynamic_range_1m_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;

/* Retains only queried prefixes for the handle lifetime. Retained prefixes
 * must be disjoint from one another and config; handle output must not overlap config or
 * supplied workspace. A disjoint handle output is null on failure. Flush
 * preserves frames; ResetBlock is unsupported. Ended/error states are sticky.
 * Decoder publication is atomic per frame, not for the whole stream. */
MARC_API marc_status marc_lzss_position_distance_dynamic_range_1m_create(
    const marc_lzss_position_distance_dynamic_range_1m_config* config,
    marc_buffer primary_workspace, marc_buffer secondary_workspace,
    marc_buffer views_workspace, marc_transform** transform) MARC_NOEXCEPT;

/* Four-MiB fixed-window scalar baseline; explicit profile-only defaults.
 * Query reports caller storage and checks actual codec/guard/handle aggregate.
 * Failed queries preserve requirements, including metadata-overlap failures. */
MARC_API marc_status marc_lzss_position_distance_dynamic_range_4m_config_init(
    marc_direction direction, marc_lzss_position_distance_dynamic_range_4m_config* config) MARC_NOEXCEPT;
MARC_API marc_status marc_lzss_position_distance_dynamic_range_4m_workspace_requirements(
    const marc_lzss_position_distance_dynamic_range_4m_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;
/* Encode primary=raw/secondary=serialized; decode reverses roles. All supplied
 * workspace capacities, including unused tails, are borrowed and charged until
 * destruction, and must be disjoint from each other and config/output metadata.
 * Process input/output must not overlap any retained workspace or handle.
 * A disjoint handle output is null on failure; aliased outputs are unchanged.
 * Flush preserves frames; ResetBlock is unsupported. End/error are sticky.
 * Decoder publication is atomic per frame; failed frames publish no raw bytes. */
MARC_API marc_status marc_lzss_position_distance_dynamic_range_4m_create(
    const marc_lzss_position_distance_dynamic_range_4m_config* config,
    marc_buffer primary_workspace, marc_buffer secondary_workspace,
    marc_buffer views_workspace, marc_transform** transform) MARC_NOEXCEPT;

/* Logical resource accounting includes full capacities and conservatively
 * duplicated owner/control/helper charges; it is not RSS. Query is unchanged
 * on failure. INITIAL_ONLY admits no future candidate generation: budget
 * refusal can occur later, without exposing bytes of the failed frame.
 * create has no caller workspace or allocator; config need not remain alive.
 * A disjoint output pointer is null on failure; aliased metadata is unchanged.
 * Flush is neutral; ResetBlock unsupported. Repeat EndInput on a final suffix.
 * Terminal states are sticky for valid buffers; the generic process entry still
 * rejects null nonempty buffers before dispatch. Previous frames remain valid. */
MARC_API marc_status marc_lzss_position_distance_dynamic_range_8m_config_init(
    marc_lzss_position_distance_dynamic_range_8m_config* config) MARC_NOEXCEPT;
MARC_API marc_status marc_lzss_position_distance_dynamic_range_8m_resource_requirements(
    const marc_lzss_position_distance_dynamic_range_8m_config* config,
    marc_lzss_position_distance_dynamic_range_8m_resources* requirements) MARC_NOEXCEPT;
MARC_API marc_status marc_lzss_position_distance_dynamic_range_8m_create_encoder(
    const marc_lzss_position_distance_dynamic_range_8m_config* config,
    marc_transform** transform) MARC_NOEXCEPT;

MARC_API marc_status marc_lzss_contextual_dynamic_range_config_init(
    marc_direction direction,
    marc_lzss_contextual_dynamic_range_config* config) MARC_NOEXCEPT;
MARC_API marc_status
marc_lzss_contextual_dynamic_range_config_apply_profile(
    marc_lzss_contextual_dynamic_range_config* config,
    marc_lzss_contextual_profile profile) MARC_NOEXCEPT;
MARC_API marc_status marc_lzss_contextual_dynamic_range_workspace_requirements(
    const marc_lzss_contextual_dynamic_range_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;
/*
 * Format 2 keeps primary and secondary byte workspaces direction-specific.
 * Aligned views_workspace is opaque typed-token/model staging and must remain
 * caller-owned for the transform lifetime.
 */
MARC_API marc_status marc_lzss_contextual_dynamic_range_create(
    const marc_lzss_contextual_dynamic_range_config* config,
    marc_buffer primary_workspace,
    marc_buffer secondary_workspace,
    marc_buffer views_workspace,
    marc_transform** transform) MARC_NOEXCEPT;
MARC_API marc_status marc_lzss_contextual_rans_config_init(
    marc_direction direction,
    marc_lzss_contextual_rans_config* config) MARC_NOEXCEPT;
/*
 * Contextual rANS accepts the 64K, 1M, 4M, 16M, and 64M profiles.
 * Initialization remains 64K; applying a profile changes local limits but
 * never infers one from input stream fields.
 */
MARC_API marc_status marc_lzss_contextual_rans_config_apply_profile(
    marc_lzss_contextual_rans_config* config,
    marc_lzss_contextual_profile profile) MARC_NOEXCEPT;
MARC_API marc_status marc_lzss_contextual_rans_workspace_requirements(
    const marc_lzss_contextual_rans_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;
/*
 * Format 2 contextual rANS emits entropy variant 3's canonical variable
 * descriptor. Encoding stores tokens in aligned opaque views; decoding stores
 * rANS tables followed by tokens.
 */
MARC_API marc_status marc_lzss_contextual_rans_create(
    const marc_lzss_contextual_rans_config* config,
    marc_buffer primary_workspace,
    marc_buffer secondary_workspace,
    marc_buffer views_workspace,
    marc_transform** transform) MARC_NOEXCEPT;
MARC_API marc_status marc_lzss_contextual_tans_config_init(
    marc_direction direction,
    marc_lzss_contextual_tans_config* config) MARC_NOEXCEPT;
/*
 * Contextual tANS accepts the 64K, 1M, 4M, and 16M profiles. Initialization
 * remains 64K; applying a profile changes local limits but never infers one
 * from input stream fields.
 */
MARC_API marc_status marc_lzss_contextual_tans_config_apply_profile(
    marc_lzss_contextual_tans_config* config,
    marc_lzss_contextual_profile profile) MARC_NOEXCEPT;
MARC_API marc_status marc_lzss_contextual_tans_workspace_requirements(
    const marc_lzss_contextual_tans_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;
/*
 * Format 2 contextual tANS uses aligned opaque typed views. Encoding stores
 * typed LZSS tokens followed by tANS encode tables; decoding stores tANS
 * decode tables followed by typed tokens. These layouts remain C++-private.
 */
MARC_API marc_status marc_lzss_contextual_tans_create(
    const marc_lzss_contextual_tans_config* config,
    marc_buffer primary_workspace,
    marc_buffer secondary_workspace,
    marc_buffer views_workspace,
    marc_transform** transform) MARC_NOEXCEPT;
MARC_API marc_status marc_lzss_contextual_adaptive_huffman_config_init(
    marc_direction direction,
    marc_lzss_contextual_adaptive_huffman_config* config) MARC_NOEXCEPT;
/*
 * Contextual Adaptive Huffman accepts the 64K, 1M, 4M, 16M, and 64M profiles.
 * Initialization remains 64K; applying a profile changes local limits but
 * never infers one from input stream fields.
 */
MARC_API marc_status
marc_lzss_contextual_adaptive_huffman_config_apply_profile(
    marc_lzss_contextual_adaptive_huffman_config* config,
    marc_lzss_contextual_profile profile) MARC_NOEXCEPT;
MARC_API marc_status
marc_lzss_contextual_adaptive_huffman_workspace_requirements(
    const marc_lzss_contextual_adaptive_huffman_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;
/*
 * Contextual Adaptive Huffman encoder views contain typed LZSS tokens,
 * FGK nodes, then symbol indices. Decoder views contain nodes, symbols, then
 * tokens. All three typed layouts remain private to the C++ implementation.
 */
MARC_API marc_status marc_lzss_contextual_adaptive_huffman_create(
    const marc_lzss_contextual_adaptive_huffman_config* config,
    marc_buffer primary_workspace,
    marc_buffer secondary_workspace,
    marc_buffer views_workspace,
    marc_transform** transform) MARC_NOEXCEPT;
MARC_API marc_status marc_lzss_contextual_blocked_huffman_config_init(
    marc_direction direction,
    marc_lzss_contextual_blocked_huffman_config* config) MARC_NOEXCEPT;
/*
 * Contextual Blocked Huffman accepts the 64K, 1M, 4M, 16M, and 64M profiles.
 * Initialization remains 64K; applying a profile changes local limits but
 * never infers one from input stream fields.
 */
MARC_API marc_status
marc_lzss_contextual_blocked_huffman_config_apply_profile(
    marc_lzss_contextual_blocked_huffman_config* config,
    marc_lzss_contextual_profile profile) MARC_NOEXCEPT;
MARC_API marc_status
marc_lzss_contextual_blocked_huffman_workspace_requirements(
    const marc_lzss_contextual_blocked_huffman_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;
/*
 * Contextual Blocked Huffman stores typed LZSS tokens in encoder views.
 * Decoder views contain bounded Huffman tables followed by typed tokens.
 * Both layouts remain private to the C++ implementation.
 */
MARC_API marc_status marc_lzss_contextual_blocked_huffman_create(
    const marc_lzss_contextual_blocked_huffman_config* config,
    marc_buffer primary_workspace,
    marc_buffer secondary_workspace,
    marc_buffer views_workspace,
    marc_transform** transform) MARC_NOEXCEPT;
MARC_API marc_status marc_lzss_rans_config_init(
    marc_direction direction, marc_lzss_rans_config* config) MARC_NOEXCEPT;
MARC_API marc_status marc_lzss_rans_workspace_requirements(
    const marc_lzss_rans_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;
/*
 * secondary_workspace is partitioned internally into LZSS token staging and
 * frame storage. Decoding uses aligned views_workspace for rANS blocks.
 */
MARC_API marc_status marc_lzss_rans_create(
    const marc_lzss_rans_config* config,
    marc_buffer primary_workspace,
    marc_buffer secondary_workspace,
    marc_buffer views_workspace,
    marc_transform** transform) MARC_NOEXCEPT;
MARC_API marc_status marc_lzss_tans_config_init(
    marc_direction direction, marc_lzss_tans_config* config) MARC_NOEXCEPT;
MARC_API marc_status marc_lzss_tans_workspace_requirements(
    const marc_lzss_tans_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;
/*
 * Encoding partitions secondary_workspace into alignment allowance, private
 * match-finder storage, LZSS token staging, and frame storage. Decoding uses
 * aligned views_workspace for tANS blocks.
 */
MARC_API marc_status marc_lzss_tans_create(
    const marc_lzss_tans_config* config,
    marc_buffer primary_workspace,
    marc_buffer secondary_workspace,
    marc_buffer views_workspace,
    marc_transform** transform) MARC_NOEXCEPT;
MARC_API marc_status marc_lz78_config_init(
    marc_direction direction, marc_lz78_config* config) MARC_NOEXCEPT;
MARC_API marc_status marc_lz78_workspace_requirements(
    const marc_lz78_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;
/* LZ78 uses aligned views_workspace for its private phrase table. */
MARC_API marc_status marc_lz78_create(
    const marc_lz78_config* config,
    marc_buffer primary_workspace,
    marc_buffer secondary_workspace,
    marc_buffer views_workspace,
    marc_transform** transform) MARC_NOEXCEPT;
MARC_API marc_status marc_lz78_blocked_huffman_config_init(
    marc_direction direction, marc_lz78_blocked_huffman_config* config)
    MARC_NOEXCEPT;
MARC_API marc_status marc_lz78_blocked_huffman_workspace_requirements(
    const marc_lz78_blocked_huffman_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;
/*
 * secondary_workspace is partitioned internally into LZ78 token staging and
 * frame storage. Aligned views_workspace holds private encoder entries or the
 * decoder's entropy views and phrase entries.
 */
MARC_API marc_status marc_lz78_blocked_huffman_create(
    const marc_lz78_blocked_huffman_config* config,
    marc_buffer primary_workspace,
    marc_buffer secondary_workspace,
    marc_buffer views_workspace,
    marc_transform** transform) MARC_NOEXCEPT;
MARC_API marc_status marc_lz78_adaptive_huffman_config_init(
    marc_direction direction, marc_lz78_adaptive_huffman_config* config)
    MARC_NOEXCEPT;
MARC_API marc_status marc_lz78_adaptive_huffman_workspace_requirements(
    const marc_lz78_adaptive_huffman_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;
/*
 * secondary_workspace is partitioned into LZ78 token staging and frame/raw
 * storage. Aligned views_workspace holds opaque encoder or phrase entries.
 */
MARC_API marc_status marc_lz78_adaptive_huffman_create(
    const marc_lz78_adaptive_huffman_config* config,
    marc_buffer primary_workspace,
    marc_buffer secondary_workspace,
    marc_buffer views_workspace,
    marc_transform** transform) MARC_NOEXCEPT;
MARC_API marc_status marc_lz78_dynamic_range_config_init(
    marc_direction direction, marc_lz78_dynamic_range_config* config)
    MARC_NOEXCEPT;
MARC_API marc_status marc_lz78_dynamic_range_workspace_requirements(
    const marc_lz78_dynamic_range_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;
/*
 * secondary_workspace is partitioned into LZ78 token staging and frame/raw
 * storage. Aligned views_workspace holds opaque encoder or phrase entries.
 */
MARC_API marc_status marc_lz78_dynamic_range_create(
    const marc_lz78_dynamic_range_config* config,
    marc_buffer primary_workspace,
    marc_buffer secondary_workspace,
    marc_buffer views_workspace,
    marc_transform** transform) MARC_NOEXCEPT;
MARC_API marc_status marc_lz78_rans_config_init(
    marc_direction direction, marc_lz78_rans_config* config) MARC_NOEXCEPT;
MARC_API marc_status marc_lz78_rans_workspace_requirements(
    const marc_lz78_rans_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;
/*
 * secondary_workspace is partitioned into LZ78 token staging and frame/raw
 * storage. Aligned views_workspace holds opaque encoder entries or the
 * decoder's rANS block views and phrase entries.
 */
MARC_API marc_status marc_lz78_rans_create(
    const marc_lz78_rans_config* config,
    marc_buffer primary_workspace,
    marc_buffer secondary_workspace,
    marc_buffer views_workspace,
    marc_transform** transform) MARC_NOEXCEPT;
MARC_API marc_status marc_lz78_tans_config_init(
    marc_direction direction, marc_lz78_tans_config* config) MARC_NOEXCEPT;
MARC_API marc_status marc_lz78_tans_workspace_requirements(
    const marc_lz78_tans_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;
/*
 * secondary_workspace is partitioned into LZ78 token staging and frame/raw
 * storage. Aligned views_workspace holds opaque encoder entries or the
 * decoder's tANS block views and phrase entries.
 */
MARC_API marc_status marc_lz78_tans_create(
    const marc_lz78_tans_config* config,
    marc_buffer primary_workspace,
    marc_buffer secondary_workspace,
    marc_buffer views_workspace,
    marc_transform** transform) MARC_NOEXCEPT;
MARC_API marc_status marc_lzw_config_init(
    marc_direction direction, marc_lzw_config* config) MARC_NOEXCEPT;
MARC_API marc_status marc_lzw_workspace_requirements(
    const marc_lzw_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;
/* LZW uses aligned views_workspace for its private phrase table. */
MARC_API marc_status marc_lzw_create(
    const marc_lzw_config* config,
    marc_buffer primary_workspace,
    marc_buffer secondary_workspace,
    marc_buffer views_workspace,
    marc_transform** transform) MARC_NOEXCEPT;
MARC_API marc_status marc_lzw_blocked_huffman_config_init(
    marc_direction direction, marc_lzw_blocked_huffman_config* config)
    MARC_NOEXCEPT;
MARC_API marc_status marc_lzw_blocked_huffman_workspace_requirements(
    const marc_lzw_blocked_huffman_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;
/*
 * secondary_workspace is partitioned internally into LZW byte staging and
 * frame storage. Aligned views_workspace holds private encoder entries or the
 * decoder's entropy views and phrase entries.
 */
MARC_API marc_status marc_lzw_blocked_huffman_create(
    const marc_lzw_blocked_huffman_config* config,
    marc_buffer primary_workspace,
    marc_buffer secondary_workspace,
    marc_buffer views_workspace,
    marc_transform** transform) MARC_NOEXCEPT;
MARC_API marc_status marc_lzw_adaptive_huffman_config_init(
    marc_direction direction, marc_lzw_adaptive_huffman_config* config)
    MARC_NOEXCEPT;
MARC_API marc_status marc_lzw_adaptive_huffman_workspace_requirements(
    const marc_lzw_adaptive_huffman_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;
/*
 * secondary_workspace is partitioned into packed LZW staging and frame/raw
 * storage. Aligned views_workspace holds opaque encoder or phrase entries.
 */
MARC_API marc_status marc_lzw_adaptive_huffman_create(
    const marc_lzw_adaptive_huffman_config* config,
    marc_buffer primary_workspace,
    marc_buffer secondary_workspace,
    marc_buffer views_workspace,
    marc_transform** transform) MARC_NOEXCEPT;
MARC_API marc_status marc_lzw_dynamic_range_config_init(
    marc_direction direction, marc_lzw_dynamic_range_config* config)
    MARC_NOEXCEPT;
MARC_API marc_status marc_lzw_dynamic_range_workspace_requirements(
    const marc_lzw_dynamic_range_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;
/*
 * secondary_workspace is partitioned into packed LZW staging and frame/raw
 * storage. Aligned views_workspace holds opaque encoder or phrase entries.
 */
MARC_API marc_status marc_lzw_dynamic_range_create(
    const marc_lzw_dynamic_range_config* config,
    marc_buffer primary_workspace,
    marc_buffer secondary_workspace,
    marc_buffer views_workspace,
    marc_transform** transform) MARC_NOEXCEPT;
MARC_API marc_status marc_lzw_rans_config_init(
    marc_direction direction, marc_lzw_rans_config* config) MARC_NOEXCEPT;
MARC_API marc_status marc_lzw_rans_workspace_requirements(
    const marc_lzw_rans_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;
/*
 * secondary_workspace is partitioned into packed LZW staging and frame/raw
 * storage. Aligned views_workspace holds opaque encoder entries or the
 * decoder's rANS block views and LZW phrase entries.
 */
MARC_API marc_status marc_lzw_rans_create(
    const marc_lzw_rans_config* config,
    marc_buffer primary_workspace,
    marc_buffer secondary_workspace,
    marc_buffer views_workspace,
    marc_transform** transform) MARC_NOEXCEPT;
MARC_API marc_status marc_lzw_tans_config_init(
    marc_direction direction, marc_lzw_tans_config* config) MARC_NOEXCEPT;
MARC_API marc_status marc_lzw_tans_workspace_requirements(
    const marc_lzw_tans_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;
/*
 * secondary_workspace is partitioned into packed LZW staging and frame/raw
 * storage. Aligned views_workspace holds opaque encoder entries or the
 * decoder's tANS block views and LZW phrase entries.
 */
MARC_API marc_status marc_lzw_tans_create(
    const marc_lzw_tans_config* config,
    marc_buffer primary_workspace,
    marc_buffer secondary_workspace,
    marc_buffer views_workspace,
    marc_transform** transform) MARC_NOEXCEPT;

MARC_API marc_status marc_lzd_config_init(
    marc_direction direction, marc_lzd_config* config) MARC_NOEXCEPT;
MARC_API marc_status marc_lzd_workspace_requirements(
    const marc_lzd_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;
/*
 * LZD uses one aligned, opaque views_workspace. The decoder partitions it
 * internally into a phrase table and a bounded phrase-expansion stack.
 */
MARC_API marc_status marc_lzd_create(
    const marc_lzd_config* config,
    marc_buffer primary_workspace,
    marc_buffer secondary_workspace,
    marc_buffer views_workspace,
    marc_transform** transform) MARC_NOEXCEPT;
MARC_API marc_status marc_lzd_blocked_huffman_config_init(
    marc_direction direction, marc_lzd_blocked_huffman_config* config)
    MARC_NOEXCEPT;
MARC_API marc_status marc_lzd_blocked_huffman_workspace_requirements(
    const marc_lzd_blocked_huffman_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;
/*
 * secondary_workspace is partitioned internally into LZD token staging and
 * frame storage. Aligned views_workspace holds private encoder entries or the
 * decoder's entropy views, phrase entries, and phrase-expansion stack.
 */
MARC_API marc_status marc_lzd_blocked_huffman_create(
    const marc_lzd_blocked_huffman_config* config,
    marc_buffer primary_workspace,
    marc_buffer secondary_workspace,
    marc_buffer views_workspace,
    marc_transform** transform) MARC_NOEXCEPT;
MARC_API marc_status marc_lzd_adaptive_huffman_config_init(
    marc_direction direction, marc_lzd_adaptive_huffman_config* config)
    MARC_NOEXCEPT;
MARC_API marc_status marc_lzd_adaptive_huffman_workspace_requirements(
    const marc_lzd_adaptive_huffman_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;
/*
 * secondary_workspace is partitioned into canonical LZD token staging and
 * frame/raw storage. Aligned views_workspace holds opaque encoder entries or
 * the decoder's phrase entries and bounded phrase-expansion stack.
 */
MARC_API marc_status marc_lzd_adaptive_huffman_create(
    const marc_lzd_adaptive_huffman_config* config,
    marc_buffer primary_workspace,
    marc_buffer secondary_workspace,
    marc_buffer views_workspace,
    marc_transform** transform) MARC_NOEXCEPT;
MARC_API marc_status marc_lzd_dynamic_range_config_init(
    marc_direction direction, marc_lzd_dynamic_range_config* config)
    MARC_NOEXCEPT;
MARC_API marc_status marc_lzd_dynamic_range_workspace_requirements(
    const marc_lzd_dynamic_range_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;
/*
 * secondary_workspace is partitioned into canonical LZD token staging and
 * frame/raw storage. Aligned views_workspace holds opaque encoder entries or
 * the decoder's phrase entries and bounded phrase-expansion stack.
 */
MARC_API marc_status marc_lzd_dynamic_range_create(
    const marc_lzd_dynamic_range_config* config,
    marc_buffer primary_workspace,
    marc_buffer secondary_workspace,
    marc_buffer views_workspace,
    marc_transform** transform) MARC_NOEXCEPT;
MARC_API marc_status marc_lzd_rans_config_init(
    marc_direction direction, marc_lzd_rans_config* config) MARC_NOEXCEPT;
MARC_API marc_status marc_lzd_rans_workspace_requirements(
    const marc_lzd_rans_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;
/*
 * secondary_workspace is partitioned into canonical LZD token staging and
 * frame/raw storage. Aligned views_workspace holds opaque encoder entries or
 * decoder rANS block views, phrase entries, and bounded phrase expansion.
 */
MARC_API marc_status marc_lzd_rans_create(
    const marc_lzd_rans_config* config,
    marc_buffer primary_workspace,
    marc_buffer secondary_workspace,
    marc_buffer views_workspace,
    marc_transform** transform) MARC_NOEXCEPT;
MARC_API marc_status marc_lzd_tans_config_init(
    marc_direction direction, marc_lzd_tans_config* config) MARC_NOEXCEPT;
MARC_API marc_status marc_lzd_tans_workspace_requirements(
    const marc_lzd_tans_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;
/*
 * secondary_workspace is partitioned into canonical LZD token staging and
 * frame/raw storage. Aligned views_workspace holds opaque encoder entries or
 * decoder tANS block views, phrase entries, and bounded phrase expansion.
 */
MARC_API marc_status marc_lzd_tans_create(
    const marc_lzd_tans_config* config,
    marc_buffer primary_workspace,
    marc_buffer secondary_workspace,
    marc_buffer views_workspace,
    marc_transform** transform) MARC_NOEXCEPT;
MARC_API marc_status marc_lzmw_rans_config_init(
    marc_direction direction, marc_lzmw_rans_config* config) MARC_NOEXCEPT;
MARC_API marc_status marc_lzmw_rans_workspace_requirements(
    const marc_lzmw_rans_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;
/*
 * secondary_workspace is partitioned into canonical LZMW token staging and
 * frame/raw storage. Aligned views_workspace holds opaque encoder entries or
 * decoder rANS block views, phrase entries, and bounded phrase expansion.
 */
MARC_API marc_status marc_lzmw_rans_create(
    const marc_lzmw_rans_config* config,
    marc_buffer primary_workspace,
    marc_buffer secondary_workspace,
    marc_buffer views_workspace,
    marc_transform** transform) MARC_NOEXCEPT;
MARC_API marc_status marc_lzmw_tans_config_init(
    marc_direction direction, marc_lzmw_tans_config* config) MARC_NOEXCEPT;
MARC_API marc_status marc_lzmw_tans_workspace_requirements(
    const marc_lzmw_tans_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;
/*
 * secondary_workspace is partitioned into canonical LZMW token staging and
 * frame/raw storage. Aligned views_workspace holds opaque encoder entries or
 * decoder tANS block views, phrase entries, and bounded phrase expansion.
 */
MARC_API marc_status marc_lzmw_tans_create(
    const marc_lzmw_tans_config* config,
    marc_buffer primary_workspace,
    marc_buffer secondary_workspace,
    marc_buffer views_workspace,
    marc_transform** transform) MARC_NOEXCEPT;
MARC_API marc_status marc_lzmw_config_init(
    marc_direction direction, marc_lzmw_config* config) MARC_NOEXCEPT;
MARC_API marc_status marc_lzmw_workspace_requirements(
    const marc_lzmw_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;
/*
 * LZMW uses one aligned, opaque views_workspace. The decoder partitions it
 * internally into a phrase table and a bounded phrase-expansion stack.
 */
MARC_API marc_status marc_lzmw_create(
    const marc_lzmw_config* config,
    marc_buffer primary_workspace,
    marc_buffer secondary_workspace,
    marc_buffer views_workspace,
    marc_transform** transform) MARC_NOEXCEPT;
MARC_API marc_status marc_lzmw_blocked_huffman_config_init(
    marc_direction direction, marc_lzmw_blocked_huffman_config* config)
    MARC_NOEXCEPT;
MARC_API marc_status marc_lzmw_blocked_huffman_workspace_requirements(
    const marc_lzmw_blocked_huffman_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;
/*
 * secondary_workspace is partitioned internally into LZMW reference staging
 * and frame storage. Aligned views_workspace holds private encoder entries or
 * the decoder's entropy views, phrase entries, and phrase-expansion stack.
 */
MARC_API marc_status marc_lzmw_blocked_huffman_create(
    const marc_lzmw_blocked_huffman_config* config,
    marc_buffer primary_workspace,
    marc_buffer secondary_workspace,
    marc_buffer views_workspace,
    marc_transform** transform) MARC_NOEXCEPT;
MARC_API marc_status marc_lzmw_adaptive_huffman_config_init(
    marc_direction direction, marc_lzmw_adaptive_huffman_config* config)
    MARC_NOEXCEPT;
MARC_API marc_status marc_lzmw_adaptive_huffman_workspace_requirements(
    const marc_lzmw_adaptive_huffman_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;
/*
 * secondary_workspace is partitioned into canonical LZMW reference staging
 * and frame/raw storage. Aligned views_workspace holds opaque encoder entries
 * or the decoder's phrase entries and bounded phrase-expansion stack.
 */
MARC_API marc_status marc_lzmw_adaptive_huffman_create(
    const marc_lzmw_adaptive_huffman_config* config,
    marc_buffer primary_workspace,
    marc_buffer secondary_workspace,
    marc_buffer views_workspace,
    marc_transform** transform) MARC_NOEXCEPT;
MARC_API marc_status marc_lzmw_dynamic_range_config_init(
    marc_direction direction, marc_lzmw_dynamic_range_config* config)
    MARC_NOEXCEPT;
MARC_API marc_status marc_lzmw_dynamic_range_workspace_requirements(
    const marc_lzmw_dynamic_range_config* config,
    marc_workspace_requirements* requirements) MARC_NOEXCEPT;
/*
 * secondary_workspace is partitioned into canonical LZMW reference staging
 * and frame/raw storage. Aligned views_workspace holds opaque encoder entries
 * or the decoder's phrase entries and bounded phrase-expansion stack.
 */
MARC_API marc_status marc_lzmw_dynamic_range_create(
    const marc_lzmw_dynamic_range_config* config,
    marc_buffer primary_workspace,
    marc_buffer secondary_workspace,
    marc_buffer views_workspace,
    marc_transform** transform) MARC_NOEXCEPT;
MARC_API void marc_transform_destroy(marc_transform* transform) MARC_NOEXCEPT;
MARC_API marc_status marc_lzss_position_distance_dynamic_range_8m_decoder_config_init(
    marc_lzss_position_distance_dynamic_range_8m_decoder_config* config) MARC_NOEXCEPT;
/* Initialize result metadata before query; failure leaves result unchanged. */
MARC_API marc_status marc_lzss_position_distance_dynamic_range_8m_decoder_workspace_requirements(
    const marc_lzss_position_distance_dynamic_range_8m_decoder_config* config,
    marc_lzss_position_distance_dynamic_range_8m_decoder_requirements* requirements) MARC_NOEXCEPT;
/* All five full capacities remain borrowed until transform destruction. */
MARC_API marc_status marc_lzss_position_distance_dynamic_range_8m_create_decoder(
    const marc_lzss_position_distance_dynamic_range_8m_decoder_config* config,
    const marc_lzss_position_distance_dynamic_range_8m_decoder_buffers* buffers,
    marc_transform** transform) MARC_NOEXCEPT;
#define MARC_LZSS_POSITION_DISTANCE_16M_COMPACT_OWNING UINT32_C(1)
#define MARC_LZSS_POSITION_DISTANCE_16M_INITIAL_ONLY UINT32_C(1)
typedef struct marc_lzss_position_distance_dynamic_range_16m_config {
    uint32_t struct_size, abi_version, encoder_strategy, reserved;
    uint64_t original_size;
    uint32_t frame_size, reserved2;
    uint64_t max_total_output_size, max_frame_size, max_block_size;
    uint64_t max_compressed_payload_size, max_internal_buffered_bytes;
    uint64_t max_lz_distance, max_lz_match_length;
    uint64_t max_entropy_table_entries, max_range_model_total;
    uint64_t max_expansion_ratio, expansion_slack;
    uint64_t external_retained_bytes, input_capacity_bytes, output_capacity_bytes;
} marc_lzss_position_distance_dynamic_range_16m_config;
typedef struct marc_lzss_position_distance_dynamic_range_16m_resources {
    uint32_t struct_size, abi_version;
    uint64_t external_charge_bytes, fixed_bytes, initial_raw_bytes;
    uint64_t initial_index_entries, initial_bytes;
    uint32_t admission_scope, reserved;
} marc_lzss_position_distance_dynamic_range_16m_resources;

/* Five borrowed workspaces; opaque token storage, never a wire representation.
 * This decoder has its own configuration and immutable decode direction. */
#define MARC_LZSS_POSITION_DISTANCE_16M_CAPACITY_ONLY UINT32_C(1)
typedef struct marc_lzss_position_distance_dynamic_range_16m_decoder_config {
    uint32_t struct_size, abi_version, reserved, reserved2;
    uint64_t max_total_output_size, max_frame_size, max_block_size;
    uint64_t max_compressed_payload_size, max_internal_buffered_bytes;
    uint64_t max_lz_distance, max_lz_match_length;
    uint64_t max_entropy_table_entries, max_range_model_total;
    uint64_t max_expansion_ratio, expansion_slack;
    uint64_t external_retained_bytes, input_capacity_bytes, output_capacity_bytes;
} marc_lzss_position_distance_dynamic_range_16m_decoder_config;
typedef struct marc_lzss_position_distance_dynamic_range_16m_decoder_requirements {
    uint32_t struct_size, abi_version, admission_scope, reserved;
    uint64_t serialized_bytes, token_bytes, token_scratch_bytes;
    uint64_t raw_bytes, raw_scratch_bytes, token_alignment, token_elements;
    uint64_t minimum_aggregate_bytes;
} marc_lzss_position_distance_dynamic_range_16m_decoder_requirements;
typedef struct marc_lzss_position_distance_dynamic_range_16m_decoder_buffers {
    uint32_t struct_size, abi_version, reserved, reserved2;
    marc_buffer serialized, tokens, token_scratch, raw, raw_scratch;
} marc_lzss_position_distance_dynamic_range_16m_decoder_buffers;

/* Sixteen-MiB position-distance profile. Initializers set profile fields only;
 * callers explicitly supply remaining limits and full retained/call capacities.
 * Encoder admission queries cover initial storage; each frame is admitted again.
 * Decoder queries cover capacities, not validation of a serialized stream. */
MARC_API marc_status marc_lzss_position_distance_dynamic_range_16m_config_init(
    marc_lzss_position_distance_dynamic_range_16m_config* config) MARC_NOEXCEPT;
MARC_API marc_status marc_lzss_position_distance_dynamic_range_16m_resource_requirements(
    const marc_lzss_position_distance_dynamic_range_16m_config* config,
    marc_lzss_position_distance_dynamic_range_16m_resources* requirements) MARC_NOEXCEPT;
MARC_API marc_status marc_lzss_position_distance_dynamic_range_16m_create_encoder(
    const marc_lzss_position_distance_dynamic_range_16m_config* config,
    marc_transform** transform) MARC_NOEXCEPT;
MARC_API marc_status marc_lzss_position_distance_dynamic_range_16m_decoder_config_init(
    marc_lzss_position_distance_dynamic_range_16m_decoder_config* config) MARC_NOEXCEPT;
MARC_API marc_status marc_lzss_position_distance_dynamic_range_16m_decoder_workspace_requirements(
    const marc_lzss_position_distance_dynamic_range_16m_decoder_config* config,
    marc_lzss_position_distance_dynamic_range_16m_decoder_requirements* requirements) MARC_NOEXCEPT;
/* All five full capacities remain borrowed until transform destruction. */
MARC_API marc_status marc_lzss_position_distance_dynamic_range_16m_create_decoder(
    const marc_lzss_position_distance_dynamic_range_16m_decoder_config* config,
    const marc_lzss_position_distance_dynamic_range_16m_decoder_buffers* buffers,
    marc_transform** transform) MARC_NOEXCEPT;

/* BEGIN thirty-two-MiB public extension */
#define MARC_LZSS_POSITION_DISTANCE_32M_COMPACT_OWNING UINT32_C(1)
#define MARC_LZSS_POSITION_DISTANCE_32M_INITIAL_ONLY UINT32_C(1)
typedef struct marc_lzss_position_distance_dynamic_range_32m_config {
    uint32_t struct_size, abi_version, encoder_strategy, reserved;
    uint64_t original_size;
    uint32_t frame_size, reserved2;
    uint64_t max_total_output_size, max_frame_size, max_block_size;
    uint64_t max_compressed_payload_size, max_internal_buffered_bytes;
    uint64_t max_lz_distance, max_lz_match_length;
    uint64_t max_entropy_table_entries, max_range_model_total;
    uint64_t max_expansion_ratio, expansion_slack;
    uint64_t external_retained_bytes, input_capacity_bytes, output_capacity_bytes;
} marc_lzss_position_distance_dynamic_range_32m_config;
typedef struct marc_lzss_position_distance_dynamic_range_32m_resources {
    uint32_t struct_size, abi_version;
    uint64_t external_charge_bytes, fixed_bytes, initial_raw_bytes;
    uint64_t initial_index_entries, initial_bytes;
    uint32_t admission_scope, reserved;
} marc_lzss_position_distance_dynamic_range_32m_resources;

/* Five borrowed byte workspaces; compact records are private, never a wire representation.
 * Token alignment is one; each token buffer has at least 3*max_frame_bytes.
 * record_capacity_bytes is a byte capacity, not a typed token count.
 * This decoder has its own configuration and immutable decode direction. */
#define MARC_LZSS_POSITION_DISTANCE_32M_CAPACITY_ONLY UINT32_C(1)
typedef struct marc_lzss_position_distance_dynamic_range_32m_decoder_config {
    uint32_t struct_size, abi_version, reserved, reserved2;
    uint64_t max_total_output_size, max_frame_size, max_block_size;
    uint64_t max_compressed_payload_size, max_internal_buffered_bytes;
    uint64_t max_lz_distance, max_lz_match_length;
    uint64_t max_entropy_table_entries, max_range_model_total;
    uint64_t max_expansion_ratio, expansion_slack;
    uint64_t external_retained_bytes, input_capacity_bytes, output_capacity_bytes;
} marc_lzss_position_distance_dynamic_range_32m_decoder_config;
typedef struct marc_lzss_position_distance_dynamic_range_32m_decoder_requirements {
    uint32_t struct_size, abi_version, admission_scope, reserved;
    uint64_t serialized_bytes, token_bytes, token_scratch_bytes;
    uint64_t raw_bytes, raw_scratch_bytes, token_alignment, record_capacity_bytes;
    uint64_t minimum_aggregate_bytes;
} marc_lzss_position_distance_dynamic_range_32m_decoder_requirements;
typedef struct marc_lzss_position_distance_dynamic_range_32m_decoder_buffers {
    uint32_t struct_size, abi_version, reserved, reserved2;
    marc_buffer serialized, tokens, token_scratch, raw, raw_scratch;
} marc_lzss_position_distance_dynamic_range_32m_decoder_buffers;

/* Thirty-two-MiB position-distance profile. Initializers set profile fields only;
 * callers explicitly supply remaining limits and full retained/call capacities.
 * Encoder admission queries cover initial storage; each frame is admitted again.
 * Decoder queries cover capacities, not validation of a serialized stream. */
MARC_API marc_status marc_lzss_position_distance_dynamic_range_32m_config_init(
    marc_lzss_position_distance_dynamic_range_32m_config* config) MARC_NOEXCEPT;
MARC_API marc_status marc_lzss_position_distance_dynamic_range_32m_resource_requirements(
    const marc_lzss_position_distance_dynamic_range_32m_config* config,
    marc_lzss_position_distance_dynamic_range_32m_resources* requirements) MARC_NOEXCEPT;
MARC_API marc_status marc_lzss_position_distance_dynamic_range_32m_create_encoder(
    const marc_lzss_position_distance_dynamic_range_32m_config* config,
    marc_transform** transform) MARC_NOEXCEPT;
MARC_API marc_status marc_lzss_position_distance_dynamic_range_32m_decoder_config_init(
    marc_lzss_position_distance_dynamic_range_32m_decoder_config* config) MARC_NOEXCEPT;
MARC_API marc_status marc_lzss_position_distance_dynamic_range_32m_decoder_workspace_requirements(
    const marc_lzss_position_distance_dynamic_range_32m_decoder_config* config,
    marc_lzss_position_distance_dynamic_range_32m_decoder_requirements* requirements) MARC_NOEXCEPT;
/* All five full capacities remain borrowed until transform destruction. */
MARC_API marc_status marc_lzss_position_distance_dynamic_range_32m_create_decoder(
    const marc_lzss_position_distance_dynamic_range_32m_decoder_config* config,
    const marc_lzss_position_distance_dynamic_range_32m_decoder_buffers* buffers,
    marc_transform** transform) MARC_NOEXCEPT;

/* END thirty-two-MiB public extension */

/* BEGIN sixty-four-MiB public extension */
#define MARC_LZSS_POSITION_DISTANCE_64M_COMPACT_OWNING UINT32_C(1)
#define MARC_LZSS_POSITION_DISTANCE_64M_INITIAL_ONLY UINT32_C(1)
typedef struct marc_lzss_position_distance_dynamic_range_64m_config {
    uint32_t struct_size, abi_version, encoder_strategy, reserved;
    uint64_t original_size;
    uint32_t frame_size, reserved2;
    uint64_t max_total_output_size, max_frame_size, max_block_size;
    uint64_t max_compressed_payload_size, max_internal_buffered_bytes;
    uint64_t max_lz_distance, max_lz_match_length;
    uint64_t max_entropy_table_entries, max_range_model_total;
    uint64_t max_expansion_ratio, expansion_slack;
    uint64_t external_retained_bytes, input_capacity_bytes, output_capacity_bytes;
} marc_lzss_position_distance_dynamic_range_64m_config;
typedef struct marc_lzss_position_distance_dynamic_range_64m_resources {
    uint32_t struct_size, abi_version;
    uint64_t external_charge_bytes, fixed_bytes, initial_raw_bytes;
    uint64_t initial_index_entries, initial_bytes;
    uint32_t admission_scope, reserved;
} marc_lzss_position_distance_dynamic_range_64m_resources;

/* Five borrowed byte workspaces; compact records are private, never a wire representation.
 * Token alignment is one; each token buffer has at least 3*max_frame_bytes.
 * record_capacity_bytes is a byte capacity, not a typed token count.
 * This decoder has its own configuration and immutable decode direction. */
#define MARC_LZSS_POSITION_DISTANCE_64M_CAPACITY_ONLY UINT32_C(1)
typedef struct marc_lzss_position_distance_dynamic_range_64m_decoder_config {
    uint32_t struct_size, abi_version, reserved, reserved2;
    uint64_t max_total_output_size, max_frame_size, max_block_size;
    uint64_t max_compressed_payload_size, max_internal_buffered_bytes;
    uint64_t max_lz_distance, max_lz_match_length;
    uint64_t max_entropy_table_entries, max_range_model_total;
    uint64_t max_expansion_ratio, expansion_slack;
    uint64_t external_retained_bytes, input_capacity_bytes, output_capacity_bytes;
} marc_lzss_position_distance_dynamic_range_64m_decoder_config;
typedef struct marc_lzss_position_distance_dynamic_range_64m_decoder_requirements {
    uint32_t struct_size, abi_version, admission_scope, reserved;
    uint64_t serialized_bytes, token_bytes, token_scratch_bytes;
    uint64_t raw_bytes, raw_scratch_bytes, token_alignment, record_capacity_bytes;
    uint64_t minimum_aggregate_bytes;
} marc_lzss_position_distance_dynamic_range_64m_decoder_requirements;
typedef struct marc_lzss_position_distance_dynamic_range_64m_decoder_buffers {
    uint32_t struct_size, abi_version, reserved, reserved2;
    marc_buffer serialized, tokens, token_scratch, raw, raw_scratch;
} marc_lzss_position_distance_dynamic_range_64m_decoder_buffers;

/* Sixty-four-MiB position-distance profile. Initializers set profile fields only;
 * callers explicitly supply remaining limits and full retained/call capacities.
 * Encoder admission queries cover initial storage; each frame is admitted again.
 * Decoder queries cover capacities, not validation of a serialized stream. */
MARC_API marc_status marc_lzss_position_distance_dynamic_range_64m_config_init(
    marc_lzss_position_distance_dynamic_range_64m_config* config) MARC_NOEXCEPT;
MARC_API marc_status marc_lzss_position_distance_dynamic_range_64m_resource_requirements(
    const marc_lzss_position_distance_dynamic_range_64m_config* config,
    marc_lzss_position_distance_dynamic_range_64m_resources* requirements) MARC_NOEXCEPT;
MARC_API marc_status marc_lzss_position_distance_dynamic_range_64m_create_encoder(
    const marc_lzss_position_distance_dynamic_range_64m_config* config,
    marc_transform** transform) MARC_NOEXCEPT;
MARC_API marc_status marc_lzss_position_distance_dynamic_range_64m_decoder_config_init(
    marc_lzss_position_distance_dynamic_range_64m_decoder_config* config) MARC_NOEXCEPT;
MARC_API marc_status marc_lzss_position_distance_dynamic_range_64m_decoder_workspace_requirements(
    const marc_lzss_position_distance_dynamic_range_64m_decoder_config* config,
    marc_lzss_position_distance_dynamic_range_64m_decoder_requirements* requirements) MARC_NOEXCEPT;
/* All five full capacities remain borrowed until transform destruction. */
MARC_API marc_status marc_lzss_position_distance_dynamic_range_64m_create_decoder(
    const marc_lzss_position_distance_dynamic_range_64m_decoder_config* config,
    const marc_lzss_position_distance_dynamic_range_64m_decoder_buffers* buffers,
    marc_transform** transform) MARC_NOEXCEPT;

/* END sixty-four-MiB public extension */

/* Owning position-distance rANS, dictionary 2/9, context 1/10, entropy 4/4.
 * Fixed-five encoder; decoder accepts match lengths 3..258.
 * Resource queries cover capacity budgets, not operating-system RSS. */
typedef struct marc_lzss_position_rans_1m_config {
    uint32_t struct_size, abi_version;
    marc_direction direction;
    uint32_t reserved;
    uint64_t original_size;
    uint32_t frame_size, reserved2;
    uint64_t max_total_output_size, max_frame_size, max_block_size;
    uint64_t max_compressed_payload_size, max_internal_buffered_bytes;
    uint64_t max_lz_distance, max_lz_match_length, max_entropy_table_entries;
    uint64_t max_expansion_ratio, expansion_slack;
    uint64_t external_retained_bytes, input_capacity_bytes, output_capacity_bytes;
} marc_lzss_position_rans_1m_config;
typedef struct marc_lzss_position_rans_1m_resources {
    uint32_t struct_size, abi_version;
    uint64_t raw_bytes, token_bytes, serialized_bytes, finder_bytes;
    uint64_t fixed_working_bytes, external_charge_bytes, minimum_aggregate_bytes;
} marc_lzss_position_rans_1m_resources;
MARC_API marc_status marc_lzss_position_rans_1m_config_init(
    marc_direction direction, marc_lzss_position_rans_1m_config* config) MARC_NOEXCEPT;
MARC_API marc_status marc_lzss_position_rans_1m_resource_requirements(
    const marc_lzss_position_rans_1m_config* config,
    marc_lzss_position_rans_1m_resources* resources) MARC_NOEXCEPT;
MARC_API marc_status marc_lzss_position_rans_1m_create(
    const marc_lzss_position_rans_1m_config* config, marc_transform** transform) MARC_NOEXCEPT;

/* Native64KiB position-distance rANS (2/8 + 1/9 + 4/5).
 * Fixed-five encoder; decoder accepts match lengths3..258.
 * Queries account capacities and caller buffers, not OS RSS. */
typedef struct marc_lzss_position_distance_rans_config {
    uint32_t struct_size, abi_version;
    marc_direction direction;
    uint32_t reserved;
    uint64_t original_size;
    uint32_t frame_size, reserved2;
    uint64_t max_total_output_size, max_frame_size, max_block_size;
    uint64_t max_compressed_payload_size, max_internal_buffered_bytes;
    uint64_t max_lz_distance, max_lz_match_length, max_entropy_table_entries;
    uint64_t max_expansion_ratio, expansion_slack;
    uint64_t external_retained_bytes, input_capacity_bytes, output_capacity_bytes;
} marc_lzss_position_distance_rans_config;
typedef struct marc_lzss_position_distance_rans_resources {
    uint32_t struct_size, abi_version;
    uint64_t raw_bytes, token_bytes, serialized_bytes, finder_bytes;
    uint64_t fixed_working_bytes, external_charge_bytes, minimum_aggregate_bytes;
} marc_lzss_position_distance_rans_resources;
MARC_API marc_status marc_lzss_position_distance_rans_config_init(
    marc_direction direction, marc_lzss_position_distance_rans_config* config) MARC_NOEXCEPT;
MARC_API marc_status marc_lzss_position_distance_rans_resource_requirements(
    const marc_lzss_position_distance_rans_config* config,
    marc_lzss_position_distance_rans_resources* resources) MARC_NOEXCEPT;
MARC_API marc_status marc_lzss_position_distance_rans_create(
    const marc_lzss_position_distance_rans_config* config, marc_transform** transform) MARC_NOEXCEPT;

/* Native4MiB position-distance rANS (2/10 + 1/16 + 4/7).
 * Fixed-five encoder; decoder accepts match lengths3..258.
 * Queries account capacities and caller buffers, not OS RSS. */
typedef struct marc_lzss_position_distance_rans_4m_config {
    uint32_t struct_size, abi_version;
    marc_direction direction;
    uint32_t reserved;
    uint64_t original_size;
    uint32_t frame_size, reserved2;
    uint64_t max_total_output_size, max_frame_size, max_block_size;
    uint64_t max_compressed_payload_size, max_internal_buffered_bytes;
    uint64_t max_lz_distance, max_lz_match_length, max_entropy_table_entries;
    uint64_t max_expansion_ratio, expansion_slack;
    uint64_t external_retained_bytes, input_capacity_bytes, output_capacity_bytes;
} marc_lzss_position_distance_rans_4m_config;
typedef struct marc_lzss_position_distance_rans_4m_resources {
    uint32_t struct_size, abi_version;
    uint64_t raw_bytes, token_bytes, serialized_bytes, finder_bytes;
    uint64_t fixed_working_bytes, external_charge_bytes, minimum_aggregate_bytes;
} marc_lzss_position_distance_rans_4m_resources;
MARC_API marc_status marc_lzss_position_distance_rans_4m_config_init(
    marc_direction direction, marc_lzss_position_distance_rans_4m_config* config) MARC_NOEXCEPT;
MARC_API marc_status marc_lzss_position_distance_rans_4m_resource_requirements(
    const marc_lzss_position_distance_rans_4m_config* config,
    marc_lzss_position_distance_rans_4m_resources* resources) MARC_NOEXCEPT;
MARC_API marc_status marc_lzss_position_distance_rans_4m_create(
    const marc_lzss_position_distance_rans_4m_config* config, marc_transform** transform) MARC_NOEXCEPT;

MARC_API marc_process_result marc_transform_process(
    marc_transform* transform, marc_const_buffer input, marc_buffer output,
    marc_process_flags flags) MARC_NOEXCEPT;

#ifdef __cplusplus
}
#endif

#undef MARC_NOEXCEPT

#endif

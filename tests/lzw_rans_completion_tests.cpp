#include <marc/marc.h>

// Reuse the LZW public-ABI admission matrix with only the rANS block ceiling,
// additional block configuration, and public symbol family changed. Keeping
// all data, chunking, terminal, and malformed schedules identical prevents
// evidence drift between the LZW compositions.
#define MARC_LZW_COMPLETION_MAXIMUM_PAYLOAD(dictionary_size) \
    ((dictionary_size) + (((dictionary_size) + 63) / 64) * 8)
#define MARC_LZW_COMPLETION_MAXIMUM_FRAME(dictionary_size) \
    (56 + (((dictionary_size) + 63) / 64) * 528 \
     + MARC_LZW_COMPLETION_MAXIMUM_PAYLOAD(dictionary_size))
#define MARC_LZW_COMPLETION_CONFIGURE_PROFILE(config) \
    do { \
        (config).entropy_block_size = 64; \
        (config).max_block_size = 64; \
        (config).max_blocks_per_frame = 2; \
    } while (false)
#define marc_lzw_adaptive_huffman_config marc_lzw_rans_config
#define marc_lzw_adaptive_huffman_config_init marc_lzw_rans_config_init
#define marc_lzw_adaptive_huffman_workspace_requirements \
    marc_lzw_rans_workspace_requirements
#define marc_lzw_adaptive_huffman_create marc_lzw_rans_create
#define LzwAdaptiveHuffmanCompletion LzwRansCompletion

#include "lzw_adaptive_huffman_completion_tests.cpp"

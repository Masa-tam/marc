#include <marc/marc.h>

// Reuse the reviewed LZD public-ABI admission matrix with only the public
// symbol family changed. At the 64-byte test frame size, LZMW's 4F reference
// ceiling and LZD's 8*ceil(F/2) token ceiling are both exactly 256 bytes, so
// the same four-block rANS capacity schedule applies without approximation.
#define MARC_LZD_COMPLETION_MAXIMUM_PAYLOAD(dictionary_size) \
    ((dictionary_size) + (((dictionary_size) + 63) / 64) * 8)
#define MARC_LZD_COMPLETION_MAXIMUM_FRAME(dictionary_size) \
    (56 + (((dictionary_size) + 63) / 64) * 528 \
     + MARC_LZD_COMPLETION_MAXIMUM_PAYLOAD(dictionary_size))
#define MARC_LZD_COMPLETION_CONFIGURE_PROFILE(config) \
    do { \
        (config).entropy_block_size = 64; \
        (config).max_block_size = 64; \
        (config).max_blocks_per_frame = 4; \
    } while (false)
#define marc_lzd_adaptive_huffman_config marc_lzmw_rans_config
#define marc_lzd_adaptive_huffman_config_init marc_lzmw_rans_config_init
#define marc_lzd_adaptive_huffman_workspace_requirements \
    marc_lzmw_rans_workspace_requirements
#define marc_lzd_adaptive_huffman_create marc_lzmw_rans_create
#define LzdAdaptiveHuffmanCompletion LzmwRansCompletion

#include "lzd_adaptive_huffman_completion_tests.cpp"

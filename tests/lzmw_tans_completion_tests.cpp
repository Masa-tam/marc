#include <marc/marc.h>

// Reuse the reviewed LZD public-ABI admission matrix with only the tANS block
// ceiling, additional block configuration, and public LZMW symbol family
// changed. At the 64-byte frame size, LZMW's 4F reference ceiling and LZD's
// 8*ceil(F/2) ceiling are both exactly 256 bytes, so every data, chunking,
// terminal, and malformed schedule remains identical without approximation.
#define MARC_LZD_COMPLETION_MAXIMUM_PAYLOAD(dictionary_size) \
    ((((dictionary_size) * 12 + 7) / 8) \
     + (((dictionary_size) + 63) / 64) * 2)
#define MARC_LZD_COMPLETION_MAXIMUM_FRAME(dictionary_size) \
    (56 + (((dictionary_size) + 63) / 64) * 528 \
     + MARC_LZD_COMPLETION_MAXIMUM_PAYLOAD(dictionary_size))
#define MARC_LZD_COMPLETION_CONFIGURE_PROFILE(config) \
    do { \
        (config).entropy_block_size = 64; \
        (config).max_block_size = 64; \
        (config).max_blocks_per_frame = 4; \
    } while (false)
#define marc_lzd_adaptive_huffman_config marc_lzmw_tans_config
#define marc_lzd_adaptive_huffman_config_init marc_lzmw_tans_config_init
#define marc_lzd_adaptive_huffman_workspace_requirements \
    marc_lzmw_tans_workspace_requirements
#define marc_lzd_adaptive_huffman_create marc_lzmw_tans_create
#define LzdAdaptiveHuffmanCompletion LzmwTansCompletion

#include "lzd_adaptive_huffman_completion_tests.cpp"

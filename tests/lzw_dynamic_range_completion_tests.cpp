#include <marc/marc.h>

// Reuse the LZW public-ABI admission matrix with only the fixed entropy
// profile, payload bound, and public symbol family changed. Keeping the
// schedules and malformed-frame assertions identical prevents evidence drift
// between the two LZW compositions.
#define MARC_LZW_COMPLETION_MAXIMUM_PAYLOAD(dictionary_size) \
    ((dictionary_size) * 2 + 5)
#define marc_lzw_adaptive_huffman_config marc_lzw_dynamic_range_config
#define marc_lzw_adaptive_huffman_config_init \
    marc_lzw_dynamic_range_config_init
#define marc_lzw_adaptive_huffman_workspace_requirements \
    marc_lzw_dynamic_range_workspace_requirements
#define marc_lzw_adaptive_huffman_create marc_lzw_dynamic_range_create
#define LzwAdaptiveHuffmanCompletion LzwDynamicRangeCompletion

#include "lzw_adaptive_huffman_completion_tests.cpp"

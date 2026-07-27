#include <marc/marc.h>

// Reuse the LZD public-ABI admission matrix with only the fixed entropy
// profile, payload bound, and public symbol family changed. Keeping the
// schedules and malformed-frame assertions identical prevents evidence drift
// between the two LZD compositions.
#define MARC_LZD_COMPLETION_MAXIMUM_PAYLOAD(dictionary_size) \
    ((dictionary_size) * 2 + 5)
#define marc_lzd_adaptive_huffman_config marc_lzd_dynamic_range_config
#define marc_lzd_adaptive_huffman_config_init \
    marc_lzd_dynamic_range_config_init
#define marc_lzd_adaptive_huffman_workspace_requirements \
    marc_lzd_dynamic_range_workspace_requirements
#define marc_lzd_adaptive_huffman_create marc_lzd_dynamic_range_create
#define LzdAdaptiveHuffmanCompletion LzdDynamicRangeCompletion

#include "lzd_adaptive_huffman_completion_tests.cpp"

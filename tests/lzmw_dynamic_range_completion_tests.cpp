#include <marc/marc.h>

// Reuse the LZMW public-ABI admission matrix with only the fixed entropy
// profile, payload bound, and public symbol family changed. Keeping the
// schedules and malformed-frame assertions identical prevents evidence drift
// between the two LZMW compositions.
#define MARC_LZMW_COMPLETION_MAXIMUM_PAYLOAD(dictionary_size) \
    ((dictionary_size) * 2 + 5)
#define marc_lzmw_adaptive_huffman_config marc_lzmw_dynamic_range_config
#define marc_lzmw_adaptive_huffman_config_init \
    marc_lzmw_dynamic_range_config_init
#define marc_lzmw_adaptive_huffman_workspace_requirements \
    marc_lzmw_dynamic_range_workspace_requirements
#define marc_lzmw_adaptive_huffman_create marc_lzmw_dynamic_range_create
#define LzmwAdaptiveHuffmanCompletion LzmwDynamicRangeCompletion

#include "lzmw_adaptive_huffman_completion_tests.cpp"

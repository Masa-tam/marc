#include "frame/lzmw_adaptive_huffman_frame_streaming_decoder.hpp"
#include "frame/lzmw_adaptive_huffman_frame_streaming_encoder.hpp"
#include "frame/lzmw_dynamic_range_frame_streaming_decoder.hpp"
#include "frame/lzmw_dynamic_range_frame_streaming_encoder.hpp"

// Keep permanent malformed-stream assertions synchronized across the two LZMW
// entropy compositions while selecting the Dynamic Range symbols explicitly.
#define adaptive_huffman dynamic_range
#define adaptive_huffman_descriptor_size dynamic_range_descriptor_size
#define lzmw_adaptive_huffman_stream_prefix_size \
    lzmw_dynamic_range_stream_prefix_size
#define LzmwAdaptiveHuffmanFrameStreamingDecoder \
    LzmwDynamicRangeFrameStreamingDecoder
#define LzmwAdaptiveHuffmanFrameStreamingEncoder \
    LzmwDynamicRangeFrameStreamingEncoder
#define LzmwAdaptiveHuffmanFuzzRegression LzmwDynamicRangeFuzzRegression

#include "lzmw_adaptive_huffman_fuzz_regression_tests.cpp"

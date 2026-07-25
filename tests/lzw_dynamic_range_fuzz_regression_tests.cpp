#include "frame/lzw_adaptive_huffman_frame_streaming_decoder.hpp"
#include "frame/lzw_adaptive_huffman_frame_streaming_encoder.hpp"
#include "frame/lzw_dynamic_range_frame_streaming_decoder.hpp"
#include "frame/lzw_dynamic_range_frame_streaming_encoder.hpp"

// Keep permanent malformed-stream assertions synchronized across the two LZW
// entropy compositions while selecting the Dynamic Range symbols explicitly.
#define adaptive_huffman dynamic_range
#define adaptive_huffman_descriptor_size dynamic_range_descriptor_size
#define lzw_adaptive_huffman_stream_prefix_size \
    lzw_dynamic_range_stream_prefix_size
#define LzwAdaptiveHuffmanFrameStreamingDecoder \
    LzwDynamicRangeFrameStreamingDecoder
#define LzwAdaptiveHuffmanFrameStreamingEncoder \
    LzwDynamicRangeFrameStreamingEncoder
#define LzwAdaptiveHuffmanFuzzRegression LzwDynamicRangeFuzzRegression

#include "lzw_adaptive_huffman_fuzz_regression_tests.cpp"

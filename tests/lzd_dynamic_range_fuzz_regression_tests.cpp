#include "frame/lzd_adaptive_huffman_frame_streaming_decoder.hpp"
#include "frame/lzd_adaptive_huffman_frame_streaming_encoder.hpp"
#include "frame/lzd_dynamic_range_frame_streaming_decoder.hpp"
#include "frame/lzd_dynamic_range_frame_streaming_encoder.hpp"

// Keep permanent malformed-stream assertions synchronized across the two LZD
// entropy compositions while selecting the Dynamic Range symbols explicitly.
#define adaptive_huffman dynamic_range
#define adaptive_huffman_descriptor_size dynamic_range_descriptor_size
#define lzd_adaptive_huffman_stream_prefix_size \
    lzd_dynamic_range_stream_prefix_size
#define LzdAdaptiveHuffmanFrameStreamingDecoder \
    LzdDynamicRangeFrameStreamingDecoder
#define LzdAdaptiveHuffmanFrameStreamingEncoder \
    LzdDynamicRangeFrameStreamingEncoder
#define LzdAdaptiveHuffmanFuzzRegression LzdDynamicRangeFuzzRegression

#include "lzd_adaptive_huffman_fuzz_regression_tests.cpp"

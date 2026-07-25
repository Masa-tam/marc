#include "frame/lzw_adaptive_huffman_frame.hpp"
#include "frame/lzw_adaptive_huffman_frame_streaming_decoder.hpp"
#include "frame/lzw_dynamic_range_frame.hpp"
#include "frame/lzw_dynamic_range_frame_streaming_decoder.hpp"

// Instantiate the common bounded LZW dual-decoder harness for the fixed
// Dynamic Range profile. All storage ceilings and the finite call budget stay
// identical; only the entropy identity and combined frame entry points change.
#define adaptive_huffman dynamic_range
#define adaptive_huffman_descriptor_size dynamic_range_descriptor_size
#define lzw_adaptive_huffman_stream_prefix_size \
    lzw_dynamic_range_stream_prefix_size
#define decode_lzw_adaptive_huffman_frame_to_staging \
    decode_lzw_dynamic_range_frame_to_staging
#define LzwAdaptiveHuffmanFrameStreamingDecoder \
    LzwDynamicRangeFrameStreamingDecoder

#include "lzw_adaptive_huffman_stream_fuzz.cpp"

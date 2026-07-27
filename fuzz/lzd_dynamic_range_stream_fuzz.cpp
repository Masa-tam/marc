#include "frame/lzd_adaptive_huffman_frame.hpp"
#include "frame/lzd_adaptive_huffman_frame_streaming_decoder.hpp"
#include "frame/lzd_dynamic_range_frame.hpp"
#include "frame/lzd_dynamic_range_frame_streaming_decoder.hpp"

// Instantiate the common bounded LZD dual-decoder harness for the fixed
// Dynamic Range profile. All storage ceilings and the finite call budget stay
// identical; only the entropy identity and combined frame entry points change.
#define adaptive_huffman dynamic_range
#define adaptive_huffman_descriptor_size dynamic_range_descriptor_size
#define lzd_adaptive_huffman_stream_prefix_size \
    lzd_dynamic_range_stream_prefix_size
#define decode_lzd_adaptive_huffman_frame_to_staging \
    decode_lzd_dynamic_range_frame_to_staging
#define LzdAdaptiveHuffmanFrameStreamingDecoder \
    LzdDynamicRangeFrameStreamingDecoder

#include "lzd_adaptive_huffman_stream_fuzz.cpp"

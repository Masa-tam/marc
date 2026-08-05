#include "frame/lzd_rans_frame_streaming_decoder.hpp"
#include "frame/lzd_rans_frame_streaming_encoder.hpp"
#include "frame/lzd_tans_frame_streaming_decoder.hpp"
#include "frame/lzd_tans_frame_streaming_encoder.hpp"

#define rans tans
#define RansBlockView TansBlockView
#define LzdRansFrameStreamingEncoder LzdTansFrameStreamingEncoder
#define LzdRansFrameStreamingDecoder LzdTansFrameStreamingDecoder
#define lzd_rans_stream_prefix_size lzd_tans_stream_prefix_size
#define LzdRansFuzzRegression LzdTansFuzzRegression
#define NonzeroDescriptorReservedByteIsAtomic InvalidDescriptorIsAtomic

#include "lzd_rans_fuzz_regression_tests.cpp"

#undef NonzeroDescriptorReservedByteIsAtomic
#undef LzdRansFuzzRegression
#undef lzd_rans_stream_prefix_size
#undef LzdRansFrameStreamingDecoder
#undef LzdRansFrameStreamingEncoder
#undef RansBlockView
#undef rans

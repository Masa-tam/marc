#include <marc/marc.h>

#include "frame/lzd_rans_frame.hpp"
#include "frame/lzd_rans_frame_streaming_encoder.hpp"
#include "frame/lzd_tans_frame.hpp"
#include "frame/lzd_tans_frame_streaming_encoder.hpp"

#define rans tans
#define RansBlockView TansBlockView
#define rans_descriptor_size tans_descriptor_size
#define lzd_rans_stream_prefix_size lzd_tans_stream_prefix_size
#define decode_lzd_rans_frame_to_staging decode_lzd_tans_frame_to_staging
#define marc_lzd_rans_config marc_lzd_tans_config
#define marc_lzd_rans_config_init marc_lzd_tans_config_init
#define marc_lzd_rans_workspace_requirements marc_lzd_tans_workspace_requirements
#define marc_lzd_rans_create marc_lzd_tans_create

#include "lzd_rans_stream_fuzz.cpp"

#undef marc_lzd_rans_create
#undef marc_lzd_rans_workspace_requirements
#undef marc_lzd_rans_config_init
#undef marc_lzd_rans_config
#undef decode_lzd_rans_frame_to_staging
#undef lzd_rans_stream_prefix_size
#undef rans_descriptor_size
#undef RansBlockView
#undef rans

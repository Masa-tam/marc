#include <marc/marc.h>

#include "frame/lzmw_rans_frame.hpp"
#include "frame/lzmw_rans_frame_streaming_encoder.hpp"
#include "frame/lzmw_tans_frame.hpp"
#include "frame/lzmw_tans_frame_streaming_encoder.hpp"

#define rans tans
#define RansBlockView TansBlockView
#define rans_descriptor_size tans_descriptor_size
#define lzmw_rans_stream_prefix_size lzmw_tans_stream_prefix_size
#define decode_lzmw_rans_frame_to_staging decode_lzmw_tans_frame_to_staging
#define marc_lzmw_rans_config marc_lzmw_tans_config
#define marc_lzmw_rans_config_init marc_lzmw_tans_config_init
#define marc_lzmw_rans_workspace_requirements \
    marc_lzmw_tans_workspace_requirements
#define marc_lzmw_rans_create marc_lzmw_tans_create

#include "lzmw_rans_stream_fuzz.cpp"

#undef marc_lzmw_rans_create
#undef marc_lzmw_rans_workspace_requirements
#undef marc_lzmw_rans_config_init
#undef marc_lzmw_rans_config
#undef decode_lzmw_rans_frame_to_staging
#undef lzmw_rans_stream_prefix_size
#undef rans_descriptor_size
#undef RansBlockView
#undef rans

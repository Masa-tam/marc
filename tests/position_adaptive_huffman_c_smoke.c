#include <marc/marc.h>
#include <string.h>
static marc_lzss_position_distance_adaptive_huffman_profile selected_profile=MARC_LZSS_POSITION_DISTANCE_ADAPTIVE_HUFFMAN_PROFILE_64K;

static int run(marc_direction direction,const unsigned char* input,size_t size,
    unsigned char* output,size_t capacity,size_t* written,int expect_failure) {
    marc_lzss_position_distance_adaptive_huffman_config config;
    marc_lzss_position_distance_adaptive_huffman_resources resources;
    marc_transform* transform=NULL;
    size_t consumed=0,calls;
    marc_status status;
    if (marc_lzss_position_distance_adaptive_huffman_config_init(direction,&config)) return 1;
    if (config.profile!=MARC_LZSS_POSITION_DISTANCE_ADAPTIVE_HUFFMAN_PROFILE_64K
        || marc_lzss_position_distance_adaptive_huffman_config_apply_profile(&config,selected_profile)) return 1;
    config.frame_size=17;config.input_capacity_bytes=config.output_capacity_bytes=1;
    config.original_size=direction==MARC_DIRECTION_ENCODE ? size : 0;
    if (marc_lzss_position_distance_adaptive_huffman_resource_requirements(&config,&resources)) return 2;
    config.max_internal_buffered_bytes=resources.minimum_aggregate_bytes;
    if (marc_lzss_position_distance_adaptive_huffman_create(&config,&transform)) return 3;
    *written=0;
    for (calls=0;calls<400000;++calls) {
        unsigned char guarded[3]={0x61,0x61,0x61};
        const size_t n=consumed<size ? 1 : 0;
        marc_const_buffer source={input ? input+consumed : NULL,n};
        marc_buffer destination={guarded+1,1};
        const marc_process_result r=marc_transform_process(transform,source,destination,
            consumed+n==size ? MARC_PROCESS_END_INPUT : MARC_PROCESS_FLUSH);
        if (r.input_consumed>n || r.output_produced>1 || guarded[0]!=0x61 || guarded[2]!=0x61
            || (!r.output_produced && guarded[1]!=0x61)) {marc_transform_destroy(transform);return 4;}
        consumed+=r.input_consumed;
        if (r.output_produced) {
            if (*written==capacity) {marc_transform_destroy(transform);return 5;}
            output[(*written)++]=guarded[1];
        }
        status=r.status;
        if (status==MARC_STATUS_END_OF_STREAM || status>=100) {
            marc_const_buffer empty={NULL,0};marc_buffer sink={NULL,0};
            const marc_process_result stable=marc_transform_process(transform,empty,sink,0);
            marc_transform_destroy(transform);
            if (stable.status!=status || stable.input_consumed || stable.output_produced) return 6;
            if (expect_failure) return status==MARC_STATUS_MALFORMED_STREAM && !r.output_produced ? 0 : 7;
            return status==MARC_STATUS_END_OF_STREAM && consumed==size ? 0 : 8;
        }
        if (status==MARC_STATUS_PROGRESS && !r.input_consumed && !r.output_produced) {marc_transform_destroy(transform);return 9;}
    }
    marc_transform_destroy(transform);return 10;
}
int main(int argc,char** argv) {
    unsigned char raw[256],wire[4096],restored[256];size_t encoded=0,decoded=0,i;
    if (argc==2 && strcmp(argv[1],"1m")==0) selected_profile=MARC_LZSS_POSITION_DISTANCE_ADAPTIVE_HUFFMAN_PROFILE_1M;
    else if (argc==2 && strcmp(argv[1],"4m")==0) selected_profile=MARC_LZSS_POSITION_DISTANCE_ADAPTIVE_HUFFMAN_PROFILE_4M;
    else if (argc==2 && strcmp(argv[1],"8m")==0) selected_profile=MARC_LZSS_POSITION_DISTANCE_ADAPTIVE_HUFFMAN_PROFILE_8M;
    else if (argc==2 && strcmp(argv[1],"16m")==0) selected_profile=MARC_LZSS_POSITION_DISTANCE_ADAPTIVE_HUFFMAN_PROFILE_16M;
    else if (argc!=1) return 11;
    if (run(MARC_DIRECTION_ENCODE,NULL,0,wire,sizeof(wire),&encoded,0) || encoded!=112) return 1;
    if (memcmp(wire,"MARC",4) || wire[12]!=2 || wire[14]!=8+selected_profile || wire[16]!=1
        || wire[18]!=3+selected_profile || wire[96]!=1 || wire[98]!=35+selected_profile) return 2;
    for (i=0;i<256;++i) raw[i]=(unsigned char)i;
    if (run(MARC_DIRECTION_ENCODE,raw,sizeof(raw),wire,sizeof(wire),&encoded,0)) return 3;
    if (run(MARC_DIRECTION_DECODE,wire,encoded,restored,sizeof(restored),&decoded,0) || decoded!=sizeof(raw)
        || memcmp(raw,restored,sizeof(raw))) return 4;
    wire[encoded-1]|=0x80;
    if (run(MARC_DIRECTION_DECODE,wire,encoded,restored,sizeof(restored),&decoded,1) || decoded!=255) return 5;
    return 0;
}

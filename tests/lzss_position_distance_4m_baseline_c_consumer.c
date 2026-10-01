#include "frame/lzss_position_distance_4m_baseline_c_adapter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int transform(marc_direction direction,const uint8_t* input,size_t count,
                     uint8_t* output,size_t capacity,size_t* written,marc_status expected) {
    marc_private_position_distance_4m_config config;
    marc_workspace_requirements requirements;
    marc_transform* handle=NULL;
    uint8_t *primary=NULL,*secondary=NULL,*views=NULL;
    size_t consumed=0,produced=0,call,i;
    int ok=0,terminal=0;
    if(marc_private_position_distance_4m_baseline_config_init(direction,&config)!=MARC_STATUS_OK)return 0;
    config.original_size=count;config.frame_size=21;
    config.max_frame_size=config.max_block_size=21;
    config.max_compressed_payload_size=383;config.max_internal_buffered_bytes=2U<<20;
    if(marc_private_position_distance_4m_baseline_workspace_requirements(&config,&requirements)!=MARC_STATUS_OK)return 0;
    primary=(uint8_t*)malloc(requirements.primary_bytes+64);
    secondary=(uint8_t*)malloc(requirements.secondary_bytes+64);
    views=(uint8_t*)malloc(requirements.views_bytes+64);
    if(!primary || !secondary || !views)goto cleanup;
    memset(primary,0xcd,requirements.primary_bytes+64);
    memset(secondary,0xcd,requirements.secondary_bytes+64);
    memset(views,0xcd,requirements.views_bytes+64);
    {
        marc_buffer p={primary,requirements.primary_bytes+64};
        marc_buffer s={secondary,requirements.secondary_bytes+64};
        marc_buffer v={views,requirements.views_bytes+64};
        if(marc_private_position_distance_4m_baseline_create(&config,p,s,v,&handle)!=MARC_STATUS_OK || !handle)goto cleanup;
    }
    for(call=0;call<32768;++call) {
        uint8_t buffer[9];
        size_t n=count-consumed,cap=call%7==0?0:7;
        marc_const_buffer in;
        marc_buffer out;
        marc_process_result result;
        if(n>1)n=1;
        memset(buffer,0xcd,sizeof(buffer));in.data=n?input+consumed:NULL;in.size=n;
        out.data=buffer+1;out.size=cap;
        result=marc_transform_process(handle,in,out,MARC_PROCESS_FLUSH|(consumed+n==count?MARC_PROCESS_END_INPUT:0u));
        if(result.input_consumed>n || result.output_produced>cap || buffer[0]!=0xcd
            ||(result.status==MARC_STATUS_PROGRESS && !result.input_consumed && !result.output_produced))goto cleanup;
        for(i=1+result.output_produced;i<sizeof(buffer);++i)if(buffer[i]!=0xcd)goto cleanup;
        if(result.output_produced>capacity-produced)goto cleanup;
        memcpy(output+produced,buffer+1,result.output_produced);
        consumed+=result.input_consumed;produced+=result.output_produced;
        if(result.status==MARC_STATUS_END_OF_STREAM || result.status>=100) {
            marc_const_buffer empty={NULL,0};marc_buffer empty_output={NULL,0};
            marc_process_result again;
            if(result.status!=expected ||(expected==MARC_STATUS_END_OF_STREAM && consumed!=count)) {
                fprintf(stderr,"C consumer terminal: direction=%u input=%zu status=%u expected=%u consumed=%zu produced=%zu\n",
                    direction,count,result.status,expected,consumed,produced);
                goto cleanup;
            }
            again=marc_transform_process(handle,empty,empty_output,MARC_PROCESS_END_INPUT|MARC_PROCESS_FLUSH);
            if(again.status!=result.status || again.input_consumed || again.output_produced
                ||again.error_byte_position!=result.error_byte_position || again.error_bit_position!=result.error_bit_position)goto cleanup;
            terminal=1;break;
        }
    }
    if(!terminal)goto cleanup;
    marc_transform_destroy(handle);handle=NULL;
    for(i=0;i<64;++i)if(primary[requirements.primary_bytes+i]!=0xcd
        ||secondary[requirements.secondary_bytes+i]!=0xcd ||views[requirements.views_bytes+i]!=0xcd)goto cleanup;
    *written=produced;ok=1;
cleanup:
    marc_transform_destroy(handle);free(primary);free(secondary);free(views);return ok;
}

int main(void) {
    uint8_t raw[49],encoded[2048],restored[128];
    size_t i,index;
    const size_t sizes[]={0,1,21,22,49};
    for(i=0;i<sizeof(raw);++i)raw[i]=(uint8_t)(i*71+i/11);
    for(index=0;index<sizeof(sizes)/sizeof(sizes[0]);++index) {
        size_t encoded_size=0,restored_size=0;
        if(!transform(MARC_DIRECTION_ENCODE,raw,sizes[index],encoded,sizeof(encoded),&encoded_size,MARC_STATUS_END_OF_STREAM)
            ||!transform(MARC_DIRECTION_DECODE,encoded,encoded_size,restored,sizeof(restored),&restored_size,MARC_STATUS_END_OF_STREAM)
            ||restored_size!=sizes[index] ||memcmp(restored,raw,restored_size))return 1;
        encoded[14]=9;restored_size=0;
        if(!transform(MARC_DIRECTION_DECODE,encoded,encoded_size,restored,sizeof(restored),&restored_size,MARC_STATUS_UNSUPPORTED)
            ||restored_size)return 1;
    }
    puts("PASS linked C17 baseline consumer: five round-trips, crossed identities, tiny buffers, guards and sticky terminal states");
    return 0;
}

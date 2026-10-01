#include "marc/marc.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

namespace {
std::vector<uint8_t> read_bounded(const char* path) {
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    if(!file) return {};
    const auto size=file.tellg();
    if(size<0 || static_cast<uint64_t>(size)>128U*1024U*1024U) return {};
    std::vector<uint8_t> bytes(static_cast<size_t>(size));file.seekg(0);
    if(size && !file.read(reinterpret_cast<char*>(bytes.data()),size)) return {};
    return bytes;
}
TEST(PositionDistance4mPublicCCorpus, CompleteFrozenStreamsAndRestoration) {
    const auto* input_path=std::getenv("MARC_POSITION_4M_PUBLIC_C_INPUT");
    const auto* archive_path=std::getenv("MARC_POSITION_4M_PUBLIC_C_ARCHIVE");
    if(!input_path || !archive_path) GTEST_SKIP()<<"Optional verified corpus C boundary check";
    const auto raw=read_bounded(input_path),archive=read_bounded(archive_path);
    ASSERT_FALSE(raw.empty());ASSERT_GT(archive.size(),112);
    for(unsigned schedule=0;schedule<2;++schedule)
    for(auto direction:{MARC_DIRECTION_ENCODE,MARC_DIRECTION_DECODE}) {
        SCOPED_TRACE(schedule);
        SCOPED_TRACE(direction);
        marc_lzss_position_distance_dynamic_range_4m_config config{};
        ASSERT_EQ(marc_lzss_position_distance_dynamic_range_4m_config_init(direction,&config),MARC_STATUS_OK);
        config.original_size=raw.size();
        marc_workspace_requirements required{};
        ASSERT_EQ(marc_lzss_position_distance_dynamic_range_4m_workspace_requirements(&config,&required),MARC_STATUS_OK);
        std::vector<uint8_t> primary(required.primary_bytes+64,0xcd),secondary(required.secondary_bytes+64,0xcd);
        std::vector<std::max_align_t> aligned((required.views_bytes+64+sizeof(std::max_align_t)-1)/sizeof(std::max_align_t));
        const auto views_size=aligned.size()*sizeof(std::max_align_t);
        auto* views=reinterpret_cast<uint8_t*>(aligned.data());std::memset(views,0xcd,views_size);
        marc_transform* handle{};
        ASSERT_EQ(marc_lzss_position_distance_dynamic_range_4m_create(&config,{primary.data(),primary.size()},
            {secondary.data(),secondary.size()},{views,views_size},&handle),MARC_STATUS_OK);
        ASSERT_NE(handle,nullptr);
        std::unique_ptr<marc_transform,decltype(&marc_transform_destroy)> owner(handle,marc_transform_destroy);
        const auto& input=direction==MARC_DIRECTION_ENCODE?raw:archive;
        const auto& expected=direction==MARC_DIRECTION_ENCODE?archive:raw;
        const size_t chunk=schedule?65521:8191,capacity=schedule?65519:4093;
        std::vector<uint8_t> output(capacity+2);size_t consumed{},produced{};bool ended{};
        for(size_t call=0;call<200000;++call) {
            const auto n=std::min(chunk,input.size()-consumed),cap=call%17==0?0:capacity;
            std::fill(output.begin(),output.end(),0xcd);
            const auto final=schedule?consumed==input.size():consumed+n==input.size();
            const auto result=marc_transform_process(handle,{n?input.data()+consumed:nullptr,n},
                {output.data()+1,cap},MARC_PROCESS_FLUSH|(final?MARC_PROCESS_END_INPUT:0u));
            ASSERT_LT(result.status,100);ASSERT_LE(result.input_consumed,n);ASSERT_LE(result.output_produced,cap);
            ASSERT_FALSE(result.status==MARC_STATUS_PROGRESS && !result.input_consumed && !result.output_produced);
            ASSERT_EQ(output.front(),0xcd);
            ASSERT_TRUE(std::all_of(output.begin()+1+result.output_produced,output.end(),[](auto b){return b==0xcd;}));
            ASSERT_LE(result.output_produced,expected.size()-produced);
            ASSERT_TRUE(std::equal(output.begin()+1,output.begin()+1+result.output_produced,expected.begin()+produced));
            consumed+=result.input_consumed;produced+=result.output_produced;
            if(result.status==MARC_STATUS_END_OF_STREAM) {ended=true;break;}
        }
        ASSERT_TRUE(ended);EXPECT_EQ(consumed,input.size());EXPECT_EQ(produced,expected.size());
        for(auto flags:{MARC_PROCESS_NONE,MARC_PROCESS_END_INPUT,MARC_PROCESS_FLUSH}) {
            const auto terminal=marc_transform_process(handle,{nullptr,0},{nullptr,0},flags);
            EXPECT_EQ(terminal.status,MARC_STATUS_END_OF_STREAM);
            EXPECT_EQ(terminal.input_consumed,0);EXPECT_EQ(terminal.output_produced,0);
        }
        owner.reset();
        EXPECT_TRUE(std::all_of(primary.begin()+required.primary_bytes,primary.end(),[](auto b){return b==0xcd;}));
        EXPECT_TRUE(std::all_of(secondary.begin()+required.secondary_bytes,secondary.end(),[](auto b){return b==0xcd;}));
        EXPECT_TRUE(std::all_of(views+required.views_bytes,views+views_size,[](auto b){return b==0xcd;}));
    }
    std::cout<<"C_CORPUS frames="<<(raw.size()+4194303)/4194304<<" raw="<<raw.size()
        <<" archive="<<archive.size()<<" schedules=2 directions=2\n";
}
}

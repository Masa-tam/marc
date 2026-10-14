#include <marc/marc.h>
#include "core/endian.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <vector>

namespace {
using Config=marc_lzss_position_distance_tans_8m_config;
using Resources=marc_lzss_position_distance_tans_8m_resources;
int failures{};
void check(bool value,const char* name) {
    if (!value) { ++failures; std::cerr << name << '\n'; }
}
Config configuration(marc_direction direction,std::uint32_t frame,std::uint64_t original=0) {
    Config value{};
    check(marc_lzss_position_distance_tans_8m_config_init(direction,&value)==MARC_STATUS_OK,"config init");
    value.frame_size=frame;
    value.max_frame_size=frame;
    value.max_block_size=UINT64_C(9)*8388608;
    value.original_size=original;
    value.external_retained_bytes=65536;
    value.input_capacity_bytes=1;
    value.output_capacity_bytes=3;
    return value;
}
std::vector<std::uint8_t> run(marc_transform* transform,std::span<const std::uint8_t> input,marc_status& terminal) {
    std::vector<std::uint8_t> output;
    std::size_t consumed{};
    for (std::size_t calls=0;calls<4096;++calls) {
        const auto count=consumed<input.size() ? std::size_t{1} : std::size_t{};
        std::array<std::uint8_t,5> scratch{};
        scratch.fill(0xa5);
        const auto result=marc_transform_process(transform,
            {count ? input.data()+consumed : nullptr,count},{scratch.data()+1,3},
            consumed+count==input.size() ? MARC_PROCESS_END_INPUT : MARC_PROCESS_FLUSH);
        check(result.input_consumed<=count && result.output_produced<=3,"process bounds");
        check(scratch.front()==0xa5 && std::all_of(scratch.begin()+1+result.output_produced,scratch.end(),
            [](auto value){ return value==0xa5; }),"unpublished output tail");
        consumed+=result.input_consumed;
        output.insert(output.end(),scratch.begin()+1,scratch.begin()+1+result.output_produced);
        if (result.status==MARC_STATUS_END_OF_STREAM || result.status>=100) {
            terminal=result.status;
            return output;
        }
    }
    check(false,"bounded process driver");
    terminal=MARC_STATUS_INTERNAL_ERROR;
    return output;
}
std::vector<std::uint8_t> encode(std::span<const std::uint8_t> raw) {
    auto config=configuration(MARC_DIRECTION_ENCODE,1,raw.size());
    Resources resources{};
    check(marc_lzss_position_distance_tans_8m_resource_requirements(&config,&resources)==MARC_STATUS_OK,"encoder requirements");
    config.max_internal_buffered_bytes=resources.minimum_aggregate_bytes;
    marc_transform* encoder{};
    check(marc_lzss_position_distance_tans_8m_create(&config,&encoder)==MARC_STATUS_OK && encoder,"encoder create");
    marc_status status{};
    const auto bytes=encoder ? run(encoder,raw,status) : std::vector<std::uint8_t>{};
    if (encoder) marc_transform_destroy(encoder);
    check(status==MARC_STATUS_END_OF_STREAM,"encoder eos");
    return bytes;
}
std::vector<std::uint8_t> decode(std::span<const std::uint8_t> wire,marc_status& status) {
    auto config=configuration(MARC_DIRECTION_DECODE,1);
    Resources resources{};
    check(marc_lzss_position_distance_tans_8m_resource_requirements(&config,&resources)==MARC_STATUS_OK,"decoder requirements");
    config.max_internal_buffered_bytes=resources.minimum_aggregate_bytes;
    marc_transform* decoder{};
    check(marc_lzss_position_distance_tans_8m_create(&config,&decoder)==MARC_STATUS_OK && decoder,"decoder create");
    const auto bytes=decoder ? run(decoder,wire,status) : std::vector<std::uint8_t>{};
    if (decoder) marc_transform_destroy(decoder);
    return bytes;
}
}

int main() {
    for (auto direction : {MARC_DIRECTION_ENCODE,MARC_DIRECTION_DECODE}) {
        Config config{};
        check(marc_lzss_position_distance_tans_8m_config_init(direction,&config)==MARC_STATUS_OK
            && config.frame_size==8388608 && config.max_frame_size==8388608
            && config.max_block_size==UINT64_C(9)*8388608
            && config.max_compressed_payload_size==113246210
            && config.max_entropy_table_entries==229376,"public defaults");
        Resources resources{};
        check(marc_lzss_position_distance_tans_8m_resource_requirements(&config,&resources)==MARC_STATUS_OK
            && resources.minimum_aggregate_bytes>=resources.private_admission_bytes
            && resources.owned_buffer_bytes==resources.raw_bytes+resources.token_bytes+resources.serialized_bytes
                +resources.finder_bytes+resources.table_bytes+resources.private_payload_bytes+resources.private_serialized_bytes,
            "full-window resource query");
        auto invalid=config;
        invalid.frame_size=8388609;
        const auto saved=resources;
        check(marc_lzss_position_distance_tans_8m_resource_requirements(&invalid,&resources)==MARC_STATUS_INVALID_ARGUMENT
            && std::memcmp(&saved,&resources,sizeof(resources))==0,"invalid config leaves resources unchanged");
    }
    const std::array<std::uint8_t,2> raw{{65,66}};
    const auto wire=encode(raw);
    marc_status status{};
    check(decode(wire,status)==std::vector<std::uint8_t>(raw.begin(),raw.end()) && status==MARC_STATUS_END_OF_STREAM,
        "public two-frame roundtrip");
    auto damaged=wire;
    check(damaged.size()>=2,"wire has terminal state");
    if (damaged.size()>=2) (void)marc::core::store_le<std::uint16_t>(std::as_writable_bytes(std::span(damaged)),damaged.size()-2,1);
    const auto partial=decode(damaged,status);
    check(status==MARC_STATUS_MALFORMED_STREAM && partial==std::vector<std::uint8_t>{65},
        "failed second frame is not published");
    return failures ? 1 : 0;
}

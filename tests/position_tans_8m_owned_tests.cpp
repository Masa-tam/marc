#include "frame/lzss_position_tans_8m_owned.hpp"
#include <iostream>

namespace {
using namespace marc;
using namespace marc::frame::internal;
int failures{};
void check(bool value,const char* name) {
    if (!value) { ++failures; std::cerr << name << '\n'; }
}
core::DecoderLimits limits() {
    core::DecoderLimits value{};
    value.max_frame_size=8388608;
    value.max_block_size=UINT64_C(9)*8388608;
    value.max_compressed_payload_size=113246210;
    value.max_internal_buffered_bytes=UINT64_C(1)<<30;
    value.max_lz_distance=8388608;
    value.max_lz_match_length=258;
    value.max_entropy_table_entries=229376;
    return value;
}
}

int main() {
    auto configured=limits();
    PositionTans8mStreamHeader stream{};
    stream.frame_size=1;
    stream.original_size=1;
    PositionTans8mWorkspace encoder{};
    check(PositionTans8mOwnedEncoder::requirements(stream,configured,encoder)==core::ErrorCode::none
        && encoder.raw_bytes==1 && encoder.token_count==1
        && encoder.aggregate_bytes<=configured.max_internal_buffered_bytes,
        "encoder requirements for bounded frame");
    auto below=configured;
    below.max_internal_buffered_bytes=encoder.aggregate_bytes-1;
    PositionTans8mWorkspace preserved{};
    preserved.aggregate_bytes=999;
    check(PositionTans8mOwnedEncoder::requirements(stream,below,preserved)==core::ErrorCode::limit_exceeded
        && preserved.aggregate_bytes==999,"encoder requirements retain output below limit");
    core::ErrorCode error{};
    auto encoder_instance=PositionTans8mOwnedEncoder::create(stream,configured,error);
    check(error==core::ErrorCode::none && encoder_instance!=nullptr,"encoder creates after admission");

    PositionTans8mWorkspace decoder{};
    check(PositionTans8mOwnedDecoder::requirements(1,configured,decoder)==core::ErrorCode::none
        && decoder.raw_bytes==1 && decoder.token_count==1 && decoder.finder_bytes==0
        && decoder.aggregate_bytes<=configured.max_internal_buffered_bytes,
        "decoder requirements for bounded frame");
    below=configured;
    below.max_internal_buffered_bytes=decoder.aggregate_bytes-1;
    preserved.aggregate_bytes=999;
    check(PositionTans8mOwnedDecoder::requirements(1,below,preserved)==core::ErrorCode::limit_exceeded
        && preserved.aggregate_bytes==999,"decoder requirements retain output below limit");
    error=core::ErrorCode::none;
    auto decoder_instance=PositionTans8mOwnedDecoder::create(1,configured,error);
    check(error==core::ErrorCode::none && decoder_instance!=nullptr,"decoder creates after admission");
    return failures ? 1 : 0;
}

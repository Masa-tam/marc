#include "frame/lzss_position_distance_frame_decoder.hpp"
#include "frame/lzss_position_distance_preflight.hpp"
#include "frame/lzss_reduced_literal_frame_decoder.hpp"
#include "core/endian.hpp"

#include <gtest/gtest.h>
#include <array>
#include <cstring>
#include <span>
#include <vector>

namespace {
using namespace marc::frame::internal;
using marc::dictionary::internal::LzssTypedToken;
using Error=LzssShortMatchFrameDecodeError;
constexpr std::array payload{std::byte{0},std::byte{48},std::byte{152},std::byte{79},
    std::byte{209},std::byte{96},std::byte{9},std::byte{207},std::byte{77},
    std::byte{61},std::byte{39},std::byte{231},std::byte{140},std::byte{67},
    std::byte{173},std::byte{72},std::byte{11},std::byte{64}};
TypedContextStreamHeader stream(std::uint32_t raw=21) {
    TypedContextStreamHeader s{};
    s.frame_size=raw; s.original_size=raw; s.dictionary={65536,3,258,0};
    s.range_model_total=32768; s.context_count=40; s.dictionary_variant=8; s.context_variant=9;
    return s;
}
std::vector<std::byte> frame(std::span<const std::byte> bytes=payload,
                            std::uint32_t raw=21,std::uint32_t tokens=17,
                            std::uint32_t events=36,std::uint32_t decisions=38) {
    std::vector<std::byte> out(80+bytes.size());
    out[0]=std::byte{0x4d}; out[1]=std::byte{0x52}; out[2]=std::byte{0x46}; out[3]=std::byte{0x32};
    const std::span<std::byte> target{out};
    EXPECT_TRUE(marc::core::store_le(target,4,std::uint16_t{64}));
    EXPECT_TRUE(marc::core::store_le(target,16,raw));
    EXPECT_TRUE(marc::core::store_le(target,20,tokens));
    EXPECT_TRUE(marc::core::store_le(target,24,events));
    EXPECT_TRUE(marc::core::store_le(target,28,decisions));
    EXPECT_TRUE(marc::core::store_le(target,32,static_cast<std::uint32_t>(bytes.size())));
    EXPECT_TRUE(marc::core::store_le(target,36,std::uint32_t{16}));
    EXPECT_TRUE(marc::core::store_le(target,64,decisions));
    EXPECT_TRUE(marc::core::store_le(target,68,static_cast<std::uint32_t>(bytes.size())));
    EXPECT_TRUE(marc::core::store_le(target,72,std::uint16_t{40}));
    std::memcpy(out.data()+80,bytes.data(),bytes.size());
    return out;
}

TEST(LzssPositionDistanceFrameDecoder, FixedPayloadReconstructsAndConsumesExactPrefix) {
    auto bytes=frame(); bytes.push_back(std::byte{0xff});
    std::array<LzssTypedToken,18> tokens{}; tokens.back().literal=0xa5;
    std::array<std::byte,22> raw{}; raw.fill(std::byte{0xa5});
    const auto decoded=decode_lzss_position_distance_frame(bytes,{stream(),{}},tokens,raw);
    ASSERT_EQ(decoded.error,Error::none);
    EXPECT_EQ(decoded.serialized_consumed,98);
    EXPECT_EQ(decoded.required_raw_size,21); EXPECT_EQ(decoded.required_token_count,17);
    for (std::size_t i=0;i<21;++i) EXPECT_EQ(raw[i],std::byte{97});
    EXPECT_EQ(raw.back(),std::byte{0xa5}); EXPECT_EQ(tokens.back().literal,0xa5);
    EXPECT_EQ(decode_lzss_reduced_literal_frame(bytes,{stream(),{}},tokens,raw).error,Error::preflight_error);
}

TEST(LzssPositionDistanceFrameDecoder, OverlappingDictionaryCopy) {
    constexpr std::array p{std::byte{0},std::byte{0x30},std::byte{0xbf},
        std::byte{0xff},std::byte{0x9e},std::byte{0x80},std::byte{0}};
    const auto bytes=frame(p,6,2,5,5); // Literal a then length 5 distance 1
    std::array<LzssTypedToken,2> tokens{}; std::array<std::byte,6> raw{};
    ASSERT_EQ(decode_lzss_position_distance_frame(bytes,{stream(6),{}},tokens,raw).error,Error::none);
    for (const auto value:raw) EXPECT_EQ(value,std::byte{97});
}

TEST(LzssPositionDistanceFrameDecoder, TruncationAndCapacityPreserveOutputs) {
    const auto bytes=frame();
    std::array<LzssTypedToken,17> tokens{};
    for (auto& t:tokens) t.literal=0xa5;
    std::array<std::byte,21> raw{}; raw.fill(std::byte{0xa5});
    for (std::size_t n=0;n<bytes.size();++n) {
        const auto r=decode_lzss_position_distance_frame(std::span{bytes}.first(n),{stream(),{}},tokens,raw);
        EXPECT_EQ(r.error,Error::preflight_error); EXPECT_EQ(r.serialized_consumed,0);
    }
    EXPECT_EQ(decode_lzss_position_distance_frame(bytes,{stream(),{}},std::span{tokens}.first(16),raw).error,Error::token_output_too_small);
    EXPECT_EQ(decode_lzss_position_distance_frame(bytes,{stream(),{}},tokens,std::span{raw}.first(20)).error,Error::raw_output_too_small);
    for (const auto& t:tokens) EXPECT_EQ(t.literal,0xa5);
    for (const auto b:raw) EXPECT_EQ(b,std::byte{0xa5});
}

TEST(LzssPositionDistanceFrameDecoder, InvalidHistoryAndTailNeverPublishRawBytes) {
    constexpr std::array invalid{std::byte{0},std::byte{48},std::byte{152},
        std::byte{190},std::byte{146},std::byte{107},std::byte{61},std::byte{34},std::byte{142}};
    std::array<LzssTypedToken,17> tokens{};
    std::array<std::byte,21> raw{}; raw.fill(std::byte{0xa5});
    auto r=decode_lzss_position_distance_frame(frame(invalid,9,4,14,14),{stream(9),{}},tokens,raw);
    EXPECT_EQ(r.error,Error::token_decode_error); EXPECT_EQ(r.serialized_consumed,0);
    auto bad=frame(); bad.back()=std::byte{65};
    r=decode_lzss_position_distance_frame(bad,{stream(),{}},tokens,raw);
    EXPECT_EQ(r.error,Error::token_decode_error); EXPECT_EQ(r.serialized_consumed,0);
    for (const auto b:raw) EXPECT_EQ(b,std::byte{0xa5});
}

TEST(LzssPositionDistanceFrameDecoder, AggregateLimitAndAllWorkspaceOverlaps) {
    auto bytes=frame(); const auto s=stream();
    auto limits=marc::core::DecoderLimits{};
    TypedContextFrameLayout layout{}; LzssShortMatchFrameRequirements requirements{};
    ASSERT_EQ(preflight_lzss_position_distance_frame_bytes(bytes,{s,limits},layout,requirements),LzssShortMatchPreflightError::none);
    limits.max_block_size=21; limits.max_internal_buffered_bytes=requirements.aggregate_working_bytes;
    std::array<LzssTypedToken,17> tokens{}; std::array<std::byte,21> raw{};
    ASSERT_EQ(decode_lzss_position_distance_frame(bytes,{s,limits},tokens,raw).error,Error::none);
    --limits.max_internal_buffered_bytes; raw.fill(std::byte{0xa5});
    EXPECT_EQ(decode_lzss_position_distance_frame(bytes,{s,limits},tokens,raw).error,Error::preflight_error);
    for (const auto b:raw) EXPECT_EQ(b,std::byte{0xa5});
    EXPECT_EQ(decode_lzss_position_distance_frame(bytes,{s,{}},tokens,std::span{bytes}.first(21)).error,Error::overlapping_workspaces);
    auto token_bytes=std::as_writable_bytes(std::span{tokens});
    EXPECT_EQ(decode_lzss_position_distance_frame(bytes,{s,{}},tokens,token_bytes.first(21)).error,Error::overlapping_workspaces);
    std::memcpy(token_bytes.data(),bytes.data(),bytes.size());
    EXPECT_EQ(decode_lzss_position_distance_frame(token_bytes.first(bytes.size()),{s,{}},tokens,raw).error,Error::overlapping_workspaces);
}
void compare_frame_scratch(std::span<const std::byte> bytes) {
    for(std::size_t capacity:{16U,17U}) for(std::size_t raw_capacity:{20U,21U}) {
        std::array<LzssTypedToken,19> a{},b{};
        for(auto& t:a) t.literal=0xa5;
        b=a;
        std::array<std::byte,23> x{},y{}; x.fill(std::byte{0xa5}); y=x;
        const auto r=decode_lzss_position_distance_frame(bytes,{stream(),{}},
            std::span{a}.subspan(1,capacity),std::span{x}.subspan(1,raw_capacity));
        const auto s=decode_lzss_position_distance_frame_scratch(bytes,{stream(),{}},
            std::span{b}.subspan(1,capacity),std::span{y}.subspan(1,raw_capacity));
        EXPECT_EQ(r.error,s.error); EXPECT_EQ(r.preflight_error,s.preflight_error);
        EXPECT_EQ(r.serialized_consumed,s.serialized_consumed);
        EXPECT_EQ(r.required_token_count,s.required_token_count);
        EXPECT_EQ(r.required_raw_size,s.required_raw_size);
        EXPECT_EQ(r.token_decode.error,s.token_decode.error);
        EXPECT_EQ(r.token_decode.token_error,s.token_decode.token_error);
        EXPECT_EQ(r.token_decode.token_count,s.token_decode.token_count);
        EXPECT_EQ(r.token_decode.token_index,s.token_decode.token_index);
        EXPECT_EQ(r.token_decode.raw_size,s.token_decode.raw_size);
        EXPECT_EQ(r.token_decode.entropy.error,s.token_decode.entropy.error);
        EXPECT_EQ(r.token_decode.entropy.event_count,s.token_decode.entropy.event_count);
        EXPECT_EQ(r.token_decode.entropy.decision_count,s.token_decode.entropy.decision_count);
        EXPECT_EQ(r.token_decode.entropy.payload_consumed,s.token_decode.entropy.payload_consumed);
        EXPECT_EQ(r.reconstruction.error,s.reconstruction.error);
        EXPECT_EQ(x,y); EXPECT_EQ(y.front(),std::byte{0xa5}); EXPECT_EQ(y.back(),std::byte{0xa5});
        EXPECT_EQ(b.front().literal,0xa5); EXPECT_EQ(b.back().literal,0xa5);
        if(r.error!=Error::none) {
            for(const auto byte:y) EXPECT_EQ(byte,std::byte{0xa5});
            for(const auto& token:a) EXPECT_EQ(token.literal,0xa5);
        } else for(std::size_t i=1;i<=17;++i) {
            EXPECT_EQ(a[i].kind,b[i].kind); EXPECT_EQ(a[i].literal,b[i].literal);
            EXPECT_EQ(a[i].distance,b[i].distance); EXPECT_EQ(a[i].length,b[i].length);
        }
    }
}

TEST(LzssPositionDistanceFrameDecoder, ScratchDifferentialEveryBitAndTruncation) {
    const auto bytes=frame(); compare_frame_scratch(bytes);
    for(std::size_t n=0;n<bytes.size();++n) compare_frame_scratch(std::span{bytes}.first(n));
    for(std::size_t n=0;n<bytes.size();++n) for(unsigned bit=0;bit<8;++bit) {
        auto bad=bytes; bad[n]^=std::byte{static_cast<unsigned char>(1U<<bit)};
        compare_frame_scratch(bad);
    }
}

TEST(LzssPositionDistanceFrameDecoder, ScratchRejectsAllWorkspaceOverlapsBeforeWrites) {
    auto bytes=frame(); std::array<LzssTypedToken,17> tokens{};
    std::array<std::byte,21> raw{}; raw.fill(std::byte{0xa5});
    const auto original=bytes;
    EXPECT_EQ(decode_lzss_position_distance_frame_scratch(bytes,{stream(),{}},tokens,
        std::span{bytes}.first(21)).error,Error::overlapping_workspaces);
    EXPECT_EQ(bytes,original);
    auto token_bytes=std::as_writable_bytes(std::span{tokens});
    std::memcpy(token_bytes.data(),bytes.data(),bytes.size());
    const auto before=tokens;
    EXPECT_EQ(decode_lzss_position_distance_frame_scratch(bytes,{stream(),{}},tokens,
        token_bytes.first(21)).error,Error::overlapping_workspaces);
    EXPECT_EQ(decode_lzss_position_distance_frame_scratch(token_bytes.first(bytes.size()),
        {stream(),{}},tokens,raw).error,Error::overlapping_workspaces);
    EXPECT_EQ(std::memcmp(tokens.data(),before.data(),sizeof(tokens)),0);
    for(auto byte:raw) EXPECT_EQ(byte,std::byte{0xa5});
}
} // namespace

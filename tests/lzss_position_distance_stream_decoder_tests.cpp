#include "frame/lzss_position_distance_stream_decoder.hpp"
#include "frame/lzss_position_distance_preflight.hpp"
#include "core/endian.hpp"
#include <gtest/gtest.h>
#include <array>
#include <algorithm>
#include <cstring>
#include <span>
#include <vector>

namespace {
using namespace marc::frame::internal;
using marc::dictionary::internal::LzssTypedToken;
using Error=LzssShortMatchStreamDecodeError;
[[nodiscard]] std::array<std::byte, typed_context_stream_header_size>
stream_header(const std::uint64_t original_size) {
    std::array<std::byte, typed_context_stream_header_size> bytes{};
    bytes[0] = std::byte{0x4d};
    bytes[1] = std::byte{0x41};
    bytes[2] = std::byte{0x52};
    bytes[3] = std::byte{0x43};
    const std::span<std::byte> out{bytes};
    EXPECT_TRUE(marc::core::store_le(out, 4, std::uint16_t{2}));
    EXPECT_TRUE(marc::core::store_le(out, 8, std::uint16_t{64}));
    EXPECT_TRUE(marc::core::store_le(out, 10, std::uint16_t{1}));
    EXPECT_TRUE(marc::core::store_le(out, 12, std::uint16_t{2}));
    EXPECT_TRUE(marc::core::store_le(out, 14, std::uint16_t{8}));
    EXPECT_TRUE(marc::core::store_le(out, 16, std::uint16_t{3}));
    EXPECT_TRUE(marc::core::store_le(out, 18, std::uint16_t{2}));
    EXPECT_TRUE(marc::core::store_le(out, 20, std::uint32_t{21}));
    EXPECT_TRUE(marc::core::store_le(out, 28, std::uint32_t{16}));
    EXPECT_TRUE(marc::core::store_le(out, 32, std::uint32_t{16}));
    EXPECT_TRUE(marc::core::store_le(out, 40, original_size));
    EXPECT_TRUE(marc::core::store_le(out, 48, std::uint32_t{16}));
    EXPECT_TRUE(marc::core::store_le(out, 64, std::uint32_t{65536}));
    EXPECT_TRUE(marc::core::store_le(out, 68, std::uint32_t{3}));
    EXPECT_TRUE(marc::core::store_le(out, 72, std::uint32_t{258}));
    EXPECT_TRUE(marc::core::store_le(out, 80, typed_context_model_total));
    EXPECT_TRUE(marc::core::store_le(out, 84, std::uint16_t{40}));
    EXPECT_TRUE(marc::core::store_le(out, 96, std::uint16_t{1}));
    EXPECT_TRUE(marc::core::store_le(out, 98, std::uint16_t{9}));
    return bytes;
}


constexpr std::array payload{std::byte{0},std::byte{48},std::byte{152},std::byte{79},
    std::byte{209},std::byte{96},std::byte{9},std::byte{207},std::byte{77},
    std::byte{61},std::byte{39},std::byte{231},std::byte{140},std::byte{67},
    std::byte{173},std::byte{72},std::byte{11},std::byte{64}};
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


std::vector<std::byte> stream(std::size_t count, bool short_last=false) {
    const auto h=stream_header(count*21+(short_last ? 6 : 0));
    std::vector<std::byte> bytes(h.begin(),h.end());
    for (std::size_t i=0;i<count+(short_last ? 1 : 0);++i) {
        constexpr std::array p{std::byte{0},std::byte{0x30},std::byte{0xbf},
            std::byte{0xff},std::byte{0x9e},std::byte{0x80},std::byte{0}};
        auto next=i<count ? frame() : frame(p,6,2,5,5);
        EXPECT_TRUE(marc::core::store_le(std::span{next},8,static_cast<std::uint64_t>(i)));
        bytes.insert(bytes.end(),next.begin(),next.end());
    }
    return bytes;
}

TEST(LzssPositionDistanceStreamDecoder, FixedVectorsEmptyFullAndShortFinalFrames) {
    std::array<LzssTypedToken,17> tokens{};
    std::array<std::byte,21> scratch{};
    for (std::size_t count:{0U,1U,2U}) for (bool final:{false,true}) {
        SCOPED_TRACE(count);
        SCOPED_TRACE(final);
        const auto bytes=stream(count,final);
        const auto size=count*21+(final ? 6 : 0);
        std::vector<std::byte> output(size+2,std::byte{0xcc});
        const auto result=decode_lzss_position_distance_stream(bytes,{},tokens,scratch,
            std::span{output}.subspan(1,size));
        ASSERT_EQ(result.error,Error::none);
        EXPECT_EQ(result.serialized_consumed,bytes.size());
        EXPECT_EQ(result.raw_produced,size); EXPECT_EQ(result.frame_count,count+(final ? 1 : 0));
        for(std::size_t i=1;i<=size;++i) EXPECT_EQ(output[i],std::byte{97});
        EXPECT_EQ(output.front(),std::byte{0xcc}); EXPECT_EQ(output.back(),std::byte{0xcc});
    }
}

TEST(LzssPositionDistanceStreamDecoder, EveryTruncationAndLateDamagePreserveOutput) {
    std::array<LzssTypedToken,17> tokens{};
    std::array<std::byte,21> scratch{};
    std::array<std::byte,48> output; output.fill(std::byte{0xcc});
    const auto valid=stream(2,true);
    for(std::size_t n=0;n<valid.size();++n) {
        SCOPED_TRACE(n);
        const auto r=decode_lzss_position_distance_stream(std::span{valid}.first(n),{},tokens,scratch,output);
        EXPECT_NE(r.error,Error::none); EXPECT_EQ(r.raw_produced,0); EXPECT_EQ(r.serialized_consumed,0);
        for(auto b:output) EXPECT_EQ(b,std::byte{0xcc});
    }
    for(unsigned fault=0;fault<5;++fault) {
        auto bytes=valid;
        const std::size_t second=112+98;
        switch(fault) {
        case 0: bytes.push_back(std::byte{0}); break;
        case 1: bytes[second+80]=std::byte{1}; break;
        case 2: bytes[second+8]=std::byte{2}; break;
        case 3: bytes[second+6]=std::byte{1}; break;
        case 4: bytes[second+48]=std::byte{1}; break;
        }
        const auto r=decode_lzss_position_distance_stream(bytes,{},tokens,scratch,output);
        EXPECT_EQ(r.error,fault==0 ? Error::trailing_data : Error::frame_error);
        EXPECT_EQ(r.raw_produced,0); EXPECT_EQ(r.serialized_consumed,0);
        if(fault!=0) EXPECT_EQ(r.error_offset,second);
        for(auto b:output) EXPECT_EQ(b,std::byte{0xcc});
    }
}

TEST(LzssPositionDistanceStreamDecoder, RejectsCrossedIdentitiesAndInsufficientCapacity) {
    std::array<LzssTypedToken,17> tokens{};
    std::array<std::byte,21> scratch{};
    std::array<std::byte,21> output; output.fill(std::byte{0xcc});
    for(const std::size_t offset:{12U,14U,16U,18U,84U,96U,98U}) {
        auto bytes=stream(1); bytes[offset]^=std::byte{1};
        EXPECT_EQ(decode_lzss_position_distance_stream(bytes,{},tokens,scratch,output).error,Error::stream_header_error);
    }
    const auto bytes=stream(1);
    EXPECT_EQ(decode_lzss_position_distance_stream(bytes,{},tokens,scratch,std::span{output}.first(20)).error,Error::output_too_small);
    EXPECT_EQ(decode_lzss_position_distance_stream(bytes,{},std::span{tokens}.first(16),scratch,output).error,Error::frame_error);
    EXPECT_EQ(decode_lzss_position_distance_stream(bytes,{},tokens,std::span{scratch}.first(20),output).error,Error::frame_error);
    EXPECT_EQ(decode_lzss_short_match_stream(bytes,{},tokens,scratch,output).error,Error::stream_header_error);
    for(auto b:output) EXPECT_EQ(b,std::byte{0xcc});
}

TEST(LzssPositionDistanceStreamDecoder, AggregateAndOutputLimitsAreExact) {
    const auto bytes=stream(1);
    std::array<LzssTypedToken,17> tokens{};
    std::array<std::byte,21> scratch{},output{};
    TypedContextStreamHeader header{}; std::size_t consumed{};
    ASSERT_EQ(parse_lzss_position_distance_stream_header(bytes,{},header,consumed),LzssShortMatchPreflightError::none);
    TypedContextFrameLayout layout{}; LzssShortMatchFrameRequirements requirements{};
    ASSERT_EQ(preflight_lzss_position_distance_frame_bytes(std::span{bytes}.subspan(consumed),
        {header,{}},layout,requirements),LzssShortMatchPreflightError::none);
    auto limits=marc::core::DecoderLimits{};
    limits.max_internal_buffered_bytes=requirements.aggregate_working_bytes;
    limits.max_total_output_size=21;
    limits.max_frame_size=21;
    limits.max_block_size=21;
    ASSERT_EQ(decode_lzss_position_distance_stream(bytes,limits,tokens,scratch,output).error,Error::none);
    output.fill(std::byte{0xcc});
    --limits.max_internal_buffered_bytes;
    EXPECT_NE(decode_lzss_position_distance_stream(bytes,limits,tokens,scratch,output).error,Error::none);
    ++limits.max_internal_buffered_bytes; --limits.max_total_output_size;
    --limits.max_frame_size;
    EXPECT_NE(decode_lzss_position_distance_stream(bytes,limits,tokens,scratch,output).error,Error::none);
    for(auto b:output) EXPECT_EQ(b,std::byte{0xcc});
}

TEST(LzssPositionDistanceStreamDecoder, RejectsAllWorkspaceOverlapPairs) {
    for(unsigned pair=0;pair<6;++pair) {
        auto bytes=stream(1);
        std::array<LzssTypedToken,32> tokens{};
        std::array<std::byte,21> scratch{},output{};
        auto input=std::span<const std::byte>{bytes};
        std::span<std::byte> raw{scratch}; std::span<std::byte> out{output};
        auto token_bytes=std::as_writable_bytes(std::span{tokens});
        switch(pair) {
        case 0:
            ASSERT_GE(token_bytes.size(),bytes.size());
            std::memcpy(token_bytes.data(),bytes.data(),bytes.size());
            input=token_bytes.first(bytes.size()); break;
        case 1: raw=std::span{bytes}.first(21); break;
        case 2: out=std::span{bytes}.first(21); break;
        case 3: raw=token_bytes.first(21); break;
        case 4: out=token_bytes.first(21); break;
        case 5: out=raw; break;
        }
        const auto before=std::vector<std::byte>(out.begin(),out.end());
        EXPECT_EQ(decode_lzss_position_distance_stream(input,{},tokens,raw,out).error,Error::overlapping_workspaces);
        EXPECT_TRUE(std::equal(before.begin(),before.end(),out.begin()));
    }
}
TEST(LzssPositionDistanceStreamDecoder, LaterFrameContradictoryCountsPreserveWholeOutput) {
    // Independent fixed vectors: a valid first frame must not be published
    // when the following frame disagrees with its own descriptor or extent.
    struct Fault { std::size_t offset; std::uint32_t value; };
    constexpr std::array faults{
        Fault{16,0}, Fault{16,22}, Fault{20,0}, Fault{20,UINT32_MAX},
        Fault{24,0}, Fault{24,UINT32_MAX}, Fault{28,0},
        Fault{32,UINT32_MAX}, Fault{36,0}, Fault{64,39},
        Fault{68,19}, Fault{72,41}};
    for(const auto fault:faults) {
        SCOPED_TRACE(fault.offset);
        SCOPED_TRACE(fault.value);
        auto bytes=stream(2,true);
        constexpr std::size_t second=112+98;
        ASSERT_TRUE(marc::core::store_le(std::span{bytes},second+fault.offset,fault.value));
        std::array<LzssTypedToken,17> tokens{};
        std::array<std::byte,21> scratch{};
        std::array<std::byte,50> output; output.fill(std::byte{0xcc});
        const auto r=decode_lzss_position_distance_stream(bytes,{},tokens,scratch,
            std::span{output}.subspan(1,48));
        EXPECT_EQ(r.error,Error::frame_error);
        EXPECT_EQ(r.error_offset,second); EXPECT_EQ(r.frame_count,1);
        EXPECT_EQ(r.raw_produced,0); EXPECT_EQ(r.serialized_consumed,0);
        EXPECT_TRUE(std::ranges::all_of(output,[](auto b){return b==std::byte{0xcc};}));
    }
}

TEST(LzssPositionDistanceStreamDecoder, DeclaredTerminationRejectsMissingAndExtraFrames) {
    for(unsigned fault=0;fault<4;++fault) {
        SCOPED_TRACE(fault);
        auto bytes=stream(2,true);
        std::size_t expected_offset{};
        auto expected_error=Error::frame_error;
        switch(fault) {
        case 0: // A declared empty stream cannot contain even a valid frame.
            ASSERT_TRUE(marc::core::store_le(std::span{bytes},40,UINT64_C(0)));
            expected_offset=112; expected_error=Error::trailing_data; break;
        case 1: // Full frames do not authorize an undeclared final short frame.
            ASSERT_TRUE(marc::core::store_le(std::span{bytes},40,UINT64_C(42)));
            expected_offset=112+2*98; expected_error=Error::trailing_data; break;
        case 2: // A missing final frame is not successful end-of-stream.
            bytes.resize(112+2*98); expected_offset=bytes.size(); break;
        case 3: // A short frame cannot appear before the declared final extent.
            ASSERT_TRUE(marc::core::store_le(std::span{bytes},40,UINT64_C(49)));
            expected_offset=112+2*98; break;
        }
        std::array<LzssTypedToken,17> tokens{};
        std::array<std::byte,21> scratch{};
        std::array<std::byte,49> output; output.fill(std::byte{0xcc});
        const auto r=decode_lzss_position_distance_stream(bytes,{},tokens,scratch,output);
        EXPECT_EQ(r.error,expected_error); EXPECT_EQ(r.error_offset,expected_offset);
        EXPECT_EQ(r.raw_produced,0); EXPECT_EQ(r.serialized_consumed,0);
        EXPECT_TRUE(std::ranges::all_of(output,[](auto b){return b==std::byte{0xcc};}));
    }
}
} // namespace

#include "frame/lzss_position_distance_workspace.hpp"
#include "frame/lzss_position_distance_frame_streaming_decoder.hpp"
#include "frame/lzss_position_distance_raw_stream_encoder.hpp"
#include "entropy/lzss_position_distance_range_encoder.hpp"
#include "entropy/lzss_position_distance_range_state.hpp"

#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <limits>
#include <vector>

namespace {
using namespace marc::frame::internal;
using Direction = LzssPositionDistanceWorkspaceDirection;
using Error = LzssPositionDistanceWorkspaceError;
using Requirements = LzssPositionDistanceWorkspaceRequirements;
using Views = LzssPositionDistanceWorkspaceViews;
using Decoder = LzssPositionDistanceFrameStreamingDecoder;
using Token = marc::dictionary::internal::LzssTypedToken;
using Operation = marc::context::internal::ModeledOperation;
constexpr std::size_t encoder_owner_bytes = 256; // Simulated private owner, not a frozen ABI.
TypedContextStreamHeader stream_for(std::uint32_t n) {
    return {n, n, {65536,3,258,0},32768,40,8,1,9};
}
struct Storage {
    std::vector<std::byte> raw, serialized;
    std::vector<std::max_align_t> aligned;
    std::size_t bytes;
    explicit Storage(const Requirements& r)
        : raw(r.raw_bytes), serialized(r.serialized_bytes),
          aligned((r.views_bytes + sizeof(std::max_align_t) - 1) / sizeof(std::max_align_t)), bytes(r.views_bytes) {}
    std::span<std::byte> views() { return std::as_writable_bytes(std::span{aligned}).first(bytes); }
};

TEST(LzssPositionDistanceWorkspace, ComputesIndependentLayoutAndFixedCharges) {
    for (std::uint32_t n : {1U,2U,3U,21U,65536U}) for (auto direction : {Direction::encode,Direction::decode}) {
        SCOPED_TRACE(n);
        Requirements r{};
        ASSERT_EQ(calculate_lzss_position_distance_workspace(stream_for(n),{},direction,256,r),Error::none);
        EXPECT_EQ(r.raw_bytes,n); EXPECT_EQ(r.serialized_bytes,18*n+85);
        EXPECT_EQ(r.token_count,n);
        const auto decoder_model = sizeof(marc::entropy::internal::LzssPositionDistanceRangeState);
        EXPECT_EQ(r.model_state_bytes,direction==Direction::decode ? decoder_model : std::max(decoder_model,
            marc::entropy::internal::lzss_position_distance_range_encoder_state_bytes()));
        if (direction==Direction::encode) {
            EXPECT_EQ(r.operation_count,5*n);
            EXPECT_EQ(r.operation_offset%alignof(Operation),0);
            EXPECT_GE(r.operation_offset,n*sizeof(Token));
            EXPECT_LT(r.operation_offset-n*sizeof(Token),alignof(Operation));
            EXPECT_EQ(r.finder_offset%alignof(std::uint32_t),0);
            EXPECT_GE(r.finder_offset,r.operation_offset+5*n*sizeof(Operation));
            EXPECT_LT(r.finder_offset-(r.operation_offset+5*n*sizeof(Operation)),alignof(std::uint32_t));
            EXPECT_EQ(r.finder_bytes,n<3 ? 0 : (65536+n)*sizeof(std::uint32_t));
            EXPECT_EQ(r.views_bytes,r.finder_offset+r.finder_bytes);
        } else {
            EXPECT_EQ(r.operation_count,0); EXPECT_EQ(r.finder_bytes,0);
            EXPECT_EQ(r.operation_offset,0); EXPECT_EQ(r.finder_offset,0);
            EXPECT_EQ(r.views_bytes,n*sizeof(Token));
        }
        EXPECT_EQ(r.aggregate_bytes,r.raw_bytes+r.serialized_bytes+r.views_bytes+r.model_state_bytes+256);
        auto empty=stream_for(n); empty.original_size=0;
        Requirements empty_r{};
        ASSERT_EQ(calculate_lzss_position_distance_workspace(empty,{},direction,256,empty_r),Error::none);
        EXPECT_EQ(empty_r,r); // Capacity is local configuration, never incoming data.
    }
}

TEST(LzssPositionDistanceWorkspace, ExactAggregateAndOneByteLess) {
    for (auto direction : {Direction::encode,Direction::decode}) {
        const auto stream=stream_for(21);
        marc::core::DecoderLimits limits{}; limits.max_block_size=21;
        Requirements r{};
        ASSERT_EQ(calculate_lzss_position_distance_workspace(stream,limits,direction,256,r),Error::none);
        limits.max_internal_buffered_bytes=r.aggregate_bytes;
        Requirements exact{};
        ASSERT_EQ(calculate_lzss_position_distance_workspace(stream,limits,direction,256,exact),Error::none);
        EXPECT_EQ(exact,r);
        Storage storage(r); Views views{};
        EXPECT_EQ(partition_lzss_position_distance_workspace(stream,limits,direction,256,
            storage.raw,storage.serialized,storage.views(),views),Error::none);
        --limits.max_internal_buffered_bytes;
        EXPECT_EQ(calculate_lzss_position_distance_workspace(stream,limits,direction,256,exact),Error::limit_exceeded);
        EXPECT_EQ(exact,r);
    }
}

TEST(LzssPositionDistanceWorkspace, RejectsShortMisalignedAndAliasedStorageWithoutPublication) {
    for (auto direction : {Direction::encode,Direction::decode}) {
        auto stream=stream_for(21); Requirements r{};
        ASSERT_EQ(calculate_lzss_position_distance_workspace(stream,{},direction,256,r),Error::none);
        Storage storage(r);
        std::array<std::byte,1> sentinel{std::byte{0xa5}};
        Views views{}; views.raw=sentinel;
        const auto attempt=[&](auto raw,auto serialized,auto aligned) {
            auto result=partition_lzss_position_distance_workspace(stream,{},direction,256,raw,serialized,aligned,views);
            EXPECT_EQ(views.raw.data(),sentinel.data()); EXPECT_EQ(sentinel[0],std::byte{0xa5});
            return result;
        };
        EXPECT_EQ(attempt(std::span{storage.raw}.first(r.raw_bytes-1),std::span{storage.serialized},storage.views()),Error::too_small);
        EXPECT_EQ(attempt(std::span{storage.raw},std::span{storage.serialized}.first(r.serialized_bytes-1),storage.views()),Error::too_small);
        EXPECT_EQ(attempt(std::span{storage.raw},std::span{storage.serialized},storage.views().first(r.views_bytes-1)),Error::too_small);
        storage.aligned.resize(storage.aligned.size()+1);
        auto misaligned=std::as_writable_bytes(std::span{storage.aligned}).subspan(1,r.views_bytes);
        EXPECT_EQ(attempt(std::span{storage.raw},std::span{storage.serialized},misaligned),Error::misaligned);
        std::vector<std::max_align_t> large((r.views_bytes+r.serialized_bytes+r.raw_bytes)/sizeof(std::max_align_t)+1);
        auto shared=std::as_writable_bytes(std::span{large});
        EXPECT_EQ(attempt(shared.first(r.raw_bytes),shared.first(r.serialized_bytes),storage.views()),Error::overlapping_buffers);
        EXPECT_EQ(attempt(shared.first(r.raw_bytes),std::span{storage.serialized},shared.first(r.views_bytes)),Error::overlapping_buffers);
        EXPECT_EQ(attempt(std::span{storage.raw},shared.first(r.serialized_bytes),shared.first(r.views_bytes)),Error::overlapping_buffers);
    }
}

TEST(LzssPositionDistanceWorkspace, ChargesUnusedSuppliedCapacityAndPreservesGuards) {
    for (auto direction : {Direction::encode,Direction::decode}) {
        const auto stream=stream_for(21); Requirements r{};
        ASSERT_EQ(calculate_lzss_position_distance_workspace(stream,{},direction,256,r),Error::none);
        Storage storage(r); storage.raw.push_back(std::byte{0xa5}); Views views{};
        marc::core::DecoderLimits limits{}; limits.max_block_size=21; limits.max_internal_buffered_bytes=r.aggregate_bytes;
        EXPECT_EQ(partition_lzss_position_distance_workspace(stream,limits,direction,256,
            storage.raw,storage.serialized,storage.views(),views),Error::limit_exceeded);
        EXPECT_TRUE(views.raw.empty());
        ++limits.max_internal_buffered_bytes;
        ASSERT_EQ(partition_lzss_position_distance_workspace(stream,limits,direction,256,
            storage.raw,storage.serialized,storage.views(),views),Error::none);
        EXPECT_EQ(views.raw.size(),r.raw_bytes); EXPECT_EQ(storage.raw.back(),std::byte{0xa5});
        EXPECT_EQ(views.tokens.size(),r.token_count); EXPECT_EQ(views.operations.size(),r.operation_count);
        EXPECT_EQ(views.finder.size(),r.finder_bytes);
    }
}

TEST(LzssPositionDistanceWorkspace, RejectsInvalidIdentityLimitsAndOverflowTransactionally) {
    Requirements sentinel{}; sentinel.raw_bytes=123;
    for (unsigned which=0;which<7;++which) {
        auto stream=stream_for(21); marc::core::DecoderLimits limits{}; auto result=sentinel;
        switch(which) {
        case 0: stream.frame_size=0; break;
        case 1: stream.frame_size=65537; break;
        case 2: stream.context_variant=8; break;
        case 3: limits.max_block_size=20; break;
        case 4: limits.max_compressed_payload_size=18*21+4; break;
        case 5: limits.max_entropy_table_entries=2521; break;
        case 6: limits.max_range_model_total=32767; break;
        }
        EXPECT_NE(calculate_lzss_position_distance_workspace(stream,limits,Direction::encode,256,result),Error::none);
        EXPECT_EQ(result,sentinel);
    }
    auto result=sentinel;
    EXPECT_EQ(calculate_lzss_position_distance_workspace(stream_for(21),{},static_cast<Direction>(42),256,result),Error::invalid_configuration);
    EXPECT_EQ(result,sentinel);
    EXPECT_EQ(calculate_lzss_position_distance_workspace(stream_for(21),{},Direction::decode,
        std::numeric_limits<std::size_t>::max(),result),Error::arithmetic_overflow);
    EXPECT_EQ(result,sentinel);
}

TEST(LzssPositionDistanceWorkspace, ExactLayoutsEncodeAndIncrementallyDecode) {
    for (std::uint32_t n : {1U,2U,3U,21U,257U,65536U}) {
        SCOPED_TRACE(n);
        const auto stream=stream_for(n);
        marc::core::DecoderLimits encode_limits{},decode_limits{};
        encode_limits.max_block_size=n; decode_limits.max_block_size=n;
        Requirements e{},d{};
        ASSERT_EQ(calculate_lzss_position_distance_workspace(stream,encode_limits,Direction::encode,encoder_owner_bytes,e),Error::none);
        ASSERT_EQ(calculate_lzss_position_distance_workspace(stream,decode_limits,Direction::decode,sizeof(Decoder),d),Error::none);
        encode_limits.max_internal_buffered_bytes=e.aggregate_bytes;
        decode_limits.max_internal_buffered_bytes=d.aggregate_bytes;
        Storage es(e),ds(d); Views ev{},dv{};
        ASSERT_EQ(partition_lzss_position_distance_workspace(stream,encode_limits,Direction::encode,encoder_owner_bytes,
            es.raw,es.serialized,es.views(),ev),Error::none);
        ASSERT_EQ(partition_lzss_position_distance_workspace(stream,decode_limits,Direction::decode,sizeof(Decoder),
            ds.raw,ds.serialized,ds.views(),dv),Error::none);
        for(std::size_t i=0;i<n;++i) ev.raw[i]=std::byte(i%7);
        const auto encoded=encode_lzss_position_distance_raw_frame(stream,encode_limits,0,0,ev.raw,3,
            LzssPositionDistanceSearch::indexed,ev.tokens,ev.operations,ev.finder,ev.serialized);
        ASSERT_EQ(encoded.error,LzssPositionDistanceRawFrameError::none);
        std::vector<std::byte> archive(112+e.serialized_bytes);
        const auto written=encode_lzss_position_distance_raw_stream(stream,{},ev.raw,3,
            LzssPositionDistanceSearch::indexed,ev.tokens,ev.operations,ev.finder,archive);
        ASSERT_EQ(written.error,LzssPositionDistanceRawStreamError::none);
        archive.resize(written.serialized_size);
        Decoder decoder(decode_limits,dv.serialized,dv.tokens,dv.raw);
        std::vector<std::byte> output(n);
        std::size_t consumed{},produced{};
        for(std::size_t calls=0;calls<archive.size()+n+10;++calls) {
            const auto input=std::span<const std::byte>{archive}.subspan(consumed);
            auto result=decoder.process(input,std::span{output}.subspan(produced,std::min<std::size_t>(7,n-produced)),
                marc::core::flag_value(marc::core::ProcessFlags::end_input));
            ASSERT_NE(result.status,marc::core::StreamStatus::error);
            consumed+=result.input_consumed; produced+=result.output_produced;
            if(result.status==marc::core::StreamStatus::end_of_stream) break;
        }
        EXPECT_EQ(consumed,archive.size()); EXPECT_EQ(produced,n); EXPECT_EQ(output,es.raw);
    }
}
} // namespace

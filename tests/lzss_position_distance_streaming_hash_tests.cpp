#include "core/hash_tap.hpp"
#include "core/sha256.hpp"
#include "core/endian.hpp"
#include "frame/lzss_position_distance_frame_streaming_encoder.hpp"
#include "frame/lzss_position_distance_frame_streaming_decoder.hpp"
#include "frame/lzss_position_distance_raw_stream_encoder.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <vector>

namespace {
using namespace marc::frame::internal;
using Encoder=LzssPositionDistanceFrameStreamingEncoder;
using Decoder=LzssPositionDistanceFrameStreamingDecoder;
using Status=marc::core::StreamStatus;
using Digest=std::array<std::byte,32>;
constexpr auto end_flag=marc::core::flag_value(marc::core::ProcessFlags::end_input);
constexpr auto flush_flag=marc::core::flag_value(marc::core::ProcessFlags::flush);
Digest digest(std::span<const std::byte> input) {
    marc::core::Sha256 hash; Digest value{};
    EXPECT_TRUE(hash.update(input)); EXPECT_TRUE(hash.finalize(value)); return value;
}
struct Taps {
    marc::core::Sha256 input_hash,output_hash;
    marc::core::HashTap input{input_hash},output{output_hash};
};
struct Run {
    std::vector<std::byte> output;
    std::size_t consumed{};
    marc::core::ProcessResult last{};
};
Run drive(marc::core::Transform& transform,std::span<const std::byte> bytes,
    std::size_t chunk,std::size_t capacity,Taps& taps) {
    Run run{};
    for(std::size_t call=0;call<100000;++call) {
        const auto in=bytes.subspan(run.consumed,std::min(chunk,bytes.size()-run.consumed));
        std::array<std::byte,1026> out; out.fill(std::byte{0xcc});
        const auto writable=std::span{out}.subspan(1,call%5==0?0:capacity);
        run.last=transform.process(in,writable,flush_flag|((run.consumed+in.size()==bytes.size())?end_flag:0));
        EXPECT_TRUE(marc::core::is_valid(run.last,in.size(),writable.size()));
        EXPECT_EQ(out.front(),std::byte{0xcc});
        EXPECT_TRUE(std::ranges::all_of(std::span{out}.subspan(1+run.last.output_produced),[](auto b){return b==std::byte{0xcc};}));
        EXPECT_EQ(taps.input.commit(in,run.last.input_consumed),marc::core::HashTapStatus::ok);
        EXPECT_EQ(taps.output.commit(writable,run.last.output_produced),marc::core::HashTapStatus::ok);
        run.consumed+=run.last.input_consumed;
        run.output.insert(run.output.end(),out.begin()+1,out.begin()+1+run.last.output_produced);
        if(run.last.status==Status::error || run.last.status==Status::end_of_stream) {
            const auto again=transform.process({}, {},0);
            EXPECT_EQ(again.status,run.last.status);
            EXPECT_EQ(again.input_consumed,0); EXPECT_EQ(again.output_produced,0);
            EXPECT_EQ(taps.input.commit({},again.input_consumed),marc::core::HashTapStatus::ok);
            EXPECT_EQ(taps.output.commit({},again.output_produced),marc::core::HashTapStatus::ok);
            return run;
        }
    }
    ADD_FAILURE()<<"bounded hash driver did not terminate"; return run;
}
void verify(marc::core::HashTap& tap,std::span<const std::byte> bytes) {
    EXPECT_EQ(tap.total_committed(),bytes.size()); Digest actual{};
    ASSERT_EQ(tap.finalize(actual),marc::core::HashTapStatus::ok);
    EXPECT_EQ(actual,digest(bytes));
}
struct EncoderStorage {
    TypedContextStreamHeader stream{};
    std::vector<std::byte> raw,serialized;
    std::vector<std::max_align_t> storage;
    std::size_t view_bytes{};
    explicit EncoderStorage(std::size_t size) : stream{64,size,{65536,3,258,0},32768,40,8,1,9} {
        LzssPositionDistanceWorkspaceRequirements r{};
        EXPECT_EQ(calculate_lzss_position_distance_workspace(stream,{},LzssPositionDistanceWorkspaceDirection::encode,
            sizeof(Encoder),r),LzssPositionDistanceWorkspaceError::none);
        raw.resize(r.raw_bytes); serialized.resize(r.serialized_bytes); view_bytes=r.views_bytes;
        storage.resize((view_bytes+sizeof(std::max_align_t)-1)/sizeof(std::max_align_t));
    }
    std::span<std::byte> views() { return std::as_writable_bytes(std::span{storage}).first(view_bytes); }
};
std::vector<std::byte> oracle(std::span<const std::byte> raw) {
    EncoderStorage s(raw.size()); LzssPositionDistanceWorkspaceViews views{};
    EXPECT_EQ(partition_lzss_position_distance_workspace(s.stream,{},LzssPositionDistanceWorkspaceDirection::encode,
        sizeof(Encoder),s.raw,s.serialized,s.views(),views),LzssPositionDistanceWorkspaceError::none);
    std::vector<std::byte> encoded(4096);
    const auto result=encode_lzss_position_distance_raw_stream(s.stream,{},raw,3,LzssPositionDistanceSearch::indexed,
        views.tokens,views.operations,views.finder,encoded);
    EXPECT_EQ(result.error,LzssPositionDistanceRawStreamError::none);
    encoded.resize(result.serialized_size); return encoded;
}
struct DecoderStorage {
    std::array<std::byte,18*64+85> serialized{};
    std::array<std::byte,64> raw{};
    std::array<marc::dictionary::internal::LzssTypedToken,64> tokens{};
};

TEST(LzssPositionDistanceStreamingHash, CommitsExactRawAndCompleteWireBytesAcrossChunks) {
    Taps encoding,decoding;
    for(std::size_t size:{0U,1U,63U,64U,65U,128U}) for(std::size_t chunk:{1U,7U,1024U}) {
        SCOPED_TRACE(size);
        SCOPED_TRACE(chunk);
        std::vector<std::byte> raw(size);
        for(std::size_t i=0;i<size;++i) raw[i]=std::byte((i*71+i/11)%256);
        const auto expected=oracle(raw);
        encoding.input.reset(); encoding.output.reset(); decoding.input.reset(); decoding.output.reset();
        EncoderStorage es(size); Encoder encoder(es.stream,{},es.raw,es.serialized,es.views());
        const auto encoded=drive(encoder,raw,chunk,chunk,encoding);
        ASSERT_EQ(encoded.last.status,Status::end_of_stream); EXPECT_EQ(encoded.output,expected);
        verify(encoding.input,raw); verify(encoding.output,expected);
        DecoderStorage ds; Decoder decoder({},ds.serialized,ds.tokens,ds.raw);
        const auto decoded=drive(decoder,encoded.output,chunk,chunk,decoding);
        ASSERT_EQ(decoded.last.status,Status::end_of_stream); EXPECT_EQ(decoded.output,raw);
        verify(decoding.input,expected); verify(decoding.output,raw);
    }
}

TEST(LzssPositionDistanceStreamingHash, RejectedLaterFrameNeverContributesRawHashBytes) {
    std::vector<std::byte> raw(128,std::byte{'a'}); auto encoded=oracle(raw);
    std::uint32_t payload{};
    ASSERT_TRUE(marc::core::load_le(std::span<const std::byte>{encoded},112+32,payload));
    const auto second=112+80+payload;
    ASSERT_LT(second,encoded.size()); encoded[second]=std::byte{0};
    for(std::size_t chunk:{1U,7U,1024U}) {
        DecoderStorage s; Decoder decoder({},s.serialized,s.tokens,s.raw); Taps taps;
        const auto result=drive(decoder,encoded,chunk,chunk,taps);
        ASSERT_EQ(result.last.status,Status::error);
        EXPECT_EQ(result.last.error.byte_position,second);
        EXPECT_EQ(result.output,std::vector<std::byte>(64,std::byte{'a'}));
        verify(taps.output,std::span{raw}.first(64));
        // Consumed malformed bytes are still committed at the input boundary;
        // input acceptance is not the same as successful frame validation.
        verify(taps.input,std::span{encoded}.first(result.consumed));
    }
}

TEST(LzssPositionDistanceStreamingHash, TruncatedFinalFrameHashesOnlyValidatedPrefix) {
    std::vector<std::byte> raw(65,std::byte{'a'}); auto encoded=oracle(raw); encoded.pop_back();
    for(std::size_t chunk:{1U,7U,1024U}) {
        DecoderStorage s; Decoder decoder({},s.serialized,s.tokens,s.raw); Taps taps;
        const auto result=drive(decoder,encoded,chunk,chunk,taps);
        ASSERT_EQ(result.last.status,Status::error); EXPECT_EQ(result.output.size(),64);
        verify(taps.output,std::span{raw}.first(64));
        verify(taps.input,std::span{encoded}.first(result.consumed));
    }
}
} // namespace

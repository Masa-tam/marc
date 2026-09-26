#include "core/sha256.hpp"
#include "dictionary/lzss_short_prefix_match_finder.hpp"
#include "frame/lzss_position_distance_raw_stream_encoder.hpp"
#include "frame/lzss_position_distance_stream_decoder.hpp"
#include "frame/lzss_position_distance_frame_streaming_encoder.hpp"
#include "frame/lzss_position_distance_frame_streaming_decoder.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <span>
#include <string_view>
#include <system_error>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;
using Digest = std::array<std::byte,32>;
bool number(std::string_view text, std::size_t low, std::size_t high, std::size_t& value) {
    const auto r=std::from_chars(text.data(),text.data()+text.size(),value);
    return r.ec==std::errc{} && r.ptr==text.data()+text.size() && value>=low && value<=high;
}
bool digest(std::span<const std::byte> bytes, Digest& out) {
    marc::core::Sha256 hash;
    return hash.update(bytes) && hash.finalize(out);
}
void print_digest(const char* name, const Digest& value) {
    std::cout << name << '=' << std::hex << std::setfill('0');
    for(auto byte:value) std::cout << std::setw(2) << std::to_integer<unsigned>(byte);
    std::cout << std::dec << std::setfill(' ') << '\n';
}
double seconds(Clock::time_point begin, Clock::time_point end) {
    return std::chrono::duration<double>(end-begin).count();
}
bool drive(marc::core::Transform& transform, std::span<const std::byte> input,
    std::span<std::byte> output, std::size_t input_chunk, std::size_t output_chunk) {
    std::size_t consumed{}, produced{};
    // Input and archive caps bound this sum well below size_t's limit.
    for(std::size_t call=0;call<input.size()+output.size()+4;++call) {
        const auto in=input.subspan(consumed,std::min(input_chunk,input.size()-consumed));
        const auto out=output.subspan(produced,std::min(output_chunk,output.size()-produced));
        const auto flags=consumed+in.size()==input.size()
            ? marc::core::flag_value(marc::core::ProcessFlags::end_input) : 0;
        const auto result=transform.process(in,out,flags);
        if(!marc::core::is_valid(result,in.size(),out.size())
           || result.status==marc::core::StreamStatus::error) return false;
        consumed+=result.input_consumed; produced+=result.output_produced;
        if(result.status==marc::core::StreamStatus::end_of_stream)
            return consumed==input.size() && produced==output.size();
        if(result.input_consumed==0 && result.output_produced==0) return false;
    }
    return false;
}
struct Storage {
    marc::frame::internal::LzssPositionDistanceWorkspaceRequirements requirements{};
    std::vector<std::byte> raw,serialized;
    std::vector<std::max_align_t> aligned;
    void allocate() {
        raw.resize(requirements.raw_bytes); serialized.resize(requirements.serialized_bytes);
        aligned.resize((requirements.views_bytes+sizeof(std::max_align_t)-1)/sizeof(std::max_align_t));
    }
    std::span<std::byte> views() {
        return std::as_writable_bytes(std::span{aligned}).first(requirements.views_bytes);
    }
};
}

int main(int argc, const char* const argv[]) {
    if(argc!=7 && argc!=10) {
        std::cerr << "usage: marc_lzss_position_distance_stream_benchmark <input> "
            "<frame-bytes:1..65536> <eligibility:3..5> <indexed|reference> "
            "<iterations:1..10> <new-output-file> [incremental <input-chunk:1..65536> <output-chunk:1..65536>]\n";
        return 2;
    }
    std::size_t frame_bytes{}, eligibility{}, iterations{};
    const bool incremental=argc==10;
    std::size_t input_chunk{}, output_chunk{};
    if(incremental && (std::string_view{argv[7]}!="incremental"
        || !number(argv[8],1,65536,input_chunk) || !number(argv[9],1,65536,output_chunk))) return 2;
    const std::string_view search_name{argv[4]};
    if(!number(argv[2],1,65536,frame_bytes) || !number(argv[3],3,5,eligibility)
       || !number(argv[5],1,10,iterations)
       || (search_name!="indexed" && search_name!="reference")) return 2;
    std::error_code ec;
    if(std::filesystem::exists(argv[6],ec) || ec) {
        std::cerr << "output must be a new file\n"; return 2;
    }
    std::ifstream input(argv[1],std::ios::binary|std::ios::ate);
    if(!input) return 2;
    const auto extent=input.tellg();
    if(extent<0 || extent>static_cast<std::streamoff>(64U*1024U*1024U)) return 2;
    const auto raw_size=static_cast<std::size_t>(extent);
    const auto frame_count=raw_size/frame_bytes+(raw_size%frame_bytes!=0);
    if(frame_count>1024) { std::cerr << "at most 1024 complete frames\n"; return 2; }
    std::vector<std::byte> raw(raw_size), restored(raw_size);
    input.seekg(0);
    if(raw_size!=0 && !input.read(reinterpret_cast<char*>(raw.data()),
        static_cast<std::streamsize>(raw_size))) return 2;
    using namespace marc::frame::internal;
    using namespace marc::dictionary::internal;
    using marc::context::internal::ModeledOperation;
    const marc::core::DecoderLimits limits{};
    const TypedContextStreamHeader stream{static_cast<std::uint32_t>(frame_bytes),raw_size,
        {65536,3,258,0},32768,40,8,1,9};
    const auto search=search_name=="indexed" ? LzssPositionDistanceSearch::indexed
                                           : LzssPositionDistanceSearch::reference;
    std::size_t finder_bytes{};
    if(search==LzssPositionDistanceSearch::indexed && raw_size!=0) {
        const auto needed=calculate_lzss_short_prefix_workspace(std::min(frame_bytes,raw_size),
            stream.dictionary,limits,LzssTypedTokenVariant::field_context_64k_short_length_escape);
        if(needed.error!=LzssShortPrefixError::none) return 1;
        finder_bytes=needed.workspace_size;
    }
    std::vector<std::max_align_t> finder_storage(
        (finder_bytes+sizeof(std::max_align_t)-1)/sizeof(std::max_align_t));
    const auto finder=std::as_writable_bytes(std::span{finder_storage}).first(finder_bytes);
    std::vector<LzssTypedToken> tokens(frame_bytes);
    std::vector<ModeledOperation> operations(5*frame_bytes);
    std::vector<std::byte> frame(frame_bytes);
    const auto plan_begin=Clock::now();
    const auto plan=plan_lzss_position_distance_raw_stream(stream,limits,raw,
        static_cast<std::uint32_t>(eligibility),search,tokens,operations,finder);
    const auto plan_end=Clock::now();
    if(plan.error!=LzssPositionDistanceRawStreamError::none || plan.frame_count!=frame_count
       || plan.serialized_size>128U*1024U*1024U) return 1;
    std::vector<std::byte> encoded(plan.serialized_size);
    std::vector<std::byte> expected;
    using Encoder=LzssPositionDistanceFrameStreamingEncoder;
    using Decoder=LzssPositionDistanceFrameStreamingDecoder;
    Storage encoder_storage,decoder_storage;
    LzssPositionDistanceWorkspaceViews decoder_views{};
    if(incremental) {
        expected.resize(encoded.size());
        const auto oracle=encode_lzss_position_distance_raw_stream(stream,limits,raw,
            static_cast<std::uint32_t>(eligibility),search,tokens,operations,finder,expected);
        if(oracle.error!=LzssPositionDistanceRawStreamError::none
            || oracle.serialized_size!=expected.size() || oracle.frame_count!=frame_count) return 1;
        if(calculate_lzss_position_distance_workspace(stream,limits,LzssPositionDistanceWorkspaceDirection::encode,
                sizeof(Encoder),encoder_storage.requirements)!=LzssPositionDistanceWorkspaceError::none
           || calculate_lzss_position_distance_workspace(stream,limits,LzssPositionDistanceWorkspaceDirection::decode,
                sizeof(Decoder),decoder_storage.requirements)!=LzssPositionDistanceWorkspaceError::none) return 1;
        encoder_storage.allocate(); decoder_storage.allocate();
        if(partition_lzss_position_distance_workspace(stream,limits,LzssPositionDistanceWorkspaceDirection::decode,
            sizeof(Decoder),decoder_storage.raw,decoder_storage.serialized,decoder_storage.views(),decoder_views)
            !=LzssPositionDistanceWorkspaceError::none) return 1;
    }
    std::array<double,10> encode_seconds{}, decode_seconds{};
    Digest input_digest{}, archive_digest{}, previous_digest{};
    if(!digest(raw,input_digest)) return 1;
    for(std::size_t i=0;i<iterations;++i) {
        if(incremental) {
            const auto encode_begin=Clock::now();
            Encoder encoder(stream,limits,encoder_storage.raw,encoder_storage.serialized,
                encoder_storage.views(),static_cast<std::uint32_t>(eligibility),search);
            const bool encoded_ok=drive(encoder,raw,encoded,input_chunk,output_chunk);
            const auto encode_end=Clock::now();
            if(!encoded_ok || encoder.frame_preparation_count()!=frame_count || encoded!=expected) return 1;
            const auto decode_begin=Clock::now();
            Decoder decoder(limits,decoder_views.serialized,decoder_views.tokens,decoder_views.raw);
            const bool decoded_ok=drive(decoder,encoded,restored,input_chunk,output_chunk);
            const auto decode_end=Clock::now();
            if(!decoded_ok || raw!=restored || !digest(encoded,archive_digest)
               || (i!=0 && archive_digest!=previous_digest)) return 1;
            previous_digest=archive_digest;
            encode_seconds[i]=seconds(encode_begin,encode_end);
            decode_seconds[i]=seconds(decode_begin,decode_end);
            continue;
        }
        const auto encode_begin=Clock::now();
        const auto written=encode_lzss_position_distance_raw_stream(stream,limits,raw,
            static_cast<std::uint32_t>(eligibility),search,tokens,operations,finder,encoded);
        const auto encode_end=Clock::now();
        if(written.error!=LzssPositionDistanceRawStreamError::none
           || written.serialized_size!=encoded.size() || written.frame_count!=frame_count) return 1;
        const auto decode_begin=Clock::now();
        const auto decoded=decode_lzss_position_distance_stream(encoded,limits,tokens,frame,restored);
        const auto decode_end=Clock::now();
        if(decoded.error!=LzssShortMatchStreamDecodeError::none
           || decoded.raw_produced!=raw_size || decoded.serialized_consumed!=encoded.size()
           || decoded.frame_count!=frame_count || raw!=restored || !digest(encoded,archive_digest)
           || (i!=0 && archive_digest!=previous_digest)) return 1;
        previous_digest=archive_digest;
        encode_seconds[i]=seconds(encode_begin,encode_end);
        decode_seconds[i]=seconds(decode_begin,decode_end);
    }
    std::ofstream output(argv[6],std::ios::binary);
    if(!output) return 2;
    output.write(reinterpret_cast<const char*>(encoded.data()),static_cast<std::streamsize>(encoded.size()));
    output.close();
    if(!output) return 2;
    std::cout << std::fixed << std::setprecision(9)
        << "mode=position-distance-stream\nsearch=" << search_name
        << "\ninput_bytes=" << raw_size << "\narchive_bytes=" << encoded.size()
        << "\nframe_bytes=" << frame_bytes << "\nframe_count=" << frame_count
        << "\neligibility=" << eligibility << "\niterations=" << iterations
        << "\nplan_seconds=" << seconds(plan_begin,plan_end)
        << (incremental?"\noracle_encode_scratch_bytes=":"\nencode_scratch_bytes=") << tokens.size()*sizeof(LzssTypedToken)
            +operations.size()*sizeof(ModeledOperation)+finder.size()
        << (incremental?"\noracle_decode_scratch_bytes=":"\ndecode_scratch_bytes=") << tokens.size()*sizeof(LzssTypedToken)+frame.size()
        << "\ninput_buffer_bytes=" << raw.size() << "\narchive_buffer_bytes=" << encoded.size()
        << "\nrestored_buffer_bytes=" << restored.size() << '\n';
    std::cout << "processing=" << (incremental?"incremental":"one-shot")
        << "\ninput_chunk_bytes=" << input_chunk << "\noutput_chunk_bytes=" << output_chunk
        << "\noracle_buffer_bytes=" << expected.size() << '\n';
    if(incremental) {
        std::cout << "encoder_aggregate_bytes=" << encoder_storage.requirements.aggregate_bytes
            << "\ndecoder_aggregate_bytes=" << decoder_storage.requirements.aggregate_bytes
            << "\nencoder_model_state_bytes=" << encoder_storage.requirements.model_state_bytes
            << "\ndecoder_model_state_bytes=" << decoder_storage.requirements.model_state_bytes
            << "\nencoder_owner_bytes=" << sizeof(Encoder) << "\ndecoder_owner_bytes=" << sizeof(Decoder)
            << "\nframe_preparations=" << frame_count << "\noracle_byte_equal=1\n";
    }
    print_digest("input_sha256",input_digest);
    print_digest("archive_sha256",archive_digest);
    for(std::size_t i=0;i<iterations;++i)
        std::cout << "iteration_" << i << "_encode_seconds=" << encode_seconds[i]
            << "\niteration_" << i << "_decode_seconds=" << decode_seconds[i] << '\n';
    std::cout << "verified_iterations=" << iterations << '\n';
    return 0;
}

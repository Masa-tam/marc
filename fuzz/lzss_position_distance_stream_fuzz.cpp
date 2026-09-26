#include "frame/lzss_position_distance_stream_decoder.hpp"
#include "frame/lzss_position_distance_raw_stream_encoder.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>

namespace {
constexpr std::size_t max_input = 4096;
constexpr std::size_t max_output = 128;
constexpr std::byte sentinel{0xcc};

marc::core::DecoderLimits limits() noexcept {
    marc::core::DecoderLimits l{};
    l.max_total_output_size = max_output;
    l.max_frame_size = l.max_block_size = 64;
    l.max_compressed_payload_size = 18 * 64 + 5;
    l.max_internal_buffered_bytes = 1 << 20;
    l.max_lz_distance = 65536;
    l.max_lz_match_length = 258;
    l.max_entropy_table_entries = 2522;
    l.max_range_model_total = 32768;
    return l;
}
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    if (size > max_input) return 0;
    std::array<marc::dictionary::internal::LzssTypedToken,64> tokens{};
    std::array<std::byte,64> frame{};
    std::array<std::byte,max_output+2> output;
    output.fill(sentinel);
    const auto input = std::as_bytes(std::span{data,size});
    const auto result = marc::frame::internal::decode_lzss_position_distance_stream(
        input,limits(),tokens,frame,std::span{output}.subspan(1,max_output));
    if (output.front()!=sentinel || output.back()!=sentinel) std::abort();
    if (result.error!=marc::frame::internal::LzssShortMatchStreamDecodeError::none) {
        if (result.raw_produced!=0 || result.serialized_consumed!=0
            || !std::ranges::all_of(output,[](auto b){return b==sentinel;})) std::abort();
    } else if (result.serialized_consumed!=size || result.raw_produced>max_output
        || !std::ranges::all_of(std::span{output}.subspan(1+result.raw_produced),
            [](auto b){return b==sentinel;})) std::abort();
    if (size <= max_output) {
        using namespace marc::frame::internal;
        TypedContextStreamHeader stream{64,size,{65536,3,258,0},32768,40,8,1,9};
        std::array<marc::context::internal::ModeledOperation,320> operations{};
        std::array<std::byte,3000> encoded{};
        const auto written = encode_lzss_position_distance_raw_stream(
            stream,limits(),input,3,LzssPositionDistanceSearch::reference,
            tokens,operations,{},encoded);
        if (written.error!=LzssPositionDistanceRawStreamError::none
            || written.serialized_size>=encoded.size()) std::abort();
        const auto valid = std::span{encoded}.first(written.serialized_size);
        output.fill(sentinel);
        const auto round_trip = decode_lzss_position_distance_stream(valid,limits(),
            tokens,frame,std::span{output}.subspan(1,max_output));
        if (round_trip.error!=LzssShortMatchStreamDecodeError::none
            || round_trip.raw_produced!=size || round_trip.serialized_consumed!=valid.size()
            || !std::ranges::equal(std::span{output}.subspan(1,size),input)) std::abort();
        if (size != 0) {
            const auto offset=(static_cast<std::size_t>(data[0])*257+data[size-1])%valid.size();
            encoded[offset]^=std::byte{static_cast<unsigned char>(1U+data[size/2]%255U)};
            output.fill(sentinel);
            const auto changed=decode_lzss_position_distance_stream(valid,limits(),
                tokens,frame,std::span{output}.subspan(1,max_output));
            if (output.front()!=sentinel || output.back()!=sentinel) std::abort();
            if (changed.error!=LzssShortMatchStreamDecodeError::none
                && (changed.raw_produced!=0 || changed.serialized_consumed!=0
                    || !std::ranges::all_of(output,[](auto b){return b==sentinel;})))
                std::abort();
        }
    }
    return 0;
}

#ifdef MARC_POSITION_DISTANCE_FUZZ_SMOKE
int main() {
    std::array<std::uint8_t,4097> input{};
    for (const auto size:{0U,1U,63U,64U,65U,128U,129U,4096U,4097U}) {
        for (unsigned pattern=0;pattern<3;++pattern) {
            for (std::size_t i=0;i<input.size();++i)
                input[i]=static_cast<std::uint8_t>(pattern==0 ? 0 : pattern==1 ? i%7 : i*71+i/11);
            LLVMFuzzerTestOneInput(input.data(),size);
        }
    }
}
#endif

#include "entropy/position_rans_64k_encoder.hpp"
#include "entropy/position_rans_64k_decoder.hpp"
#include "entropy/position_rans_1m_encoder.hpp"
#include "entropy/position_rans_1m_decoder.hpp"
#include "entropy/position_rans_4m_encoder.hpp"
#include "entropy/position_rans_4m_decoder.hpp"
#include "entropy/position_rans_4m_full_literal_encoder.hpp"
#include "entropy/position_rans_4m_full_literal_decoder.hpp"
#include "entropy/position_rans_8m_full_literal_encoder.hpp"
#include "entropy/position_rans_8m_full_literal_decoder.hpp"
#include "entropy/position_rans_16m_full_literal_encoder.hpp"
#include "entropy/position_rans_16m_full_literal_decoder.hpp"
#include "entropy/position_rans_32m_full_literal_encoder.hpp"
#include "entropy/position_rans_32m_full_literal_decoder.hpp"
#include "entropy/position_rans_64m_full_literal_encoder.hpp"
#include "entropy/position_rans_64m_full_literal_decoder.hpp"
#include "entropy/position_tans_64k.hpp"
#include "entropy/position_tans_1m.hpp"
#include "entropy/position_tans_4m.hpp"
#include "entropy/position_tans_8m.hpp"
#include "entropy/position_tans_16m.hpp"
#include "entropy/position_tans_32m.hpp"
#include "entropy/position_tans_64m.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
using namespace marc::entropy::internal;
void require(bool condition) {
    if (!condition) throw std::runtime_error("ANS applicability check failed");
}
template<class Error> void ok(Error error) { require(static_cast<unsigned>(error)==0); }

// Independent observations: context0 sees one hundred zeros, context1 one
// hundred ones. Alternating their order exercises a real ANS state trajectory.
template<class Builder, class Descriptor> Descriptor independent() {
    Builder builder;
    for (int i=0;i<200;++i) ok(builder.add(i%2,static_cast<unsigned>(i%2)));
    Descriptor result; ok(builder.finish(result));
    require(result.decision_count==200);
    require(result.frequencies[0]==4096 && result.frequencies[1]==0);
    require(result.frequencies[2]==0 && result.frequencies[3]==4096);
    require(std::all_of(result.frequencies.begin()+4,result.frequencies.end(),
        [](auto frequency) { return frequency==0; }));
    Descriptor again; ok(builder.finish(again)); require(again==result);
    ok(builder.add(-1,0)); ok(builder.add(-1,1));
    Descriptor uniform; ok(builder.finish(uniform));
    require(uniform.decision_count==202 && uniform.frequencies==result.frequencies);
    return result;
}
template<class Descriptor> Descriptor repeated_pool(Descriptor original) {
    // Both context slots must still be serialized. There is no model-reference
    // rule in these formats; this intentionally tests repeated equal models.
    for (std::size_t i=0;i<4;++i) original.frequencies[i]=2048;
    return original;
}

#define RANS_TRAIT(Name, Stem, Layout) \
struct Name##Probe { \
    using Descriptor=Name##Descriptor; using Builder=Name##ModelBuilder; \
    using Writer=Name##ReverseWriter; using Decoder=Name##Decoder; \
    static constexpr auto offsets=marc::context::internal::Layout; \
    static constexpr auto capacity=Stem##_descriptor_capacity; \
    static auto serialize(const Descriptor& d,std::span<std::byte> bytes,std::size_t& n) { \
        return serialize_##Stem##_descriptor(d,{},bytes,n); } \
};
RANS_TRAIT(PositionRans64k,position_rans_64k,lzss_position_distance_offsets)
RANS_TRAIT(PositionRans1m,position_rans_1m,lzss_position_distance_1m_offsets)
RANS_TRAIT(PositionRans4m,position_rans_4m,lzss_position_distance_4m_offsets)
RANS_TRAIT(PositionRans4mFullLiteral,position_rans_4m_full_literal,lzss_position_distance_4m_full_literal_offsets)
RANS_TRAIT(PositionRans8mFullLiteral,position_rans_8m_full_literal,lzss_position_distance_8m_full_literal_offsets)
RANS_TRAIT(PositionRans16mFullLiteral,position_rans_16m_full_literal,lzss_position_distance_16m_full_literal_offsets)
RANS_TRAIT(PositionRans32mFullLiteral,position_rans_32m_full_literal,lzss_position_distance_32m_full_literal_offsets)
RANS_TRAIT(PositionRans64mFullLiteral,position_rans_64m_full_literal,lzss_position_distance_64m_full_literal_offsets)
#undef RANS_TRAIT

struct Extent {
    std::size_t descriptor{},payload{};
    std::size_t total() const { return descriptor+payload; }
};
template<class Profile> Extent rans_extent(typename Profile::Descriptor d) {
    typename Profile::Writer measure(d,{},{});
    for (int i=199;i>=0;--i) ok(measure.write(i%2,static_cast<unsigned>(i%2)));
    std::size_t payload{}; ok(measure.finish(payload));
    d.payload_size=static_cast<std::uint32_t>(payload);
    std::vector<std::byte> wire(payload),description(Profile::capacity);
    typename Profile::Writer writer(d,{},wire);
    for (int i=199;i>=0;--i) ok(writer.write(i%2,static_cast<unsigned>(i%2)));
    std::size_t written{}; ok(writer.finish(written)); require(written==payload);
    std::size_t extent{}; ok(Profile::serialize(d,description,extent)); description.resize(extent);
    typename Profile::Decoder decoder; ok(decoder.begin(description,200,wire,{}));
    for (int i=0;i<200;++i) {
        std::uint32_t symbol=77; ok(decoder.read(i%2,symbol)); require(symbol==static_cast<unsigned>(i%2));
    }
    ok(decoder.finish());
    return {extent,payload};
}

#define TANS_TRAIT(Name, Stem) \
struct Name##Probe { \
    using Descriptor=Name##Descriptor; using Builder=Name##ModelBuilder; \
    using Writer=Name##ReverseWriter; using Decoder=Name##Decoder; \
    static constexpr auto offsets=Stem##_offsets; \
    static constexpr auto capacity=Stem##_descriptor_capacity; \
    static constexpr auto entries=Stem##_table_entries; \
    static auto serialize(const Descriptor& d,std::span<std::byte> bytes,std::size_t& n) { \
        return serialize_##Stem##_descriptor(d,{},bytes,n); } \
    static auto tables(const Descriptor& d,std::span<std::uint16_t> out) { \
        return build_##Stem##_encode_tables(d,{},out); } \
    static auto parse(std::span<const std::byte> bytes,const Descriptor& d,Descriptor& out) { \
        return parse_##Stem##_descriptor(bytes,d.decision_count,d.payload_size,{},out); } \
};
TANS_TRAIT(PositionTans64k,position_tans_64k)
TANS_TRAIT(PositionTans1m,position_tans_1m)
TANS_TRAIT(PositionTans4m,position_tans_4m)
TANS_TRAIT(PositionTans8m,position_tans_8m)
TANS_TRAIT(PositionTans16m,position_tans_16m)
TANS_TRAIT(PositionTans32m,position_tans_32m)
TANS_TRAIT(PositionTans64m,position_tans_64m)
#undef TANS_TRAIT

template<class Profile> Extent tans_extent(typename Profile::Descriptor d) {
    std::vector<std::uint16_t> tables(Profile::entries); ok(Profile::tables(d,tables));
    typename Profile::Writer measure(d,{},tables,{});
    for (int i=199;i>=0;--i) ok(measure.add(i%2,static_cast<unsigned>(i%2)));
    std::size_t payload{}; std::uint8_t valid{}; ok(measure.finish(payload,valid));
    d.payload_size=static_cast<std::uint32_t>(payload); d.final_valid_bits=valid;
    std::vector<std::byte> wire(payload),description(Profile::capacity);
    typename Profile::Writer writer(d,{},tables,wire);
    for (int i=199;i>=0;--i) ok(writer.add(i%2,static_cast<unsigned>(i%2)));
    std::size_t written{}; std::uint8_t actual_valid{}; ok(writer.finish(written,actual_valid));
    require(written==payload && actual_valid==valid);
    std::size_t extent{}; ok(Profile::serialize(d,description,extent)); description.resize(extent);
    typename Profile::Descriptor parsed; ok(Profile::parse(description,d,parsed)); require(parsed==d);
    std::vector<TansDecodeEntry> decoding(Profile::entries);
    typename Profile::Decoder decoder; ok(decoder.begin(parsed,wire,{},decoding));
    for (int i=0;i<200;++i) {
        std::uint32_t symbol=77; ok(decoder.read(i%2,symbol)); require(symbol==static_cast<unsigned>(i%2));
    }
    ok(decoder.finish());
    return {extent,payload};
}

template<class Profile> void run(const char* name,bool tans) {
    const auto original=independent<typename Profile::Builder,typename Profile::Descriptor>();
    const auto pooled=repeated_pool(original);
    Extent before{},after{}; std::size_t entries{};
    if constexpr (requires { Profile::entries; }) {
        before=tans_extent<Profile>(original); after=tans_extent<Profile>(pooled); entries=Profile::entries;
    } else {
        before=rans_extent<Profile>(original); after=rans_extent<Profile>(pooled);
    }
    require(after.total()>before.total());
    require(after.descriptor==before.descriptor+2 && after.payload==before.payload+25);
    std::cout << "{\"profile\":\"" << name << "\",\"family\":\"" << (tans?"tans":"rans")
        << "\",\"contexts\":" << Profile::offsets.size()-1
        << ",\"frequency_entries\":" << original.frequencies.size()
        << ",\"descriptor_capacity\":" << Profile::capacity
        << ",\"table_entries\":" << entries
        << ",\"independent_complete_bytes\":" << before.total()
        << ",\"repeated_pool_complete_bytes\":" << after.total()
        << ",\"independent_descriptor_bytes\":" << before.descriptor
        << ",\"repeated_pool_descriptor_bytes\":" << after.descriptor
        << ",\"independent_payload_bytes\":" << before.payload
        << ",\"repeated_pool_payload_bytes\":" << after.payload
        << ",\"restored\":true,\"uniform_did_not_change_models\":true}\n";
}
}

int main() {
    try {
        run<PositionRans64kProbe>("rans-64k",false);
        run<PositionRans1mProbe>("rans-1m",false);
        run<PositionRans4mProbe>("rans-4m-compact-retained",false);
        run<PositionRans4mFullLiteralProbe>("rans-4m",false);
        run<PositionRans8mFullLiteralProbe>("rans-8m",false);
        run<PositionRans16mFullLiteralProbe>("rans-16m",false);
        run<PositionRans32mFullLiteralProbe>("rans-32m",false);
        run<PositionRans64mFullLiteralProbe>("rans-64m",false);
        run<PositionTans64kProbe>("tans-64k",true);
        run<PositionTans1mProbe>("tans-1m",true);
        run<PositionTans4mProbe>("tans-4m",true);
        run<PositionTans8mProbe>("tans-8m",true);
        run<PositionTans16mProbe>("tans-16m",true);
        run<PositionTans32mProbe>("tans-32m",true);
        run<PositionTans64mProbe>("tans-64m",true);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}

#include "entropy/position_tans_64k.hpp"
#include "core/endian.hpp"
#include <algorithm>
#include <cstdlib>
#include <span>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,std::size_t size) {
    using namespace marc::entropy::internal;
    if (size>position_tans_64k_descriptor_capacity+1) return 0;
    const auto bytes=std::as_bytes(std::span(data,size));
    std::uint32_t decisions{},payload{};
    (void)marc::core::load_le(bytes,0,decisions);
    (void)marc::core::load_le(bytes,4,payload);
    PositionTans64kDescriptor sentinel{};sentinel.frequencies.fill(77);
    sentinel.decision_count=777;
    auto decoded=sentinel;
    const auto error=parse_position_tans_64k_descriptor(bytes,decisions,payload,{},decoded);
    if (error!=PositionTans64kError::none) {
        if (decoded!=sentinel) std::abort();
        return 0;
    }
    std::array<std::byte,position_tans_64k_descriptor_capacity> output{};
    std::size_t count{};
    if (serialize_position_tans_64k_descriptor(decoded,{},output,count)!=PositionTans64kError::none
        || count!=bytes.size() || !std::equal(bytes.begin(),bytes.end(),output.begin())) std::abort();
    return 0;
}

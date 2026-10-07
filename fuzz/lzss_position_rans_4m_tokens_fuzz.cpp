#include "context/lzss_position_rans_4m_tokens.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>

namespace {
using namespace marc::context::internal;
using namespace marc::dictionary::internal;
void compare(std::span<const std::byte> descriptor,std::span<const std::byte> payload,
    const LzssFieldContextValidationContext& context,const marc::core::DecoderLimits& limits) {
    constexpr LzssParameters p{4194304,3,258,0};
    const LzssTypedToken sentinel{LzssTypedTokenKind::literal,0xcc,0,0};
    std::array<LzssTypedToken,130> transactional{}, scratch{};
    transactional.fill(sentinel); scratch.fill(sentinel);
    auto a = std::span(transactional).subspan(1,context.declared_token_count);
    auto b = std::span(scratch).subspan(1,context.declared_token_count);
    const auto validated = validate_lzss_position_rans_4m_tokens(descriptor,payload,p,context,limits);
    const auto x = decode_lzss_position_rans_4m_tokens(descriptor,payload,p,context,limits,a);
    const auto y = decode_lzss_position_rans_4m_token_scratch(descriptor,payload,p,context,limits,b);
    if (validated.error != x.error || x.error != y.error
        || transactional.front().literal != 0xcc || transactional.back().literal != 0xcc
        || scratch.front().literal != 0xcc || scratch.back().literal != 0xcc) std::abort();
    if (x.error != LzssPositionRans4mTokenError::none) {
        if (!std::all_of(a.begin(),a.end(),[](auto t) { return t.literal == 0xcc && t.distance == 0 && t.length == 0; })) std::abort();
    } else {
        for (std::size_t i = 0; i < a.size(); ++i) {
            if (a[i].kind != b[i].kind || a[i].literal != b[i].literal
                || a[i].distance != b[i].distance || a[i].length != b[i].length) std::abort();
        }
    }
}
}
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,std::size_t size) {
    if (size > 8192) return 0;
    const auto input = std::as_bytes(std::span(data,size));
    marc::core::DecoderLimits limits{};
    limits.max_frame_size = 384; limits.max_total_output_size = 768;
    limits.max_block_size = 4096; limits.max_internal_buffered_bytes = 1<<20;
    std::array<LzssTypedToken,128> tokens{};
    std::uint32_t raw{};
    const auto count = std::min<std::size_t>(size,128);
    for (std::size_t i = 0; i < count; ++i) {
        if (i && (data[i]&1)) { tokens[i] = {LzssTypedTokenKind::match,0,1,3}; raw += 3; }
        else { tokens[i] = {LzssTypedTokenKind::literal,data[i],0,0}; ++raw; }
    }
    constexpr LzssParameters p{4194304,3,258,0};
    const auto used = std::span(tokens).first(count);
    marc::entropy::internal::PositionRans4mDescriptor model{};
    const auto planned = plan_lzss_position_rans_4m_tokens(used,p,{static_cast<std::uint32_t>(count),raw,0},limits,model);
    if (planned.error != LzssPositionRans4mTokenError::none) std::abort();
    std::array<std::byte,5110> descriptor{};
    std::array<std::byte,6920> payload{};
    const auto encoded = encode_lzss_position_rans_4m_tokens(used,p,{static_cast<std::uint32_t>(count),raw,0},limits,descriptor,payload);
    if (encoded.error != LzssPositionRans4mTokenError::none) std::abort();
    const LzssFieldContextValidationContext context{static_cast<std::uint32_t>(count),encoded.event_count,encoded.decision_count,raw,0};
    auto desc = std::span(descriptor).first(encoded.descriptor_size);
    auto body = std::span(payload).first(encoded.payload_size);
    compare(desc,body,context,limits);
    compare(input,body,context,limits);
    compare(desc,input,context,limits);
    if (size) {
        if (data[0]&1) desc[data[0]%desc.size()] ^= static_cast<std::byte>(data[size-1]);
        else body[data[0]%body.size()] ^= static_cast<std::byte>(data[size-1]);
        compare(desc,body,context,limits);
    }
    return 0;
}

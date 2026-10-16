#include "context/lzss_position_blocked_huffman_64k_tokens.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <vector>

namespace {
using namespace marc::context::internal;
using namespace marc::dictionary::internal;
using namespace marc::entropy::internal;
void require(bool value) { if (!value) std::abort(); }
bool same(const LzssTypedToken& a,const LzssTypedToken& b) {
    return a.kind==b.kind && a.literal==b.literal && a.distance==b.distance && a.length==b.length;
}
void compare(std::span<const std::byte> descriptor,std::span<const std::byte> payload,
    const PositionBlockedHuffman64kTokenContext& context,const marc::core::DecoderLimits& limits) {
    const LzssTypedToken sentinel{LzssTypedTokenKind::literal,0xcc,0,0};
    std::array<LzssTypedToken,130> ordinary{},private_a{},private_b{};
    ordinary.fill(sentinel);private_a.fill(sentinel);private_b.fill(sentinel);
    std::vector<HuffmanDecodeTable> tables(position_blocked_huffman_64k_tables);
    auto output=std::span(ordinary).subspan(1,context.token_count);
    auto a=std::span(private_a).subspan(1,context.token_count);
    auto b=std::span(private_b).subspan(1,context.token_count);
    const auto x=decode_position_blocked_huffman_64k_tokens(descriptor,payload,context,limits,tables,a,output);
    const auto y=decode_position_blocked_huffman_64k_token_scratch(descriptor,payload,context,limits,tables,b);
    require(x.error==y.error && x.entropy_error==y.entropy_error && x.token_error==y.token_error);
    require(same(ordinary.front(),sentinel) && same(ordinary.back(),sentinel));
    require(same(private_a.front(),sentinel) && same(private_a.back(),sentinel));
    require(same(private_b.front(),sentinel) && same(private_b.back(),sentinel));
    for (const auto* guarded : {&ordinary,&private_a,&private_b})
        require(std::all_of(guarded->begin()+1+context.token_count,guarded->end(),
            [&](const auto& t){return same(t,sentinel);}));
    if (x.error!=PositionBlockedHuffman64kTokenError::none)
        require(std::all_of(output.begin(),output.end(),[&](const auto& t){return same(t,sentinel);}));
    else require(std::equal(output.begin(),output.end(),b.begin(),same));
}
}
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,std::size_t size) {
    if (size>4096) return 0;
    marc::core::DecoderLimits limits{};
    limits.max_lz_distance=65536;limits.max_frame_size=384;
    limits.max_total_output_size=768;limits.max_block_size=4096;
    limits.max_internal_buffered_bytes=UINT64_C(4)<<20;
    std::array<LzssTypedToken,128> tokens{};
    const auto count=std::min<std::size_t>(size,tokens.size());std::uint32_t raw{};
    for (std::size_t i=0;i<count;++i) {
        if (i && (data[i]&1)) { tokens[i]={LzssTypedTokenKind::match,0,1,3};raw+=3; }
        else { tokens[i]={LzssTypedTokenKind::literal,data[i],0,0};++raw; }
    }
    std::vector<CanonicalHuffmanTable> tables(position_blocked_huffman_64k_tables);
    std::array<std::byte,2518> descriptor{};
    std::array<std::byte,8192> payload{},scratch{};
    const auto encoded=encode_position_blocked_huffman_64k_tokens(std::span(tokens).first(count),
        {static_cast<std::uint32_t>(count),raw,0},limits,tables,scratch,descriptor,payload);
    require(encoded.error==PositionBlockedHuffman64kTokenError::none);
    const PositionBlockedHuffman64kTokenContext context{static_cast<std::uint32_t>(count),raw,encoded.decision_count,0};
    auto desc=std::span(descriptor).first(encoded.descriptor_size);
    auto body=std::span(payload).first(encoded.payload_size);
    const auto input=std::as_bytes(std::span(data,size));
    compare(desc,body,context,limits);compare(input,body,context,limits);compare(desc,input,context,limits);
    if (size) {
        if ((data[0]&1) || body.empty()) desc[data[0]%desc.size()]^=static_cast<std::byte>(data[size-1]|1);
        else body[data[0]%body.size()]^=static_cast<std::byte>(data[size-1]|1);
        compare(desc,body,context,limits);
    }
    return 0;
}

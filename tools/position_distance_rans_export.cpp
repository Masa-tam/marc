// DD-1511 trusted local token exporter, not a public codec entry point.
#include "dictionary/lzss_typed_encoder.hpp"
#include "dictionary/lzss_position_distance_1m_candidate.hpp"
#include "context/lzss_position_distance_1m_tokens.hpp"
#include "context/lzss_position_distance_1m_field_cursor.hpp"
#include "context/lzss_contextual_rans_encoder.hpp"
#include "context/lzss_contextual_rans_decoder.hpp"
#include "entropy/lzss_position_distance_1m_range_encoder.hpp"
#include <array>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <source_location>
#include <vector>

namespace {
void check(bool value, std::source_location location=std::source_location::current()) {
    if (!value) throw std::runtime_error("diagnostic precondition/codec failure at line "
        +std::to_string(location.line()));
}
void u32(std::ostream& out, std::uint32_t value) {
    for (unsigned i=0;i<4;++i) out.put(static_cast<char>((value>>(8*i))&255));
}
}
int main(int argc, char** argv) try {
    using namespace marc;
    using namespace dictionary::internal;
    using namespace context::internal;
    using namespace entropy::internal;
    check(argc==4);
    const auto minimum=std::string(argv[3])=="3" ? 3U : 5U;
    check(std::string(argv[3])=="3" || std::string(argv[3])=="5");
    std::ifstream source(argv[1],std::ios::binary); check(bool(source));
    // Keep the entire diagnostic export private until all native calls pass.
    std::vector<std::byte> raw(1048576);
    source.read(reinterpret_cast<char*>(raw.data()),raw.size());
    raw.resize(static_cast<std::size_t>(source.gcount()));
    check(source.peek()==std::char_traits<char>::eof()); // caller supplies one bounded frame
    if(raw.empty()) {
        std::ofstream output(argv[2],std::ios::binary);check(bool(output));
        output.write("PDOP",4);output.close();check(bool(output));return 0;
    }
    core::DecoderLimits limits;
    limits.max_internal_buffered_bytes=UINT64_C(256)<<20;
    // Entropy block size counts decisions, not uncompressed bytes.
    limits.max_block_size=UINT64_C(32)<<20;
    LzssParameters parameters{1048576,minimum,258,0};
    const auto variant=minimum==3 ? LzssTypedTokenVariant::field_context_1m_short_length_escape
                                 : LzssTypedTokenVariant::field_context_1m;
    std::vector<LzssTypedToken> tokens(raw.size());
    if(minimum==3) {
        const auto work=calculate_lzss_position_distance_1m_match_workspace(raw.size(),parameters,limits);
        check(work.error==LzssShortPrefixError::none);
        std::vector<std::size_t> workspace((work.workspace_size+sizeof(std::size_t)-1)/sizeof(std::size_t));
        const auto encoded=tokenize_lzss_position_distance_1m_candidate(raw,parameters,limits,3,
            LzssPositionDistance1mSearch::indexed,tokens,
            std::as_writable_bytes(std::span(workspace)).first(work.workspace_size));
        check(encoded.error==LzssShortMatchCandidateError::none);tokens.resize(encoded.token_count);
    } else {
        const auto work=calculate_lzss_hash_chain_workspace(raw.size(),parameters,limits);
        check(work.error==LzssHashChainError::none);
        std::vector<std::size_t> workspace((work.workspace_size+sizeof(std::size_t)-1)/sizeof(std::size_t));
        const auto encoded=encode_lzss_typed_tokens_hash_chain(raw,parameters,limits,tokens,
            std::as_writable_bytes(std::span(workspace)).first(work.workspace_size),variant);
        check(encoded.error==LzssTypedEncodeError::none);tokens.resize(encoded.token_count);
    }
    const LzssTypedFrameValidationContext context{static_cast<std::uint32_t>(tokens.size()),
        static_cast<std::uint32_t>(raw.size()),0};
    const LzssParameters position_parameters{1048576,3,258,0};
    const auto plan=plan_lzss_position_distance_1m_operations(tokens,position_parameters,context,limits);
    check(plan.error==LzssFieldContextError::none);
    std::vector<ModeledOperation> operations(plan.operation_count);
    check(model_lzss_position_distance_1m_tokens(tokens,position_parameters,context,limits,operations).error
          ==LzssFieldContextError::none);
    // A local differential fingerprint of the native modeled decisions.
    std::uint64_t fingerprint=UINT64_C(14695981039346656037);
    const auto decision=[&](std::uint16_t id,std::uint32_t value) {
        for(auto byte:{static_cast<unsigned>(id&255),static_cast<unsigned>(id>>8),value}) {
            fingerprint^=byte;fingerprint*=UINT64_C(1099511628211);
        }
    };
    LzssPositionDistance1mFieldCursor cursor;
    for(const auto& op:operations) {
        const auto request=cursor.next();
        if(op.kind==ModeledOperationKind::symbol) decision(op.context_id,op.value);
        else for(unsigned bit=0;bit<op.bit_count;++bit)
            decision(request.field==LzssPositionDistance1mField::adaptive_distance_extra
                ? static_cast<std::uint16_t>(24+bit) : UINT16_MAX,(op.value>>bit)&1);
        check(cursor.accept(op)==LzssFieldContextError::none);
    }
    check(cursor.finish()==LzssFieldContextError::none);
    ContextualDynamicRangeDescriptor range;
    const auto range_plan=plan_lzss_position_distance_1m_range_operations(operations,limits,range);
    check(range_plan.error==ContextualDynamicRangeEncodeError::none);
    std::vector<std::byte> range_payload(range.payload_size);
    const auto range_encoded=encode_lzss_position_distance_1m_range_operations(
        operations,limits,range_payload,range);
    check(range_encoded.error==ContextualDynamicRangeEncodeError::none);
    std::vector<LzssTypedToken> restored(tokens.size());
    const LzssFieldContextValidationContext decode_context{context.declared_token_count,
        static_cast<std::uint32_t>(operations.size()),plan.decision_count,context.declared_raw_size,0};
    check(decode_lzss_position_distance_1m_tokens(range,range_payload,position_parameters,
        decode_context,limits,restored).error==LzssContextualRangeDecodeError::none);
    for(std::size_t i=0;i<tokens.size();++i) {
        check(tokens[i].kind==restored[i].kind && tokens[i].literal==restored[i].literal
            && tokens[i].distance==restored[i].distance && tokens[i].length==restored[i].length);
    }
    std::uint32_t contextual_model=0,contextual_payload=0;
    if(minimum==5 && !tokens.empty()) {
        ContextualRansDescriptor descriptor;
        const auto result=plan_lzss_contextual_rans_tokens(tokens,parameters,context,limits,
            descriptor,LzssFieldContextVariant::field_context_1m);
        if(result.error!=LzssContextualRansEncodeError::none)
            std::cerr<<"contextual error="<<static_cast<unsigned>(result.error)
                     <<" entropy="<<static_cast<unsigned>(result.entropy_error)
                     <<" token="<<static_cast<unsigned>(result.token_validation.error)<<'\n';
        check(result.error==LzssContextualRansEncodeError::none);
        contextual_model=static_cast<std::uint32_t>(result.descriptor_size);
        contextual_payload=static_cast<std::uint32_t>(result.payload_size);
        std::vector<std::byte> payload(contextual_payload);
        const auto actual=encode_lzss_contextual_rans_tokens(tokens,parameters,context,limits,
            payload,descriptor,LzssFieldContextVariant::field_context_1m);
        check(actual.error==LzssContextualRansEncodeError::none
            && actual.payload_size==contextual_payload && actual.descriptor_size==contextual_model);
        std::vector<std::byte> model(contextual_model);
        std::size_t written{};
        check(serialize_contextual_rans_descriptor(descriptor,actual.decision_count,
            contextual_payload,limits,model,written,LzssFieldContextVariant::field_context_1m)
            ==ContextualRansFormatError::none && written==model.size());
        std::vector<RansDecodeEntry> tables(contextual_rans_decode_table_entries);
        const LzssFieldContextValidationContext control_context{context.declared_token_count,
            static_cast<std::uint32_t>(actual.event_count),actual.decision_count,context.declared_raw_size,0};
        const auto decoded=decode_lzss_contextual_rans_tokens(model,payload,parameters,
            control_context,limits,tables,restored,LzssFieldContextVariant::field_context_1m);
        check(decoded.format_error==ContextualRansFormatError::none
            && decoded.decode.error==LzssContextualRansDecodeError::none);
        for(std::size_t i=0;i<tokens.size();++i)
            check(tokens[i].kind==restored[i].kind && tokens[i].literal==restored[i].literal
                && tokens[i].distance==restored[i].distance && tokens[i].length==restored[i].length);
    }
    std::ofstream output(argv[2],std::ios::binary); check(bool(output));
    output.write("PDOP",4);
    if(!raw.empty()) {
        for(auto v:{static_cast<std::uint32_t>(raw.size()),static_cast<std::uint32_t>(tokens.size()),
                    range.payload_size,contextual_model,contextual_payload,minimum}) u32(output,v);
        output.write(reinterpret_cast<const char*>(raw.data()),raw.size());
        for(const auto& token:tokens) {
            output.put(static_cast<char>(token.kind)); output.put(static_cast<char>(token.literal));
            u32(output,token.distance); u32(output,token.length);
        }
    }
    output.close();check(bool(output));
    std::cout<<plan.decision_count<<' '<<fingerprint<<'\n';
    return 0;
} catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }

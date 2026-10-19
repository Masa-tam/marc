#include "frame/lzss_position_adaptive_huffman_owned.hpp"
#include "dictionary/lzss_short_length_escape_candidate.hpp"
#include "dictionary/lzss_position_distance_1m_five_prefix_candidate.hpp"
#include "dictionary/lzss_position_distance_4m_five_prefix_candidate.hpp"
#include "dictionary/lzss_position_distance_8m_five_prefix_candidate.hpp"
#include "dictionary/lzss_position_distance_16m_five_prefix_candidate.hpp"
#include "dictionary/lzss_position_distance_32m_five_prefix_candidate.hpp"
#include "dictionary/lzss_position_distance_64m_five_prefix_candidate.hpp"
#include "core/buffer_overlap.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <new>

namespace marc::frame::internal {
namespace {
using Code=core::ErrorCode;
using Token=dictionary::internal::LzssTypedToken;
using Profile=entropy::internal::PositionAdaptiveHuffmanProfile;
namespace Dictionary=dictionary::internal;
using Variant=Dictionary::LzssTypedTokenVariant;
using FinderQuery=Dictionary::LzssShortPrefixWorkspaceRequirements(*)(std::size_t,
    const Dictionary::LzssParameters&,const core::DecoderLimits&,Variant) noexcept;
using Tokenizer=Dictionary::LzssShortMatchCandidateResult(*)(std::span<const std::byte>,
    const Dictionary::LzssParameters&,const core::DecoderLimits&,std::uint32_t,
    std::span<Token>,std::span<std::byte>) noexcept;
// Immutable ordered dispatch; only qualified private owners are admitted.
constexpr std::array<FinderQuery,7> finder_queries{
    Dictionary::calculate_lzss_short_prefix_workspace,
    Dictionary::calculate_lzss_position_distance_1m_five_prefix_workspace,
    Dictionary::calculate_lzss_position_distance_4m_five_prefix_workspace,
    Dictionary::calculate_lzss_position_distance_8m_five_prefix_workspace,
    Dictionary::calculate_lzss_position_distance_16m_five_prefix_workspace,
    Dictionary::calculate_lzss_position_distance_32m_five_prefix_workspace,
    Dictionary::calculate_lzss_position_distance_64m_five_prefix_workspace};
constexpr std::array variants{Variant::field_context_64k_short_length_escape,
    Variant::field_context_1m_short_length_escape,Variant::field_context_4m_short_length_escape,
    Variant::field_context_8m_short_length_escape,Variant::field_context_16m_short_length_escape,
    Variant::field_context_32m_short_length_escape,
    Variant::field_context_64m_short_length_escape};
constexpr std::array<Tokenizer,7> tokenizers{
    Dictionary::tokenize_lzss_short_length_escape_candidate_indexed_scratch,
    Dictionary::tokenize_lzss_position_distance_1m_five_prefix_candidate,
    Dictionary::tokenize_lzss_position_distance_4m_five_prefix_candidate,
    Dictionary::tokenize_lzss_position_distance_8m_five_prefix_candidate,
    Dictionary::tokenize_lzss_position_distance_16m_five_prefix_candidate,
    Dictionary::tokenize_lzss_position_distance_32m_five_prefix_candidate,
    Dictionary::tokenize_lzss_position_distance_64m_five_prefix_candidate};
static_assert(finder_queries.size()==variants.size() && variants.size()==tokenizers.size()
    && tokenizers.size()==static_cast<std::size_t>(Profile::window_64m)+1);

template<class T> bool allocate(std::unique_ptr<T[]>& target,std::size_t count) noexcept {
    target.reset(new(std::nothrow) T[count]);return static_cast<bool>(target);
}
Code mapped(PositionAdaptiveHuffmanFormatError e) noexcept {
    return e==PositionAdaptiveHuffmanFormatError::limit_exceeded
        || e==PositionAdaptiveHuffmanFormatError::arithmetic_overflow ? Code::limit_exceeded : Code::invalid_argument;
}
Code workspace(const PositionAdaptiveHuffmanStreamHeader& stream,const core::DecoderLimits& limits,
    bool encode,std::size_t owner,PositionAdaptiveHuffmanWorkspace& output) noexcept {
    // Reject unknown identities before querying or indexing the ordered tables.
    if (stream.profile>Profile::window_64m) return Code::unsupported;
    if (const auto e=validate_position_adaptive_huffman_stream(stream,limits);e!=PositionAdaptiveHuffmanFormatError::none) return mapped(e);
    const auto* spec=entropy::internal::position_adaptive_huffman_spec(stream.profile);
    PositionAdaptiveHuffmanWorkspace r{};r.raw_bytes=stream.frame_size;r.token_count=stream.frame_size;
    r.owner_bytes=owner;r.node_count=spec->node_entries();r.symbol_count=spec->alphabet_entries;
    r.control_bytes=encode ? std::size_t{262144} : position_adaptive_huffman_stream_decode_control_bytes;
    std::size_t bits{};
    if (!core::checked_multiply(r.raw_bytes,std::size_t{267},bits) || !core::checked_add(bits,std::size_t{7},bits)
        || !core::checked_add(bits/8,std::size_t{80},r.serialized_bytes)
        || !core::checked_multiply(r.token_count,sizeof(Token),r.token_bytes)
        || !core::checked_multiply(r.node_count,sizeof(entropy::internal::AdaptiveHuffmanNode),r.node_bytes)
        || !core::checked_multiply(r.symbol_count,sizeof(std::uint16_t),r.symbol_bytes)) return Code::limit_exceeded;
    if (encode) {
        if (stream.profile<Profile::window_4m) {
            r.payload_bytes=bits/8;r.private_serialized_bytes=r.serialized_bytes;
        }
        const auto index=static_cast<std::size_t>(stream.profile);
        const auto f=finder_queries[index](r.raw_bytes,stream.dictionary,limits,variants[index]);
        if (f.error!=dictionary::internal::LzssShortPrefixError::none) return Code::limit_exceeded;
        r.finder_bytes=f.workspace_size;
    }
    for (auto n:{r.raw_bytes,r.token_bytes,r.node_bytes,r.symbol_bytes,r.serialized_bytes,r.payload_bytes,r.private_serialized_bytes,r.finder_bytes})
        if (!core::checked_add(r.owned_bytes,n,r.owned_bytes)) return Code::limit_exceeded;
    if (!core::checked_add(r.owned_bytes,r.owner_bytes,r.aggregate_bytes)
        || !core::checked_add(r.aggregate_bytes,r.control_bytes,r.aggregate_bytes)
        || r.aggregate_bytes>limits.max_internal_buffered_bytes) return Code::limit_exceeded;
    output=r;return Code::none;
}
}
Code PositionAdaptiveHuffmanOwnedEncoder::requirements(const PositionAdaptiveHuffmanStreamHeader& s,
    const core::DecoderLimits& l,PositionAdaptiveHuffmanWorkspace& r) noexcept {
    return workspace(s,l,true,sizeof(PositionAdaptiveHuffmanOwnedEncoder),r);
}
std::unique_ptr<PositionAdaptiveHuffmanOwnedEncoder> PositionAdaptiveHuffmanOwnedEncoder::create(
    const PositionAdaptiveHuffmanStreamHeader& s,const core::DecoderLimits& l,Code& error) noexcept {
    PositionAdaptiveHuffmanWorkspace w{};error=requirements(s,l,w);if (error!=Code::none) return {};
    std::unique_ptr<PositionAdaptiveHuffmanOwnedEncoder> p(new(std::nothrow) PositionAdaptiveHuffmanOwnedEncoder);
    if (!p) {error=Code::out_of_memory;return {};}
    p->stream_=s;p->limits_=l;p->workspace_=w;
    if (!allocate(p->raw_,w.raw_bytes) || !allocate(p->serialized_,w.serialized_bytes)
        || (w.payload_bytes!=0 && !allocate(p->payload_,w.payload_bytes))
        || (w.private_serialized_bytes!=0 && !allocate(p->staged_,w.private_serialized_bytes))
        || !allocate(p->finder_,w.finder_bytes) || !allocate(p->tokens_,w.token_count)
        || !allocate(p->nodes_,w.node_count) || !allocate(p->symbols_,w.symbol_count)) {
        error=Code::out_of_memory;return {};
    }
    if (const auto e=serialize_position_adaptive_huffman_stream(s,l,p->header_);e!=PositionAdaptiveHuffmanFormatError::none) {
        error=mapped(e);return {};
    }
    return p;
}
bool PositionAdaptiveHuffmanOwnedEncoder::disjoint(std::span<const std::byte> in,std::span<std::byte> out) const noexcept {
    struct R {const void* p;std::size_t n;};const auto& w=workspace_;
    const std::array regions{R{this,sizeof(*this)},R{in.data(),in.size()},R{out.data(),out.size()},
        R{raw_.get(),w.raw_bytes},R{serialized_.get(),w.serialized_bytes},R{payload_.get(),w.payload_bytes},
        R{staged_.get(),w.private_serialized_bytes},R{finder_.get(),w.finder_bytes},R{tokens_.get(),w.token_bytes},
        R{nodes_.get(),w.node_bytes},R{symbols_.get(),w.symbol_bytes}};
    for (std::size_t i=0;i<regions.size();++i) for (std::size_t j=i+1;j<regions.size();++j)
        if (core::check_buffer_overlap(regions[i].p,regions[i].n,regions[j].p,regions[j].n)!=core::BufferOverlap::disjoint) return false;
    return true;
}
core::ProcessResult PositionAdaptiveHuffmanOwnedEncoder::fail(Code e,std::size_t consumed) noexcept {
    state_=State::error;error_={e,received_,0};return {consumed,0,core::StreamStatus::error,error_};
}
core::ProcessResult PositionAdaptiveHuffmanOwnedEncoder::process(std::span<const std::byte> in,
    std::span<std::byte> out,std::uint32_t flags) noexcept {
    using S=core::StreamStatus;
    if (state_==State::error) return {0,0,S::error,error_};
    if (state_==State::ended) return {0,0,S::end_of_stream,{}};
    constexpr auto end=core::flag_value(core::ProcessFlags::end_input);
    if (flags & ~(end|core::flag_value(core::ProcessFlags::flush))) return fail(Code::unsupported);
    if (!disjoint(in,out)) return fail(Code::invalid_argument);
    const auto remaining=stream_.original_size-received_;
    const bool final=(flags&end)!=0;
    if (in.size()>remaining || (final && in.size()!=remaining) || (end_seen_ && !in.empty())) return fail(Code::invalid_argument);
    std::size_t consumed{};
    for (;;) {
        if (final && consumed==in.size()) end_seen_=true;
        if (state_==State::header || state_==State::drain) {
            const auto src=state_==State::header ? std::span<const std::byte>(header_) : std::span<const std::byte>(serialized_.get(),pending_);
            const auto n=std::min(out.size(),pending_-drained_);
            if (n) std::memcpy(out.data(),src.data()+drained_,n);
            drained_+=n;
            if (drained_!=pending_) return {consumed,n,S::need_output,{}};
            collected_=drained_=pending_=0;state_=committed_==stream_.original_size ? State::await_end : State::collect;
            if (n) return {consumed,n,S::progress,{}};
            continue;
        }
        if (state_==State::await_end) {
            if (end_seen_) {state_=State::ended;return {consumed,0,S::end_of_stream,{}};}
            return {consumed,0,consumed ? S::progress : S::need_input,{}};
        }
        const auto target=static_cast<std::size_t>(std::min<std::uint64_t>(stream_.frame_size,stream_.original_size-committed_));
        const auto n=std::min(target-collected_,in.size()-consumed);
        if (n) std::memcpy(raw_.get()+collected_,in.data()+consumed,n);
        consumed+=n;received_+=n;collected_+=n;
        if (collected_!=target) return {consumed,0,consumed ? S::progress : S::need_input,{}};
        const auto& w=workspace_;
        const auto parsed=tokenizers[static_cast<std::size_t>(stream_.profile)](
            {raw_.get(),target},stream_.dictionary,limits_,5,{tokens_.get(),w.token_count},{finder_.get(),w.finder_bytes});
        if (parsed.error!=dictionary::internal::LzssShortMatchCandidateError::none) return fail(Code::internal_error,consumed);
        const auto r=stream_.profile<Profile::window_4m
            ? encode_position_adaptive_huffman_frame({tokens_.get(),parsed.token_count},
                {stream_,limits_,sequence_,committed_},{nodes_.get(),w.node_count},{symbols_.get(),w.symbol_count},
                {payload_.get(),w.payload_bytes},{staged_.get(),w.private_serialized_bytes},{serialized_.get(),w.serialized_bytes})
            : encode_position_adaptive_huffman_frame_scratch({tokens_.get(),parsed.token_count},
                {stream_,limits_,sequence_,committed_},{nodes_.get(),w.node_count},{symbols_.get(),w.symbol_count},
                {serialized_.get(),w.serialized_bytes});
        if (r.error!=PositionAdaptiveHuffmanFrameError::none) return fail(
            r.error==PositionAdaptiveHuffmanFrameError::limit_exceeded || r.error==PositionAdaptiveHuffmanFrameError::arithmetic_overflow
                ? Code::limit_exceeded : Code::internal_error,consumed);
        committed_+=target;++sequence_;pending_=r.serialized_size;state_=State::drain;
    }
}
Code PositionAdaptiveHuffmanOwnedDecoder::requirements(Profile profile,std::uint32_t capacity,
    const core::DecoderLimits& l,PositionAdaptiveHuffmanWorkspace& r) noexcept {
    PositionAdaptiveHuffmanStreamHeader s{};s.profile=profile;s.frame_size=capacity;
    if (const auto* spec=entropy::internal::position_adaptive_huffman_spec(profile))
        s.dictionary.window_size=spec->window_size;
    return workspace(s,l,false,sizeof(PositionAdaptiveHuffmanOwnedDecoder),r);
}
std::unique_ptr<PositionAdaptiveHuffmanOwnedDecoder> PositionAdaptiveHuffmanOwnedDecoder::create(
    Profile profile,std::uint32_t capacity,const core::DecoderLimits& limits,Code& error) noexcept {
    PositionAdaptiveHuffmanWorkspace w{};error=requirements(profile,capacity,limits,w);if (error!=Code::none) return {};
    std::unique_ptr<PositionAdaptiveHuffmanOwnedDecoder> p(new(std::nothrow) PositionAdaptiveHuffmanOwnedDecoder);
    if (!p) {error=Code::out_of_memory;return {};}
    if (!allocate(p->raw_,w.raw_bytes) || !allocate(p->serialized_,w.serialized_bytes) || !allocate(p->tokens_,w.token_count)
        || !allocate(p->nodes_,w.node_count) || !allocate(p->symbols_,w.symbol_count)) {error=Code::out_of_memory;return {};}
    auto l=limits;l.max_frame_size=std::min<std::uint64_t>(l.max_frame_size,capacity);
    p->decoder_.emplace(profile,l,std::span(p->serialized_.get(),w.serialized_bytes),std::span(p->tokens_.get(),w.token_count),
        std::span(p->raw_.get(),w.raw_bytes),std::span(p->nodes_.get(),w.node_count),std::span(p->symbols_.get(),w.symbol_count),
        sizeof(PositionAdaptiveHuffmanOwnedDecoder)-sizeof(PositionAdaptiveHuffmanStreamDecoder));
    const auto r=p->decoder_->process({},{},0);if (r.status==core::StreamStatus::error) {error=r.error.code;return {};}
    return p;
}
core::ProcessResult PositionAdaptiveHuffmanOwnedDecoder::process(std::span<const std::byte> in,
    std::span<std::byte> out,std::uint32_t flags) noexcept {
    if (error_.code!=Code::none) return {0,0,core::StreamStatus::error,error_};
    if (ended_) return {0,0,core::StreamStatus::end_of_stream,{}};
    if (core::check_buffer_overlap(this,sizeof(*this),in.data(),in.size())!=core::BufferOverlap::disjoint
        || core::check_buffer_overlap(this,sizeof(*this),out.data(),out.size())!=core::BufferOverlap::disjoint) {
        error_={Code::invalid_argument,0,0};return {0,0,core::StreamStatus::error,error_};
    }
    const auto r=decoder_->process(in,out,flags);if (r.status==core::StreamStatus::error) error_=r.error;
    ended_=r.status==core::StreamStatus::end_of_stream;return r;
}
} // namespace marc::frame::internal

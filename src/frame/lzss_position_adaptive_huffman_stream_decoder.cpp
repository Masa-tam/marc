#include "frame/lzss_position_adaptive_huffman_stream_decoder.hpp"
#include "core/buffer_overlap.hpp"
#include <algorithm>
#include <cstring>

namespace marc::frame::internal {
namespace {
core::ErrorCode category(PositionAdaptiveHuffmanFormatError e) noexcept {
    if (e==PositionAdaptiveHuffmanFormatError::unsupported_identity) return core::ErrorCode::unsupported;
    if (e==PositionAdaptiveHuffmanFormatError::limit_exceeded
        || e==PositionAdaptiveHuffmanFormatError::arithmetic_overflow) return core::ErrorCode::limit_exceeded;
    return core::ErrorCode::malformed_stream;
}
}
bool PositionAdaptiveHuffmanStreamDecoder::disjoint(std::span<const std::byte> input,
    std::span<std::byte> output) const noexcept {
    struct Region { const void* data;std::size_t size; };
    const std::array regions{Region{this,sizeof(*this)},Region{input.data(),input.size()},
        Region{output.data(),output.size()},Region{serialized_.data(),serialized_.size()},
        Region{tokens_.data(),token_bytes_},Region{raw_.data(),raw_.size()},
        Region{nodes_.data(),node_bytes_},Region{symbols_.data(),symbol_bytes_}};
    for (std::size_t i=0;i<regions.size();++i) for (std::size_t j=i+1;j<regions.size();++j)
        if (core::check_buffer_overlap(regions[i].data,regions[i].size,regions[j].data,regions[j].size)
            !=core::BufferOverlap::disjoint) return false;
    return true;
}
PositionAdaptiveHuffmanStreamDecoder::PositionAdaptiveHuffmanStreamDecoder(
    entropy::internal::PositionAdaptiveHuffmanProfile profile,core::DecoderLimits limits,
    std::span<std::byte> serialized,std::span<dictionary::internal::LzssTypedToken> tokens,
    std::span<std::byte> raw,std::span<entropy::internal::AdaptiveHuffmanNode> nodes,
    std::span<std::uint16_t> symbols,std::size_t owner) noexcept
    :profile_(profile),limits_(limits),serialized_(serialized),raw_(raw),tokens_(tokens),nodes_(nodes),symbols_(symbols) {
    const auto* spec=entropy::internal::position_adaptive_huffman_spec(profile);
    if (!spec || core::validate_limits(limits)!=core::LimitError::none
        || !core::checked_multiply(tokens.size(),sizeof(tokens[0]),token_bytes_)
        || !core::checked_multiply(nodes.size(),sizeof(nodes[0]),node_bytes_)
        || !core::checked_multiply(symbols.size(),sizeof(symbols[0]),symbol_bytes_) || !disjoint({},{})) {
        state_=State::error;error_={core::ErrorCode::invalid_argument,0,0};return;
    }
    std::size_t charge=sizeof(*this);
    for (const auto bytes:{owner,serialized.size(),raw.size(),token_bytes_,node_bytes_,symbol_bytes_,
                          position_adaptive_huffman_stream_decode_control_bytes}) {
        if (!core::checked_add(charge,bytes,charge)) {
            state_=State::error;error_={core::ErrorCode::limit_exceeded,0,0};return;
        }
    }
    if (charge>limits.max_internal_buffered_bytes || nodes.size()<spec->node_entries()
        || symbols.size()<spec->alphabet_entries) {
        state_=State::error;error_={core::ErrorCode::limit_exceeded,0,0};
    }
}
core::ProcessResult PositionAdaptiveHuffmanStreamDecoder::fail(core::ErrorCode code,
    std::uint64_t position,std::size_t consumed) noexcept {
    state_=State::error;error_={code,position,0};
    return {consumed,0,core::StreamStatus::error,error_};
}
core::ProcessResult PositionAdaptiveHuffmanStreamDecoder::process(std::span<const std::byte> input,
    std::span<std::byte> output,std::uint32_t flags) noexcept {
    using S=core::StreamStatus;using E=core::ErrorCode;
    if (state_==State::error) return {0,0,S::error,error_};
    if (state_==State::ended) return {0,0,S::end_of_stream,{}};
    constexpr auto end=core::flag_value(core::ProcessFlags::end_input);
    constexpr auto allowed=end|core::flag_value(core::ProcessFlags::flush);
    if (flags & ~allowed) return fail(E::unsupported,position_,0);
    if (!disjoint(input,output)) return fail(E::invalid_argument,position_,0);
    if (end_seen_ && !input.empty()) return fail(E::malformed_stream,position_,0);
    std::size_t consumed{};
    while (true) {
        if ((flags & end) && consumed==input.size()) end_seen_=true;
        if (state_==State::drain) {
            const auto n=std::min(output.size(),layout_.requirements.raw_size-drained_);
            if (n) std::memcpy(output.data(),raw_.data()+drained_,n);
            drained_+=n;
            if (drained_!=layout_.requirements.raw_size) return {consumed,n,S::need_output,{}};
            collected_=0;frame_start_=position_;
            state_=validated_==stream_.original_size ? State::await_end : State::frame_header;
            // Never parse a later failing frame in the call publishing this frame.
            if (n) return {consumed,n,S::progress,{}};
            continue;
        }
        if (state_==State::await_end) {
            if (consumed!=input.size()) return fail(E::malformed_stream,position_,consumed);
            if (end_seen_) {state_=State::ended;return {consumed,0,S::end_of_stream,{}};}
            return {consumed,0,consumed ? S::progress : S::need_input,{}};
        }
        const auto extent=state_==State::stream_header ? header_.size()
            : state_==State::frame_header ? std::size_t{64}
            : state_==State::descriptor ? std::size_t{80} : layout_.requirements.serialized_size;
        if (state_!=State::stream_header && extent>serialized_.size()) return fail(E::limit_exceeded,frame_start_,consumed);
        auto destination=state_==State::stream_header ? std::span<std::byte>(header_) : serialized_.first(extent);
        const auto n=std::min(extent-collected_,input.size()-consumed);
        std::uint64_t next{};
        if (!core::checked_add(position_,static_cast<std::uint64_t>(n),next)) return fail(E::limit_exceeded,position_,consumed);
        if (n) std::memcpy(destination.data()+collected_,input.data()+consumed,n);
        collected_+=n;consumed+=n;position_=next;
        if ((flags & end) && consumed==input.size()) end_seen_=true;
        if (collected_!=extent) {
            if (end_seen_) return fail(E::malformed_stream,position_,consumed);
            return {consumed,0,consumed ? S::progress : S::need_input,{}};
        }
        if (state_==State::stream_header) {
            std::size_t parsed{};
            const auto e=parse_position_adaptive_huffman_stream(profile_,header_,limits_,stream_,parsed);
            if (e!=PositionAdaptiveHuffmanFormatError::none) return fail(category(e),0,consumed);
            collected_=0;frame_start_=position_;
            state_=stream_.original_size ? State::frame_header : State::await_end;
        } else if (state_==State::frame_header) {
            const auto e=parse_position_adaptive_huffman_frame_header(destination,{stream_,limits_,sequence_,validated_},layout_);
            if (e!=PositionAdaptiveHuffmanFormatError::none) return fail(category(e),frame_start_,consumed);
            if (layout_.requirements.serialized_size>serialized_.size() || layout_.requirements.token_count>tokens_.size()
                || layout_.requirements.raw_size>raw_.size()) return fail(E::limit_exceeded,frame_start_,consumed);
            state_=State::descriptor;
        } else if (state_==State::descriptor) {
            const auto e=preflight_position_adaptive_huffman_frame_prefix(destination,{stream_,limits_,sequence_,validated_},layout_);
            if (e!=PositionAdaptiveHuffmanFormatError::none) return fail(category(e),frame_start_+64,consumed);
            state_=State::payload;
        } else {
            const auto r=decode_position_adaptive_huffman_frame_scratch(destination,{stream_,limits_,sequence_,validated_},
                nodes_,symbols_,tokens_,raw_);
            if (r.error!=PositionAdaptiveHuffmanFrameError::none) {
                const bool resource=r.error==PositionAdaptiveHuffmanFrameError::limit_exceeded
                    || r.error==PositionAdaptiveHuffmanFrameError::arithmetic_overflow;
                return fail(resource ? E::limit_exceeded : E::malformed_stream,frame_start_+80,consumed);
            }
            validated_+=layout_.requirements.raw_size;++sequence_;drained_=0;state_=State::drain;
        }
    }
}
} // namespace marc::frame::internal

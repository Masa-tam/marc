#include "c_api/status.hpp"
#include "c_api/transform_handle.hpp"
#include "frame/lzss_position_adaptive_huffman_owned.hpp"
#include "core/buffer_overlap.hpp"
#include <marc/marc.h>
#include <algorithm>
#include <array>
#include <limits>
#include <new>


namespace {
using marc::c_api::profile_factory_status;

using namespace marc::frame::internal;
using Config=marc_lzss_position_distance_adaptive_huffman_config;
using Resources=marc_lzss_position_distance_adaptive_huffman_resources;
using Code=marc::core::ErrorCode;
bool separate(const void* a,std::size_t an,const void* b,std::size_t bn) noexcept {
    return marc::core::check_buffer_overlap(a,an,b,bn)==marc::core::BufferOverlap::disjoint;
}

class Boundary final : public marc::core::Transform {
public:
    Boundary(std::unique_ptr<marc::core::Transform> p,std::size_t in,std::size_t out) noexcept
        :p_(std::move(p)),in_(in),out_(out) {}
    void handle(marc_transform* h) noexcept {handle_=h;}
    marc::core::ProcessResult process(std::span<const std::byte> in,std::span<std::byte> out,std::uint32_t flags) noexcept override {
        if (terminal_) return {0,0,last_.status,last_.error};
        std::uint64_t maximum_position{};
        const bool limited=in.size()>in_ || out.size()>out_
            || !marc::core::checked_add(accepted_,static_cast<std::uint64_t>(in.size()),maximum_position);
        if (limited || !separate(in.data(),in.size(),handle_,sizeof(*handle_))
            || !separate(out.data(),out.size(),handle_,sizeof(*handle_))
            || !separate(in.data(),in.size(),this,sizeof(*this)) || !separate(out.data(),out.size(),this,sizeof(*this))) {
            last_={0,0,marc::core::StreamStatus::error,{limited ? Code::limit_exceeded : Code::invalid_argument,accepted_,0}};
        } else {
            last_=p_->process(in,out,flags);
            accepted_+=last_.input_consumed;
        }
        terminal_=last_.status==marc::core::StreamStatus::error || last_.status==marc::core::StreamStatus::end_of_stream;
        return last_;
    }
private:
    std::unique_ptr<marc::core::Transform> p_;marc_transform* handle_{};
    std::size_t in_{},out_{};std::uint64_t accepted_{};
    marc::core::ProcessResult last_{};bool terminal_{};
};
marc_status prepare(const Config* c,marc::core::DecoderLimits& limits,PositionAdaptiveHuffmanStreamHeader& stream,
    PositionAdaptiveHuffmanWorkspace& requirements,std::size_t& external) noexcept {
    if (!c || c->struct_size!=sizeof(*c) || c->abi_version!=MARC_ABI_VERSION || c->reserved2
        || c->profile>MARC_LZSS_POSITION_DISTANCE_ADAPTIVE_HUFFMAN_PROFILE_64M
        || (c->direction!=MARC_DIRECTION_ENCODE && c->direction!=MARC_DIRECTION_DECODE)
        || !c->frame_size) return MARC_STATUS_INVALID_ARGUMENT;
    const auto selected=static_cast<marc::entropy::internal::PositionAdaptiveHuffmanProfile>(c->profile);
    const auto& spec=*marc::entropy::internal::position_adaptive_huffman_spec(selected);
    if (c->frame_size>spec.window_size) return MARC_STATUS_INVALID_ARGUMENT;
    limits={};limits.max_total_output_size=c->max_total_output_size;limits.max_frame_size=c->max_frame_size;
    limits.max_block_size=c->max_block_size;limits.max_compressed_payload_size=c->max_compressed_payload_size;
    limits.max_internal_buffered_bytes=c->max_internal_buffered_bytes;limits.max_lz_distance=c->max_lz_distance;
    limits.max_lz_match_length=c->max_lz_match_length;limits.max_entropy_table_entries=c->max_entropy_table_entries;
    limits.max_expansion_ratio=c->max_expansion_ratio;limits.expansion_slack=c->expansion_slack;
    if (marc::core::validate_limits(limits)!=marc::core::LimitError::none) return MARC_STATUS_INVALID_ARGUMENT;
    external=sizeof(Boundary)+sizeof(marc_transform)+4096;
    for (auto bytes:{c->external_retained_bytes,c->input_capacity_bytes,c->output_capacity_bytes}) {
        if (bytes>std::numeric_limits<std::size_t>::max()
            || !marc::core::checked_add(external,static_cast<std::size_t>(bytes),external)) return MARC_STATUS_LIMIT_EXCEEDED;
    }
    if (external>=limits.max_internal_buffered_bytes || limits.max_internal_buffered_bytes-external<limits.max_block_size)
        return MARC_STATUS_LIMIT_EXCEEDED;
    limits.max_internal_buffered_bytes-=external;
    stream={};stream.frame_size=c->frame_size;stream.original_size=c->direction==MARC_DIRECTION_ENCODE ? c->original_size : 0;
    stream.profile=selected;stream.dictionary.window_size=spec.window_size;
    return profile_factory_status(c->direction==MARC_DIRECTION_ENCODE ? PositionAdaptiveHuffmanOwnedEncoder::requirements(stream,limits,requirements)
        : PositionAdaptiveHuffmanOwnedDecoder::requirements(stream.profile,c->frame_size,limits,requirements));
}
}
marc_status marc_lzss_position_distance_adaptive_huffman_config_init(marc_direction direction,Config* out) noexcept {
    if (!out || (direction!=MARC_DIRECTION_ENCODE && direction!=MARC_DIRECTION_DECODE)) return MARC_STATUS_INVALID_ARGUMENT;
    Config c{};c.struct_size=sizeof(c);c.abi_version=MARC_ABI_VERSION;c.direction=direction;
    c.profile=MARC_LZSS_POSITION_DISTANCE_ADAPTIVE_HUFFMAN_PROFILE_64K;
    c.frame_size=65536;c.max_frame_size=65536;c.max_block_size=7*UINT64_C(65536);
    c.max_total_output_size=UINT64_C(1)<<40;c.max_compressed_payload_size=UINT64_C(2187264);
    c.max_internal_buffered_bytes=UINT64_C(16)<<20;c.max_lz_distance=65536;c.max_lz_match_length=258;
    c.max_entropy_table_entries=13758;c.max_expansion_ratio=1024;c.expansion_slack=65536;
    c.input_capacity_bytes=c.output_capacity_bytes=65536;*out=c;return MARC_STATUS_OK;
}
marc_status marc_lzss_position_distance_adaptive_huffman_config_apply_profile(Config* out,
    marc_lzss_position_distance_adaptive_huffman_profile profile) noexcept {
    if (!out || out->struct_size!=sizeof(*out) || out->abi_version!=MARC_ABI_VERSION || out->reserved2
        || (out->direction!=MARC_DIRECTION_ENCODE && out->direction!=MARC_DIRECTION_DECODE)
        || profile>MARC_LZSS_POSITION_DISTANCE_ADAPTIVE_HUFFMAN_PROFILE_64M) return MARC_STATUS_INVALID_ARGUMENT;
    Config next{};
    const auto error=marc_lzss_position_distance_adaptive_huffman_config_init(out->direction,&next);
    if (error!=MARC_STATUS_OK) return error;
    if (profile!=MARC_LZSS_POSITION_DISTANCE_ADAPTIVE_HUFFMAN_PROFILE_64K) {
        const auto selected=static_cast<marc::entropy::internal::PositionAdaptiveHuffmanProfile>(profile);
        const auto& spec=*marc::entropy::internal::position_adaptive_huffman_spec(selected);
        next.profile=profile;next.frame_size=spec.window_size;next.max_frame_size=spec.window_size;
        next.max_lz_distance=spec.window_size;
        next.max_block_size=static_cast<std::uint64_t>(spec.decisions_per_byte)*spec.window_size;
        next.max_compressed_payload_size=(UINT64_C(267)*spec.window_size+7)/8;
        // Qualified private encode admissions, before public IO/control:
        // 1MiB132410060,4MiB246050184,8MiB490893070,16MiB980578452 bytes.
        // 32MiB1959948826,64MiB3918689184 bytes.
        constexpr std::array<std::uint64_t,7> memory_mib{16,128,256,512,1024,2048,4096};
        static_assert(memory_mib.size()==MARC_LZSS_POSITION_DISTANCE_ADAPTIVE_HUFFMAN_PROFILE_64M+1);
        next.max_internal_buffered_bytes=memory_mib[profile]<<20;
        next.max_entropy_table_entries=spec.node_entries()+spec.alphabet_entries;
    }
    next.original_size=out->original_size;next.external_retained_bytes=out->external_retained_bytes;
    next.input_capacity_bytes=out->input_capacity_bytes;next.output_capacity_bytes=out->output_capacity_bytes;
    *out=next;return MARC_STATUS_OK;
}
marc_status marc_lzss_position_distance_adaptive_huffman_resource_requirements(const Config* c,Resources* out) noexcept {
    if (!c || !out || !separate(c,sizeof(*c),out,sizeof(*out))) return MARC_STATUS_INVALID_ARGUMENT;
    marc::core::DecoderLimits limits;PositionAdaptiveHuffmanStreamHeader s;PositionAdaptiveHuffmanWorkspace r;std::size_t external{};
    const auto error=prepare(c,limits,s,r,external);if (error!=MARC_STATUS_OK) return error;
    Resources result{sizeof(Resources),MARC_ABI_VERSION,r.raw_bytes,r.token_bytes,r.serialized_bytes,r.finder_bytes,
        r.payload_bytes,r.private_serialized_bytes,r.node_bytes,r.symbol_bytes,r.owned_bytes,r.owner_bytes,r.control_bytes,
        external,r.aggregate_bytes,0};
    if (!marc::core::checked_add(std::max(static_cast<std::uint64_t>(r.aggregate_bytes),limits.max_block_size),
        static_cast<std::uint64_t>(external),result.minimum_aggregate_bytes)) return MARC_STATUS_LIMIT_EXCEEDED;
    *out=result;return MARC_STATUS_OK;
}
marc_status marc_lzss_position_distance_adaptive_huffman_create(const Config* c,marc_transform** out) noexcept {
    if (!out || (c && !separate(c,sizeof(*c),out,sizeof(*out)))) return MARC_STATUS_INVALID_ARGUMENT;
    *out=nullptr;marc::core::DecoderLimits limits;PositionAdaptiveHuffmanStreamHeader stream;PositionAdaptiveHuffmanWorkspace r;std::size_t external{};
    const auto error=prepare(c,limits,stream,r,external);if (error!=MARC_STATUS_OK) return error;
    Code code{};std::unique_ptr<marc::core::Transform> p;
    if (c->direction==MARC_DIRECTION_ENCODE) p=PositionAdaptiveHuffmanOwnedEncoder::create(stream,limits,code);
    else p=PositionAdaptiveHuffmanOwnedDecoder::create(stream.profile,c->frame_size,limits,code);
    if (!p) return profile_factory_status(code);
    std::unique_ptr<Boundary> b(new(std::nothrow) Boundary(std::move(p),static_cast<std::size_t>(c->input_capacity_bytes),static_cast<std::size_t>(c->output_capacity_bytes)));
    if (!b) return MARC_STATUS_OUT_OF_MEMORY;
    auto* h=new(std::nothrow) marc_transform{b.get()};if (!h) return MARC_STATUS_OUT_OF_MEMORY;
    b->handle(h);b.release();*out=h;return MARC_STATUS_OK;
}

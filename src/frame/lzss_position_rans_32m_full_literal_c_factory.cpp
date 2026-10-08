#include "frame/lzss_position_rans_32m_full_literal_owned.hpp"
#include "core/buffer_overlap.hpp"
#include "marc/marc.h"
#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <new>

struct marc_transform { marc::core::Transform* implementation; };
namespace {
using namespace marc;
using namespace frame::internal;
using Config=marc_lzss_position_distance_rans_32m_config;
using Resources=marc_lzss_position_distance_rans_32m_resources;
using Code=core::ErrorCode;
bool separate(const void* a,std::size_t an,const void* b,std::size_t bn) noexcept {
    return core::check_buffer_overlap(a,an,b,bn)==core::BufferOverlap::disjoint;
}
marc_status status(Code c) noexcept {
    if (c==Code::none) return MARC_STATUS_OK;
    if (c==Code::limit_exceeded) return MARC_STATUS_LIMIT_EXCEEDED;
    if (c==Code::out_of_memory) return MARC_STATUS_OUT_OF_MEMORY;
    if (c==Code::invalid_argument) return MARC_STATUS_INVALID_ARGUMENT;
    return MARC_STATUS_INTERNAL_ERROR;
}
class Boundary final : public core::Transform {
public:
    Boundary(std::unique_ptr<core::Transform> transform,std::size_t in,std::size_t out) noexcept
        : transform_(std::move(transform)),input_capacity_(in),output_capacity_(out) {}
    void handle(marc_transform* h) noexcept { handle_=h; }
    core::ProcessResult process(std::span<const std::byte> input,std::span<std::byte> output,std::uint32_t flags) noexcept override {
        if (terminal_) return {0,0,last_.status,last_.error};
        const bool limited=input.size()>input_capacity_ || output.size()>output_capacity_;
        if (limited || !separate(input.data(),input.size(),handle_,sizeof(*handle_))
            || !separate(output.data(),output.size(),handle_,sizeof(*handle_))
            || !separate(input.data(),input.size(),this,sizeof(*this))
            || !separate(output.data(),output.size(),this,sizeof(*this))) {
            last_={0,0,core::StreamStatus::error,{limited ? Code::limit_exceeded : Code::invalid_argument,accepted_,0}};
            terminal_=true; return last_;
        }
        last_=transform_->process(input,output,flags);
        if (!core::checked_add(accepted_,static_cast<std::uint64_t>(last_.input_consumed),accepted_))
            last_.status=core::StreamStatus::error,last_.error={Code::internal_error,accepted_,0};
        terminal_=last_.status==core::StreamStatus::error || last_.status==core::StreamStatus::end_of_stream;
        return last_;
    }
private:
    std::unique_ptr<core::Transform> transform_;
    marc_transform* handle_{};
    std::size_t input_capacity_{},output_capacity_{};
    std::uint64_t accepted_{};
    core::ProcessResult last_{};
    bool terminal_{};
};
constexpr std::size_t public_working_bytes=4096;
marc_status prepare(const Config* c,core::DecoderLimits& l,PositionRans32mFullLiteralStreamHeader& s,
    PositionRans32mFullLiteralWorkspace& r,std::size_t& external) noexcept {
    if (!c || c->struct_size!=sizeof(*c) || c->abi_version!=MARC_ABI_VERSION || c->reserved || c->reserved2
        || (c->direction!=MARC_DIRECTION_ENCODE && c->direction!=MARC_DIRECTION_DECODE)
        || !c->frame_size || c->frame_size>33554432) return MARC_STATUS_INVALID_ARGUMENT;
    l={};
    l.max_total_output_size=c->max_total_output_size; l.max_frame_size=c->max_frame_size;
    l.max_block_size=c->max_block_size; l.max_compressed_payload_size=c->max_compressed_payload_size;
    l.max_internal_buffered_bytes=c->max_internal_buffered_bytes; l.max_lz_distance=c->max_lz_distance;
    l.max_lz_match_length=c->max_lz_match_length; l.max_entropy_table_entries=c->max_entropy_table_entries;
    l.max_expansion_ratio=c->max_expansion_ratio; l.expansion_slack=c->expansion_slack;
    if (core::validate_limits(l)!=core::LimitError::none) return MARC_STATUS_INVALID_ARGUMENT;
    external=sizeof(Boundary)+sizeof(marc_transform)+public_working_bytes;
    for (const auto charge : {c->external_retained_bytes,c->input_capacity_bytes,c->output_capacity_bytes}) {
        if (charge>std::numeric_limits<std::size_t>::max()
            || !core::checked_add(external,static_cast<std::size_t>(charge),external)) return MARC_STATUS_LIMIT_EXCEEDED;
    }
    if (external>=l.max_internal_buffered_bytes || l.max_internal_buffered_bytes-external<l.max_block_size)
        return MARC_STATUS_LIMIT_EXCEEDED;
    l.max_internal_buffered_bytes-=external;
    s={c->frame_size,c->direction==MARC_DIRECTION_ENCODE ? c->original_size : 0};
    return status(c->direction==MARC_DIRECTION_ENCODE ? PositionRans32mFullLiteralOwnedEncoder::requirements(s,l,r)
        : PositionRans32mFullLiteralOwnedDecoder::requirements(c->frame_size,l,r));
}
}
marc_status marc_lzss_position_distance_rans_32m_config_init(marc_direction direction,Config* out) noexcept {
    if (!out || (direction!=MARC_DIRECTION_ENCODE && direction!=MARC_DIRECTION_DECODE)) return MARC_STATUS_INVALID_ARGUMENT;
    Config c{}; c.struct_size=sizeof(c); c.abi_version=MARC_ABI_VERSION; c.direction=direction;
    c.frame_size=33554432; c.max_frame_size=33554432; c.max_block_size=10*UINT64_C(33554432);
    c.max_total_output_size=UINT64_C(1)<<40; c.max_compressed_payload_size=20*UINT64_C(33554432)+8;
    c.max_internal_buffered_bytes=UINT64_C(1536)<<20; c.max_lz_distance=33554432;
    c.max_lz_match_length=258; c.max_entropy_table_entries=4669;
    c.max_expansion_ratio=1024; c.expansion_slack=33554432;
    c.input_capacity_bytes=c.output_capacity_bytes=65536;
    *out=c; return MARC_STATUS_OK;
}
marc_status marc_lzss_position_distance_rans_32m_resource_requirements(const Config* c,Resources* out) noexcept {
    if (!c || !out || !separate(c,sizeof(*c),out,sizeof(*out))) return MARC_STATUS_INVALID_ARGUMENT;
    core::DecoderLimits l{}; PositionRans32mFullLiteralStreamHeader s{}; PositionRans32mFullLiteralWorkspace r{}; std::size_t external{};
    const auto e=prepare(c,l,s,r,external); if (e!=MARC_STATUS_OK) return e;
    Resources result{sizeof(Resources),MARC_ABI_VERSION,r.raw_bytes,r.token_count*sizeof(dictionary::internal::LzssTypedToken),
        r.serialized_bytes,r.finder_bytes,context::internal::lzss_position_rans_32m_full_literal_fixed_working_bytes,external,0};
    const auto admitted=std::max(static_cast<std::uint64_t>(r.aggregate_bytes),l.max_block_size);
    if (!core::checked_add(admitted,static_cast<std::uint64_t>(external),result.minimum_aggregate_bytes))
        return MARC_STATUS_LIMIT_EXCEEDED;
    *out=result; return MARC_STATUS_OK;
}
marc_status marc_lzss_position_distance_rans_32m_create(const Config* c,marc_transform** out) noexcept {
    if (!out || (c && !separate(c,sizeof(*c),out,sizeof(*out)))) return MARC_STATUS_INVALID_ARGUMENT;
    *out=nullptr;
    core::DecoderLimits l{}; PositionRans32mFullLiteralStreamHeader s{}; PositionRans32mFullLiteralWorkspace r{}; std::size_t external{};
    const auto e=prepare(c,l,s,r,external); if (e!=MARC_STATUS_OK) return e;
    Code error{}; std::unique_ptr<core::Transform> transform;
    if (c->direction==MARC_DIRECTION_ENCODE) transform=PositionRans32mFullLiteralOwnedEncoder::create(s,l,error);
    else transform=PositionRans32mFullLiteralOwnedDecoder::create(c->frame_size,l,error);
    if (!transform) return status(error);
    std::unique_ptr<Boundary> boundary(new(std::nothrow) Boundary(std::move(transform),
        static_cast<std::size_t>(c->input_capacity_bytes),static_cast<std::size_t>(c->output_capacity_bytes)));
    if (!boundary) return MARC_STATUS_OUT_OF_MEMORY;
    auto* handle=new(std::nothrow) marc_transform{boundary.get()};
    if (!handle) return MARC_STATUS_OUT_OF_MEMORY;
    boundary->handle(handle); boundary.release(); *out=handle; return MARC_STATUS_OK;
}

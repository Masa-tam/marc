#include "c_api/owned_boundary.hpp"
#include "c_api/status.hpp"
#include "c_api/transform_handle.hpp"
#include "frame/lzss_position_rans_64m_full_literal_owned.hpp"
#include "core/buffer_overlap.hpp"
#include "marc/marc.h"
#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <new>


namespace {
using marc::c_api::factory_status;

using namespace marc;
using namespace frame::internal;
using Config=marc_lzss_position_distance_rans_64m_config;
using Resources=marc_lzss_position_distance_rans_64m_resources;
using Code=core::ErrorCode;
using marc::c_api::disjoint_buffers;

using Boundary = marc::c_api::OwnedBoundary;
constexpr std::size_t public_working_bytes=4096;
marc_status prepare(const Config* c,core::DecoderLimits& l,PositionRans64mFullLiteralStreamHeader& s,
    PositionRans64mFullLiteralWorkspace& r,std::size_t& external) noexcept {
    if (!c || c->struct_size!=sizeof(*c) || c->abi_version!=MARC_ABI_VERSION || c->reserved || c->reserved2
        || (c->direction!=MARC_DIRECTION_ENCODE && c->direction!=MARC_DIRECTION_DECODE)
        || !c->frame_size || c->frame_size>67108864) return MARC_STATUS_INVALID_ARGUMENT;
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
    return factory_status(c->direction==MARC_DIRECTION_ENCODE ? PositionRans64mFullLiteralOwnedEncoder::requirements(s,l,r)
        : PositionRans64mFullLiteralOwnedDecoder::requirements(c->frame_size,l,r));
}
}
marc_status marc_lzss_position_distance_rans_64m_config_init(marc_direction direction,Config* out) noexcept {
    if (!out || (direction!=MARC_DIRECTION_ENCODE && direction!=MARC_DIRECTION_DECODE)) return MARC_STATUS_INVALID_ARGUMENT;
    Config c{}; c.struct_size=sizeof(c); c.abi_version=MARC_ABI_VERSION; c.direction=direction;
    c.frame_size=67108864; c.max_frame_size=67108864; c.max_block_size=10*UINT64_C(67108864);
    c.max_total_output_size=UINT64_C(1)<<40; c.max_compressed_payload_size=20*UINT64_C(67108864)+8;
    c.max_internal_buffered_bytes=UINT64_C(3072)<<20; c.max_lz_distance=67108864;
    c.max_lz_match_length=258; c.max_entropy_table_entries=4680;
    c.max_expansion_ratio=1024; c.expansion_slack=67108864;
    c.input_capacity_bytes=c.output_capacity_bytes=65536;
    *out=c; return MARC_STATUS_OK;
}
marc_status marc_lzss_position_distance_rans_64m_resource_requirements(const Config* c,Resources* out) noexcept {
    if (!c || !out || !disjoint_buffers(c,sizeof(*c),out,sizeof(*out))) return MARC_STATUS_INVALID_ARGUMENT;
    core::DecoderLimits l{}; PositionRans64mFullLiteralStreamHeader s{}; PositionRans64mFullLiteralWorkspace r{}; std::size_t external{};
    const auto e=prepare(c,l,s,r,external); if (e!=MARC_STATUS_OK) return e;
    Resources result{sizeof(Resources),MARC_ABI_VERSION,r.raw_bytes,r.token_count*sizeof(dictionary::internal::LzssTypedToken),
        r.serialized_bytes,r.finder_bytes,context::internal::lzss_position_rans_64m_full_literal_fixed_working_bytes,external,0};
    const auto admitted=std::max(static_cast<std::uint64_t>(r.aggregate_bytes),l.max_block_size);
    if (!core::checked_add(admitted,static_cast<std::uint64_t>(external),result.minimum_aggregate_bytes))
        return MARC_STATUS_LIMIT_EXCEEDED;
    *out=result; return MARC_STATUS_OK;
}
marc_status marc_lzss_position_distance_rans_64m_create(const Config* c,marc_transform** out) noexcept {
    if (!out || (c && !disjoint_buffers(c,sizeof(*c),out,sizeof(*out)))) return MARC_STATUS_INVALID_ARGUMENT;
    *out=nullptr;
    core::DecoderLimits l{}; PositionRans64mFullLiteralStreamHeader s{}; PositionRans64mFullLiteralWorkspace r{}; std::size_t external{};
    const auto e=prepare(c,l,s,r,external); if (e!=MARC_STATUS_OK) return e;
    Code error{}; std::unique_ptr<core::Transform> transform;
    if (c->direction==MARC_DIRECTION_ENCODE) transform=PositionRans64mFullLiteralOwnedEncoder::create(s,l,error);
    else transform=PositionRans64mFullLiteralOwnedDecoder::create(c->frame_size,l,error);
    if (!transform) return factory_status(error);
    std::unique_ptr<Boundary> boundary(new(std::nothrow) Boundary(std::move(transform),
        static_cast<std::size_t>(c->input_capacity_bytes),static_cast<std::size_t>(c->output_capacity_bytes)));
    if (!boundary) return MARC_STATUS_OUT_OF_MEMORY;
    auto* handle=new(std::nothrow) marc_transform{boundary.get()};
    if (!handle) return MARC_STATUS_OUT_OF_MEMORY;
    boundary->handle(handle); boundary.release(); *out=handle; return MARC_STATUS_OK;
}

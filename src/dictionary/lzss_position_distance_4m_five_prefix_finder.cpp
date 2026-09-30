#include "dictionary/lzss_position_distance_4m_five_prefix_finder.hpp"

#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>

namespace marc::dictionary::internal {
namespace {

constexpr std::size_t bucket_count = 65536;
constexpr std::size_t prefix_size = 3;
// Every inserted position is below 4 MiB; UINT32_MAX is never a live link.
constexpr std::uint32_t empty_link = std::numeric_limits<std::uint32_t>::max();

[[nodiscard]] std::size_t key_bucket(std::uint32_t key) noexcept {
    key ^= key >> 11U;
    return static_cast<std::size_t>((key * UINT32_C(2654435761)) >> 16U);
}

[[nodiscard]] std::size_t bucket(const std::span<const std::byte> input,
                                 const std::size_t position,
                                 const std::size_t count) noexcept {
    std::uint32_t key = std::to_integer<std::uint32_t>(input[position]);
    key |= std::to_integer<std::uint32_t>(input[position + 1]) << 8U;
    key |= std::to_integer<std::uint32_t>(input[position + 2]) << 16U;
    if (count >= 4)
        key |= std::to_integer<std::uint32_t>(input[position + 3]) << 24U;
    if (count == 5) key ^= std::to_integer<std::uint32_t>(input[position+4]) * UINT32_C(2246822519);
    return key_bucket(key);
}

} // namespace

LzssShortPrefixWorkspaceRequirements calculate_lzss_position_distance_4m_five_prefix_workspace(
    const std::size_t input_size, const LzssParameters& parameters,
    const core::DecoderLimits& limits) noexcept {
    return calculate_lzss_position_distance_4m_five_prefix_workspace(
        input_size, parameters, limits,
        LzssTypedTokenVariant::field_context_4m_short_length_escape);
}

LzssShortPrefixWorkspaceRequirements calculate_lzss_position_distance_4m_five_prefix_workspace(
    const std::size_t input_size, const LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const LzssTypedTokenVariant variant) noexcept {
    LzssShortPrefixWorkspaceRequirements result{};
    if (variant != LzssTypedTokenVariant::field_context_4m_short_length_escape) {
        result.error = LzssShortPrefixError::invalid_parameters;
        return result;
    }
    const auto parameter_error = validate_lzss_typed_parameters(
        parameters, limits, variant);
    if (parameter_error != LzssTypedTokenError::none) {
        result.error = parameter_error == LzssTypedTokenError::limit_exceeded
            ? LzssShortPrefixError::input_limit_exceeded
            : LzssShortPrefixError::invalid_parameters;
        return result;
    }
    if (input_size > 4194304 || input_size > limits.max_frame_size
        || input_size > limits.max_block_size
        || input_size > limits.max_total_output_size) {
        result.error = LzssShortPrefixError::input_limit_exceeded;
        return result;
    }
    std::size_t array_bytes{}, aggregate{};
    if (input_size >= prefix_size &&
        (!core::checked_add(bucket_count,input_size,array_bytes)
         || !core::checked_multiply(array_bytes,std::size_t{12},array_bytes))) {
        result.error=LzssShortPrefixError::arithmetic_overflow;return result;
    }
    result.workspace_size=array_bytes;
    if (!core::checked_add(input_size,array_bytes,aggregate)
        || !core::checked_add(aggregate,sizeof(LzssPositionDistance4mFivePrefixFinder),aggregate))
        result.error=LzssShortPrefixError::arithmetic_overflow;
    else if (aggregate>limits.max_internal_buffered_bytes)
        result.error=LzssShortPrefixError::workspace_limit_exceeded;
    return result;
}

LzssShortPrefixError initialize_lzss_position_distance_4m_five_prefix_finder(
    const std::span<const std::byte> input,
    const LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::span<std::byte> workspace,
    LzssPositionDistance4mFivePrefixFinder& finder) noexcept {
    return initialize_lzss_position_distance_4m_five_prefix_finder(
        input, parameters, limits, workspace, finder,
        LzssTypedTokenVariant::field_context_4m_short_length_escape);
}

LzssShortPrefixError initialize_lzss_position_distance_4m_five_prefix_finder(
    const std::span<const std::byte> input,
    const LzssParameters& parameters,
    const core::DecoderLimits& limits,
    const std::span<std::byte> workspace,
    LzssPositionDistance4mFivePrefixFinder& finder,
    const LzssTypedTokenVariant variant) noexcept {
    const auto required = calculate_lzss_position_distance_4m_five_prefix_workspace(
        input.size(), parameters, limits, variant);
    if (required.error != LzssShortPrefixError::none) return required.error;
    if (workspace.size() < required.workspace_size)
        return LzssShortPrefixError::workspace_too_small;
    const auto active = workspace.first(required.workspace_size);
    if (!active.empty()
        && reinterpret_cast<std::uintptr_t>(active.data())
               % required.workspace_alignment != 0) {
        return LzssShortPrefixError::misaligned_workspace;
    }
    const auto overlap = core::check_buffer_overlap(
        input.data(), input.size(), active.data(), active.size());
    if (overlap == core::BufferOverlap::overlap)
        return LzssShortPrefixError::overlapping_buffers;
    if (overlap == core::BufferOverlap::arithmetic_overflow)
        return LzssShortPrefixError::arithmetic_overflow;

    for (const auto region : {std::pair<const void*,std::size_t>{&parameters,sizeof(parameters)},
            {&limits,sizeof(limits)}, {&finder,sizeof(finder)}}) {
        const auto check=core::check_buffer_overlap(active.data(),active.size(),region.first,region.second);
        if (check!=core::BufferOverlap::disjoint)
            return check==core::BufferOverlap::arithmetic_overflow
                ? LzssShortPrefixError::arithmetic_overflow : LzssShortPrefixError::overlapping_buffers;
    }
    LzssPositionDistance4mFivePrefixFinder initialized{};
    for (const auto region : {std::pair<const void*,std::size_t>{input.data(),input.size()},
            {&parameters,sizeof(parameters)}, {&limits,sizeof(limits)}}) {
        const auto check=core::check_buffer_overlap(&finder,sizeof(finder),region.first,region.second);
        if (check!=core::BufferOverlap::disjoint)
            return check==core::BufferOverlap::arithmetic_overflow
                ? LzssShortPrefixError::arithmetic_overflow : LzssShortPrefixError::overlapping_buffers;
    }
    initialized.input_ = input;
    initialized.parameters_ = parameters;
    if (!active.empty()) {
        auto* const words = reinterpret_cast<std::uint32_t*>(active.data());
        for (std::size_t i=0;i<3;++i) {
            initialized.heads_[i]={words+i*bucket_count,bucket_count};
            initialized.links_[i]={words+3*bucket_count+i*input.size(),input.size()};
        }
        for (std::size_t index=0;index<3*(bucket_count+input.size());++index)
            std::construct_at(words+index,empty_link);
    }
    finder = initialized;
    return LzssShortPrefixError::none;
}

LzssMatch LzssPositionDistance4mFivePrefixFinder::find_match(std::size_t p) const noexcept {
    LzssMatch best{};
    if(p!=next_position_ || p>=input_.size() || input_.size()-p<3 || heads_[0].empty()) return best;
    const auto maximum=std::min<std::size_t>(parameters_.max_match_length,input_.size()-p);
    const auto equal=[&](std::size_t c,std::size_t n) {
        for(std::size_t i=0;i<n;++i) if(input_[p+i]!=input_[c+i]) return false;
        return true;
    };
    const auto in_window=[&](std::size_t c) {return c<p && p-c<=parameters_.window_size;};
    auto nearest=empty_link;
    for(auto c=heads_[0][bucket(input_,p,3)];c!=empty_link;c=links_[0][c]) {
        if(!in_window(c)) break;
        if(equal(c,3)) {nearest=c;best={static_cast<std::uint32_t>(p-c),3};break;}
    }
    if(!best.length || maximum==3) return best;
    auto first=input_[p+3]==input_[nearest+3]?nearest:heads_[1][bucket(input_,p,4)];
    nearest=empty_link;
    for(auto c=first;c!=empty_link;c=links_[1][c]) {
        if(!in_window(c)) break;
        if(equal(c,4)) {nearest=c;best={static_cast<std::uint32_t>(p-c),4};break;}
    }
    if(nearest==empty_link || maximum==4) return best;
    first=input_[p+4]==input_[nearest+4]?nearest:heads_[2][bucket(input_,p,5)];
    for(auto c=first;c!=empty_link;c=links_[2][c]) {
        if(!in_window(c)) break;
        if(input_[p+best.length]!=input_[c+best.length] || !equal(c,5)) continue;
        std::size_t length=5;
        while(length<maximum && input_[p+length]==input_[c+length]) ++length;
        if(length>best.length) {
            best={static_cast<std::uint32_t>(p-c),static_cast<std::uint32_t>(length)};
            if(length==maximum) break;
        }
    }
    return best;
}

void LzssPositionDistance4mFivePrefixFinder::advance(std::size_t p,std::size_t next) noexcept {
    if(p!=next_position_ || next<p || next>input_.size()) {next_position_=input_.size()+1;return;}
    const auto end=std::min(next,input_.size()>=5?input_.size()-4:0);
    for(;p<end;++p) {
        const auto three=std::to_integer<std::uint32_t>(input_[p])
            | (std::to_integer<std::uint32_t>(input_[p+1])<<8)
            | (std::to_integer<std::uint32_t>(input_[p+2])<<16);
        const auto four=three | (std::to_integer<std::uint32_t>(input_[p+3])<<24);
        const std::array<std::size_t,3> buckets{key_bucket(three),key_bucket(four),
            key_bucket(four ^ (std::to_integer<std::uint32_t>(input_[p+4])*UINT32_C(2246822519)))};
        for(std::size_t i=0;i<3;++i) {
            links_[i][p]=heads_[i][buckets[i]];heads_[i][buckets[i]]=static_cast<std::uint32_t>(p);
        }
    }
    for(;p<next;++p) for(std::size_t i=0;i<3;++i) {
        if(input_.size()-p<i+3) break;
        const auto b=bucket(input_,p,i+3);links_[i][p]=heads_[i][b];heads_[i][b]=static_cast<std::uint32_t>(p);
    }
    next_position_=next;
}
} // namespace marc::dictionary::internal

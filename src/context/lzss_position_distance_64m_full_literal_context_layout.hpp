#ifndef MARC_CONTEXT_LZSS_POSITION_DISTANCE_64M_FULL_LITERAL_CONTEXT_LAYOUT_HPP
#define MARC_CONTEXT_LZSS_POSITION_DISTANCE_64M_FULL_LITERAL_CONTEXT_LAYOUT_HPP
#include "context/lzss_field_context.hpp"
namespace marc::context::internal {
inline constexpr std::uint16_t lzss_position_distance_64m_full_literal_context_count=58;
inline constexpr std::size_t lzss_position_distance_64m_full_literal_frequency_entries=4680;
inline constexpr auto lzss_position_distance_64m_full_literal_alphabets=[] {
    std::array<std::uint16_t,58> a{};
    for(std::size_t i=0;i<3;++i)a[i]=2;
    for(std::size_t i=3;i<20;++i)a[i]=256;
    for(std::size_t i=20;i<23;++i)a[i]=9;
    for(std::size_t i=23;i<32;++i)a[i]=27;
    for(std::size_t i=32;i<58;++i)a[i]=2;
    return a;
}();
inline constexpr auto lzss_position_distance_64m_full_literal_offsets=[] {
    std::array<std::size_t,59> a{};
    for(std::size_t i=0;i<58;++i)a[i+1]=a[i]+lzss_position_distance_64m_full_literal_alphabets[i];
    return a;
}();
static_assert(lzss_position_distance_64m_full_literal_offsets[32]==4628);
static_assert(lzss_position_distance_64m_full_literal_offsets.back()==4680);
[[nodiscard]] constexpr LzssFieldContextError validate_lzss_position_distance_64m_full_literal_symbol(
    const ModeledOperation& op) noexcept {
    if(op.kind!=ModeledOperationKind::symbol)return LzssFieldContextError::unexpected_operation_kind;
    if(op.context_id>=32)return LzssFieldContextError::unexpected_context;
    const auto alphabet=lzss_position_distance_64m_full_literal_alphabets[op.context_id];
    if(op.alphabet_size!=alphabet)return LzssFieldContextError::unexpected_alphabet;
    if(op.value>=alphabet)return LzssFieldContextError::invalid_symbol;
    if(op.bit_count!=0)return LzssFieldContextError::nonzero_unused_field;
    return LzssFieldContextError::none;
}
}
#endif

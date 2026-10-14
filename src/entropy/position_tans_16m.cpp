#include "entropy/position_tans_16m.hpp"
#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"
#include "core/endian.hpp"
#include <algorithm>
#include <limits>

namespace marc::entropy::internal {
namespace {
using Error=PositionTans16mError;
constexpr std::uint32_t L=4096;
constexpr auto& offsets=position_tans_16m_offsets;
constexpr auto& alphabets=position_tans_16m_alphabets;
bool active(const PositionTans16mDescriptor& d,std::size_t c) noexcept {
    for (auto i=offsets[c];i<offsets[c+1];++i) if (d.frequencies[i]) return true;
    return false;
}
Error resources(const PositionTans16mDescriptor& d,const core::DecoderLimits& limits,
                std::size_t table_bytes) noexcept {
    std::size_t extent{};
    if (const auto e=validate_position_tans_16m_descriptor(d,limits,extent);e!=Error::none) return e;
    const auto charged=static_cast<std::uint64_t>(extent)+d.payload_size+table_bytes+UINT64_C(131072);
    return charged>limits.max_internal_buffered_bytes ? Error::limit_exceeded : Error::none;
}
bool disjoint(const void* a,std::size_t n,const void* b,std::size_t m) noexcept {
    return core::check_buffer_overlap(a,n,b,m)==core::BufferOverlap::disjoint;
}
TansDescriptor model(const PositionTans16mDescriptor& d,std::size_t c) noexcept {
    TansDescriptor result{};
    if (c==56) result.frequencies[0]=result.frequencies[1]=2048;
    else std::copy_n(d.frequencies.begin()+offsets[c],alphabets[c],result.frequencies.begin());
    return result;
}
template<class Entry> Error build(const PositionTans16mDescriptor& input,
    const core::DecoderLimits& limits,std::span<Entry> output) noexcept {
    if (output.size()<position_tans_16m_table_entries) return Error::output_too_small;
    std::size_t bytes{};
    if (!core::checked_multiply(output.size(),sizeof(Entry),bytes)) return Error::arithmetic_overflow;
    if (!disjoint(&input,sizeof(input),output.data(),bytes)) return Error::overlapping_buffers;
    if (const auto e=resources(input,limits,bytes);e!=Error::none) return e;
    const auto d=input;
    std::fill_n(output.begin(),position_tans_16m_table_entries,Entry{});
    for (std::size_t c=0;c<=56;++c) {
        if (c!=56 && !active(d,c)) continue;
        TansTables t{};
        if (build_tans_tables(model(d,c),t)!=TansTableError::none) return Error::invalid_table;
        if constexpr (sizeof(Entry)==sizeof(std::uint16_t))
            std::copy(t.encode_states.begin(),t.encode_states.end(),output.begin()+c*L);
        else std::copy(t.decode.begin(),t.decode.end(),output.begin()+c*L);
    }
    return Error::none;
}
}
PositionTans16mError PositionTans16mModelBuilder::add(int c,std::uint32_t symbol) noexcept {
    if (c < -1 || c>=56) return Error::invalid_context;
    if (symbol>=(c==-1 ? 2U : alphabets[static_cast<std::size_t>(c)])) return Error::invalid_symbol;
    if (decisions_==UINT32_MAX) return Error::arithmetic_overflow;
    if (c!=-1) {
        const auto index=offsets[static_cast<std::size_t>(c)]+symbol;
        if (counts_[index]==UINT32_MAX || totals_[static_cast<std::size_t>(c)]==UINT32_MAX)
            return Error::arithmetic_overflow;
        ++counts_[index];++totals_[static_cast<std::size_t>(c)];
    }
    ++decisions_;return Error::none;
}
PositionTans16mError PositionTans16mModelBuilder::finish(PositionTans16mDescriptor& output) const noexcept {
    PositionTans16mDescriptor d{};d.decision_count=decisions_;
    for (std::size_t c=0;c<56;++c) {
        const auto total=totals_[c];if (!total) continue;
        std::uint32_t sum{};
        for (auto i=offsets[c];i<offsets[c+1];++i) {
            if (!counts_[i]) continue;
            d.frequencies[i]=static_cast<std::uint16_t>(std::max<std::uint64_t>(1,UINT64_C(4096)*counts_[i]/total));
            sum+=d.frequencies[i];
        }
        const auto error=[&](std::size_t i) { return static_cast<std::int64_t>(counts_[i])*L
                -static_cast<std::int64_t>(d.frequencies[i])*total; };
        while (sum!=L) {
            std::size_t selected=offsets[c+1];std::int64_t best{};
            for (auto i=offsets[c];i<offsets[c+1];++i) {
                if (sum<L ? counts_[i]==0 : d.frequencies[i]<=1) continue;
                const auto candidate=error(i);
                if (selected==offsets[c+1] || (sum<L ? candidate>best : candidate<=best)) {
                    selected=i;best=candidate;
                }
            }
            if (selected==offsets[c+1]) return Error::invalid_frequencies;
            if (sum<L) { ++d.frequencies[selected];++sum; }
            else { --d.frequencies[selected];--sum; }
        }
    }
    output=d;return Error::none;
}
PositionTans16mError build_position_tans_16m_encode_tables(const PositionTans16mDescriptor& d,
    const core::DecoderLimits& limits,std::span<std::uint16_t> output) noexcept {
    return build(d,limits,output);
}
PositionTans16mReverseWriter::PositionTans16mReverseWriter(const PositionTans16mDescriptor& d,
    const core::DecoderLimits& limits,std::span<const std::uint16_t> tables,
    std::span<std::byte> output) noexcept : descriptor_(d),tables_(tables),output_(output) {
    std::size_t table_bytes{};
    if (!core::checked_multiply(tables.size(),sizeof(std::uint16_t),table_bytes)) {
        error_=Error::arithmetic_overflow;return;
    }
    error_=resources(d,limits,table_bytes);
    if (error_!=Error::none) return;
    if (tables.size()<position_tans_16m_table_entries) { error_=Error::output_too_small;return; }
    if (!disjoint(output.data(),output.size(),tables.data(),table_bytes)
        || !disjoint(output.data(),output.size(),&d,sizeof(d))
        || !disjoint(output.data(),output.size(),this,sizeof(*this))) {
        error_=Error::overlapping_buffers;return;
    }
    if (!output.empty()) {
        if (output.size()!=d.payload_size) { error_=Error::output_too_small;return; }
        if (d.payload_size>2
            && (!core::checked_multiply(static_cast<std::size_t>(d.payload_size-3),std::size_t{8},cursor_)
                || !core::checked_add(cursor_,static_cast<std::size_t>(d.final_valid_bits),cursor_))) {
            error_=Error::arithmetic_overflow;return;
        }
        std::fill(output.begin(),output.end(),std::byte{0});
    }
}
PositionTans16mError PositionTans16mReverseWriter::add(int c,std::uint32_t symbol) noexcept {
    if (error_!=Error::none) return error_;
    if (finished_) return error_=Error::already_finished;
    if (c < -1 || c>=56) return error_=Error::invalid_context;
    if (symbol>=(c==-1 ? 2U : alphabets[static_cast<std::size_t>(c)])) return error_=Error::invalid_symbol;
    if (decisions_>=descriptor_.decision_count) return error_=Error::count_mismatch;
    const auto table_index=c==-1 ? 56U : static_cast<unsigned>(c);
    std::uint32_t frequency{},cumulative{};
    if (c==-1) { frequency=2048;cumulative=symbol*2048; }
    else {
        for (std::size_t s=0;s<symbol;++s) cumulative+=descriptor_.frequencies[offsets[table_index]+s];
        frequency=descriptor_.frequencies[offsets[table_index]+symbol];
        if (!frequency) return error_=Error::inactive_context;
    }
    bool found{};std::uint32_t next{};unsigned selected{};
    for (unsigned k=0;k<=12;++k) {
        const auto q=state_>>k;
        if (q<frequency || q>=2*frequency) continue;
        if (found || cumulative+q-frequency>=L) return error_=Error::invalid_table;
        next=tables_[table_index*L+cumulative+q-frequency];selected=k;found=true;
    }
    if (!found || next<L || next>=2*L) return error_=Error::invalid_table;
    if (bits_>std::numeric_limits<std::size_t>::max()-selected) return error_=Error::arithmetic_overflow;
    if (!output_.empty()) {
        if (selected>cursor_) return error_=Error::truncated;
        cursor_-=selected;
        for (unsigned bit=0;bit<selected;++bit) {
            if ((state_>>bit)&1U) output_[2+(cursor_+bit)/8]|=static_cast<std::byte>(1U<<((cursor_+bit)%8));
        }
    }
    if (c!=-1) used_[table_index]=true;
    state_=next;bits_+=selected;++decisions_;return Error::none;
}
PositionTans16mError PositionTans16mReverseWriter::finish(std::size_t& size,std::uint8_t& valid) noexcept {
    if (error_!=Error::none) return error_;
    if (finished_) return error_=Error::already_finished;
    if (decisions_!=descriptor_.decision_count) return error_=Error::count_mismatch;
    for (std::size_t c=0;c<56;++c) if (active(descriptor_,c)!=used_[c]) return error_=Error::unused_context;
    if (bits_>std::numeric_limits<std::size_t>::max()-7) return error_=Error::arithmetic_overflow;
    const auto n=2+(bits_+7)/8;
    const auto v=static_cast<std::uint8_t>(bits_==0 ? 0 : (bits_-1)%8+1);
    if (!output_.empty()) {
        if (n!=output_.size() || cursor_!=0 || v!=descriptor_.final_valid_bits) return error_=Error::count_mismatch;
        (void)core::store_le(output_,0,static_cast<std::uint16_t>(state_-L));
    }
    size=n;valid=v;finished_=true;return Error::none;
}
PositionTans16mError PositionTans16mDecoder::begin(const PositionTans16mDescriptor& input,
    std::span<const std::byte> payload,const core::DecoderLimits& limits,
    std::span<TansDecodeEntry> output) noexcept {
    std::size_t table_bytes{};
    if (!core::checked_multiply(output.size(),sizeof(TansDecodeEntry),table_bytes))
        return error_=Error::arithmetic_overflow;
    if (!disjoint(output.data(),table_bytes,this,sizeof(*this))
        || !disjoint(payload.data(),payload.size(),this,sizeof(*this)))
        return error_=Error::overlapping_buffers;
    *this=PositionTans16mDecoder{};
    if (const auto e=resources(input,limits,table_bytes);e!=Error::none) return error_=e;
    if (payload.size()!=input.payload_size) return error_=Error::invalid_payload_size;
    if (!disjoint(output.data(),table_bytes,payload.data(),payload.size())) return error_=Error::overlapping_buffers;
    std::uint16_t initial{};(void)core::load_le(payload,0,initial);
    if (initial>=L) return error_=Error::invalid_state;
    std::size_t bits{};
    if (payload.size()>2
        && (!core::checked_multiply(payload.size()-3,std::size_t{8},bits)
            || !core::checked_add(bits,static_cast<std::size_t>(input.final_valid_bits),bits)))
        return error_=Error::arithmetic_overflow;
    if (payload.size()>2 && (std::to_integer<unsigned>(payload.back())>>input.final_valid_bits)!=0)
        return error_=Error::nonzero_padding;
    if (const auto e=build(input,limits,output);e!=Error::none) return error_=e;
    descriptor_=input;payload_=payload;tables_=output.first(position_tans_16m_table_entries);
    state_=L+initial;bits_=bits;error_=Error::none;return error_;
}
PositionTans16mError PositionTans16mDecoder::read(int c,std::uint32_t& symbol) noexcept {
    if (error_!=Error::none) return error_;
    if (finished_) return error_=Error::already_finished;
    if (c < -1 || c>=56) return error_=Error::invalid_context;
    if (!disjoint(&symbol,sizeof(symbol),payload_.data(),payload_.size())
        || !disjoint(&symbol,sizeof(symbol),tables_.data(),tables_.size_bytes())
        || !disjoint(&symbol,sizeof(symbol),this,sizeof(*this)))
        return error_=Error::overlapping_buffers;
    if (decisions_>=descriptor_.decision_count) return error_=Error::count_mismatch;
    const auto index=c==-1 ? 56U : static_cast<unsigned>(c);
    if (c!=-1 && !active(descriptor_,index)) return error_=Error::inactive_context;
    if (state_<L || state_>=2*L) return error_=Error::invalid_state;
    const auto e=tables_[index*L+state_-L];
    if (e.bit_count>12 || e.symbol>=(c==-1 ? 2U : alphabets[index])
        || e.state_base<L || static_cast<std::uint32_t>(e.state_base)+(1U<<e.bit_count)>2*L)
        return error_=Error::invalid_table;
    if (cursor_>bits_ || e.bit_count>bits_-cursor_) return error_=Error::truncated;
    std::uint32_t value{};
    for (unsigned b=0;b<e.bit_count;++b) {
        const auto position=cursor_+b;
        value|=((std::to_integer<unsigned>(payload_[2+position/8])>>(position%8))&1U)<<b;
    }
    state_=e.state_base+value;cursor_+=e.bit_count;++decisions_;
    if (c!=-1) used_[index]=true;
    symbol=e.symbol;return Error::none;
}
PositionTans16mError PositionTans16mDecoder::finish() noexcept {
    if (error_!=Error::none) return error_;
    if (finished_) return error_=Error::already_finished;
    if (decisions_!=descriptor_.decision_count) return error_=Error::count_mismatch;
    if (state_!=L) return error_=Error::invalid_terminal_state;
    if (cursor_!=bits_) return error_=Error::trailing_bits;
    for (std::size_t c=0;c<56;++c) if (active(descriptor_,c)!=used_[c]) return error_=Error::unused_context;
    finished_=true;return Error::none;
}
} // namespace marc::entropy::internal





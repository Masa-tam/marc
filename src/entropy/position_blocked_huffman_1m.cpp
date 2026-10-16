#include "entropy/position_blocked_huffman_1m.hpp"
#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"
#include <algorithm>

namespace marc::entropy::internal {
namespace {
using Error=PositionBlockedHuffmanError;
using Descriptor=PositionBlockedHuffman1mDescriptor;
struct Region { const void* data{};std::size_t size{}; };
template<class T> bool region(std::span<T> span,Region& r) noexcept {
    r.data=span.data();return core::checked_multiply(span.size(),sizeof(T),r.size);
}
template<std::size_t N> Error separate(const std::array<Region,N>& regions) noexcept {
    for (std::size_t i=0;i<N;++i) for (std::size_t j=0;j<i;++j) {
        const auto e=core::check_buffer_overlap(regions[i].data,regions[i].size,regions[j].data,regions[j].size);
        if (e!=core::BufferOverlap::disjoint)
            return e==core::BufferOverlap::arithmetic_overflow ? Error::arithmetic_overflow : Error::overlapping_buffers;
    }
    return Error::none;
}
template<std::size_t N> Error budget(const std::array<Region,N>& regions,const core::DecoderLimits& limits) noexcept {
    // Bounded temporary canonical/decode table construction, not a public
    // grant or a measured native-stack/RSS figure.
    std::size_t total=16384;
    for (const auto r:regions) if (!core::checked_add(total,r.size,total)) return Error::arithmetic_overflow;
    return total>limits.max_internal_buffered_bytes ? Error::limit_exceeded : Error::none;
}
bool active(const Descriptor& d,std::size_t i) noexcept {
    return i<5 ? (d.field_mask&(1U<<i))!=0 : (d.override_mask&(UINT64_C(1)<<(i-5)))!=0;
}
Error select(const Descriptor& d,int context,std::size_t& slot,std::uint8_t& fields,std::uint64_t& overrides) noexcept {
    if (context<0 || context>=52) return Error::invalid_context;
    const auto c=static_cast<std::size_t>(context);
    const auto f=position_blocked_huffman_1m_field(c);
    if (!(d.field_mask&(1U<<f))) return Error::inactive_context;
    fields|=static_cast<std::uint8_t>(1U<<f);
    if (d.override_mask&(UINT64_C(1)<<c)) { slot=5+c;overrides|=UINT64_C(1)<<c; }
    else slot=f;
    return Error::none;
}
std::uint64_t valid_bits(const Descriptor& d) noexcept {
    return d.payload_size ? UINT64_C(8)*(d.payload_size-1)+d.final_valid_bits : 0;
}
} // namespace
PositionBlockedHuffman1mModelBuilder::PositionBlockedHuffman1mModelBuilder(std::uint32_t maximum) noexcept
    : maximum_decisions_(maximum) {
    if (maximum>8388608) error_=Error::limit_exceeded;
}
PositionBlockedHuffmanError PositionBlockedHuffman1mModelBuilder::add(int context,std::uint32_t symbol) noexcept {
    if (error_!=Error::none) return error_;
    if (decisions_>=maximum_decisions_) return error_=Error::limit_exceeded;
    if (context==-1) {
        if (symbol>1) return error_=Error::invalid_symbol;
        ++uniform_bits_;
    } else {
        if (context<0 || context>=52) return error_=Error::invalid_context;
        const auto c=static_cast<std::size_t>(context);
        if (symbol>=position_blocked_huffman_1m_alphabets[c]) return error_=Error::invalid_symbol;
        // Each counter is at most the bounded total decisions, so the
        // uint64 frequencies cannot overflow before the admission guard.
        ++counts_[position_blocked_huffman_1m_field(c)][symbol];++counts_[5+c][symbol];
    }
    ++decisions_;return Error::none;
}
PositionBlockedHuffmanError PositionBlockedHuffman1mModelBuilder::finish(
    const core::DecoderLimits& limits,Descriptor& output) const noexcept {
    const std::array<Region,3> regions{{{this,sizeof(*this)},{&limits,sizeof(limits)},{&output,sizeof(output)}}};
    if (const auto e=separate(regions);e!=Error::none) return e;
    if (error_!=Error::none) return error_;
    if (core::validate_limits(limits)!=core::LimitError::none || decisions_>limits.max_block_size) return Error::limit_exceeded;
    // The first-party package primitive has bounded 256-symbol/15-level
    // temporary storage. This is a conservative workspace charge, not RSS.
    if (sizeof(*this)+UINT64_C(2)*sizeof(Descriptor)+131072+2547>limits.max_internal_buffered_bytes) return Error::limit_exceeded;
    Descriptor planned{};planned.decision_count=decisions_;
    auto build=[](const HuffmanFrequencies& counts,PositionBlockedHuffmanModel& model) noexcept {
        std::size_t present{};std::uint16_t sole{};
        for (std::size_t s=0;s<counts.size();++s) if (counts[s]) { ++present;sole=static_cast<std::uint16_t>(s); }
        if (!present) return Error::inactive_context;
        if (present==1) { model.single_symbol=sole;return Error::none; }
        return build_length_limited_code_lengths(counts,model.lengths)==HuffmanBuildError::none ? Error::none : Error::invalid_model;
    };
    auto cost=[](const HuffmanFrequencies& counts,const PositionBlockedHuffmanModel& model,std::uint64_t& bits) noexcept {
        bits=0;
        for (std::size_t s=0;s<counts.size();++s) {
            std::uint64_t contribution{};
            if (!core::checked_multiply(counts[s],static_cast<std::uint64_t>(model.lengths[s]),contribution)
                || !core::checked_add(bits,contribution,bits)) return Error::arithmetic_overflow;
        }
        return Error::none;
    };
    for (std::size_t f=0;f<5;++f) {
        const auto e=build(counts_[f],planned.models[f]);
        if (e==Error::inactive_context) continue;
        if (e!=Error::none) return e;
        planned.field_mask|=static_cast<std::uint8_t>(1U<<f);
    }
    std::uint64_t payload_bits=uniform_bits_;
    for (std::size_t c=0;c<52;++c) {
        PositionBlockedHuffmanModel alternative{};
        const auto e=build(counts_[5+c],alternative);
        if (e==Error::inactive_context) continue;
        if (e!=Error::none) return e;
        const auto f=position_blocked_huffman_1m_field(c);
        std::uint64_t base_bits{},context_bits{};
        if (const auto status=cost(counts_[5+c],planned.models[f],base_bits);status!=Error::none) return status;
        if (const auto status=cost(counts_[5+c],alternative,context_bits);status!=Error::none) return status;
        const auto a=position_blocked_huffman_1m_alphabets[c];
        const auto present=static_cast<std::size_t>(std::count_if(alternative.lengths.begin(),alternative.lengths.end(),[](auto n){return n!=0;}));
        const auto record_bytes=alternative.single_symbol!=position_blocked_huffman_no_single ? std::size_t{2}
            : std::min(std::size_t{1}+(a+1)/2,std::size_t{3}+2*present);
        const bool use_override=base_bits>context_bits && base_bits-context_bits>UINT64_C(8)*record_bytes;
        if (use_override) {
            planned.override_mask|=UINT64_C(1)<<c;planned.models[5+c]=alternative;
        }
        if (!core::checked_add(payload_bits,use_override ? context_bits : base_bits,payload_bits)) return Error::arithmetic_overflow;
    }
    const auto payload_bytes=(payload_bits+7)/8;
    if (payload_bytes>UINT32_MAX) return Error::arithmetic_overflow;
    planned.payload_size=static_cast<std::uint32_t>(payload_bytes);
    planned.final_valid_bits=payload_bits ? static_cast<std::uint8_t>((payload_bits-1)%8+1) : 0;
    std::size_t extent{};
    if (const auto e=validate_position_blocked_huffman_1m_descriptor(planned,limits,extent);e!=Error::none) return e;
    output=planned;return Error::none;
}
PositionBlockedHuffmanError encode_position_blocked_huffman_1m(
    const Descriptor& d,std::span<const PositionBlockedHuffmanDecision> requests,const core::DecoderLimits& limits,
    std::span<CanonicalHuffmanTable> tables,std::span<std::byte> output,std::size_t& written) noexcept {
    std::array<Region,6> regions{{{&d,sizeof(d)},{&limits,sizeof(limits)},{},{},{},{&written,sizeof(written)}}};
    if (!region(requests,regions[2]) || !region(tables,regions[3]) || !region(output,regions[4])) return Error::arithmetic_overflow;
    if (const auto e=separate(regions);e!=Error::none) return e;
    if (const auto e=budget(regions,limits);e!=Error::none) return e;
    std::size_t extent{};
    if (const auto e=validate_position_blocked_huffman_1m_descriptor(d,limits,extent);e!=Error::none) return e;
    if (requests.size()!=d.decision_count) return Error::count_mismatch;
    if (tables.size()<57) return Error::scratch_too_small;
    if (output.size()<d.payload_size) return Error::output_too_small;
    for (std::size_t i=0;i<57;++i) if (active(d,i) && d.models[i].single_symbol==position_blocked_huffman_no_single)
        if (build_canonical_table(d.models[i].lengths,tables[i])!=HuffmanTableError::none) return Error::invalid_model;
    std::uint64_t bits{},used_overrides{};std::uint8_t used_fields{};
    for (const auto request:requests) {
        if (request.context==-1) {
            if (request.symbol>1) return Error::invalid_symbol;
            ++bits;continue;
        }
        std::size_t slot{};
        if (const auto e=select(d,request.context,slot,used_fields,used_overrides);e!=Error::none) return e;
        const auto& m=d.models[slot];
        if (m.single_symbol!=position_blocked_huffman_no_single) {
            if (request.symbol!=m.single_symbol) return Error::invalid_symbol;
        } else {
            if (request.symbol>=256 || !m.lengths[request.symbol]) return Error::invalid_symbol;
            bits+=m.lengths[request.symbol];
        }
    }
    if (bits!=valid_bits(d)) return Error::count_mismatch;
    if (used_fields!=d.field_mask || used_overrides!=d.override_mask) return Error::unused_model;
    // The complete preflight above makes the following write pass infallible.
    std::fill_n(output.begin(),d.payload_size,std::byte{0});std::uint64_t cursor{};
    for (const auto request:requests) {
        std::uint32_t code=request.symbol;std::uint8_t width=1;
        if (request.context!=-1) {
            const auto c=static_cast<std::size_t>(request.context);
            const auto slot=(d.override_mask&(UINT64_C(1)<<c)) ? 5+c : position_blocked_huffman_1m_field(c);
            if (d.models[slot].single_symbol!=position_blocked_huffman_no_single) continue;
            code=tables[slot][request.symbol].lsb_first;width=tables[slot][request.symbol].length;
        }
        for (unsigned bit=0;bit<width;++bit,++cursor)
            output[static_cast<std::size_t>(cursor/8)]|=static_cast<std::byte>(((code>>bit)&1U)<<(cursor%8));
    }
    written=d.payload_size;return Error::none;
}
PositionBlockedHuffmanError PositionBlockedHuffman1mPrivateWriter::begin(
    const Descriptor& d,const core::DecoderLimits& limits,
    std::span<CanonicalHuffmanTable> tables,std::span<std::byte> payload) noexcept {
    std::array<Region,5> regions{{{this,sizeof(*this)},{&d,sizeof(d)},{&limits,sizeof(limits)},{},{}}};
    if (!region(payload,regions[3]) || !region(tables,regions[4])) return Error::arithmetic_overflow;
    if (const auto e=separate(regions);e!=Error::none) return e;
    begun_=false;finished_=false;error_=Error::none;
    if (const auto e=budget(regions,limits);e!=Error::none) return error_=e;
    std::size_t extent{};
    if (const auto e=validate_position_blocked_huffman_1m_descriptor(d,limits,extent);e!=Error::none) return error_=e;
    if (tables.size()<57) return error_=Error::scratch_too_small;
    if (payload.size()<d.payload_size) return error_=Error::output_too_small;
    for (std::size_t i=0;i<57;++i) if (active(d,i) && d.models[i].single_symbol==position_blocked_huffman_no_single)
        if (build_canonical_table(d.models[i].lengths,tables[i])!=HuffmanTableError::none) return error_=Error::invalid_model;
    descriptor_=d;payload_=payload;tables_=tables;valid_bits_=valid_bits(d);
    cursor_=0;decisions_=0;used_fields_=0;used_overrides_=0;
    std::fill_n(payload.begin(),d.payload_size,std::byte{0});
    begun_=true;return Error::none;
}
PositionBlockedHuffmanError PositionBlockedHuffman1mPrivateWriter::add(int context,std::uint32_t symbol) noexcept {
    if (error_!=Error::none) return error_;
    if (!begun_) return Error::not_started;
    if (finished_) return Error::already_finished;
    if (decisions_>=descriptor_.decision_count) return error_=Error::count_mismatch;
    std::uint32_t code=symbol;std::uint8_t width=1;
    if (context==-1) {
        if (symbol>1) return error_=Error::invalid_symbol;
    } else {
        std::size_t slot{};
        if (const auto e=select(descriptor_,context,slot,used_fields_,used_overrides_);e!=Error::none) return error_=e;
        const auto& model=descriptor_.models[slot];
        if (model.single_symbol!=position_blocked_huffman_no_single) {
            if (symbol!=model.single_symbol) return error_=Error::invalid_symbol;
            code=0;width=0;
        } else {
            if (symbol>=256 || !model.lengths[symbol]) return error_=Error::invalid_symbol;
            code=tables_[slot][symbol].lsb_first;width=tables_[slot][symbol].length;
        }
    }
    if (width>valid_bits_-cursor_) return error_=Error::count_mismatch;
    for (unsigned bit=0;bit<width;++bit,++cursor_)
        payload_[static_cast<std::size_t>(cursor_/8)]|=static_cast<std::byte>(((code>>bit)&1U)<<(cursor_%8));
    ++decisions_;return Error::none;
}
PositionBlockedHuffmanError PositionBlockedHuffman1mPrivateWriter::finish(std::size_t& written) noexcept {
    for (const auto r : {Region{this,sizeof(*this)},Region{payload_.data(),payload_.size()},Region{tables_.data(),tables_.size_bytes()}}) {
        const auto e=core::check_buffer_overlap(r.data,r.size,&written,sizeof(written));
        if (e!=core::BufferOverlap::disjoint)
            return e==core::BufferOverlap::arithmetic_overflow ? Error::arithmetic_overflow : Error::overlapping_buffers;
    }
    if (error_!=Error::none) return error_;
    if (!begun_) return Error::not_started;
    if (!finished_) {
        if (decisions_!=descriptor_.decision_count) return error_=Error::count_mismatch;
        if (cursor_!=valid_bits_) return error_=Error::trailing_bits;
        if (used_fields_!=descriptor_.field_mask || used_overrides_!=descriptor_.override_mask) return error_=Error::unused_model;
        finished_=true;
    }
    written=descriptor_.payload_size;return Error::none;
}
PositionBlockedHuffmanError PositionBlockedHuffman1mDecoder::begin(
    const Descriptor& d,std::span<const std::byte> payload,const core::DecoderLimits& limits,
    std::span<HuffmanDecodeTable> tables) noexcept {
    std::array<Region,5> regions{{{this,sizeof(*this)},{&d,sizeof(d)},{&limits,sizeof(limits)},{},{}}};
    if (!region(payload,regions[3]) || !region(tables,regions[4])) return Error::arithmetic_overflow;
    if (const auto e=separate(regions);e!=Error::none) return e;
    begun_=false;finished_=false;error_=Error::none;
    if (const auto e=budget(regions,limits);e!=Error::none) return error_=e;
    std::size_t extent{};
    if (const auto e=validate_position_blocked_huffman_1m_descriptor(d,limits,extent);e!=Error::none) return error_=e;
    if (payload.size()!=d.payload_size) return error_=Error::invalid_metadata;
    if (tables.size()<57) return error_=Error::scratch_too_small;
    if (!payload.empty() && (std::to_integer<unsigned>(payload.back())>>d.final_valid_bits)) return error_=Error::nonzero_padding;
    for (std::size_t i=0;i<57;++i) if (active(d,i) && d.models[i].single_symbol==position_blocked_huffman_no_single)
        if (build_decode_table(d.models[i].lengths,tables[i])!=HuffmanTableError::none) return error_=Error::invalid_model;
    descriptor_=d;payload_=payload;tables_=tables;valid_bits_=valid_bits(d);
    cursor_=0;decisions_=0;used_fields_=0;used_overrides_=0;begun_=true;return Error::none;
}
PositionBlockedHuffmanError PositionBlockedHuffman1mDecoder::read(int context,std::uint32_t& output) noexcept {
    for (const auto r : {Region{this,sizeof(*this)},Region{payload_.data(),payload_.size()},Region{tables_.data(),tables_.size_bytes()}}) {
        const auto e=core::check_buffer_overlap(r.data,r.size,&output,sizeof(output));
        if (e!=core::BufferOverlap::disjoint)
            return e==core::BufferOverlap::arithmetic_overflow ? Error::arithmetic_overflow : Error::overlapping_buffers;
    }
    if (error_!=Error::none) return error_;
    if (!begun_) return Error::not_started;
    if (finished_) return Error::already_finished;
    if (decisions_>=descriptor_.decision_count) return error_=Error::count_mismatch;
    std::uint32_t symbol{};std::uint8_t width{};
    if (context==-1) {
        if (cursor_==valid_bits_) return error_=Error::truncated;
        symbol=(std::to_integer<unsigned>(payload_[static_cast<std::size_t>(cursor_/8)])>>(cursor_%8))&1U;width=1;
    } else {
        std::size_t slot{};
        if (const auto e=select(descriptor_,context,slot,used_fields_,used_overrides_);e!=Error::none) return error_=e;
        const auto& m=descriptor_.models[slot];
        if (m.single_symbol!=position_blocked_huffman_no_single) symbol=m.single_symbol;
        else {
            const auto available=static_cast<std::uint8_t>(std::min<std::uint64_t>(15,valid_bits_-cursor_));
            std::uint16_t bits{};
            for (unsigned b=0;b<available;++b)
                bits|=static_cast<std::uint16_t>(((std::to_integer<unsigned>(payload_[static_cast<std::size_t>((cursor_+b)/8)])>>((cursor_+b)%8))&1U)<<b);
            const auto r=decode_symbol(tables_[slot],bits,available);
            if (r.status!=HuffmanDecodeStatus::symbol)
                return error_=r.status==HuffmanDecodeStatus::need_input ? Error::truncated : Error::invalid_code;
            if (!r.bits_consumed || r.bits_consumed>available) return error_=Error::invalid_code;
            symbol=r.symbol;width=r.bits_consumed;
        }
    }
    cursor_+=width;++decisions_;output=symbol;return Error::none;
}
PositionBlockedHuffmanError PositionBlockedHuffman1mDecoder::finish() noexcept {
    if (error_!=Error::none) return error_;
    if (!begun_) return Error::not_started;
    if (finished_) return Error::none;
    if (decisions_!=descriptor_.decision_count) return error_=Error::count_mismatch;
    if (cursor_!=valid_bits_) return error_=Error::trailing_bits;
    if (used_fields_!=descriptor_.field_mask || used_overrides_!=descriptor_.override_mask) return error_=Error::unused_model;
    finished_=true;return Error::none;
}
} // namespace marc::entropy::internal


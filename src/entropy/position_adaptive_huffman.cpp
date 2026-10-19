#include "entropy/position_adaptive_huffman.hpp"
#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"
#include <algorithm>
#include <bit>
#include <initializer_list>

namespace marc::entropy::internal {
namespace {
using Error=PositionAdaptiveHuffmanError;
constexpr std::size_t controls=65536+position_adaptive_huffman_descriptor_size;
struct Region { const void* data;std::size_t size; };
Error separate(std::initializer_list<Region> regions) noexcept {
    for (auto a=regions.begin();a!=regions.end();++a) for (auto b=regions.begin();b!=a;++b) {
        const auto overlap=core::check_buffer_overlap(a->data,a->size,b->data,b->size);
        if (overlap==core::BufferOverlap::arithmetic_overflow) return Error::arithmetic_overflow;
        if (overlap!=core::BufferOverlap::disjoint) return Error::overlapping_buffers;
    }
    return Error::none;
}
Error charge(std::span<AdaptiveHuffmanNode> nodes,std::span<std::uint16_t> symbols,
             std::size_t payload,const core::DecoderLimits& limits) noexcept {
    std::size_t nb{},sb{},sum=controls;
    if (!core::checked_multiply(nodes.size(),sizeof(AdaptiveHuffmanNode),nb)
        || !core::checked_multiply(symbols.size(),sizeof(std::uint16_t),sb)
        || !core::checked_add(sum,nb,sum) || !core::checked_add(sum,sb,sum)
        || !core::checked_add(sum,payload,sum)) return Error::arithmetic_overflow;
    return core::validate_limits(limits)!=core::LimitError::none || sum>limits.max_internal_buffered_bytes
        ? Error::limit_exceeded : Error::none;
}
Error storage(PositionAdaptiveHuffmanProfile profile,const core::DecoderLimits& limits,
              std::span<AdaptiveHuffmanNode> nodes,std::span<std::uint16_t> symbols,
              std::size_t payload) noexcept {
    const auto* spec=position_adaptive_huffman_spec(profile);
    if (!spec) return Error::invalid_profile;
    if (nodes.size()<spec->node_entries() || symbols.size()<spec->alphabet_entries)
        return Error::output_too_small;
    if (spec->node_entries()+spec->alphabet_entries>limits.max_entropy_table_entries)
        return Error::limit_exceeded;
    return charge(nodes,symbols,payload,limits);
}
std::uint64_t valid_bits(const PositionAdaptiveHuffmanDescriptor& d) noexcept {
    return d.payload_size ? UINT64_C(8)*(d.payload_size-1)+d.final_valid_bits : 0;
}
}
PositionAdaptiveHuffmanError position_adaptive_huffman_workspace(
    PositionAdaptiveHuffmanProfile profile,std::size_t payload,std::size_t& output) noexcept {
    const auto* spec=position_adaptive_huffman_spec(profile);
    if (!spec) return Error::invalid_profile;
    std::size_t nb{},sb{},sum=controls;
    if (!core::checked_multiply(spec->node_entries(),sizeof(AdaptiveHuffmanNode),nb)
        || !core::checked_multiply(static_cast<std::size_t>(spec->alphabet_entries),sizeof(std::uint16_t),sb)
        || !core::checked_add(sum,nb,sum) || !core::checked_add(sum,sb,sum)
        || !core::checked_add(sum,payload,sum)) return Error::arithmetic_overflow;
    output=sum;return Error::none;
}
PositionAdaptiveHuffmanError PositionAdaptiveHuffmanModelBank::initialize(
    PositionAdaptiveHuffmanProfile profile,const core::DecoderLimits& limits,
    std::span<AdaptiveHuffmanNode> nodes,std::span<std::uint16_t> symbols) noexcept {
    if (const auto e=storage(profile,limits,nodes,symbols,0);e!=Error::none) return e;
    if (const auto e=separate({{nodes.data(),nodes.size_bytes()},{symbols.data(),symbols.size_bytes()},
            {this,sizeof(*this)},{&limits,sizeof(limits)}});e!=Error::none) return e;
    const auto* selected=position_adaptive_huffman_spec(profile);
    std::size_t n{},s{};
    for (std::size_t c=0;c<selected->contexts;++c) {
        const auto a=position_adaptive_huffman_alphabet(*selected,c);
        const auto count=2*static_cast<std::size_t>(a)+1;
        if (trees_[c].initialize(a,nodes.subspan(n,count),symbols.subspan(s,a))
            !=ContextualAdaptiveHuffmanTreeError::none) return Error::invalid_tree;
        n+=count;s+=a;
    }
    spec_=selected;
    return n==selected->node_entries() && s==selected->alphabet_entries ? Error::none : Error::invalid_tree;
}
ContextualAdaptiveHuffmanTree* PositionAdaptiveHuffmanModelBank::tree(int context) noexcept {
    return spec_ && context>=0 && static_cast<unsigned>(context)<spec_->contexts ? &trees_[static_cast<std::size_t>(context)] : nullptr;
}
bool PositionAdaptiveHuffmanModelBank::validate() const noexcept {
    if (!spec_) return false;
    for (std::size_t c=0;c<spec_->contexts;++c) if (!trees_[c].validate()) return false;
    return true;
}
PositionAdaptiveHuffmanError PositionAdaptiveHuffmanEncoder::begin(
    PositionAdaptiveHuffmanProfile profile,const core::DecoderLimits& limits,
    std::span<AdaptiveHuffmanNode> nodes,std::span<std::uint16_t> symbols,
    std::span<std::byte> output,const PositionAdaptiveHuffmanDescriptor* expected) noexcept {
    if (const auto e=storage(profile,limits,nodes,symbols,output.size());e!=Error::none) return error_=e;
    if (const auto e=separate({{nodes.data(),nodes.size_bytes()},{symbols.data(),symbols.size_bytes()},
            {output.data(),output.size()},{this,sizeof(*this)},{&limits,sizeof(limits)},
            {expected,expected ? sizeof(*expected) : 0}});e!=Error::none) return e;
    if (expected) {
        if (const auto e=validate_position_adaptive_huffman_descriptor(profile,*expected,
                expected->decision_count,expected->payload_size,limits);e!=Error::none) return error_=e;
        if (output.size()!=expected->payload_size) return error_=Error::output_too_small;
    }
    if (const auto e=bank_.initialize(profile,limits,nodes,symbols);e!=Error::none) return error_=e;
    profile_=profile;spec_=position_adaptive_huffman_spec(profile);limits_=limits;
    nodes_=nodes;symbols_=symbols;output_=output;expected_=expected ? *expected : PositionAdaptiveHuffmanDescriptor{};
    decisions_=0;bits_=0;writing_=expected!=nullptr;finished_=false;error_=Error::none;
    std::fill(output_.begin(),output_.end(),std::byte{});
    return Error::none;
}
PositionAdaptiveHuffmanError PositionAdaptiveHuffmanEncoder::begin_plan(
    PositionAdaptiveHuffmanProfile profile,const core::DecoderLimits& limits,
    std::span<AdaptiveHuffmanNode> nodes,std::span<std::uint16_t> symbols) noexcept {
    return begin(profile,limits,nodes,symbols,{},nullptr);
}
PositionAdaptiveHuffmanError PositionAdaptiveHuffmanEncoder::begin_write(
    PositionAdaptiveHuffmanProfile profile,const PositionAdaptiveHuffmanDescriptor& d,
    const core::DecoderLimits& limits,std::span<AdaptiveHuffmanNode> nodes,
    std::span<std::uint16_t> symbols,std::span<std::byte> output) noexcept {
    return begin(profile,limits,nodes,symbols,output,&d);
}
PositionAdaptiveHuffmanError PositionAdaptiveHuffmanEncoder::emit(int context,std::uint32_t symbol) noexcept {
    if (error_!=Error::none) return error_;
    if (finished_) return error_=Error::already_finished;
    if (context < -1 || context>=spec_->contexts) return error_=Error::invalid_context;
    const auto alphabet=context==-1 ? 2 : position_adaptive_huffman_alphabet(*spec_,static_cast<std::size_t>(context));
    if (symbol>=static_cast<unsigned>(alphabet)) return error_=Error::invalid_symbol;
    if (decisions_>=static_cast<std::uint64_t>(spec_->decisions_per_byte)*spec_->window_size
        || decisions_>=limits_.max_block_size) return error_=Error::limit_exceeded;
    if (writing_ && decisions_==expected_.decision_count) return error_=Error::count_mismatch;
    std::array<std::uint8_t,264> path{};std::size_t length{};
    ContextualAdaptiveHuffmanTree* tree=nullptr;bool known=false;
    if (context==-1) { path[0]=static_cast<std::uint8_t>(symbol);length=1; }
    else {
        tree=bank_.tree(context);known=tree->contains(static_cast<std::uint16_t>(symbol));
        const auto e=known ? tree->path_for_symbol(static_cast<std::uint16_t>(symbol),path,length)
                           : tree->path_for_nyt(path,length);
        if (e!=ContextualAdaptiveHuffmanTreeError::none) return error_=Error::invalid_tree;
        if (!known) for (unsigned b=0;b<std::bit_width(static_cast<unsigned>(alphabet-1));++b)
            path[length++]=static_cast<std::uint8_t>((symbol>>b)&1U);
    }
    std::size_t next{};
    if (!core::checked_add(bits_,length,next) || next>SIZE_MAX-7) return error_=Error::arithmetic_overflow;
    const auto payload=(next+7)/8;
    if (payload>limits_.max_compressed_payload_size
        || payload>(UINT64_C(267)*spec_->window_size+7)/8) return error_=Error::limit_exceeded;
    if (const auto e=charge(nodes_,symbols_,writing_ ? output_.size() : payload,limits_);e!=Error::none) return error_=e;
    if (writing_ && next>valid_bits(expected_)) return error_=Error::output_too_small;
    if (writing_) for (std::size_t b=0;b<length;++b)
        if (path[b]) output_[(bits_+b)/8]|=static_cast<std::byte>(1U<<((bits_+b)%8));
    if (tree && (known ? tree->observe_existing(static_cast<std::uint16_t>(symbol))
                       : tree->observe_new(static_cast<std::uint16_t>(symbol)))!=ContextualAdaptiveHuffmanTreeError::none)
        return error_=Error::invalid_tree;
    bits_=next;++decisions_;return Error::none;
}
PositionAdaptiveHuffmanError PositionAdaptiveHuffmanEncoder::finish_plan(PositionAdaptiveHuffmanDescriptor& output) noexcept {
    if (const auto e=separate({{&output,sizeof(output)},{this,sizeof(*this)},
            {nodes_.data(),nodes_.size_bytes()},{symbols_.data(),symbols_.size_bytes()},
            {output_.data(),output_.size()}});e!=Error::none) return e;
    if (error_!=Error::none) return error_;
    if (finished_ || writing_) return error_=Error::already_finished;
    if (!bank_.validate()) return error_=Error::invalid_tree;
    const auto payload=(bits_+7)/8;
    if (payload>UINT32_MAX) return error_=Error::arithmetic_overflow;
    PositionAdaptiveHuffmanDescriptor d{decisions_,static_cast<std::uint32_t>(payload),spec_->contexts,
        static_cast<std::uint8_t>(bits_ ? (bits_-1)%8+1 : 0),0};
    if (const auto e=validate_position_adaptive_huffman_descriptor(profile_,d,decisions_,d.payload_size,limits_);e!=Error::none) return error_=e;
    output=d;finished_=true;return Error::none;
}
PositionAdaptiveHuffmanError PositionAdaptiveHuffmanEncoder::finish_write() noexcept {
    if (error_!=Error::none) return error_;
    if (finished_ || !writing_) return error_=Error::already_finished;
    if (decisions_!=expected_.decision_count || bits_!=valid_bits(expected_)) return error_=Error::count_mismatch;
    if (!bank_.validate()) return error_=Error::invalid_tree;
    finished_=true;return Error::none;
}
PositionAdaptiveHuffmanError PositionAdaptiveHuffmanDecoder::begin(
    PositionAdaptiveHuffmanProfile profile,const PositionAdaptiveHuffmanDescriptor& d,
    std::span<const std::byte> payload,const core::DecoderLimits& limits,
    std::span<AdaptiveHuffmanNode> nodes,std::span<std::uint16_t> symbols) noexcept {
    if (payload.size()>UINT32_MAX) return error_=Error::invalid_payload;
    if (const auto e=validate_position_adaptive_huffman_descriptor(profile,d,d.decision_count,
            static_cast<std::uint32_t>(payload.size()),limits);e!=Error::none) return error_=e;
    if (const auto e=storage(profile,limits,nodes,symbols,payload.size());e!=Error::none) return error_=e;
    if (const auto e=separate({{nodes.data(),nodes.size_bytes()},{symbols.data(),symbols.size_bytes()},
            {payload.data(),payload.size()},{&d,sizeof(d)},{&limits,sizeof(limits)},{this,sizeof(*this)}});e!=Error::none) return e;
    if (!payload.empty() && (std::to_integer<unsigned>(payload.back())>>d.final_valid_bits)!=0)
        return error_=Error::nonzero_padding;
    const auto bit_count=valid_bits(d);
    if (bit_count>SIZE_MAX) return error_=Error::arithmetic_overflow;
    if (const auto e=bank_.initialize(profile,limits,nodes,symbols);e!=Error::none) return error_=e;
    spec_=position_adaptive_huffman_spec(profile);expected_=d;payload_=payload;nodes_=nodes;symbols_=symbols;
    bits_=static_cast<std::size_t>(bit_count);cursor_=0;decisions_=0;finished_=false;error_=Error::none;
    return Error::none;
}
PositionAdaptiveHuffmanError PositionAdaptiveHuffmanDecoder::read(int context,std::uint32_t& output) noexcept {
    if (const auto e=separate({{&output,sizeof(output)},{this,sizeof(*this)},
            {nodes_.data(),nodes_.size_bytes()},{symbols_.data(),symbols_.size_bytes()},
            {payload_.data(),payload_.size()}});e!=Error::none) return e;
    if (error_!=Error::none) return error_;
    if (finished_) return error_=Error::already_finished;
    if (decisions_==expected_.decision_count) return error_=Error::count_mismatch;
    if (context < -1 || context>=spec_->contexts) return error_=Error::invalid_context;
    auto cursor=cursor_;std::uint32_t value{};
    const auto bit=[&](std::uint32_t& v) {
        if (cursor==bits_) return false;
        v=(std::to_integer<unsigned>(payload_[cursor/8])>>(cursor%8))&1U;++cursor;return true;
    };
    if (context==-1) {
        if (!bit(value)) return error_=Error::truncated;
    } else {
        auto* tree=bank_.tree(context);auto index=tree->root();bool complete=false;
        for (std::size_t step=0;step<=512;++step) {
            if (index>=tree->node_count()) return error_=Error::invalid_path;
            const auto node=tree->node(index);
            if (node.kind==AdaptiveHuffmanNodeKind::internal) {
                std::uint32_t direction{};if (!bit(direction)) return error_=Error::truncated;
                index=direction ? node.right : node.left;continue;
            }
            const auto alphabet=position_adaptive_huffman_alphabet(*spec_,static_cast<std::size_t>(context));
            const bool fresh=node.kind==AdaptiveHuffmanNodeKind::nyt;
            if (fresh) {
                for (unsigned b=0;b<std::bit_width(static_cast<unsigned>(alphabet-1));++b) {
                    std::uint32_t raw{};if (!bit(raw)) return error_=Error::truncated;value|=raw<<b;
                }
                if (value>=alphabet || tree->contains(static_cast<std::uint16_t>(value))) return error_=Error::invalid_nyt_symbol;
            } else if (node.kind==AdaptiveHuffmanNodeKind::symbol) value=node.symbol;
            else return error_=Error::invalid_path;
            if (value>=alphabet) return error_=Error::invalid_symbol;
            const auto e=fresh ? tree->observe_new(static_cast<std::uint16_t>(value))
                               : tree->observe_existing(static_cast<std::uint16_t>(value));
            if (e!=ContextualAdaptiveHuffmanTreeError::none) return error_=Error::invalid_tree;
            complete=true;break;
        }
        if (!complete) return error_=Error::invalid_path;
    }
    cursor_=cursor;++decisions_;output=value;return Error::none;
}
PositionAdaptiveHuffmanError PositionAdaptiveHuffmanDecoder::finish() noexcept {
    if (error_!=Error::none) return error_;
    if (finished_) return error_=Error::already_finished;
    if (decisions_!=expected_.decision_count) return error_=Error::count_mismatch;
    if (cursor_!=bits_) return error_=Error::trailing_bits;
    if (!bank_.validate()) return error_=Error::invalid_tree;
    finished_=true;return Error::none;
}
static_assert(sizeof(PositionAdaptiveHuffmanEncoder)+sizeof(PositionAdaptiveHuffmanDecoder)+1024<65536);
} // namespace marc::entropy::internal

#ifndef MARC_FRAME_LZSS_PHASE_WORKSPACE_HPP
#define MARC_FRAME_LZSS_PHASE_WORKSPACE_HPP

#include "core/buffer_overlap.hpp"
#include "context/lzss_field_context.hpp"
#include <algorithm>
#include <array>
#include <memory>
#include <span>
#include <type_traits>

namespace marc::frame::internal {

// Private storage prototype only: no codec, model, wire identity or public ABI.
struct LzssPhaseConfig {
    std::size_t frame_capacity{}, budget{}, external_retained{}, peak_helper{};
};
struct LzssPhaseRequirements {
    std::size_t raw{}, serialized{}, token_count{}, operation_count{}, finder_bytes{};
    std::size_t phase_offset{}, arena{}, alignment{}, aggregate{};
    bool operator==(const LzssPhaseRequirements&) const = default;
};
enum class LzssPhaseError { none, invalid_argument, overflow, limit, too_small, overlap, misaligned, state };

class LzssPhaseWorkspace final {
    using Token = dictionary::internal::LzssTypedToken;
    using Operation = context::internal::ModeledOperation;
    static_assert(std::is_nothrow_default_constructible_v<Token> && std::is_trivially_destructible_v<Token>);
    static_assert(std::is_nothrow_default_constructible_v<Operation> && std::is_trivially_destructible_v<Operation>);
public:
    enum class Phase { unbound, bound, search, operations, error };
    LzssPhaseWorkspace() noexcept = default;
    LzssPhaseWorkspace(const LzssPhaseWorkspace&) = delete;
    LzssPhaseWorkspace& operator=(const LzssPhaseWorkspace&) = delete;
    ~LzssPhaseWorkspace() { release(); }

    // Failure preserves output; supplied helper/retained charges are not an
    // assertion about any future eight-MiB codec state.
    static LzssPhaseError query(const LzssPhaseConfig& c, LzssPhaseRequirements& output) noexcept {
        const auto overlap = core::check_buffer_overlap(&c,sizeof(c),&output,sizeof(output));
        if (overlap != core::BufferOverlap::disjoint) return LzssPhaseError::overlap;
        if (!c.frame_capacity || c.frame_capacity > 8388608) return LzssPhaseError::invalid_argument;
        LzssPhaseRequirements r{};
        r.raw=r.token_count=c.frame_capacity;
        r.alignment=std::max({alignof(Token),alignof(Operation),alignof(std::uint32_t)});
        std::size_t tokens{},operations{},words{},payload{},end{},total{};
        if (!core::checked_multiply(r.raw,std::size_t{18},payload)
            || !core::checked_add(payload,std::size_t{85},r.serialized)
            || !core::checked_multiply(r.raw,sizeof(Token),tokens)
            || !core::checked_multiply(r.raw,std::size_t{2},r.operation_count)
            || !core::checked_multiply(r.operation_count,sizeof(Operation),operations)
            || !core::checked_add(r.raw,std::size_t{65536},words)
            || !core::checked_multiply(words,std::size_t{12},r.finder_bytes)
            || !core::checked_add(tokens,(r.alignment-tokens%r.alignment)%r.alignment,end)
            || !core::checked_add(end,std::max(operations,r.finder_bytes),r.arena)) return LzssPhaseError::overflow;
        r.phase_offset=end;
        if (!charge(c,r.raw,r.serialized,r.arena,total)) return LzssPhaseError::overflow;
        if (total>c.budget) return LzssPhaseError::limit;
        r.aggregate=total;output=r;return LzssPhaseError::none;
    }

    // Entire supplied capacities remain borrowed; only typed prefixes exposed.
    LzssPhaseError bind(const LzssPhaseConfig& c, std::span<std::byte> raw,
        std::span<std::byte> serialized, std::span<std::byte> arena) noexcept {
        if (phase_!=Phase::unbound) return LzssPhaseError::state;
        LzssPhaseRequirements r{};
        auto error=query(c,r);if (error!=LzssPhaseError::none) return error;
        struct Region {const void* data;std::size_t size;};
        const std::array regions{Region{raw.data(),raw.size()},Region{serialized.data(),serialized.size()},
            Region{arena.data(),arena.size()},Region{&c,sizeof(c)},Region{this,sizeof(*this)}};
        // Validate every address extent, including a null nonempty span.
        for (const auto& region:regions) {
            std::uintptr_t end{};
            if (region.size && !region.data) return LzssPhaseError::invalid_argument;
            if (!core::checked_add(reinterpret_cast<std::uintptr_t>(region.data),region.size,end)) return LzssPhaseError::overflow;
        }
        for (std::size_t i=0;i<regions.size();++i) for (std::size_t j=i+1;j<regions.size();++j)
            if (core::check_buffer_overlap(regions[i].data,regions[i].size,regions[j].data,regions[j].size)
                != core::BufferOverlap::disjoint) return LzssPhaseError::overlap;
        if (raw.size()<r.raw || serialized.size()<r.serialized || arena.size()<r.arena) return LzssPhaseError::too_small;
        if (reinterpret_cast<std::uintptr_t>(arena.data())%r.alignment) return LzssPhaseError::misaligned;
        std::size_t total{};
        if (!charge(c,raw.size(),serialized.size(),arena.size(),total)) return LzssPhaseError::overflow;
        if (total>c.budget) return LzssPhaseError::limit;
        // All potentially failing checks precede construction/publication.
        auto* tokens=reinterpret_cast<Token*>(arena.data());
        for (std::size_t i=0;i<r.token_count;++i) std::construct_at(tokens+i);
        requirements_=r;arena_=arena;tokens_={tokens,r.token_count};phase_=Phase::bound;
        return LzssPhaseError::none;
    }
    [[nodiscard]] Phase phase() const noexcept {return phase_;}
    [[nodiscard]] std::span<Token> tokens() noexcept {return tokens_;}
    [[nodiscard]] std::span<std::uint32_t> finder_words() noexcept {
        return phase_==Phase::search ? std::span{reinterpret_cast<std::uint32_t*>(phase_data()),requirements_.finder_bytes/4} : std::span<std::uint32_t>{};
    }
    [[nodiscard]] std::span<Operation> operations() noexcept {
        return phase_==Phase::operations ? std::span{reinterpret_cast<Operation*>(phase_data()),requirements_.operation_count} : std::span<Operation>{};
    }
    // Caller must retire all finder objects/references before switching.
    LzssPhaseError begin_search() noexcept {
        if (phase_!=Phase::bound) return LzssPhaseError::state;
        auto* p=reinterpret_cast<std::uint32_t*>(phase_data());
        for (std::size_t i=0;i<requirements_.finder_bytes/4;++i) std::construct_at(p+i);
        phase_=Phase::search;return LzssPhaseError::none;
    }
    LzssPhaseError begin_operations() noexcept {
        if (phase_!=Phase::search) return LzssPhaseError::state;
        retire_phase();
        auto* p=reinterpret_cast<Operation*>(phase_data());
        for (std::size_t i=0;i<requirements_.operation_count;++i) std::construct_at(p+i);
        phase_=Phase::operations;return LzssPhaseError::none;
    }
    LzssPhaseError finish_frame() noexcept {
        if (phase_!=Phase::operations) return LzssPhaseError::state;
        retire_phase();phase_=Phase::bound;return LzssPhaseError::none;
    }
    void fail() noexcept {
        if (phase_==Phase::unbound) return;
        retire_phase();phase_=Phase::error;
    }
    void release() noexcept {
        retire_phase();
        for (auto& token:tokens_) std::destroy_at(&token);
        tokens_={};arena_={};requirements_={};phase_=Phase::unbound;
    }
private:
    static bool charge(const LzssPhaseConfig& c,std::size_t raw,std::size_t serialized,
        std::size_t arena,std::size_t& output) noexcept {
        std::size_t total=sizeof(LzssPhaseWorkspace);
        for (auto n:{raw,serialized,arena,c.external_retained,c.peak_helper})
            if (!core::checked_add(total,n,total)) return false;
        output=total;return true;
    }
    std::byte* phase_data() noexcept {return arena_.data()+requirements_.phase_offset;}
    void retire_phase() noexcept {
        if (phase_==Phase::search) for (auto& word:finder_words()) std::destroy_at(&word);
        if (phase_==Phase::operations) for (auto& operation:operations()) std::destroy_at(&operation);
    }
    LzssPhaseRequirements requirements_{};
    std::span<std::byte> arena_{};
    std::span<Token> tokens_{};
    Phase phase_{Phase::unbound};
};
} // namespace marc::frame::internal
#endif

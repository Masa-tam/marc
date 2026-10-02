#include "frame/lzss_phase_workspace.hpp"
#include <gtest/gtest.h>
#include <atomic>
#include <cstdlib>
#include <limits>
#include <new>
#include <vector>

namespace {std::atomic<std::size_t> allocations{};}
void* operator new(std::size_t n) {++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n) {return ::operator new(n);}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete[](void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void operator delete[](void* p,std::size_t) noexcept {std::free(p);}
namespace {
using namespace marc::frame::internal;
using E=LzssPhaseError;using P=LzssPhaseWorkspace::Phase;
constexpr auto unlimited=std::numeric_limits<std::size_t>::max();
struct Buffers {
    LzssPhaseConfig c{17,unlimited,32,64};LzssPhaseRequirements r{};
    std::vector<std::byte> raw,serialized;
    std::vector<std::uint32_t> arena;
    Buffers() {EXPECT_EQ(LzssPhaseWorkspace::query(c,r),E::none);raw.resize(r.raw,std::byte{0x61});serialized.resize(r.serialized,std::byte{0x62});arena.resize((r.arena+3)/4,0x63636363);}
    std::span<std::byte> storage() {return std::as_writable_bytes(std::span{arena}).first(r.arena);}
};
TEST(LzssPhaseWorkspace, EightMiBConcreteStorageQuery) {
    LzssPhaseRequirements r{};
    ASSERT_EQ(LzssPhaseWorkspace::query({8388608,512U<<20,32,64},r),E::none);
    EXPECT_EQ(r.arena,369098752U);EXPECT_EQ(r.serialized,150995029U);
    EXPECT_EQ(r.aggregate,528482389U+sizeof(LzssPhaseWorkspace)+96U);
    EXPECT_EQ(r.alignment,4U);
    RecordProperty("prototype_owner_bytes",static_cast<int>(sizeof(LzssPhaseWorkspace)));
}
TEST(LzssPhaseWorkspace, QueryFailurePreservesOutput) {
    LzssPhaseRequirements r{1,2,3,4,5,6,7,8,9};const auto old=r;
    EXPECT_EQ(LzssPhaseWorkspace::query({0,unlimited,0,0},r),E::invalid_argument);EXPECT_EQ(r,old);
    EXPECT_EQ(LzssPhaseWorkspace::query({8388609,unlimited,0,0},r),E::invalid_argument);EXPECT_EQ(r,old);
    EXPECT_EQ(LzssPhaseWorkspace::query({1,unlimited,unlimited,0},r),E::overflow);EXPECT_EQ(r,old);
    EXPECT_EQ(LzssPhaseWorkspace::query({1,unlimited,0,unlimited},r),E::overflow);EXPECT_EQ(r,old);
    EXPECT_EQ(LzssPhaseWorkspace::query({1,1,0,0},r),E::limit);EXPECT_EQ(r,old);
}
TEST(LzssPhaseWorkspace, ExactBudgetAndOneByteLess) {
    Buffers b;LzssPhaseWorkspace w;auto c=b.c;c.budget=b.r.aggregate-1;
    const auto raw=b.raw,serial=b.serialized;const auto arena=b.arena;
    EXPECT_EQ(w.bind(c,b.raw,b.serialized,b.storage()),E::limit);
    EXPECT_EQ(w.phase(),P::unbound);EXPECT_EQ(b.raw,raw);EXPECT_EQ(b.serialized,serial);EXPECT_EQ(b.arena,arena);
    c.budget++;EXPECT_EQ(w.bind(c,b.raw,b.serialized,b.storage()),E::none);
}
TEST(LzssPhaseWorkspace, AllShortBuffersPreserveStorage) {
    Buffers b;const auto arena=b.arena;
    for(int which=0;which<3;++which) {
        LzssPhaseWorkspace w;auto raw=std::span{b.raw},serial=std::span{b.serialized},storage=b.storage();
        if(which==0)raw=raw.first(raw.size()-1);if(which==1)serial=serial.first(serial.size()-1);if(which==2)storage=storage.first(storage.size()-1);
        EXPECT_EQ(w.bind(b.c,raw,serial,storage),E::too_small);EXPECT_EQ(w.phase(),P::unbound);EXPECT_EQ(b.arena,arena);
    }
}
TEST(LzssPhaseWorkspace, FullCapacitySurplusIsCharged) {
    for(int which=0;which<3;++which) {
        Buffers b;
        if(which==0)b.raw.push_back(std::byte{0x77});
        if(which==1)b.serialized.push_back(std::byte{0x77});
        if(which==2)b.arena.push_back(0x77777777);
        auto storage=std::as_writable_bytes(std::span{b.arena}).first(b.r.arena+(which==2?1:0));
        auto c=b.c;c.budget=b.r.aggregate;const auto arena=b.arena;LzssPhaseWorkspace w;
        EXPECT_EQ(w.bind(c,b.raw,b.serialized,storage),E::limit);EXPECT_EQ(b.arena,arena);
        c.budget++;EXPECT_EQ(w.bind(c,b.raw,b.serialized,storage),E::none);
    }
}
TEST(LzssPhaseWorkspace, OuterAndOwnerOverlapsRejected) {
    Buffers b;const auto arena=b.arena;LzssPhaseWorkspace w;
    EXPECT_EQ(w.bind(b.c,b.raw,b.raw,b.storage()),E::overlap);
    EXPECT_EQ(w.bind(b.c,b.storage(),b.serialized,b.storage()),E::overlap);
    EXPECT_EQ(w.bind(b.c,b.raw,b.storage(),b.storage()),E::overlap);
    auto owner=std::as_writable_bytes(std::span{&w,1});
    EXPECT_EQ(w.bind(b.c,owner,b.serialized,b.storage()),E::overlap);
    auto metadata=std::as_writable_bytes(std::span{&b.c,1});
    EXPECT_EQ(w.bind(b.c,metadata,b.serialized,b.storage()),E::overlap);
    EXPECT_EQ(w.phase(),P::unbound);EXPECT_EQ(b.arena,arena);
}
TEST(LzssPhaseWorkspace, MisalignmentPreservesStorage) {
    Buffers b;std::vector<std::uint32_t> backing(b.arena.size()+2,0x12345678);
    auto bytes=std::as_writable_bytes(std::span{backing});const auto old=backing;LzssPhaseWorkspace w;
    EXPECT_EQ(w.bind(b.c,b.raw,b.serialized,bytes.subspan(1,b.r.arena)),E::misaligned);EXPECT_EQ(backing,old);
}
TEST(LzssPhaseWorkspace, TokensSurviveRepeatedTypedTransitions) {
    Buffers b;LzssPhaseWorkspace w;ASSERT_EQ(w.bind(b.c,b.raw,b.serialized,b.storage()),E::none);
    for(auto& token:w.tokens())token.distance=0x12345678;
    for(int frame=0;frame<4;++frame) {
        ASSERT_EQ(w.begin_search(),E::none);auto words=w.finder_words();ASSERT_FALSE(words.empty());
        words.front()=0xfedcba98;words.back()=0x76543210;
        ASSERT_EQ(w.begin_operations(),E::none);EXPECT_TRUE(w.finder_words().empty());
        for(auto& op:w.operations())op.value=0x87654321;
        for(auto& token:w.tokens())EXPECT_EQ(token.distance,0x12345678U);
        EXPECT_EQ(w.finish_frame(),E::none);EXPECT_TRUE(w.operations().empty());EXPECT_EQ(w.phase(),P::bound);
    }
    w.release();EXPECT_TRUE(w.tokens().empty());EXPECT_EQ(w.phase(),P::unbound);
    EXPECT_EQ(w.bind(b.c,b.raw,b.serialized,b.storage()),E::none);
}
TEST(LzssPhaseWorkspace, IllegalTransitionsAreInert) {
    Buffers b;LzssPhaseWorkspace w;
    EXPECT_EQ(w.begin_search(),E::state);EXPECT_EQ(w.begin_operations(),E::state);EXPECT_EQ(w.finish_frame(),E::state);
    ASSERT_EQ(w.bind(b.c,b.raw,b.serialized,b.storage()),E::none);const auto arena=b.arena;
    EXPECT_EQ(w.begin_operations(),E::state);EXPECT_EQ(w.finish_frame(),E::state);EXPECT_EQ(b.arena,arena);
    EXPECT_EQ(w.bind(b.c,b.raw,b.serialized,b.storage()),E::state);
    ASSERT_EQ(w.begin_search(),E::none);const auto words=b.arena;
    EXPECT_EQ(w.begin_search(),E::state);EXPECT_EQ(w.finish_frame(),E::state);EXPECT_EQ(b.arena,words);
}
TEST(LzssPhaseWorkspace, ErrorsRetireEitherPhaseAndRemainSticky) {
    for(bool operations:{false,true}) {
        Buffers b;LzssPhaseWorkspace w;ASSERT_EQ(w.bind(b.c,b.raw,b.serialized,b.storage()),E::none);
        ASSERT_EQ(w.begin_search(),E::none);if(operations)ASSERT_EQ(w.begin_operations(),E::none);
        w.fail();w.fail();EXPECT_EQ(w.phase(),P::error);EXPECT_TRUE(w.finder_words().empty());EXPECT_TRUE(w.operations().empty());
        EXPECT_EQ(w.begin_search(),E::state);EXPECT_EQ(w.begin_operations(),E::state);
        w.release();w.release();EXPECT_EQ(w.phase(),P::unbound);
    }
}
TEST(LzssPhaseWorkspace, DestructionCleansActiveSearchOrOperations) {
    for(bool operations:{false,true}) {
        Buffers b;{LzssPhaseWorkspace w;ASSERT_EQ(w.bind(b.c,b.raw,b.serialized,b.storage()),E::none);
            ASSERT_EQ(w.begin_search(),E::none);if(operations)ASSERT_EQ(w.begin_operations(),E::none);}
        LzssPhaseWorkspace replacement;EXPECT_EQ(replacement.bind(b.c,b.raw,b.serialized,b.storage()),E::none);
    }
}
TEST(LzssPhaseWorkspace, BindTransitionsAndCleanupDoNotAllocate) {
    Buffers b;LzssPhaseWorkspace w;E statuses[5];const auto before=allocations.load();
    statuses[0]=w.bind(b.c,b.raw,b.serialized,b.storage());statuses[1]=w.begin_search();
    statuses[2]=w.begin_operations();statuses[3]=w.finish_frame();statuses[4]=w.begin_search();w.fail();w.release();
    const auto after=allocations.load();EXPECT_EQ(after,before);for(auto s:statuses)EXPECT_EQ(s,E::none);
}
} // namespace

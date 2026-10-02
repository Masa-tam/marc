#include "position_distance_4m_control_engine.hpp"
#include <gtest/gtest.h>
#include <cstring>
#include <vector>

namespace {
namespace core = marc::core;
using namespace marc::benchmarks::control_repeatability;
struct ForbiddenClock {
    static inline std::size_t calls{};
    static std::chrono::steady_clock::time_point now() { ++calls; return {}; }
};
struct Fake {
    std::size_t creates{}, clears{}, active{}, peak{}, fail_create{}, fail_process{}, calls{};
    bool oversized{}, stall{}, mismatch{}, premature{};
    bool create(bool) {
        ++creates;
        if (creates == fail_create) return false;
        ++active; peak = std::max(peak, active); return true;
    }
    void clear() { if (active) { --active; ++clears; } }
    core::ProcessResult process(std::span<const std::byte> in, std::span<std::byte> out, bool end) {
        ++calls;
        if (creates == fail_process) return {0, 0, core::StreamStatus::error,
            {core::ErrorCode::malformed_stream, 0, 0}};
        if (oversized) return {in.size() + 1, 0, core::StreamStatus::progress, {}};
        if (stall) return {0, 0, core::StreamStatus::need_input, {}};
        if (premature) return {0, 0, core::StreamStatus::end_of_stream, {}};
        const auto n = std::min(in.size(), out.size());
        std::copy_n(in.begin(), n, out.begin());
        if (mismatch && n) out[0] ^= std::byte{1};
        return {n, n, end ? core::StreamStatus::end_of_stream : core::StreamStatus::progress, {}};
    }
};
const std::array<std::byte, 3> raw{std::byte{1}, std::byte{2}, std::byte{3}};
void unchanged_failure(Fake& engine, std::span<const std::byte> expected = raw) {
    Report report{};
    report.completed = 123;
    report.samples[0].create = 456;
    std::array<std::byte, sizeof(Report)> before{};
    std::memcpy(before.data(), &report, sizeof(report));
    ForbiddenClock::calls = 0;
    EXPECT_FALSE((run<Fake, ForbiddenClock>(engine, raw, expected, false, report)));
    EXPECT_EQ(std::memcmp(before.data(), &report, sizeof(report)), 0);
    EXPECT_EQ(engine.active, 0U);
    EXPECT_EQ(ForbiddenClock::calls, 0U);
}
TEST(ControlRepeatability, ScheduleClocksAndOwnership) {
    Fake engine;
    Report report{};
    ForbiddenClock::calls = 0;
    ASSERT_TRUE((run<Fake, ForbiddenClock>(engine, raw, raw, false, report)));
    EXPECT_EQ(report.completed, 7U);
    EXPECT_FALSE(report.timed);
    EXPECT_EQ(engine.creates, 7U); EXPECT_EQ(engine.clears, 7U);
    EXPECT_EQ(engine.peak, 1U); EXPECT_EQ(engine.active, 0U);
    EXPECT_EQ(ForbiddenClock::calls, 0U);
    const std::array<std::size_t, 7> slots{0, 1, 1, 0, 0, 1, 2};
    for (std::size_t i = 0; i < records; ++i) {
        const auto& s = report.samples[i];
        EXPECT_EQ(s.slot, slots[i]); EXPECT_EQ(s.encode, i < 6);
        EXPECT_EQ(s.consumed, raw.size()); EXPECT_EQ(s.produced, raw.size());
        EXPECT_EQ(s.seconds(), 0); EXPECT_EQ(s.prepare_calls, i < 6 ? 1U : 0U);
        for (const auto elapsed : s.frames) EXPECT_EQ(elapsed, 0);
    }
}
TEST(ControlRepeatability, EmptyHasNoPreparation) {
    Fake engine; Report report{};
    ASSERT_TRUE((run<Fake, ForbiddenClock>(engine, {}, {}, false, report)));
    for (const auto& s : report.samples) EXPECT_EQ(s.prepare_calls, 0U);
}
struct TickClock {
    static inline std::size_t calls{};
    static std::chrono::steady_clock::time_point now() {
        return std::chrono::steady_clock::time_point{std::chrono::microseconds{++calls}};
    }
};
TEST(ControlRepeatability, EnabledClockAccountsEveryCompleteOperation) {
    Fake engine; Report report{}; TickClock::calls = 0;
    ASSERT_TRUE((run<Fake, TickClock>(engine, raw, raw, true, report)));
    EXPECT_TRUE(report.timed); EXPECT_EQ(report.completed, 7U);
    EXPECT_EQ(TickClock::calls, 42U);
    for (std::size_t i = 0; i < records; ++i) {
        const auto& s = report.samples[i];
        EXPECT_DOUBLE_EQ(s.create, 1e-6); EXPECT_DOUBLE_EQ(s.destroy, 1e-6);
        EXPECT_DOUBLE_EQ(s.seconds(), 3e-6);
        EXPECT_DOUBLE_EQ(s.collect, 0); EXPECT_DOUBLE_EQ(s.drain, 0);
        EXPECT_DOUBLE_EQ(s.prepare, i < 6 ? 1e-6 : 0);
        EXPECT_DOUBLE_EQ(s.decode, i < 6 ? 0 : 1e-6);
        EXPECT_DOUBLE_EQ(s.frames[0], i < 6 ? 1e-6 : 0);
    }
    EXPECT_EQ(engine.peak, 1U); EXPECT_EQ(engine.active, 0U);
}
void enabled_failure(Fake& engine) {
    Report report{}; report.completed = 123; report.samples[0].destroy = 456;
    std::array<std::byte, sizeof(Report)> before{};
    std::memcpy(before.data(), &report, sizeof(report));
    TickClock::calls = 0;
    EXPECT_FALSE((run<Fake, TickClock>(engine, raw, raw, true, report)));
    EXPECT_EQ(std::memcmp(before.data(), &report, sizeof(report)), 0);
    EXPECT_EQ(engine.active, 0U);
}
TEST(ControlRepeatability, EnabledFactoryFailureAtEveryRecordDiscardsTimings) {
    for (std::size_t i = 1; i <= records; ++i) {
        Fake e; e.fail_create = i; enabled_failure(e);
        EXPECT_EQ(TickClock::calls, 6 * (i - 1) + 2);
    }
}
TEST(ControlRepeatability, EnabledProcessFailureAtEveryRecordDiscardsTimings) {
    for (std::size_t i = 1; i <= records; ++i) {
        Fake e; e.fail_process = i; enabled_failure(e);
        EXPECT_EQ(TickClock::calls, 6 * (i - 1) + 4);
    }
}
TEST(ControlRepeatability, EnabledInvalidProgressAndBytesDiscardTimings) {
    Fake stall; stall.stall = true; enabled_failure(stall);
    EXPECT_EQ(stall.calls, 1U);
    Fake wrong; wrong.mismatch = true; enabled_failure(wrong);
}
TEST(ControlRepeatability, EnabledEmptyAccountsHeaderDrainWithoutPreparation) {
    Fake e; Report report{}; TickClock::calls = 0;
    ASSERT_TRUE((run<Fake, TickClock>(e, {}, {}, true, report)));
    EXPECT_EQ(TickClock::calls, 42U);
    for (std::size_t i = 0; i < records; ++i) {
        EXPECT_EQ(report.samples[i].prepare_calls, 0U);
        EXPECT_DOUBLE_EQ(report.samples[i].drain, i < 6 ? 1e-6 : 0);
        EXPECT_DOUBLE_EQ(report.samples[i].seconds(), 3e-6);
    }
}
TEST(ControlRepeatability, FactoryFailureAtEveryRecordDiscardsReport) {
    for (std::size_t i = 1; i <= records; ++i) {
        Fake engine; engine.fail_create = i; unchanged_failure(engine);
        EXPECT_EQ(engine.creates, i); EXPECT_EQ(engine.clears, i - 1);
    }
}
TEST(ControlRepeatability, ProcessFailureAtEveryRecordDiscardsReport) {
    for (std::size_t i = 1; i <= records; ++i) {
        Fake engine; engine.fail_process = i; unchanged_failure(engine);
        EXPECT_EQ(engine.creates, i); EXPECT_EQ(engine.clears, i);
    }
}
TEST(ControlRepeatability, RejectsInvalidConsumption) { Fake e; e.oversized = true; unchanged_failure(e); }
TEST(ControlRepeatability, RejectsStallWithoutLooping) { Fake e; e.stall = true; unchanged_failure(e); EXPECT_EQ(e.calls, 1U); }
TEST(ControlRepeatability, RejectsByteMismatch) { Fake e; e.mismatch = true; unchanged_failure(e); }
TEST(ControlRepeatability, RejectsShortExpected) { Fake e; unchanged_failure(e, std::span{raw}.first(2)); }
TEST(ControlRepeatability, RejectsPrematureEnd) { Fake e; e.premature = true; unchanged_failure(e); }
TEST(ControlRepeatability, MaximumRawProducesSixteenFramesPerEncode) {
    std::vector<std::byte> bytes(raw_limit, std::byte{0x37});
    Fake engine; Report report{};
    ASSERT_TRUE((run<Fake, ForbiddenClock>(engine, bytes, bytes, false, report)));
    for (std::size_t i = 0; i < 6; ++i) EXPECT_EQ(report.samples[i].prepare_calls, 16U);
    EXPECT_EQ(engine.peak, 1U);
}
TEST(ControlRepeatability, BoundsRejectBeforeCreationOrClocks) {
    std::vector<std::byte> bytes(archive_limit + 1);
    Fake engine; Report report{}; report.completed = 123;
    ForbiddenClock::calls = 0;
    EXPECT_FALSE((run<Fake, ForbiddenClock>(engine, std::span{bytes}.first(raw_limit + 1), {}, false, report)));
    EXPECT_FALSE((run<Fake, ForbiddenClock>(engine, {}, bytes, false, report)));
    EXPECT_EQ(engine.creates, 0U); EXPECT_EQ(ForbiddenClock::calls, 0U);
    EXPECT_EQ(report.completed, 123U);
}
TEST(ControlRepeatability, OwnerQueryExactBudgetAndOneBelowInvariant) {
    using namespace marc::frame::internal;
    auto l = limits();
    LzssPositionDistanceWorkspaceRequirements enc{};
    ASSERT_EQ(LzssPositionDistance4mFivePrefixOwnedEncoder::requirements(configuration(3), l, enc), core::ErrorCode::none);
    EXPECT_EQ(enc.aggregate_bytes, 315365389U);
    l.max_internal_buffered_bytes = enc.aggregate_bytes;
    ASSERT_EQ(LzssPositionDistance4mFivePrefixOwnedEncoder::requirements(configuration(3), l, enc), core::ErrorCode::none);
    const auto preserved = enc;
    --l.max_internal_buffered_bytes;
    EXPECT_EQ(LzssPositionDistance4mFivePrefixOwnedEncoder::requirements(configuration(3), l, enc), core::ErrorCode::limit_exceeded);
    EXPECT_EQ(enc.aggregate_bytes, preserved.aggregate_bytes);
    core::ErrorCode error{};
    EXPECT_FALSE(LzssPositionDistance4mFivePrefixOwnedEncoder::create(configuration(3), l, error));
    EXPECT_EQ(error, core::ErrorCode::limit_exceeded);
    LzssPositionDistance4mDecodeWorkspace dec{};
    l = limits();
    ASSERT_EQ(LzssPositionDistance4mOwnedDecoder::requirements(frame, l, dec), core::ErrorCode::none);
    EXPECT_EQ(dec.aggregate_bytes, 130029573U);
    l.max_internal_buffered_bytes = dec.aggregate_bytes;
    ASSERT_EQ(LzssPositionDistance4mOwnedDecoder::requirements(frame, l, dec), core::ErrorCode::none);
    const auto saved = dec;
    --l.max_internal_buffered_bytes;
    EXPECT_EQ(LzssPositionDistance4mOwnedDecoder::requirements(frame, l, dec), core::ErrorCode::limit_exceeded);
    EXPECT_EQ(dec.aggregate_bytes, saved.aggregate_bytes);
    EXPECT_FALSE(LzssPositionDistance4mOwnedDecoder::create(frame, l, error));
    EXPECT_EQ(error, core::ErrorCode::limit_exceeded);
}
TEST(ControlRepeatability, ActualMalformedFramePublishesNoOutput) {
    using namespace marc::frame::internal;
    core::ErrorCode error{};
    auto encoder = LzssPositionDistance4mFivePrefixOwnedEncoder::create(configuration(raw.size()), limits(), error);
    ASSERT_TRUE(encoder);
    std::array<std::byte, 1024> bytes{};
    const auto encoded = encoder->process(raw, bytes, core::flag_value(core::ProcessFlags::end_input));
    ASSERT_EQ(encoded.status, core::StreamStatus::end_of_stream);
    ASSERT_GT(encoded.output_produced, 112U);
    encoder.reset();
    // Truncate the sole frame, retaining the complete header. Decoder must
    // not publish its raw bytes before the complete frame validates.
    auto decoder = LzssPositionDistance4mOwnedDecoder::create(frame, limits(), error);
    ASSERT_TRUE(decoder);
    std::array<std::byte, 32> output{}; output.fill(std::byte{0xa5});
    const auto before = output;
    const auto result = decoder->process(std::span{bytes}.first(encoded.output_produced - 1), output,
        core::flag_value(core::ProcessFlags::end_input));
    EXPECT_EQ(result.status, core::StreamStatus::error);
    EXPECT_EQ(result.output_produced, 0U);
    EXPECT_EQ(output, before);
}
}

#include "core/checked_math.hpp"
#include "frame/lzss_position_distance_8m_frame_decoder.hpp"
#include "frame/lzss_position_distance_8m_frame_encoder.hpp"
#include "lzss_position_distance_8m_large_frame_vectors.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <vector>
namespace {
using namespace marc;
using namespace frame::internal;
namespace d = dictionary::internal;
namespace c = context::internal;
namespace e = entropy::internal;
constexpr std::size_t budget = 512u << 20;
constexpr auto guard = std::byte{0xa5};
std::size_t sum(std::initializer_list<std::size_t> list) {
  std::size_t value{};
  for (auto n : list)
    if (!core::checked_add(value, n, value))
      std::abort();
  return value;
}
std::size_t mul(std::size_t a, std::size_t b) {
  std::size_t out{};
  if (!core::checked_multiply(a, b, out))
    std::abort();
  return out;
}
template <class T> std::size_t capacity_bytes(const std::vector<T> &b) {
  return mul(b.capacity(), sizeof(T));
}
// Test controller objects are concrete retained state, separate from the
// production helper's reservation. Every owned vector capacity stays live and
// charged until actual RAII destruction, never merely until logical last use.
struct Controller {
  std::span<const std::byte> raw;
  std::size_t raw_owner{}, sequence{}, committed{}, written{77};
  core::DecoderLimits limits{};
  TypedContextStreamHeader stream{};
  std::vector<d::LzssTypedToken> tokens, ts;
  std::vector<c::ModeledOperation> ops, os;
  std::vector<std::uint32_t> index;
  std::vector<std::byte> out, frame, payload;
  d::LzssPositionDistance8mParsePlan parse_plan{};
  d::LzssPositionDistance8mParseResult parse{};
  d::LzssPositionDistance8mParseMetadata parsed{};
  c::LzssFieldContextValidationContext counts{};
  c::LzssFieldContextResult mapped{};
  c::LzssPositionDistance8mMapResult map{};
  e::LzssPositionDistance8mRangeEncodePlan range_plan{}, exact_plan{};
  TypedContextFrameLayout layout{};
  LzssPositionDistance8mFrameEncodeResult encoded{};
  std::array<std::byte, sizeof(TypedContextFrameLayout)> layout_before{};
  std::size_t local{}, total{}, retained{}, planning_peak{};
  explicit Controller(const std::vector<std::byte> &input, std::size_t seq,
                      std::size_t prior, std::uint32_t frame_size)
      : raw(input), raw_owner(input.capacity()), sequence(seq),
        committed(prior) {
    limits.max_block_size = input.size();
    limits.max_internal_buffered_bytes = budget;
    stream.frame_size = frame_size;
    stream.original_size = sum({prior, input.size()});
    stream.dictionary = {8388608, 3, 258, 0};
    stream.range_model_total = 32768;
    stream.context_count = 47;
    stream.dictionary_variant = 11;
    stream.context_algorithm = 1;
    stream.context_variant = 12;
  }
  std::size_t owners() const {
    return sum({raw_owner, capacity_bytes(index), capacity_bytes(tokens),
                capacity_bytes(ts), capacity_bytes(ops), capacity_bytes(os),
                capacity_bytes(out), capacity_bytes(frame),
                capacity_bytes(payload)});
  }
  bool admits(std::size_t added, std::size_t helper) const {
    return sum({owners(), sizeof(Controller), added, helper}) <= budget;
  }
  void reconcile(std::size_t helper, std::initializer_list<std::size_t> views) {
    total = sum({owners(), sizeof(Controller), helper});
    local = sum(views);
    local = sum({local, helper});
    ASSERT_LE(local, total);
    retained = total - local;
    ASSERT_EQ(sum({local, retained}), total);
    ASSERT_LE(total, budget);
    planning_peak = std::max(planning_peak, total);
  }
  void plan() {
    ASSERT_TRUE(admits(mul(sum({65536, raw.size()}), 4),
                       d::lzss_position_distance_8m_indexed_working_bytes()));
    index = std::vector<std::uint32_t>(sum({65536, raw.size()}));
    reconcile(d::lzss_position_distance_8m_indexed_working_bytes(),
              {raw.size(), mul(index.size(), 4)});
    parse_plan = d::query_lzss_position_distance_8m_indexed(
        raw, stream.dictionary, limits, 0, 0, index, retained, committed);
    // Count-only zero-capacity probes report the expected shortage AFTER
    // counting. Never reinterpret another error as a valid count plan.
    ASSERT_EQ(parse_plan.error,
              d::LzssPositionDistance8mParseError::output_too_small);
    ASSERT_EQ(parse_plan.aggregate_bytes, total);
    ASSERT_EQ(parse_plan.details.raw_size, raw.size());
    ASSERT_GT(parse_plan.details.token_count, 0u);
    ASSERT_TRUE(admits(
        mul(parse_plan.details.token_count, 2 * sizeof(d::LzssTypedToken)),
        d::lzss_position_distance_8m_indexed_working_bytes()));
    tokens = std::vector<d::LzssTypedToken>(parse_plan.details.token_count);
    ts = std::vector<d::LzssTypedToken>(tokens.size());
    reconcile(d::lzss_position_distance_8m_indexed_working_bytes(),
              {raw.size(), mul(index.size(), 4),
               mul(tokens.size() + ts.size(), sizeof(d::LzssTypedToken))});
    parse = d::tokenize_lzss_position_distance_8m_indexed(
        raw, stream.dictionary, limits, tokens, ts, index, parsed, retained,
        committed);
    ASSERT_EQ(parse.error, d::LzssPositionDistance8mParseError::none);
    ASSERT_EQ(parse.tokens_committed, tokens.size());
    counts.declared_token_count = static_cast<std::uint32_t>(tokens.size());
    counts.declared_raw_size = static_cast<std::uint32_t>(raw.size());
    counts.output_already_committed = committed;
    for (const auto &t : tokens) {
      std::uint32_t events = 2, decisions = 2;
      if (t.kind == d::LzssTypedTokenKind::match) {
        const auto lc = std::bit_width(t.length - 4) - 1u,
                   dc = std::bit_width(t.distance) - 1u;
        events = 3 + (lc != 0) + (dc != 0);
        decisions = 3 + lc + dc;
      }
      ASSERT_TRUE(core::checked_add(counts.declared_event_count, events,
                                    counts.declared_event_count));
      ASSERT_TRUE(core::checked_add(counts.declared_decision_count, decisions,
                                    counts.declared_decision_count));
    }
    ASSERT_TRUE(admits(
        mul(counts.declared_event_count, 2 * sizeof(c::ModeledOperation)),
        c::lzss_position_distance_8m_map_working_bytes()));
    ops = std::vector<c::ModeledOperation>(counts.declared_event_count);
    os = std::vector<c::ModeledOperation>(ops.size());
    reconcile(c::lzss_position_distance_8m_map_working_bytes(),
              {mul(tokens.size(), sizeof(d::LzssTypedToken)),
               mul(ops.size() + os.size(), sizeof(c::ModeledOperation))});
    map = c::map_lzss_position_distance_8m_tokens(
        tokens, stream.dictionary, counts, limits, ops, os, mapped, retained);
    ASSERT_EQ(map.details.error, c::LzssFieldContextError::none);
    ASSERT_EQ(map.operations_committed, ops.size());
    reconcile(e::lzss_position_distance_8m_range_encode_working_bytes(),
              {mul(ops.size(), sizeof(c::ModeledOperation))});
    range_plan = e::query_lzss_position_distance_8m_range_encode(ops, limits, 0,
                                                                 0, retained);
    ASSERT_EQ(range_plan.error,
              e::ContextualDynamicRangeEncodeError::payload_output_too_small);
    ASSERT_EQ(range_plan.aggregate_bytes, total);
    ASSERT_EQ(range_plan.details.operation_count, ops.size());
    ASSERT_EQ(range_plan.details.decision_count,
              counts.declared_decision_count);
    const auto p = range_plan.details.payload_size;
    ASSERT_TRUE(admits(sum({mul(sum({80, p, 16}), 2), p}),
                       lzss_position_distance_8m_frame_encode_working_bytes()));
    out = std::vector<std::byte>(sum({80, p, 16}), guard);
    frame = std::vector<std::byte>(out.size(), guard);
    payload = std::vector<std::byte>(p, guard);
    reconcile(e::lzss_position_distance_8m_range_encode_working_bytes(),
              {mul(ops.size(), sizeof(c::ModeledOperation)), frame.size() - 80,
               payload.size()});
    exact_plan = e::query_lzss_position_distance_8m_range_encode(
        ops, limits, frame.size() - 80, payload.size(), retained);
    ASSERT_EQ(exact_plan.error, e::ContextualDynamicRangeEncodeError::none);
    ASSERT_EQ(exact_plan.aggregate_bytes, total);
    ASSERT_EQ(exact_plan.descriptor.payload_size, p);
  }
  void encode() {
    const LzssPositionDistance8mFrameEncodeWorkspace b{
        tokens, ts, index, ops, os, frame, payload};
    const TypedContextFrameValidationContext context{stream, limits, sequence,
                                                     committed};
    // Include borrowed span/context descriptors as actual outer controls.
    retained =
        sum({sizeof(Controller), sizeof(b), sizeof(context),
             owners() -
                 sum({raw.size(),
                      mul(tokens.size() + ts.size(), sizeof(d::LzssTypedToken)),
                      mul(index.size(), 4),
                      mul(ops.size() + os.size(), sizeof(c::ModeledOperation)),
                      out.size(), frame.size(), payload.size()})});
    total = sum({owners(), sizeof(Controller), sizeof(b), sizeof(context),
                 lzss_position_distance_8m_frame_encode_working_bytes()});
    ASSERT_LE(total, budget);
    layout.serialized_size = 777;
    std::memcpy(layout_before.data(), &layout, sizeof(layout));
    limits.max_internal_buffered_bytes = total - 1;
    encoded = encode_lzss_position_distance_8m_frame(raw, context, b, out,
                                                     layout, written, retained);
    ASSERT_EQ(encoded.error,
              LzssPositionDistance8mFrameEncodeError::limit_exceeded);
    ASSERT_EQ(encoded.aggregate_bytes, total);
    ASSERT_EQ(encoded.bytes_committed, 0u);
    ASSERT_EQ(written, 77u);
    ASSERT_EQ(std::memcmp(layout_before.data(), &layout, sizeof(layout)), 0);
    ASSERT_TRUE(
        std::all_of(out.begin(), out.end(), [](auto v) { return v == guard; }));
    limits.max_internal_buffered_bytes = total;
    encoded = encode_lzss_position_distance_8m_frame(raw, context, b, out,
                                                     layout, written, retained);
    ASSERT_EQ(encoded.error, LzssPositionDistance8mFrameEncodeError::none);
    ASSERT_EQ(encoded.aggregate_bytes, total);
    ASSERT_EQ(written, sum({80, range_plan.details.payload_size}));
    ASSERT_EQ(layout.header.token_count, tokens.size());
    ASSERT_EQ(layout.header.event_count, ops.size());
    ASSERT_EQ(layout.header.decision_count, counts.declared_decision_count);
    ASSERT_TRUE(std::all_of(out.begin() + written, out.end(),
                            [](auto v) { return v == guard; }));
  }
};
std::uint64_t checksum(std::span<const std::byte> b) {
  std::uint64_t h = 14695981039346656037ull;
  for (auto v : b) {
    h ^= std::to_integer<unsigned>(v);
    h *= 1099511628211ull;
  }
  return h;
}
struct DecodeController {
  std::vector<d::LzssTypedToken> tokens, ts;
  std::vector<std::byte> raw, scratch;
  LzssPositionDistance8mFrameDecodePlan plan{};
  LzssPositionDistance8mFrameDecodeResult result{};
  TypedContextFrameLayout layout{};
};
void qualify(const char *name, std::vector<std::byte> raw, unsigned period = 1,
             std::size_t sequence = 0, std::size_t committed = 0,
             std::uint32_t frame_size = 0, bool upper_history = false,
             std::span<const std::uint8_t> mathematical = {}) {
  std::vector<std::byte> encoded;
  TypedContextFrameLayout layout{};
  TypedContextStreamHeader stream{};
  std::size_t encode_total{}, planning_peak{};
  {
    Controller w(raw, sequence, committed,
                 frame_size ? frame_size
                            : static_cast<std::uint32_t>(raw.size()));
    w.plan();
    ASSERT_FALSE(::testing::Test::HasFatalFailure());
    if (!upper_history) {
      std::size_t pos{};
      for (const auto &t : w.tokens) {
        if (pos < period || raw.size() - pos < 5) {
          ASSERT_EQ(t.kind, d::LzssTypedTokenKind::literal);
          ASSERT_EQ(std::byte{t.literal}, raw[pos]);
          ++pos;
        } else {
          ASSERT_EQ(t.kind, d::LzssTypedTokenKind::match);
          ASSERT_EQ(t.distance, period);
          ASSERT_EQ(t.length, std::min<std::size_t>(258, raw.size() - pos));
          pos += t.length;
        }
      }
      ASSERT_EQ(pos, raw.size());
    } else
      ASSERT_TRUE(std::any_of(w.tokens.begin(), w.tokens.end(), [](auto t) {
        return t.kind == d::LzssTypedTokenKind::match && t.distance > 4194304;
      }));
    w.encode();
    ASSERT_FALSE(::testing::Test::HasFatalFailure());
    const auto first = checksum(std::span(w.out).first(w.written));
    // No snapshot buffer: repeat with the same owners/capacities and compare
    // test checksum; cross-compiler receipts compare actual artifact bytes.
    std::fill(w.out.begin(), w.out.end(), guard);
    w.written = 77;
    w.encode();
    ASSERT_FALSE(::testing::Test::HasFatalFailure());
    ASSERT_EQ(checksum(std::span(w.out).first(w.written)), first);
    if (!mathematical.empty()) {
      ASSERT_EQ(mathematical.size(), w.written);
      for (std::size_t i = 0; i < mathematical.size(); ++i)
        ASSERT_EQ(w.out[i], std::byte{mathematical[i]});
    }
    encode_total = w.total;
    planning_peak = w.planning_peak;
    layout = w.layout;
    stream = w.stream;
    encoded = std::move(w.out);
  } // actual destruction releases ALL encoder private owners before decode.
  ::testing::Test::RecordProperty("F", raw.size());
  ::testing::Test::RecordProperty("T", layout.header.token_count);
  ::testing::Test::RecordProperty("E", layout.header.event_count);
  ::testing::Test::RecordProperty("D", layout.header.decision_count);
  ::testing::Test::RecordProperty("P", layout.header.payload_size);
  ::testing::Test::RecordProperty("encode_total", encode_total);
  ::testing::Test::RecordProperty("planning_peak", planning_peak);
  const auto full = layout.serialized_size;
  if (const char *dir = std::getenv("MARC_LARGE_FRAME_ARTIFACT_DIR")) {
    const auto path =
        std::filesystem::path(dir) / (std::string(name) + ".frame");
    ASSERT_FALSE(std::filesystem::exists(path));
    std::ofstream file(path, std::ios::binary);
    ASSERT_TRUE(file);
    file.write(reinterpret_cast<const char *>(encoded.data()),
               static_cast<std::streamsize>(full));
    ASSERT_TRUE(file);
  }
  ASSERT_LE(sum({raw.capacity(), encoded.capacity(),
                 mul(layout.header.token_count, 2 * sizeof(d::LzssTypedToken)),
                 mul(raw.size() + 17, 2), sizeof(DecodeController),
                 sizeof(TypedContextFrameValidationContext),
                 sizeof(core::DecoderLimits), sizeof(stream), sizeof(layout),
                 sizeof(encoded), sizeof(raw),
                 c::lzss_position_distance_8m_token_working_bytes(),
                 sizeof(LzssPositionDistance8mFrameDecodePlan),
                 sizeof(LzssPositionDistance8mFrameDecodeResult)}),
            budget);
  DecodeController dc{};
  dc.tokens = std::vector<d::LzssTypedToken>(layout.header.token_count);
  dc.ts = std::vector<d::LzssTypedToken>(dc.tokens.size());
  dc.raw = std::vector<std::byte>(raw.size() + 17, guard);
  dc.scratch = std::vector<std::byte>(dc.raw.size(), guard);
  core::DecoderLimits limits{};
  limits.max_block_size = 8388608;
  limits.max_internal_buffered_bytes = budget;
  const TypedContextFrameValidationContext context{stream, limits, sequence,
                                                   committed};
  const auto spare =
      sum({raw.capacity(), encoded.capacity() - full,
           capacity_bytes(dc.tokens) -
               mul(dc.tokens.size(), sizeof(d::LzssTypedToken)),
           capacity_bytes(dc.ts) - mul(dc.ts.size(), sizeof(d::LzssTypedToken)),
           dc.raw.capacity() - dc.raw.size(),
           dc.scratch.capacity() - dc.scratch.size(), sizeof(dc),
           sizeof(context), sizeof(limits), sizeof(stream), sizeof(layout),
           sizeof(encoded), sizeof(raw)});
  dc.plan = query_lzss_position_distance_8m_frame_decode(
      std::span(encoded).first(full), context, dc.tokens.size(), dc.ts.size(),
      dc.raw.size(), dc.scratch.size(), spare);
  ASSERT_EQ(dc.plan.error, LzssPositionDistance8mFrameDecodeError::none);
  ASSERT_LE(dc.plan.token_requirements.aggregate_bytes, budget);
  ::testing::Test::RecordProperty("decode_total",
                                  dc.plan.token_requirements.aggregate_bytes);
  dc.result = decode_lzss_position_distance_8m_frame(
      std::span(encoded).first(full), context, dc.tokens, dc.ts, dc.raw,
      dc.scratch, dc.layout, spare);
  ASSERT_EQ(dc.result.error, LzssPositionDistance8mFrameDecodeError::none);
  ASSERT_EQ(dc.result.raw_produced, raw.size());
  ASSERT_EQ(dc.result.bytes_consumed, full);
  ASSERT_TRUE(std::equal(raw.begin(), raw.end(), dc.raw.begin()));
  ASSERT_TRUE(std::all_of(dc.raw.begin() + raw.size(), dc.raw.end(),
                          [](auto v) { return v == guard; }));
}
TEST(PositionDistance8mLargeFrame, Repeated1MiB) {
  qualify("repeat1m", std::vector<std::byte>(1u << 20, std::byte{65}), 1, 0, 0,
          0, false, large_frame_vectors::repeat1m);
}
TEST(PositionDistance8mLargeFrame, Repeated4MiB) {
  qualify("repeat4m", std::vector<std::byte>(4u << 20, std::byte{65}), 1, 0, 0,
          0, false, large_frame_vectors::repeat4m);
}
TEST(PositionDistance8mLargeFrame, Repeated8MiB) {
  qualify("repeat8m", std::vector<std::byte>(8u << 20, std::byte{65}), 1, 0, 0,
          0, false, large_frame_vectors::repeat8m);
}
TEST(PositionDistance8mLargeFrame, BinaryPeriod256At8MiB) {
  std::vector<std::byte> raw(8u << 20);
  for (std::size_t i = 0; i < raw.size(); ++i)
    raw[i] = std::byte(i % 256);
  qualify("binary8m", std::move(raw), 256);
}
TEST(PositionDistance8mLargeFrame, UpperHalfDistanceAt8MiB) {
  std::vector<std::byte> raw(8u << 20);
  for (std::size_t i = 0; i < 258; ++i)
    raw[i] = raw[4194305 + i] = std::byte(1 + i % 255);
  qualify("history8m", std::move(raw), 1, 0, 0, 0, true);
}
TEST(PositionDistance8mLargeFrame, FinalShort8MiBMinusOne) {
  qualify("short8m", std::vector<std::byte>((8u << 20) - 1, std::byte{65}), 1,
          1, 8u << 20, 8u << 20);
}
TEST(PositionDistance8mLargeFrame, FinalShort4MiBPlusOne) {
  qualify("short4m", std::vector<std::byte>((4u << 20) + 1, std::byte{65}), 1,
          0, 0, 8u << 20);
}
TEST(PositionDistance8mLargeFrame,
     UniversalCapacitiesRejectedBeforeAllocation) {
  const std::size_t f = 8u << 20;
  const auto buffers =
      sum({f, mul(24, f), sum({mul(4, f), 262144}), mul(64, f),
           mul(2, sum({mul(18, f), 85})), sum({mul(18, f), 5})});
  ASSERT_EQ(buffers, 1233387695u);
  const auto total =
      sum({buffers, lzss_position_distance_8m_frame_encode_working_bytes(),
           sizeof(Controller)});
  ASSERT_GT(total, budget);
  RecordProperty("prospective_total", total);
  // Numeric planning rejection only. No fabricated large spans, allocation,
  // encoder call, or claim of measured physical ownership for this case.
}
} // namespace

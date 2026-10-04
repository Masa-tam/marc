#include "core/sha256.hpp"
#include "entropy/lzss_position_distance_8m_token_range_encoder.hpp"
#include "frame/lzss_position_distance_8m_serializer.hpp"
#include "frame/lzss_position_distance_8m_stream_decoder.hpp"
#include "marc/marc.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
using namespace marc;
using namespace frame::internal;
using Token = dictionary::internal::LzssTypedToken;
using Kind = dictionary::internal::LzssTypedTokenKind;
using Config = marc_lzss_position_distance_dynamic_range_8m_decoder_config;
using Req = marc_lzss_position_distance_dynamic_range_8m_decoder_requirements;
using Buffers = marc_lzss_position_distance_dynamic_range_8m_decoder_buffers;
constexpr std::size_t F = 8388608, wire_capacity = 1048576, policy = 1073741824,
                      external = 268435456;
constexpr std::byte mark{0xa5}, value{0x41};
unsigned rows{};
void check(bool b, const char *n) {
  if (!b)
    throw std::runtime_error(n);
}
void row(const std::string &name) {
  ++rows;
  std::cout << "{\"case\":\"" << name << "\",\"passed\":true}\n" << std::flush;
}
std::string hash(std::span<const std::byte> bytes) {
  core::Sha256 h;
  std::array<std::byte, 32> d{};
  check(h.update(bytes) && h.finalize(d), "hash");
  std::string s;
  constexpr char digits[] = "0123456789abcdef";
  for (auto b : d) {
    auto n = std::to_integer<unsigned>(b);
    s += digits[n >> 4];
    s += digits[n & 15];
  }
  return s;
}
void store32(std::vector<std::byte> &b, std::size_t p, std::uint32_t n) {
  for (unsigned i = 0; i < 4; ++i)
    b[p + i] = std::byte(n >> (8 * i));
}
std::uint32_t load32(const std::vector<std::byte> &b, std::size_t p) {
  std::uint32_t n{};
  for (unsigned i = 0; i < 4; ++i)
    n |= std::uint32_t(std::to_integer<unsigned>(b[p + i])) << (8 * i);
  return n;
}
core::DecoderLimits limits() {
  core::DecoderLimits l{};
  l.max_total_output_size = 3 * F;
  l.max_frame_size = l.max_block_size = F;
  l.max_compressed_payload_size = 18 * F + 5;
  l.max_internal_buffered_bytes = policy;
  l.max_lz_distance = F;
  l.max_lz_match_length = 258;
  l.max_entropy_table_entries = 2599;
  l.max_range_model_total = 32768;
  l.max_expansion_ratio = 1048576;
  l.expansion_slack = 3 * F;
  l.max_dictionary_serialized_size = l.max_dictionary_entries =
      l.max_huffman_code_length = l.max_blocks_per_frame = 1;
  return l;
}
Config config() {
  Config c{};
  check(marc_lzss_position_distance_dynamic_range_8m_decoder_config_init(&c) ==
            MARC_STATUS_OK,
        "init");
  auto l = limits();
  c.max_total_output_size = l.max_total_output_size;
  c.max_frame_size = c.max_block_size = F;
  c.max_compressed_payload_size = l.max_compressed_payload_size;
  c.max_internal_buffered_bytes = policy;
  c.max_entropy_table_entries = 2599;
  c.max_expansion_ratio = l.max_expansion_ratio;
  c.expansion_slack = l.expansion_slack;
  c.external_retained_bytes = external;
  c.input_capacity_bytes = wire_capacity;
  c.output_capacity_bytes = 2 * F + 32;
  return c;
}
Req query(const Config &c) {
  Req q{};
  q.struct_size = sizeof(q);
  q.abi_version = MARC_ABI_VERSION;
  check(
      marc_lzss_position_distance_dynamic_range_8m_decoder_workspace_requirements(
          &c, &q) == MARC_STATUS_OK,
      "query");
  return q;
}
struct Handle {
  marc_transform *p{};
  ~Handle() { marc_transform_destroy(p); }
};
struct Workspace {
  Req q;
  std::vector<std::byte> serial, raw, scratch;
  std::unique_ptr<std::byte[]> tokens, token_scratch;
  Buffers b{};
  explicit Workspace(const Config &c)
      : q(query(c)), serial(q.serialized_bytes, mark), raw(q.raw_bytes, mark),
        scratch(q.raw_scratch_bytes, mark),
        tokens(new std::byte[q.token_bytes]),
        token_scratch(new std::byte[q.token_scratch_bytes]) {
    check(serial.capacity() == serial.size() && raw.capacity() == raw.size() &&
              scratch.capacity() == scratch.size(),
          "complete workspace capacity accounting");
    b = {sizeof(b),
         MARC_ABI_VERSION,
         0,
         0,
         {reinterpret_cast<uint8_t *>(serial.data()), serial.size()},
         {reinterpret_cast<uint8_t *>(tokens.get()),
          static_cast<std::size_t>(q.token_bytes)},
         {reinterpret_cast<uint8_t *>(token_scratch.get()),
          static_cast<std::size_t>(q.token_scratch_bytes)},
         {reinterpret_cast<uint8_t *>(raw.data()), raw.size()},
         {reinterpret_cast<uint8_t *>(scratch.data()), scratch.size()}};
    check(reinterpret_cast<std::uintptr_t>(tokens.get()) % q.token_alignment ==
              0,
          "storage alignment");
  }
  void reset() {
    std::fill(raw.begin(), raw.end(), mark);
    std::fill(scratch.begin(), scratch.end(), mark);
  }
};
std::vector<std::byte> token_stream(unsigned length) {
  const auto seed = F - length;
  std::vector<Token> tokens(seed, {Kind::literal, 0x41, 0, 0});
  if (length)
    tokens.push_back(
        {Kind::match, 0, static_cast<std::uint32_t>(seed), length});
  auto events = static_cast<std::uint32_t>(2 * seed), decisions = events;
  if (length) {
    auto extra = length < 5 ? 1u : std::bit_width(length - 4) - 1u;
    events += 5;
    decisions += 3 + extra + 22;
  }
  context::internal::LzssFieldContextValidationContext counts{
      static_cast<std::uint32_t>(tokens.size()), events, decisions, F, 0};
  dictionary::internal::LzssParameters p{F, 3, 258, 0};
  auto l = limits();
  std::vector<std::byte> payload(wire_capacity, mark),
      scratch(wire_capacity, mark);
  entropy::internal::ContextualDynamicRangeDescriptor desc{};
  auto r = entropy::internal::encode_lzss_position_distance_8m_token_range(
      tokens, p, counts, l, payload, scratch, desc, external);
  check(r.details.error ==
            entropy::internal::LzssPositionDistance8mTokenRangeError::none,
        "token encode");
  TypedContextStreamHeader header{F, F, p, 32768, 47, 11, 1, 12};
  TypedContextFrameLayout layout{};
  layout.header = {0,
                   0,
                   F,
                   counts.declared_token_count,
                   events,
                   decisions,
                   static_cast<std::uint32_t>(r.bytes_committed),
                   16,
                   0,
                   0};
  layout.descriptor = desc;
  layout.serialized_size = 80 + r.bytes_committed;
  std::vector<std::byte> wire(112 + layout.serialized_size);
  std::size_t written{};
  auto a = serialize_lzss_position_distance_8m_stream_header(
      header, l, std::span(wire).first(112), written, external);
  check(a.error == LzssPositionDistance8mSerializeError::none && written == 112,
        "header serialize");
  written = 0;
  auto b = serialize_lzss_position_distance_8m_frame_prefix(
      layout, {header, l, 0, 0}, std::span(wire).subspan(112, 80), written,
      external);
  check(b.error == LzssPositionDistance8mSerializeError::none && written == 80,
        "prefix serialize");
  std::copy_n(payload.begin(), r.bytes_committed, wire.begin() + 192);
  std::cout << "{\"recipe\":\"token-" << length << "\",\"raw\":" << F
            << ",\"tokens\":" << tokens.size()
            << ",\"distance\":" << (length ? seed : 0)
            << ",\"match\":" << length << ",\"events\":" << events
            << ",\"decisions\":" << decisions
            << ",\"wire_bytes\":" << wire.size() << ",\"wire_sha256\":\""
            << hash(wire) << "\"}\n"
            << std::flush;
  return wire;
}
std::vector<std::byte> public_stream(std::size_t n, bool contrasted = false) {
  std::vector<std::byte> raw(n, value), wire(wire_capacity, mark);
  if (contrasted)
    std::fill(raw.begin() + F, raw.end(), std::byte{0x42});
  marc_lzss_position_distance_dynamic_range_8m_config c{};
  check(marc_lzss_position_distance_dynamic_range_8m_config_init(&c) ==
            MARC_STATUS_OK,
        "encoder init");
  c.original_size = n;
  c.frame_size = F;
  c.max_frame_size = c.max_block_size = F;
  c.max_total_output_size = 3 * F;
  c.max_compressed_payload_size = 18 * F + 5;
  c.max_internal_buffered_bytes = policy;
  c.max_entropy_table_entries = 2599;
  c.max_expansion_ratio = 1048576;
  c.expansion_slack = 3 * F;
  c.external_retained_bytes = external;
  c.input_capacity_bytes = raw.size();
  c.output_capacity_bytes = wire.size();
  Handle h;
  check(marc_lzss_position_distance_dynamic_range_8m_create_encoder(&c, &h.p) ==
            MARC_STATUS_OK,
        "public encoder create");
  auto r = marc_transform_process(
      h.p, {reinterpret_cast<const uint8_t *>(raw.data()), raw.size()},
      {reinterpret_cast<uint8_t *>(wire.data()), wire.size()},
      MARC_PROCESS_END_INPUT);
  check(r.status == MARC_STATUS_END_OF_STREAM && r.input_consumed == n,
        "public encode end");
  wire.resize(r.output_produced);
  std::cout << "{\"recipe\":\"public-" << n
            << "\",\"wire_bytes\":" << wire.size() << ",\"wire_sha256\":\""
            << hash(wire) << "\"}\n"
            << std::flush;
  return wire;
}
struct Result {
  marc_status status{};
  std::uint64_t position{};
  std::size_t consumed{}, made{};
  std::string output, slot;
};
Result decode(Workspace &w, Config c, const std::vector<std::byte> &wire,
              std::size_t expected, bool oracle = false, bool split = false,
              bool two_values = false) {
  w.reset();
  std::vector<std::byte> out(2 * F + 32, mark);
  Handle handle;
  std::unique_ptr<LzssPositionDistance8mStreamDecoder> priv;
  if (oracle) {
    std::uninitialized_value_construct_n(
        reinterpret_cast<Token *>(w.tokens.get()), w.q.token_elements);
    std::uninitialized_value_construct_n(
        reinterpret_cast<Token *>(w.token_scratch.get()), w.q.token_elements);
    auto l = limits();
    l.max_internal_buffered_bytes = c.max_internal_buffered_bytes;
    l.max_frame_size = c.max_frame_size;
    l.max_block_size = c.max_block_size;
    l.max_lz_distance = c.max_lz_distance;
    l.max_lz_match_length = c.max_lz_match_length;
    l.max_total_output_size = c.max_total_output_size;
    l.max_compressed_payload_size = c.max_compressed_payload_size;
    priv = std::make_unique<LzssPositionDistance8mStreamDecoder>(
        l, w.serial,
        std::span(reinterpret_cast<Token *>(w.tokens.get()),
                  w.q.token_elements),
        std::span(reinterpret_cast<Token *>(w.token_scratch.get()),
                  w.q.token_elements),
        w.raw, w.scratch, external);
  } else
    check(marc_lzss_position_distance_dynamic_range_8m_create_decoder(
              &c, &w.b, &handle.p) == MARC_STATUS_OK,
          "public decoder create");
  std::size_t used{}, made{};
  marc_process_result r{};
  for (unsigned iteration = 0; iteration < 20000; ++iteration) {
    const auto n =
        std::min(wire.size() - used, split ? std::size_t{127} : wire.size());
    const auto o =
        split
            ? (iteration == 0 ? std::size_t{0}
                              : std::min(std::size_t{65537}, out.size() - made))
            : out.size() - made;
    const auto flags =
        used + n == wire.size() ? MARC_PROCESS_END_INPUT : MARC_PROCESS_FLUSH;
    if (oracle) {
      auto x = priv->process(std::span(wire).subspan(used, n),
                             std::span(out).subspan(made, o), flags);
      r = {x.input_consumed,
           x.output_produced,
           x.status == core::StreamStatus::error
               ? 99 + static_cast<unsigned>(x.error.code)
               : 1 + static_cast<unsigned>(x.status),
           x.error.byte_position,
           x.error.bit_position,
           {}};
    } else
      r = marc_transform_process(
          handle.p, {reinterpret_cast<const uint8_t *>(wire.data() + used), n},
          {reinterpret_cast<uint8_t *>(out.data() + made), o}, flags);
    check(r.input_consumed <= n && r.output_produced <= o, "bounded counts");
    used += r.input_consumed;
    made += r.output_produced;
    if (r.status >= 100 || r.status == MARC_STATUS_END_OF_STREAM)
      break;
    check(r.status != MARC_STATUS_PROGRESS || r.input_consumed ||
              r.output_produced,
          "progress");
  }
  check(r.status >= 100 || r.status == MARC_STATUS_END_OF_STREAM,
        "bounded completion");
  check(made == expected, "committed prefix length");
  for (std::size_t i = 0; i < made; ++i)
    check(out[i] == (two_values && i >= F ? std::byte{0x42} : value),
          "independent committed prefix bytes");
  check(std::all_of(out.begin() + made, out.end(),
                    [](auto b) { return b == mark; }),
        "whole downstream tail");
  if (expected == 0)
    check(std::all_of(w.raw.begin(), w.raw.end(),
                      [](auto b) { return b == mark; }),
          "first failed raw slot invariant");
  else if (expected >= F)
    check(std::all_of(w.raw.begin(), w.raw.end(),
                      [=](auto b) {
                        return b == (two_values ? std::byte{0x42} : value);
                      }),
          "entire prior maximum raw slot");
  if (!oracle) {
    auto sticky =
        marc_transform_process(handle.p, {nullptr, 0}, {nullptr, 0}, 0);
    check(sticky.status == r.status &&
              sticky.error_byte_position == r.error_byte_position &&
              !sticky.input_consumed && !sticky.output_produced,
          "sticky");
  }
  auto result = Result{r.status,
                       r.error_byte_position,
                       used,
                       made,
                       hash(std::span(out).first(made)),
                       hash(w.raw)};
  priv.reset();
  if (oracle) {
    std::destroy_n(reinterpret_cast<Token *>(w.tokens.get()),
                   w.q.token_elements);
    std::destroy_n(reinterpret_cast<Token *>(w.token_scratch.get()),
                   w.q.token_elements);
  }
  return result;
}
void compare(Workspace &w, Config c, const std::vector<std::byte> &wire,
             std::size_t expected, const std::string &name, bool split = false,
             bool two_values = false) {
  auto a = decode(w, c, wire, expected, false, split, two_values),
       b = decode(w, c, wire, expected, true, split, two_values);
  check(a.status == b.status && a.position == b.position &&
            a.consumed == b.consumed && a.made == b.made &&
            a.output == b.output && a.slot == b.slot,
        "public private equality");
  std::cout << "{\"result\":\"" << name << "\",\"status\":" << a.status
            << ",\"position\":" << a.position << ",\"consumed\":" << a.consumed
            << ",\"made\":" << a.made << ",\"output_sha256\":\"" << a.output
            << "\",\"slot_sha256\":\"" << a.slot << "\"}\n";
  row(name);
}
void run() {
  // Fixture generation precedes the large decoder workspaces. The external
  // reserve covers retained wire owners and test controls in both phases;
  // no live decoder storage is omitted from a generator's resource query.
  constexpr std::array<unsigned, 3> lengths{0, 3, 258};
  constexpr std::array<std::size_t, 4> raw_lengths{F - 1, F, F + 1, 2 * F};
  std::array<std::vector<std::byte>, 3> token_wires;
  std::array<std::vector<std::byte>, 4> public_wires;
  for (std::size_t i = 0; i < lengths.size(); ++i)
    token_wires[i] = token_stream(lengths[i]);
  for (std::size_t i = 0; i < raw_lengths.size(); ++i)
    public_wires[i] = public_stream(raw_lengths[i], i == 3);
  auto c = config();
  Workspace w(c);
  check(w.q.serialized_bytes == 80 + 18 * F + 5 && w.q.token_elements == F &&
            w.q.raw_bytes == F,
        "maximum recommendations");
  c.max_internal_buffered_bytes = w.q.minimum_aggregate_bytes;
  std::cout << "{\"reservation\":{\"frame\":" << F
            << ",\"serialized\":" << w.q.serialized_bytes
            << ",\"token_bytes\":" << w.q.token_bytes
            << ",\"raw\":" << w.q.raw_bytes << ",\"external\":" << external
            << ",\"input\":" << c.input_capacity_bytes
            << ",\"output\":" << c.output_capacity_bytes
            << ",\"exact_budget\":" << c.max_internal_buffered_bytes << "}}\n"
            << std::flush;
  row("maximum-query");
  auto less = c;
  --less.max_internal_buffered_bytes;
  Handle h;
  w.reset();
  std::memset(w.tokens.get(), 0xa5, w.q.token_bytes);
  check(marc_lzss_position_distance_dynamic_range_8m_create_decoder(
            &less, &w.b, &h.p) == MARC_STATUS_LIMIT_EXCEEDED &&
            !h.p,
        "one below budget");
  check(std::all_of(w.raw.begin(), w.raw.end(),
                    [](auto b) { return b == mark; }) &&
            std::all_of(w.tokens.get(), w.tokens.get() + w.q.token_bytes,
                        [](auto b) { return b == mark; }),
        "prelifetime refusal invariant");
  row("maximum-one-below-budget");
  for (std::size_t i = 0; i < lengths.size(); ++i) {
    const auto length = lengths[i];
    const auto &wire = token_wires[i];
    compare(w, c, wire, F, "maximum-token-" + std::to_string(length));
    if (!length)
      compare(w, c, wire, F, "maximum-literals-partial", true);
    if (length == 3) {
      auto bad = wire;
      store32(bad, 64, 1);
      compare(w, c, bad, 0, "maximum-distance-invalid-window");
    }
    if (length == 258) {
      auto bad = wire;
      bad.back() ^= std::byte{1};
      compare(w, c, bad, 0, "maximum-canonical-finish-failure");
    }
  }
  for (std::size_t i = 0; i < 3; ++i) {
    const auto n = raw_lengths[i];
    const auto &wire = public_wires[i];
    compare(w, c, wire, n, "public-boundary-" + std::to_string(n), n == F + 1);
  }
  const auto &wire = public_wires[3];
  const auto second = 112 + 80 + load32(wire, 144);
  check(second + 80 < wire.size(), "second prefix");
  compare(w, c, wire, 2 * F, "two-maximum-frames", false, true);
  auto bad = wire;
  bad[second + 4] ^= std::byte{1};
  compare(w, c, bad, F, "late-prefix-failure");
  bad = wire;
  bad[second + 80] = std::byte{0xff};
  compare(w, c, bad, F, "late-range-failure");
  bad = wire;
  bad.back() ^= std::byte{1};
  compare(w, c, bad, F, "late-canonical-finish-failure");
  bad = wire;
  bad.pop_back();
  compare(w, c, bad, F, "late-truncation-failure");
  bad = wire;
  bad.push_back(std::byte{});
  compare(w, c, bad, 2 * F, "trailing-after-two-maximum", false, true);
  auto restricted = c;
  restricted.max_frame_size = F - 1;
  compare(w, restricted, wire, 0, "one-below-frame-limit");
  restricted = c;
  restricted.max_block_size = F - 1;
  compare(w, restricted, wire, 0, "one-below-block-limit");
  restricted = c;
  restricted.max_lz_distance = F - 1;
  compare(w, restricted, wire, 0, "one-below-window-limit");
  restricted = c;
  restricted.max_lz_match_length = 257;
  compare(w, restricted, wire, 0, "one-below-match-limit");
  restricted = c;
  restricted.max_total_output_size = 2 * F - 1;
  compare(w, restricted, wire, 0, "one-below-total-limit");
  // A lower payload limit reduces numeric recommendations, but actual full
  // buffers are retained and charged against the same exact complete budget.
  restricted = c;
  restricted.max_compressed_payload_size = load32(wire, 144) - 1;
  compare(w, restricted, wire, 0, "one-below-payload-limit");
  std::cout << "PASS " << rows << " maximum public decoder checks\n";
}
} // namespace
int main() {
  try {
    run();
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL " << e.what() << "\n";
    return 1;
  }
}

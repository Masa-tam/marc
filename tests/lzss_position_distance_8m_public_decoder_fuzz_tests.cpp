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
constexpr std::size_t F = 64, wire_capacity = 4096, policy = 8388608,
                      external = 4194304;
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
  l.max_lz_distance = 8388608;
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
  c.output_capacity_bytes = 4160;
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
  c.input_capacity_bytes = std::max(std::size_t{1}, raw.size());
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
struct Rng {
  std::uint32_t state;
  std::uint32_t next() {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
  }
};
struct Observation {
  marc_status status{};
  std::uint64_t position{};
  std::uint8_t bit{};
  std::size_t consumed{};
  std::vector<std::byte> output, slot;
  std::vector<std::uint64_t> trace;
};
bool same_terminal(const Observation &a, const Observation &b) {
  return a.status == b.status && a.position == b.position && a.bit == b.bit &&
         a.consumed == b.consumed && a.output == b.output && a.slot == b.slot;
}
Observation scheduled(Workspace &w, const std::vector<std::byte> &wire,
                      std::uint32_t seed, unsigned schedule, bool oracle) {
  w.reset();
  auto c = config();
  Handle h;
  std::unique_ptr<LzssPositionDistance8mStreamDecoder> priv;
  if (oracle) {
    std::uninitialized_value_construct_n(
        reinterpret_cast<Token *>(w.tokens.get()), w.q.token_elements);
    std::uninitialized_value_construct_n(
        reinterpret_cast<Token *>(w.token_scratch.get()), w.q.token_elements);
    priv = std::make_unique<LzssPositionDistance8mStreamDecoder>(
        limits(), w.serial,
        std::span(reinterpret_cast<Token *>(w.tokens.get()),
                  w.q.token_elements),
        std::span(reinterpret_cast<Token *>(w.token_scratch.get()),
                  w.q.token_elements),
        w.raw, w.scratch, external);
  } else
    check(marc_lzss_position_distance_dynamic_range_8m_create_decoder(
              &c, &w.b, &h.p) == MARC_STATUS_OK,
          "create");
  // A fresh guarded output allocation belongs to the external retained reserve.
  std::vector<std::byte> out(c.output_capacity_bytes, mark);
  Rng rng{seed ? seed : 1};
  Observation result;
  std::size_t used{}, made{}, stop{};
  marc_process_result r{};
  auto call = [&](std::size_t n, std::size_t o, unsigned flags) {
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
          h.p, {reinterpret_cast<const uint8_t *>(wire.data() + used), n},
          {reinterpret_cast<uint8_t *>(out.data() + made), o}, flags);
    check(r.input_consumed <= n && r.output_produced <= o, "bounded counts");
    check(r.status != MARC_STATUS_PROGRESS || r.input_consumed ||
              r.output_produced,
          "nonempty progress");
    used += r.input_consumed;
    made += r.output_produced;
    check(made <= 3 * F, "bounded total output");
    check(std::all_of(out.begin() + made, out.end(),
                      [](auto b) { return b == mark; }),
          "uncommitted output guard");
    result.trace.insert(result.trace.end(),
                        {n, o, flags, r.input_consumed, r.output_produced,
                         r.status, r.error_byte_position,
                         r.error_bit_position});
  };
  for (unsigned iteration = 0; iteration < 4096; ++iteration) {
    if (stop == used && stop < wire.size())
      stop = std::min(wire.size(), stop + (schedule == 1 ? 1 + rng.next() % 47
                                                         : wire.size()));
    auto n = stop - used;
    auto o = schedule == 1 ? (iteration % 5 == 0 ? 0u : 1 + rng.next() % 31)
                           : out.size() - made;
    if (schedule == 2 && iteration < 2)
      o = 0;
    // Empty nonterminal input starvation must not assert EndInput early.
    if (schedule == 1 && iteration % 7 == 0 && stop < wire.size())
      n = 0;
    auto flags = used + n == wire.size() ? MARC_PROCESS_END_INPUT
                 : iteration % 2         ? MARC_PROCESS_FLUSH
                                         : 0u;
    call(n, o, flags);
    if (schedule == 2 && iteration < 2) {
      check(r.status == MARC_STATUS_NEED_OUTPUT && !made && used == wire.size(),
            "complete frame held with zero output");
      check(std::all_of(w.raw.begin(), w.raw.end(),
                        [](auto b) { return b == value; }),
            "verified pending frame raw slot");
    }
    if (r.status >= 100 || r.status == MARC_STATUS_END_OF_STREAM)
      break;
  }
  check(r.status >= 100 || r.status == MARC_STATUS_END_OF_STREAM,
        "bounded completion");
  result.status = r.status;
  result.position = r.error_byte_position;
  result.bit = r.error_bit_position;
  result.consumed = used;
  result.output.assign(out.begin(), out.begin() + made);
  result.slot = w.raw;
  const auto terminal = r;
  call(0, 0, 0);
  check(!r.input_consumed && !r.output_produced &&
            r.status == terminal.status &&
            r.error_byte_position == terminal.error_byte_position &&
            r.error_bit_position == terminal.error_bit_position,
        "sticky terminal");
  priv.reset();
  if (oracle) {
    std::destroy_n(reinterpret_cast<Token *>(w.tokens.get()),
                   w.q.token_elements);
    std::destroy_n(reinterpret_cast<Token *>(w.token_scratch.get()),
                   w.q.token_elements);
  }
  return result;
}
struct Frontier {
  std::vector<std::byte> output, slot;
};
// No streaming drain state in this finite traversal. Range arithmetic is
// shared.
Frontier finite_frontier(std::span<const std::byte> input) {
  auto l = limits();
  Frontier result{{}, std::vector<std::byte>(F, mark)};
  TypedContextStreamHeader header{};
  std::size_t used{};
  if (parse_lzss_position_distance_8m_stream_header(input, l, header, used) !=
      LzssPositionDistance8mPreflightError::none)
    return result;
  std::array<Token, F> tokens{}, scratch_tokens{};
  std::array<std::byte, F> raw{}, scratch_raw{};
  raw.fill(mark);
  std::uint64_t sequence{}, produced{};
  while (produced < header.original_size) {
    TypedContextFrameLayout layout{};
    LzssPositionDistance8mFrameRequirements needed{};
    TypedContextFrameValidationContext context{header, l, sequence, produced};
    auto rest = input.subspan(used);
    if (preflight_lzss_position_distance_8m_frame_prefix(rest, context, layout,
                                                         needed) !=
            LzssPositionDistance8mPreflightError::none ||
        needed.serialized_frame_bytes > rest.size() ||
        needed.serialized_frame_bytes > 80 + 18 * F + 5 ||
        needed.token_count > F || needed.raw_frame_bytes > F)
      break;
    auto r = decode_lzss_position_distance_8m_frame(
        rest.first(needed.serialized_frame_bytes), context, tokens,
        scratch_tokens, raw, scratch_raw, layout, external);
    if (r.error != LzssPositionDistance8mFrameDecodeError::none)
      break;
    result.output.insert(result.output.end(), raw.begin(),
                         raw.begin() + r.raw_produced);
    result.slot.assign(raw.begin(), raw.end());
    used += r.bytes_consumed;
    produced += r.raw_produced;
    ++sequence;
    check(produced <= 3 * F && sequence <= 3 * F, "finite traversal bound");
  }
  return result;
}
void digest(core::Sha256 &h, const Observation &a) {
  check(h.update(std::as_bytes(std::span(a.trace))), "trace digest");
  check(h.update(a.output) && h.update(a.slot), "publication digest");
}
void test(Workspace &w, const std::vector<std::byte> &wire, std::uint32_t seed,
          core::Sha256 &summary, unsigned schedule = 1) {
  auto a = scheduled(w, wire, seed, schedule, false);
  auto b = scheduled(w, wire, seed, schedule, true);
  check(same_terminal(a, b) && a.trace == b.trace,
        "per-call public/private equality");
  auto whole = scheduled(w, wire, seed, 0, false);
  check(same_terminal(a, whole), "split/whole terminal equality");
  auto frontier = finite_frontier(wire);
  check(a.output == frontier.output && a.slot == frontier.slot,
        "finite committed frontier and raw slot");
  digest(summary, a);
  digest(summary, whole);
}
void run(unsigned count) {
  constexpr std::array<std::size_t, 7> lengths{0, 1, 63, 64, 65, 128, 192};
  std::array<std::vector<std::byte>, 7> seeds;
  for (std::size_t i = 0; i < seeds.size(); ++i)
    seeds[i] = public_stream(lengths[i], lengths[i] > F);
  Workspace w(config());
  core::Sha256 summary;
  for (std::size_t i = 0; i < seeds.size(); ++i) {
    test(w, seeds[i], static_cast<std::uint32_t>(i + 1), summary);
    auto a = scheduled(w, seeds[i], 1, 0, false);
    check(a.status == MARC_STATUS_END_OF_STREAM &&
              a.output.size() == lengths[i],
          "known seed length");
    for (std::size_t j = 0; j < a.output.size(); ++j)
      check(a.output[j] == (j < F ? value : std::byte{0x42}),
            "independent raw seed bytes");
  }
  test(w, seeds[3], 1, summary, 2);
  row("complete-validated-frame-zero-output-then-drain");
  auto late = seeds[6];
  auto second = 112 + 80 + load32(late, 144);
  late[second + 4] ^= std::byte{1};
  test(w, late, 7, summary);
  auto prior = scheduled(w, late, 7, 1, false);
  check(prior.status == MARC_STATUS_MALFORMED_STREAM &&
            prior.output == std::vector<std::byte>(F, value) &&
            prior.slot == prior.output,
        "independent late failure preserves first raw slot");
  row("independent-late-failure-publication");
  auto failed = seeds[3];
  failed[192] = std::byte{0xff};
  test(w, failed, 13, summary);
  auto first = scheduled(w, failed, 13, 1, false);
  check(first.status == MARC_STATUS_MALFORMED_STREAM && first.output.empty() &&
            first.slot == std::vector<std::byte>(F, mark),
        "independent first invalid payload keeps entire raw slot");
  auto truncated = seeds[5];
  truncated.pop_back();
  test(w, truncated, 17, summary);
  auto last = scheduled(w, truncated, 17, 1, false);
  check(last.status == MARC_STATUS_MALFORMED_STREAM &&
            last.output == std::vector<std::byte>(F, value) &&
            last.slot == last.output,
        "independent late truncation keeps entire prior raw slot");
  Rng rng{0x14701229};
  unsigned id{};
  std::vector<std::byte> wire;
  std::uint32_t schedule_seed{};
  try {
    for (; id < count; ++id) {
      wire = seeds[rng.next() % seeds.size()];
      auto kind = id % 8;
      auto pos = rng.next() % (wire.size() + 1);
      if (kind == 0 && pos < wire.size())
        wire[pos] ^=
            std::byte{static_cast<unsigned char>(1u << (rng.next() % 8))};
      if (kind == 1)
        wire.resize(pos);
      if (kind == 2)
        wire.insert(wire.begin() + pos,
                    std::byte{static_cast<unsigned char>(rng.next())});
      if (kind == 3 && pos < wire.size())
        wire.erase(wire.begin() + pos);
      if (kind == 4 && pos + 4 <= wire.size())
        store32(wire, pos, rng.next());
      if (kind == 5) {
        wire.resize(rng.next() % 513);
        for (auto &b : wire)
          b = std::byte{static_cast<unsigned char>(rng.next())};
      }
      if (kind == 6)
        wire.push_back(std::byte{static_cast<unsigned char>(rng.next())});
      check(wire.size() <= wire_capacity, "bounded supplied input");
      schedule_seed = rng.next();
      check(summary.update(wire), "case digest");
      test(w, wire, schedule_seed, summary);
      std::cout << "{\"mutation\":" << id << ",\"kind\":" << kind
                << ",\"schedule_seed\":" << schedule_seed
                << ",\"input_bytes\":" << wire.size() << ",\"input_sha256\":\""
                << hash(wire) << "\",\"passed\":true}\n";
    }
  } catch (...) {
    std::cerr << "REPRO case=" << id
              << " initial_seed=0x14701229 schedule_seed=" << schedule_seed
              << " input=";
    constexpr char hex[] = "0123456789abcdef";
    for (auto b : wire) {
      auto n = std::to_integer<unsigned>(b);
      std::cerr << hex[n >> 4] << hex[n & 15];
    }
    std::cerr << '\n';
    throw;
  }
  std::array<std::byte, 32> d{};
  check(summary.finalize(d), "campaign digest");
  std::cout << "{\"campaign_cases\":" << count
            << ",\"initial_seed\":" << 0x14701229u << ",\"digest\":\"";
  constexpr char hex[] = "0123456789abcdef";
  for (auto b : d) {
    auto n = std::to_integer<unsigned>(b);
    std::cout << hex[n >> 4] << hex[n & 15];
  }
  std::cout
      << "\",\"passed\":true}\nPASS bounded public decoder mutation campaign\n";
}
} // namespace
int main(int argc, char **argv) {
  try {
    auto count = argc == 2 ? std::stoul(argv[1]) : 512ul;
    check(argc <= 2 && count > 0 && count <= 16384,
          "bounded campaign argument");
    run(static_cast<unsigned>(count));
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL " << e.what() << '\n';
    return 1;
  }
}

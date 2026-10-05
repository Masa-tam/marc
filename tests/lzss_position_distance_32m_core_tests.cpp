#include "context/lzss_position_distance_32m_compact_tokens.hpp"
#include "context/lzss_position_distance_32m_mapper.hpp"
#include "core/endian.hpp"
#include "entropy/lzss_position_distance_32m_range_decoder.hpp"
#include "entropy/lzss_position_distance_32m_range_encoder.hpp"
#include "frame/lzss_position_distance_32m_serializer.hpp"
#include <algorithm>
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
using namespace marc;
using namespace context::internal;
using namespace dictionary::internal;
using namespace entropy::internal;
void check(bool b,
           const std::source_location at = std::source_location::current()) {
  if (!b)
    throw std::runtime_error("conformance check failed at line " +
                             std::to_string(at.line()));
}
constexpr std::uint32_t F = 33554432;
LzssTypedToken literal(std::uint8_t b = 65) {
  return {LzssTypedTokenKind::literal, b, 0, 0};
}
LzssTypedToken match(std::uint32_t d, std::uint32_t n) {
  return {LzssTypedTokenKind::match, 0, d, n};
}
bool equal(const LzssTypedToken &a, const LzssTypedToken &b) {
  return a.kind == b.kind && a.literal == b.literal &&
         a.distance == b.distance && a.length == b.length;
}
bool equal(const ModeledOperation &a, const ModeledOperation &b) {
  return a.kind == b.kind && a.context_id == b.context_id &&
         a.alphabet_size == b.alphabet_size && a.value == b.value &&
         a.bit_count == b.bit_count;
}
std::vector<LzssTypedToken> recipe(const std::string &name) {
  std::vector<LzssTypedToken> t{literal()};
  if (name == "literal")
    return t;
  if (name == "overlap")
    return {literal(), match(1, 3), match(1, 4), literal(66), match(2, 258)};
  if (name == "lengths") {
    for (std::uint32_t n = 3; n <= 258; ++n)
      t.push_back(match(1, n));
    return t;
  }
  if (name == "rescale")
    return std::vector<LzssTypedToken>(40000, literal());
  check(name == "far3" || name == "far258");
  const auto length = name == "far3" ? 3u : 258u, distance = F - length;
  std::uint32_t remaining = distance - 1;
  while (remaining >= 258) {
    t.push_back(match(1, 258));
    remaining -= 258;
  }
  if (remaining >= 3)
    t.push_back(match(1, remaining));
  else
    while (remaining-- != 0)
      t.push_back(literal());
  t.push_back(match(distance, length));
  return t;
}
std::uint32_t read32(std::istream &s) {
  std::uint32_t v = 0;
  for (unsigned i = 0; i < 4; ++i) {
    const auto c = s.get();
    check(c != EOF);
    v |= static_cast<std::uint32_t>(c) << (8 * i);
  }
  return v;
}
void run(const std::string &name, LzssFieldContextValidationContext c,
         std::vector<std::byte> wire) {
  const auto t = recipe(name);
  check(t.size() == c.declared_token_count);
  core::DecoderLimits l{};
  l.max_frame_size = l.max_block_size = l.max_lz_distance = F;
  const LzssParameters p{F, 3, 258, 0};
  constexpr ModeledOperation guard{ModeledOperationKind::bypass_bits, 77, 88,
                                   99, 11};
  std::vector<ModeledOperation> ops(c.declared_event_count + 3, guard),
      scratch(ops.size(), guard);
  LzssFieldContextResult metadata{};
  const auto mq = query_lzss_position_distance_32m_map(t, p, c, l, ops.size(),
                                                       scratch.size(), F + 123);
  check(mq.error == LzssFieldContextError::none);
  check(mq.aggregate_bytes ==
        t.size() * sizeof(LzssTypedToken) +
            (ops.size() + scratch.size()) * sizeof(ModeledOperation) +
            mq.working_state_bytes + F + 123);
  auto exact = l;
  exact.max_internal_buffered_bytes = mq.aggregate_bytes;
  const auto mr = map_lzss_position_distance_32m_tokens(
      t, p, c, exact, ops, scratch, metadata, F + 123);
  if (mr.details.error != LzssFieldContextError::none)
    std::cerr << name
              << " map error=" << static_cast<unsigned>(mr.details.error)
              << " committed=" << mr.operations_committed << '\n';
  check(mr.details.error == LzssFieldContextError::none &&
        mr.operations_committed == c.declared_event_count);
  check(metadata.raw_size == c.declared_raw_size &&
        metadata.decision_count == c.declared_decision_count);
  for (std::size_t i = c.declared_event_count; i < ops.size(); ++i)
    check(equal(ops[i], guard));
  const auto view = std::span(ops).first(c.declared_event_count);
  const auto byteguard = std::byte{0xcc};
  std::vector<std::byte> output(wire.size() + 3, byteguard),
      temporary(output.size(), byteguard);
  ContextualDynamicRangeDescriptor descriptor{};
  const auto eq = query_lzss_position_distance_32m_range_encode(
      view, l, output.size(), temporary.size(), F + 123);
  check(eq.error == ContextualDynamicRangeEncodeError::none);
  exact = l;
  exact.max_internal_buffered_bytes = eq.aggregate_bytes;
  const auto er = encode_lzss_position_distance_32m_range_operations(
      view, exact, output, temporary, descriptor, F + 123);
  check(er.details.error == ContextualDynamicRangeEncodeError::none &&
        er.bytes_committed == wire.size());
  check(descriptor.context_count == 49 &&
        descriptor.decision_count == c.declared_decision_count &&
        descriptor.payload_size == wire.size());
  {
    using namespace frame::internal;
    TypedContextStreamHeader header{};
    header.frame_size = c.declared_raw_size;
    header.original_size = c.declared_raw_size;
    header.dictionary = p;
    header.range_model_total = 32768;
    header.context_count = 49;
    header.dictionary_variant = 13;
    header.context_algorithm = 1;
    header.context_variant = 14;
    std::vector<std::byte> bytes(115, byteguard);
    std::size_t written = 77;
    check(serialize_lzss_position_distance_32m_stream_header(header, l, bytes,
                                                             written)
              .error == LzssPositionDistance32mSerializeError::none);
    check(written == 112);
    for (std::size_t i = 112; i < bytes.size(); ++i)
      check(bytes[i] == byteguard);
    check(bytes[14] == std::byte{13} && bytes[15] == std::byte{0} &&
          bytes[98] == std::byte{14} && bytes[99] == std::byte{0});
    check(bytes[84] == std::byte{49} && bytes[85] == std::byte{0});
    TypedContextStreamHeader parsed{};
    std::size_t consumed = 77;
    check(parse_lzss_position_distance_32m_stream_header(bytes, l, parsed,
                                                         consumed) ==
              LzssPositionDistance32mPreflightError::none &&
          consumed == 112);
    check(parsed.dictionary_variant == 13 && parsed.context_variant == 14 &&
          parsed.dictionary.window_size == F &&
          parsed.original_size == c.declared_raw_size);
    const auto previous = parsed;
    for (std::size_t n = 0; n < 112; ++n) {
      consumed = 77;
      check(parse_lzss_position_distance_32m_stream_header(
                std::span(bytes).first(n), l, parsed, consumed) !=
            LzssPositionDistance32mPreflightError::none);
      check(consumed == 77 &&
            std::memcmp(&previous, &parsed, sizeof(parsed)) == 0);
    }
    for (const auto pos : {12u, 13u, 14u, 15u, 16u, 17u, 18u, 19u, 96u, 97u,
                           98u, 99u, 52u, 88u, 104u}) {
      auto bad = bytes;
      bad[pos] ^= std::byte{1};
      consumed = 77;
      check(parse_lzss_position_distance_32m_stream_header(bad, l, parsed,
                                                           consumed) !=
            LzssPositionDistance32mPreflightError::none);
      check(consumed == 77 &&
            std::memcmp(&previous, &parsed, sizeof(parsed)) == 0);
    }
    TypedContextFrameLayout layout{};
    layout.header.uncompressed_size = c.declared_raw_size;
    layout.header.token_count = c.declared_token_count;
    layout.header.event_count = c.declared_event_count;
    layout.header.decision_count = c.declared_decision_count;
    layout.header.payload_size = descriptor.payload_size;
    layout.header.descriptor_size = 16;
    layout.descriptor = descriptor;
    layout.serialized_size = 80 + wire.size();
    TypedContextFrameValidationContext fc{header, l, 0, 0};
    std::vector<std::byte> prefix(83, byteguard);
    written = 77;
    check(serialize_lzss_position_distance_32m_frame_prefix(layout, fc, prefix,
                                                            written)
                  .error == LzssPositionDistance32mSerializeError::none &&
          written == 80);
    for (std::size_t i = 80; i < prefix.size(); ++i)
      check(prefix[i] == byteguard);
    TypedContextFrameLayout actual{};
    LzssPositionDistance32mFrameRequirements requirements{};
    check(preflight_lzss_position_distance_32m_frame_prefix(
              std::span(prefix).first(80), fc, actual, requirements) ==
          LzssPositionDistance32mPreflightError::none);
    check(actual.serialized_size == layout.serialized_size &&
          requirements.token_count == t.size() &&
          requirements.raw_frame_bytes == c.declared_raw_size);
    const auto old_layout = actual;
    const auto old_req = requirements;
    for (std::size_t n = 0; n < 80; ++n) {
      check(preflight_lzss_position_distance_32m_frame_prefix(
                std::span(prefix).first(n), fc, actual, requirements) !=
            LzssPositionDistance32mPreflightError::none);
      check(std::memcmp(&old_layout, &actual, sizeof(actual)) == 0 &&
            old_req == requirements);
    }
    for (const auto pos :
         {6u, 8u, 36u, 40u, 44u, 48u, 64u, 68u, 72u, 73u, 74u, 76u}) {
      auto bad = prefix;
      bad[pos] ^= std::byte{1};
      check(preflight_lzss_position_distance_32m_frame_prefix(bad, fc, actual,
                                                              requirements) !=
            LzssPositionDistance32mPreflightError::none);
      check(std::memcmp(&old_layout, &actual, sizeof(actual)) == 0 &&
            old_req == requirements);
    }
    std::fill(bytes.begin(), bytes.end(), byteguard);
    written = 77;
    check(serialize_lzss_position_distance_32m_stream_header(
              header, l, std::span(bytes).first(111), written)
              .error ==
          LzssPositionDistance32mSerializeError::output_too_small);
    check(written == 77);
    for (auto b : bytes)
      check(b == byteguard);
    std::fill(prefix.begin(), prefix.end(), byteguard);
    written = 77;
    check(serialize_lzss_position_distance_32m_frame_prefix(
              layout, fc, std::span(prefix).first(79), written)
              .error ==
          LzssPositionDistance32mSerializeError::output_too_small);
    check(written == 77);
    for (auto b : prefix)
      check(b == byteguard);
  }
  check(std::equal(wire.begin(), wire.end(), output.begin()));
  for (std::size_t i = wire.size(); i < output.size(); ++i)
    check(output[i] == byteguard);
  LzssPositionDistance32mRangeDecoder decoder;
  check(decoder.begin(descriptor, wire, l).error ==
        ContextualDynamicRangeDecodeError::none);
  for (const auto &op : view) {
    ModeledOperation actual{};
    check(decoder.decode_next(actual).error ==
          ContextualDynamicRangeDecodeError::none);
    check(equal(op, actual));
  }
  check(
      decoder.finish(c.declared_event_count, c.declared_decision_count).error ==
      ContextualDynamicRangeDecodeError::none);
  const auto sentinel = match(12345, 54321);
  std::vector<LzssTypedToken> decoded(t.size() + 3, sentinel),
      private_tokens(decoded.size(), sentinel);
  const auto dq = query_lzss_position_distance_32m_tokens(
      descriptor, wire, p, c, l, decoded.size(), private_tokens.size(),
      F + 123);
  check(dq.error == LzssContextualRangeDecodeError::none);
  check(dq.aggregate_bytes ==
        wire.size() +
            (decoded.size() + private_tokens.size()) * sizeof(LzssTypedToken) +
            dq.working_state_bytes + F + 123);
  exact = l;
  exact.max_internal_buffered_bytes = dq.aggregate_bytes;
  const auto dr = decode_lzss_position_distance_32m_tokens(
      descriptor, wire, p, c, exact, decoded, private_tokens, F + 123);
  check(dr.error == LzssContextualRangeDecodeError::none &&
        dr.token_count == t.size() && dr.raw_size == c.declared_raw_size);
  for (std::size_t i = 0; i < t.size(); ++i)
    check(equal(decoded[i], t[i]));
  for (std::size_t i = t.size(); i < decoded.size(); ++i)
    check(equal(decoded[i], sentinel));
  // Compact decoding shares the wire model but has independent publication and
  // memory accounting. Validate every private record against the typed
  // reference.
  const auto compact_capacity = static_cast<std::size_t>(
      std::min(3 * c.declared_raw_size, 9 * c.declared_token_count));
  std::vector<std::byte> compact(compact_capacity + 3, byteguard),
      compact_scratch(compact.size(), byteguard);
  std::size_t committed = 77;
  const auto cq = query_lzss_position_distance_32m_compact_tokens(
      descriptor, wire, p, c, l, compact.size(), compact_scratch.size(),
      F + 123);
  check(cq.error == LzssContextualRangeDecodeError::none);
  check(cq.aggregate_bytes == wire.size() + compact.size() +
                                  compact_scratch.size() +
                                  cq.working_state_bytes + F + 123);
  auto compact_limits = l;
  compact_limits.max_internal_buffered_bytes = cq.aggregate_bytes;
  const auto cr = decode_lzss_position_distance_32m_compact_tokens(
      descriptor, wire, p, c, compact_limits, compact, compact_scratch,
      committed, F + 123);
  check(cr.error == LzssContextualRangeDecodeError::none &&
        cr.token_count == t.size() && cr.raw_size == c.declared_raw_size);
  std::size_t cursor{};
  for (const auto &token : t) {
    if (token.kind == LzssTypedTokenKind::literal) {
      check(compact[cursor] == std::byte{0} &&
            compact[cursor + 1] == static_cast<std::byte>(token.literal));
      cursor += 2;
    } else {
      std::uint32_t distance{}, length{};
      check(compact[cursor] == std::byte{1});
      check(core::load_le(std::span<const std::byte>(compact), cursor + 1,
                          distance));
      check(core::load_le(std::span<const std::byte>(compact), cursor + 5,
                          length));
      check(distance == token.distance && length == token.length);
      cursor += 9;
    }
  }
  check(cursor == committed && committed <= compact_capacity);
  for (std::size_t i = committed; i < compact.size(); ++i)
    check(compact[i] == byteguard);
  auto compact_refuse = [&](const auto &d, std::span<const std::byte> bytes,
                            const auto &counts, const auto &limits,
                            std::size_t oc, std::size_t sc) {
    std::fill(compact.begin(), compact.end(), byteguard);
    committed = 77;
    const auto result = decode_lzss_position_distance_32m_compact_tokens(
        d, bytes, p, counts, limits, std::span(compact).first(oc),
        std::span(compact_scratch).first(sc), committed, F + 123);
    check(result.error != LzssContextualRangeDecodeError::none &&
          committed == 77);
    for (auto b : compact)
      check(b == byteguard);
  };
  --compact_limits.max_internal_buffered_bytes;
  compact_refuse(descriptor, wire, c, compact_limits, compact.size(),
                 compact_scratch.size());
  compact_refuse(descriptor, wire, c, l, compact_capacity - 1,
                 compact_scratch.size());
  compact_refuse(descriptor, wire, c, l, compact.size(), compact_capacity - 1);
  auto compact_wrong = descriptor;
  compact_wrong.context_count = 48;
  compact_refuse(compact_wrong, wire, c, l, compact.size(),
                 compact_scratch.size());
  auto compact_badcounts = c;
  --compact_badcounts.declared_raw_size;
  compact_refuse(descriptor, wire, compact_badcounts, l, compact.size(),
                 compact_scratch.size());
  if (name == "literal" || name == "overlap")
    for (std::size_t n = 0; n < wire.size(); ++n) {
      auto d = descriptor;
      d.payload_size = static_cast<std::uint32_t>(n);
      compact_refuse(d, std::span(wire).first(n), c, l, compact.size(),
                     compact_scratch.size());
    }
  auto compact_bad = wire;
  compact_bad.back() ^= std::byte{1};
  compact_refuse(descriptor, compact_bad, c, l, compact.size(),
                 compact_scratch.size());
  compact_bad = wire;
  compact_bad.push_back(std::byte{0});
  compact_wrong = descriptor;
  ++compact_wrong.payload_size;
  compact_refuse(compact_wrong, compact_bad, c, l, compact.size(),
                 compact_scratch.size());
  committed = 77;
  check(decode_lzss_position_distance_32m_compact_tokens(
            descriptor, wire, p, c, l, compact, compact, committed, F + 123)
            .error == LzssContextualRangeDecodeError::overlapping_buffers);
  check(committed == 77);
  for (auto b : compact)
    check(b == byteguard);
  check(query_lzss_position_distance_32m_compact_tokens(descriptor, wire, p, c,
                                                        l, SIZE_MAX, 1)
            .error == LzssContextualRangeDecodeError::arithmetic_overflow);
  // Independently reconstruct with scalar overlap copies, checking all raw
  // bytes.
  std::vector<std::uint8_t> raw;
  raw.reserve(c.declared_raw_size);
  for (std::size_t i = 0; i < t.size(); ++i) {
    const auto &token = decoded[i];
    if (token.kind == LzssTypedTokenKind::literal)
      raw.push_back(token.literal);
    else {
      check(token.distance <= raw.size());
      for (std::uint32_t j = 0; j < token.length; ++j)
        raw.push_back(raw[raw.size() - token.distance]);
    }
  }
  check(raw.size() == c.declared_raw_size);
  for (std::size_t i = 0; i < raw.size(); ++i)
    check(raw[i] == (name == "overlap" && i >= 8 && (i % 2) == 0 ? 66 : 65));
  auto refuse = [&](const auto &d, std::span<const std::byte> bytes,
                    const auto &counts, const auto &limits, std::size_t oc,
                    std::size_t sc) {
    std::fill(decoded.begin(), decoded.end(), sentinel);
    const auto r = decode_lzss_position_distance_32m_tokens(
        d, bytes, p, counts, limits, std::span(decoded).first(oc),
        std::span(private_tokens).first(sc), F + 123);
    check(r.error != LzssContextualRangeDecodeError::none);
    for (const auto &token : decoded)
      check(equal(token, sentinel));
  };
  --exact.max_internal_buffered_bytes;
  refuse(descriptor, wire, c, exact, decoded.size(), private_tokens.size());
  refuse(descriptor, wire, c, l, t.size() - 1, private_tokens.size());
  refuse(descriptor, wire, c, l, decoded.size(), t.size() - 1);
  auto wrong = descriptor;
  wrong.context_count = 47;
  refuse(wrong, wire, c, l, decoded.size(), private_tokens.size());
  auto badcounts = c;
  --badcounts.declared_raw_size;
  refuse(descriptor, wire, badcounts, l, decoded.size(), private_tokens.size());
  // Every truncation of the two short recipes; final canonical-byte mutation
  // for all.
  if (name == "literal" || name == "overlap")
    for (std::size_t n = 0; n < wire.size(); ++n) {
      auto d = descriptor;
      d.payload_size = static_cast<std::uint32_t>(n);
      refuse(d, std::span(wire).first(n), c, l, decoded.size(),
             private_tokens.size());
    }
  auto altered = wire;
  altered.back() ^= std::byte{1};
  refuse(descriptor, altered, c, l, decoded.size(), private_tokens.size());
  altered = wire;
  altered.push_back(std::byte{0});
  wrong = descriptor;
  ++wrong.payload_size;
  refuse(wrong, altered, c, l, decoded.size(), private_tokens.size());
  std::fill(decoded.begin(), decoded.end(), sentinel);
  const auto alias = decode_lzss_position_distance_32m_tokens(
      descriptor, wire, p, c, l, decoded, decoded, F + 123);
  check(alias.error == LzssContextualRangeDecodeError::overlapping_buffers);
  for (const auto &token : decoded)
    check(equal(token, sentinel));
  // Map and range failures also preserve their complete output and metadata.
  std::fill(ops.begin(), ops.end(), guard);
  const auto before = metadata;
  exact = l;
  exact.max_internal_buffered_bytes = mq.aggregate_bytes - 1;
  const auto mf = map_lzss_position_distance_32m_tokens(
      t, p, c, exact, ops, scratch, metadata, F + 123);
  check(mf.details.error != LzssFieldContextError::none &&
        mf.operations_committed == 0);
  check(std::memcmp(&before, &metadata, sizeof(metadata)) == 0);
  for (const auto &op : ops)
    check(equal(op, guard));
  // Use the separately retained scratch operations because caller ops now hold
  // guards.
  std::fill(output.begin(), output.end(), byteguard);
  const auto prior = descriptor;
  exact = l;
  exact.max_internal_buffered_bytes = eq.aggregate_bytes - 1;
  const auto ef = encode_lzss_position_distance_32m_range_operations(
      std::span(scratch).first(c.declared_event_count), exact, output,
      temporary, descriptor, F + 123);
  check(ef.details.error != ContextualDynamicRangeEncodeError::none &&
        ef.bytes_committed == 0);
  check(prior.decision_count == descriptor.decision_count &&
        prior.payload_size == descriptor.payload_size &&
        prior.context_count == descriptor.context_count);
  for (const auto b : output)
    check(b == byteguard);
  std::cout << name << " T=" << t.size() << " E=" << c.declared_event_count
            << " N=" << c.declared_decision_count << " R=" << raw.size()
            << " P=" << wire.size() << " map=" << mq.aggregate_bytes
            << " encode=" << eq.aggregate_bytes
            << " decode=" << dq.aggregate_bytes << '\n';
}
} // namespace
int main(int argc, char **argv) {
  try {
    check(argc == 2);
    std::ifstream input(argv[1], std::ios::binary);
    check(input.good());
    char magic[8]{};
    input.read(magic, 8);
    check(std::memcmp(magic, "M32V0001", 8) == 0);
    check(read32(input) == 6);
    for (unsigned i = 0; i < 6; ++i) {
      const auto n = read32(input);
      check(n < 32);
      std::string name(n, '\0');
      input.read(name.data(), n);
      LzssFieldContextValidationContext c{};
      c.declared_token_count = read32(input);
      c.declared_event_count = read32(input);
      c.declared_decision_count = read32(input);
      c.declared_raw_size = read32(input);
      const auto size = read32(input);
      check(size < 1048576);
      std::vector<std::byte> wire(size);
      input.read(reinterpret_cast<char *>(wire.data()), size);
      check(input.good());
      run(name, c, std::move(wire));
    }
    check(input.get() == EOF);
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}

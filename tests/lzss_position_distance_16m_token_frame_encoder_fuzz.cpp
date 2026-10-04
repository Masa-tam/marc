#include "frame/lzss_position_distance_16m_frame_decoder.hpp"
#include "frame/lzss_position_distance_16m_frame_encoder.hpp"
#include "frame/lzss_position_distance_16m_token_frame_encoder.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <vector>
namespace {
using namespace marc::frame::internal;
using Token = marc::dictionary::internal::LzssTypedToken;
using Op = marc::context::internal::ModeledOperation;
void check(bool v) {
  if (!v)
    std::abort();
}
} // namespace
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data,
                                      std::size_t size) {
  if (size < 9 || size > 136)
    return 0;
  const auto n = size - 8;
  std::array<std::byte, 128> raw{};
  for (std::size_t i = 0; i < n; ++i)
    raw[i] = std::byte{data[i + 8]};
  std::array<Token, 128> tokens{}, ts{};
  std::vector<std::uint32_t> index(1048576 + n);
  std::array<std::byte, 2400> frame{}, payload{}, first{}, second{};
  first.fill(std::byte{0xa5});
  second = first;
  auto capacity = [&](unsigned flag, unsigned control, std::size_t full) {
    return (data[0] & flag) ? std::size_t(data[control]) % full : full;
  };
  const LzssPositionDistance16mTokenFrameWorkspace b{
      std::span(tokens).first(capacity(1, 1, tokens.size())),
      std::span(ts).first(capacity(1, 2, ts.size())), index,
      std::span(frame).first(capacity(4, 5, frame.size())),
      std::span(payload).first(capacity(4, 6, payload.size()))};
  marc::core::DecoderLimits limits{};
  limits.max_block_size = 128;
  limits.max_internal_buffered_bytes = 128u << 20;
  if (data[0] & 64)
    limits.max_internal_buffered_bytes = std::size_t(data[7]) * 4096;
  TypedContextStreamHeader s{};
  s.frame_size = static_cast<std::uint32_t>(n);
  s.original_size = n;
  s.dictionary = {16777216, 3, 258, 0};
  s.range_model_total = 32768;
  s.context_count = 48;
  s.dictionary_variant = 12;
  s.context_algorithm = 1;
  s.context_variant = 13;
  if (data[0] & 32)
    s.dictionary.min_match_length = data[1];
  const TypedContextFrameValidationContext c{s, limits,
                                             (data[0] & 16) ? data[2] : 0u, 0};
  const auto retained = (data[0] & 128) ? std::size_t(data[3]) * 1024 : 0;
  const auto outcap = capacity(8, 7, first.size());
  const auto charged = n + b.tokens.size() * sizeof(Token) +
                       b.token_scratch.size() * sizeof(Token) +
                       index.size() * sizeof(std::uint32_t) + b.frame.size() +
                       b.payload_scratch.size() + outcap;
  const auto harness =
      sizeof(raw) + sizeof(tokens) + sizeof(ts) + sizeof(frame) +
      sizeof(payload) + sizeof(first) + sizeof(second) +
      index.capacity() * sizeof(std::uint32_t) + sizeof(index) + sizeof(b) +
      sizeof(limits) + sizeof(s) + sizeof(c) +
      2 * sizeof(TypedContextFrameLayout) +
      sizeof(LzssPositionDistance16mTokenFramePlan) +
      2 * sizeof(LzssPositionDistance16mTokenFrameResult) +
      256 * sizeof(std::byte) + 256 * sizeof(Token) + 32 * sizeof(std::size_t);
  const auto honest_retained = harness - charged + retained;
  TypedContextFrameLayout a{}, z{};
  a.serialized_size = z.serialized_size = 777;
  std::array<std::byte, sizeof(a)> before{};
  std::memcpy(before.data(), &a, sizeof(a));
  std::size_t written = 77, other = 77;
  auto r = encode_lzss_position_distance_16m_token_frame(
      std::span(raw).first(n), c, b, std::span(first).first(outcap), a, written,
      honest_retained);
  auto t = encode_lzss_position_distance_16m_token_frame(
      std::span(raw).first(n), c, b, std::span(second).first(outcap), z, other,
      honest_retained);
  check(r.error == t.error && r.bytes_committed == t.bytes_committed &&
        r.aggregate_bytes == t.aggregate_bytes && first == second &&
        written == other);
  if (r.error != LzssPositionDistance16mTokenFrameError::none) {
    check(r.bytes_committed == 0 && written == 77 &&
          std::memcmp(before.data(), &a, sizeof(a)) == 0);
    check(std::all_of(first.begin(), first.end(),
                      [](auto v) { return v == std::byte{0xa5}; }));
  } else {
    check(written == r.bytes_committed && a.serialized_size == written);
    check(std::all_of(first.begin() + written, first.end(),
                      [](auto v) { return v == std::byte{0xa5}; }));
    std::array<Token, 128> rt{}, rs{};
    std::array<Op, 256> ro{}, ros{};
    std::vector<std::uint32_t> ri(1048576 + n);
    std::array<std::byte, 2400> rf{}, rp{}, reference{};
    auto reference_limits = limits;
    reference_limits.max_internal_buffered_bytes = 128u << 20;
    const TypedContextFrameValidationContext rc{s, reference_limits,
                                                c.expected_sequence, 0};
    const LzssPositionDistance16mFrameEncodeWorkspace rb{rt,  rs, ri, ro,
                                                        ros, rf, rp};
    TypedContextFrameLayout rl{};
    std::size_t rw = 0;
    const auto rr = encode_lzss_position_distance_16m_frame(
        std::span(raw).first(n), rc, rb, reference, rl, rw,
        harness + sizeof(rt) + sizeof(rs) + sizeof(ro) + sizeof(ros) +
            sizeof(ri) + sizeof(rf) + sizeof(rp) + sizeof(reference) +
            sizeof(reference_limits) + sizeof(rc) + sizeof(rb) + sizeof(rl) +
            sizeof(rw));
    check(rr.error == LzssPositionDistance16mFrameEncodeError::none &&
          rw == written &&
          std::equal(reference.begin(), reference.begin() + rw, first.begin()));
    std::array<Token, 128> dt{}, ds{};
    std::array<std::byte, 128> decoded{}, scratch{};
    TypedContextFrameLayout parsed{};
    // The reference index and both reference/private owners are still live.
    // Use the explicit sufficient consumer policy, with conservative retained
    // controls, rather than hiding them behind the mutated encoder budget.
    const auto decode_retained =
        harness + ri.capacity() * sizeof(std::uint32_t) + sizeof(rt) +
        sizeof(rs) + sizeof(ro) + sizeof(ros) + sizeof(rf) + sizeof(rp) +
        sizeof(reference) + 4096;
    auto d = decode_lzss_position_distance_16m_frame(
        std::span(first).first(written), rc, dt, ds, decoded, scratch, parsed,
        decode_retained);
    check(d.error == LzssPositionDistance16mFrameDecodeError::none &&
          d.raw_produced == n &&
          std::equal(raw.begin(), raw.begin() + n, decoded.begin()));
  }
  return 0;
}

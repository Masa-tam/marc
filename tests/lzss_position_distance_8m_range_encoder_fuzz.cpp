#include "entropy/lzss_position_distance_8m_range_decoder.hpp"
#include "entropy/lzss_position_distance_8m_range_encoder.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
namespace {
using namespace marc;
using namespace entropy::internal;
using namespace context::internal;
constexpr auto guard = std::byte{0xa5};
void require(bool b,
             std::source_location location = std::source_location::current()) {
  if (!b) {
    std::fprintf(stderr, "Invariant at line %u\n", location.line());
    std::abort();
  }
}
std::uint32_t le(const std::uint8_t *p, std::size_t n) {
  std::uint32_t v = 0;
  for (std::size_t i = 0; i < n; ++i)
    v |= std::uint32_t{p[i]} << (8 * i);
  return v;
}
struct Buffers {
  std::array<std::byte, 514> out, scratch;
  ContextualDynamicRangeDescriptor descriptor{777, 888, 999};
  Buffers() {
    out.fill(guard);
    scratch.fill(guard);
  }
};
} // namespace
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data,
                                      std::size_t size) {
  if (size < 4 || size > 2564 || (size - 4) % 10)
    return 0;
  const std::size_t output_capacity = le(data, 2) % 513,
                    scratch_capacity = le(data + 2, 2) % 513;
  std::array<ModeledOperation, 256> storage{};
  const auto count = (size - 4) / 10;
  for (std::size_t i = 0; i < count; ++i) {
    const auto p = data + 4 + 10 * i;
    storage[i] = {static_cast<ModeledOperationKind>(p[0]),
                  static_cast<std::uint16_t>(le(p + 1, 2)),
                  static_cast<std::uint16_t>(le(p + 3, 2)), le(p + 5, 4), p[9]};
  }
  const auto ops = std::span(storage).first(count);
  core::DecoderLimits limits{};
  limits.max_block_size = 512;
  limits.max_compressed_payload_size = 512;
  limits.max_internal_buffered_bytes = 65536;
  const auto q = query_lzss_position_distance_8m_range_encode(
      ops, limits, output_capacity, scratch_capacity);
  Buffers a, b;
  std::array<std::byte, sizeof(a.descriptor)> original{};
  std::memcpy(original.data(), &a.descriptor, original.size());
  const auto encode = [&](Buffers &w) {
    return encode_lzss_position_distance_8m_range_operations(
        ops, limits, std::span(w.out).first(output_capacity),
        std::span(w.scratch).first(scratch_capacity), w.descriptor);
  };
  const auto ra = encode(a), rb = encode(b);
  require(ra.details.error == q.error && ra.details.error == rb.details.error &&
          ra.bytes_committed == rb.bytes_committed);
  require(ra.details.operation_count == rb.details.operation_count &&
          ra.details.operation_index == rb.details.operation_index &&
          ra.details.decision_count == rb.details.decision_count &&
          ra.details.payload_size == rb.details.payload_size);
  require(a.out == b.out && a.scratch == b.scratch &&
          a.descriptor.decision_count == b.descriptor.decision_count &&
          a.descriptor.payload_size == b.descriptor.payload_size &&
          a.descriptor.context_count == b.descriptor.context_count);
  require(std::ranges::all_of(std::span(a.out).subspan(output_capacity),
                              [](auto x) { return x == guard; }));
  require(std::ranges::all_of(std::span(a.scratch).subspan(scratch_capacity),
                              [](auto x) { return x == guard; }));
  if (ra.details.error != ContextualDynamicRangeEncodeError::none) {
    require(!ra.bytes_committed &&
            std::ranges::all_of(a.out, [](auto x) { return x == guard; }));
    require(std::memcmp(&a.descriptor, original.data(), original.size()) == 0);
    return 0;
  }
  require(ra.bytes_committed == q.details.payload_size &&
          ra.bytes_committed <= output_capacity &&
          ra.bytes_committed <= scratch_capacity);
  require(std::ranges::all_of(std::span(a.out).subspan(ra.bytes_committed),
                              [](auto x) { return x == guard; }));
  require(std::ranges::all_of(std::span(a.scratch).subspan(ra.bytes_committed),
                              [](auto x) { return x == guard; }));
  LzssPositionDistance8mRangeDecoder decoder;
  require(decoder
              .begin(a.descriptor, std::span(a.out).first(ra.bytes_committed),
                     limits)
              .error == ContextualDynamicRangeDecodeError::none);
  for (const auto expected : ops) {
    ModeledOperation op{};
    require(decoder.decode_next(op).error ==
            ContextualDynamicRangeDecodeError::none);
    require(op.kind == expected.kind && op.context_id == expected.context_id &&
            op.alphabet_size == expected.alphabet_size &&
            op.value == expected.value && op.bit_count == expected.bit_count);
  }
  require(decoder
              .finish(static_cast<std::uint32_t>(count),
                      a.descriptor.decision_count)
              .error == ContextualDynamicRangeDecodeError::none);
  return 0;
}

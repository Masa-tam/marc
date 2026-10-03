#include "entropy/lzss_position_distance_8m_range_decoder.hpp"
#include <cstdlib>
namespace {
using namespace marc::entropy::internal;
bool same(const ContextualDynamicRangeDecodeResult &a,
          const ContextualDynamicRangeDecodeResult &b) {
  return a.error == b.error && a.event_count == b.event_count &&
         a.decision_count == b.decision_count &&
         a.payload_consumed == b.payload_consumed;
}
bool same(const marc::context::internal::ModeledOperation &a,
          const marc::context::internal::ModeledOperation &b) {
  return a.kind == b.kind && a.context_id == b.context_id &&
         a.alphabet_size == b.alphabet_size && a.value == b.value &&
         a.bit_count == b.bit_count;
}
void check(bool condition) {
  if (!condition)
    std::abort();
}
} // namespace
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data,
                                      std::size_t size) {
  if (size < 2 || size > 2050)
    return 0;
  const auto decisions = static_cast<std::uint32_t>(data[0]) |
                         (static_cast<std::uint32_t>(data[1]) << 8);
  if (decisions > 4096)
    return 0;
  auto limits = marc::core::DecoderLimits{};
  limits.max_block_size = 2048;
  limits.max_compressed_payload_size = 2048;
  limits.max_internal_buffered_bytes = 65536;
  limits.max_entropy_table_entries = 2599;
  const ContextualDynamicRangeDescriptor descriptor{
      decisions, static_cast<std::uint32_t>(size - 2), 47};
  const auto payload = std::as_bytes(std::span(data + 2, size - 2));
  LzssPositionDistance8mRangeDecoder a, b;
  auto ra = a.begin(descriptor, payload, limits),
       rb = b.begin(descriptor, payload, limits);
  check(same(ra, rb));
  std::uint32_t events{};
  while (ra.error == ContextualDynamicRangeDecodeError::none &&
         ra.decision_count < decisions) {
    auto oa = marc::context::internal::ModeledOperation{
        marc::context::internal::ModeledOperationKind::symbol, 99, 777, 999, 0};
    auto ob = oa;
    const auto before = oa;
    ra = a.decode_next(oa);
    rb = b.decode_next(ob);
    check(same(ra, rb) && same(oa, ob));
    if (ra.error != ContextualDynamicRangeDecodeError::none) {
      check(same(oa, before));
      const auto previous = ra;
      ra = a.decode_next(oa);
      check(same(ra, previous) && same(oa, before));
      break;
    }
    ++events;
    check(events <= decisions && ra.payload_consumed <= payload.size());
  }
  if (ra.error == ContextualDynamicRangeDecodeError::none) {
    ra = a.finish(events, decisions);
    rb = b.finish(events, decisions);
    check(same(ra, rb));
  }
  return 0;
}

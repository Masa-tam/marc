#include "frame/lzss_position_distance_32m_serializer.hpp"
#include "frame/lzss_position_distance_32m_stream_decoder.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <limits>
#include <new>

namespace {
bool forbid_new{};
std::size_t allocations{};
void check(bool b) {
  if (!b)
    std::abort();
}
void *allocate(std::size_t size) {
  if (forbid_new)
    std::abort();
  ++allocations;
  if (auto *p = std::malloc(size ? size : 1))
    return p;
  throw std::bad_alloc();
}
using namespace marc;
using namespace frame::internal;
using Decoder = LzssPositionDistance32mStreamDecoder;
using Code = core::ErrorCode;
using Status = core::StreamStatus;
constexpr auto end = core::flag_value(core::ProcessFlags::end_input);
constexpr auto guard = std::byte{0xa5};
core::DecoderLimits limits() {
  core::DecoderLimits l{};
  l.max_frame_size = l.max_lz_distance = 33554432;
  l.max_block_size = 1;
  return l;
}
struct Workspace {
  std::array<std::array<std::byte, 256>, 5> data{};
  std::array<std::span<std::byte>, 5> views() {
    return {std::span(data[0]).first(128), std::span(data[1]).first(16),
            std::span(data[2]).first(16), std::span(data[3]).first(8),
            std::span(data[4]).first(8)};
  }
};
std::array<std::byte, 284> stream() {
  auto l = limits();
  TypedContextStreamHeader header{};
  header.frame_size = 1;
  header.original_size = 2;
  header.dictionary = {33554432, 3, 258, 0};
  header.range_model_total = 32768;
  header.context_count = 49;
  header.dictionary_variant = 13;
  header.context_variant = 14;
  std::array<std::byte, 284> bytes{};
  std::size_t written{};
  check(serialize_lzss_position_distance_32m_stream_header(header, l, bytes,
                                                           written)
            .error == LzssPositionDistance32mSerializeError::none);
  // Generated independently by the mathematical Range oracle, TVG-1358.
  const std::array<std::array<std::byte, 6>, 2> payloads{
      {{std::byte{0}, std::byte{0x20}, std::byte{0x7f}, std::byte{0xff},
        std::byte{0xbf}, std::byte{0}},
       {std::byte{0}, std::byte{0x20}, std::byte{0xff}, std::byte{0xff},
        std::byte{0xbe}, std::byte{0}}}};
  for (std::size_t i = 0; i < 2; ++i) {
    TypedContextFrameLayout layout{};
    layout.header.sequence = i;
    layout.header.uncompressed_size = layout.header.token_count = 1;
    layout.header.event_count = layout.header.decision_count = 2;
    layout.header.payload_size = 6;
    layout.header.descriptor_size = 16;
    layout.descriptor = {2, 6, 49};
    layout.serialized_size = 86;
    check(serialize_lzss_position_distance_32m_frame_prefix(
              layout, {header, l, i, i}, std::span(bytes).subspan(112 + 86 * i),
              written)
              .error == LzssPositionDistance32mSerializeError::none);
    std::copy(payloads[i].begin(), payloads[i].end(),
              bytes.begin() + 192 + 86 * i);
  }
  return bytes;
}
void refused(Decoder &d, std::span<const std::byte> input,
             std::span<std::byte> output, Code code) {
  const auto r = d.process(input, output, end);
  check(r.status == Status::error && r.error.code == code &&
        !r.input_consumed && !r.output_produced);
  const auto again = d.process({}, {}, 0);
  check(again.status == Status::error && again.error.code == code &&
        !again.input_consumed && !again.output_produced);
}
} // namespace
void *operator new(std::size_t n) { return allocate(n); }
void *operator new[](std::size_t n) { return allocate(n); }
void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }
void operator delete[](void *p, std::size_t) noexcept { std::free(p); }

int main() {
  const auto bytes = stream();
  auto l = limits();
  const auto before = allocations;
  forbid_new = true;
  for (std::size_t a = 0; a < 5; ++a)
    for (std::size_t b = a + 1; b < 5; ++b) {
      Workspace w;
      auto v = w.views();
      v[b] = std::span(w.data[a]).first(v[b].size());
      Decoder d(l, v[0], v[1], v[2], v[3], v[4]);
      std::array<std::byte, 4> out{};
      refused(d, bytes, out, Code::invalid_argument);
    }
  for (std::size_t region = 0; region < 6; ++region)
    for (bool input_alias : {false, true}) {
      Workspace w;
      auto v = w.views();
      Decoder d(l, v[0], v[1], v[2], v[3], v[4]);
      // A byte view solely for negative alias admission; never serialized.
      const auto owner =
          std::span(reinterpret_cast<std::byte *>(&d), sizeof(d));
      const auto alias = region < 5 ? v[region].last(1) : owner.first(1);
      std::array<std::byte, 8> out{};
      refused(d, input_alias ? std::span<const std::byte>(alias) : bytes,
              input_alias ? std::span(out) : alias, Code::invalid_argument);
    }
  {
    Workspace w;
    auto v = w.views();
    Decoder d(l, v[0], v[1], v[2], v[3], v[4]);
    std::array<std::byte, 8> io{};
    refused(d, io, io, Code::invalid_argument);
  }
  for (std::size_t index = 0; index < 6; ++index) {
    std::array<std::size_t, 6> capacities{128, 16, 16, 8, 8, 17};
    capacities[index] = std::numeric_limits<std::size_t>::max();
    check(query_lzss_position_distance_32m_stream_workspace(
              l, capacities[0], capacities[1], capacities[2], capacities[3],
              capacities[4], capacities[5])
              .error == Code::limit_exceeded);
  }
  for (bool corrupt : {false, true}) {
    Workspace w;
    auto v = w.views();
    const auto q = query_lzss_position_distance_32m_stream_workspace(
        l, v[0].size(), v[1].size(), v[2].size(), v[3].size(), v[4].size(), 17);
    check(q.error == Code::none);
    auto exact = l;
    exact.max_internal_buffered_bytes = q.aggregate_bytes;
    Decoder d(exact, v[0], v[1], v[2], v[3], v[4], 17);
    auto wire = bytes;
    if (corrupt)
      wire.back() ^= std::byte{1};
    std::array<std::byte, 8> output;
    output.fill(guard);
    const auto r = d.process(wire, output, end);
    check(r.status == (corrupt ? Status::error : Status::end_of_stream));
    check(r.output_produced == (corrupt ? 1u : 2u) &&
          output[0] == std::byte{65});
    check(w.data[3][0] == (corrupt ? std::byte{65} : std::byte{66}));
    for (std::size_t i = r.output_produced; i < output.size(); ++i)
      check(output[i] == guard);
    --exact.max_internal_buffered_bytes;
    Decoder below(exact, v[0], v[1], v[2], v[3], v[4], 17);
    refused(below, wire, output, Code::limit_exceeded);
  }
  forbid_new = false;
  check(allocations == before);
}

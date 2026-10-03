#include "frame/lzss_position_distance_8m_stream_decoder.hpp"
#include "frame/lzss_position_distance_8m_stream_encoder.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <vector>
namespace {
using namespace marc;
using namespace frame::internal;
using Token = dictionary::internal::LzssTypedToken;
using Op = context::internal::ModeledOperation;
using S = core::StreamStatus;
constexpr auto end = core::flag_value(core::ProcessFlags::end_input);
constexpr auto guard = std::byte{0xa5};
void check(bool b) {
  if (!b)
    std::abort();
}
struct Buffers {
  std::vector<std::byte> raw, publication, frame, payload;
  std::vector<Token> tokens, ts;
  std::vector<Op> ops, os;
  std::vector<std::uint32_t> index;
  Buffers(std::size_t f, std::size_t t, std::size_t o, std::size_t storage)
      : raw(f, guard), publication(storage, guard), frame(storage, guard),
        payload(storage, guard), tokens(t), ts(t), ops(o), os(o),
        index(65536 + f) {}
  auto workspace() {
    return LzssPositionDistance8mFrameEncodeWorkspace{
        tokens, ts, index, ops, os, frame, payload};
  }
};
struct Result {
  std::vector<std::byte> wire;
  core::ProcessResult last{};
};
Result run(Buffers &b, TypedContextStreamHeader header,
           core::DecoderLimits limits, std::span<const std::byte> raw,
           std::size_t chunk, std::size_t cap, bool late) {
  LzssPositionDistance8mStreamEncoder e(header, limits, b.raw, b.publication,
                                        b.workspace());
  Result result;
  std::size_t offset = 0, stop = std::min(chunk, raw.size()), steps = 0;
  do {
    std::array<std::byte, 1027> output;
    output.fill(guard);
    auto in = raw.subspan(offset, stop - offset);
    const auto flags =
        core::flag_value(core::ProcessFlags::flush) |
        ((!late && stop == raw.size()) || offset == raw.size() ? end : 0);
    auto r = e.process(in, std::span(output).first(cap), flags);
    check(r.input_consumed <= in.size() && r.output_produced <= cap);
    check(r.status != S::progress || r.input_consumed || r.output_produced);
    check(std::ranges::all_of(std::span(output).subspan(r.output_produced),
                              [](auto x) { return x == guard; }));
    offset += r.input_consumed;
    result.wire.insert(result.wire.end(), output.begin(),
                       output.begin() + r.output_produced);
    result.last = r;
    if (offset == stop && offset < raw.size())
      stop = std::min(raw.size(), offset + chunk);
    check(++steps < 20000);
  } while (result.last.status != S::end_of_stream &&
           result.last.status != S::error);
  std::array<std::byte, 3> out{guard, guard, guard};
  auto sticky = e.process(out, out, 0xffffffffu);
  check(sticky.status == result.last.status && sticky.input_consumed == 0 &&
        sticky.output_produced == 0);
  check(sticky.error.code == result.last.error.code &&
        sticky.error.byte_position == result.last.error.byte_position);
  check(std::ranges::all_of(out, [](auto x) { return x == guard; }));
  return result;
}
void consumer(const Result &encoded, std::span<const std::byte> raw,
              core::DecoderLimits limits) {
  std::array<std::byte, 512> serial{};
  std::array<std::byte, 16> frame{}, scratch{};
  std::array<Token, 16> tokens{}, ts{};
  LzssPositionDistance8mStreamDecoder d(limits, serial, tokens, ts, frame,
                                        scratch);
  std::size_t offset = 0, made = 0, steps = 0;
  core::ProcessResult r;
  do {
    std::array<std::byte, 9> output;
    output.fill(guard);
    const auto in =
        std::span(encoded.wire)
            .subspan(offset,
                     std::min<std::size_t>(7, encoded.wire.size() - offset));
    r = d.process(in, std::span(output).first(3),
                  offset + in.size() == encoded.wire.size() ? end : 0);
    check(r.input_consumed <= in.size() && r.output_produced <= 3);
    check(r.status != S::progress || r.input_consumed || r.output_produced);
    check(std::ranges::all_of(std::span(output).subspan(r.output_produced),
                              [](auto x) { return x == guard; }));
    offset += r.input_consumed;
    check(made + r.output_produced <= raw.size());
    for (std::size_t i = 0; i < r.output_produced; ++i)
      check(output[i] == raw[made + i]);
    made += r.output_produced;
    check(++steps < 20000);
  } while (r.status != S::end_of_stream && r.status != S::error);
  if (encoded.last.status == S::end_of_stream)
    check(r.status == S::end_of_stream && made == raw.size());
}
} // namespace
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data,
                                      std::size_t size) {
  if (size < 8)
    return 0;
  const auto f = 1u + data[0] % 16, t = data[1] % (f + 1),
             o = data[2] % (2 * f + 1);
  const auto storage =
      data[3] & 1 ? 18 * f + 85 : static_cast<unsigned>(data[4]);
  const auto n = std::min<std::size_t>(size - 8, 64);
  std::vector<std::byte> raw(n);
  for (std::size_t i = 0; i < n; ++i)
    raw[i] = static_cast<std::byte>(data[8 + i]);
  core::DecoderLimits limits{};
  limits.max_block_size = 65536;
  TypedContextStreamHeader h{};
  h.frame_size = f;
  h.original_size = n;
  if (data[5] & 1)
    h.original_size += data[6] % 5;
  if ((data[5] & 2) && h.original_size)
    --h.original_size;
  h.dictionary = {8388608, 3, 258, 0};
  h.range_model_total = 32768;
  h.context_count = 47;
  h.dictionary_variant = 11;
  h.context_algorithm = 1;
  h.context_variant = 12;
  Buffers a(f, t, o, storage), b(f, t, o, storage);
  auto full = run(a, h, limits, raw, std::max<std::size_t>(1, n), 1024, false);
  auto split =
      run(b, h, limits, raw, 1 + data[6] % 17, 1 + data[7] % 32, data[5] & 4);
  check(full.last.status == split.last.status &&
        full.last.error.code == split.last.error.code);
  check(full.wire == split.wire);
  consumer(full, raw, limits);
  consumer(split, raw, limits);
  return 0;
}

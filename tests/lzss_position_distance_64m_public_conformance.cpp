#include "marc/marc.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>
using Bytes = std::vector<std::uint8_t>;
constexpr std::size_t F = 67108864;
void check(bool b, std::source_location at = std::source_location::current()) {
  if (!b)
    throw std::runtime_error("public conformance line " +
                             std::to_string(at.line()));
}
Bytes read(std::ifstream &f, std::size_t n) {
  check(n <= 1u << 20);
  Bytes b(n);
  f.read(reinterpret_cast<char *>(b.data()), n);
  check(bool(f));
  return b;
}
std::uint32_t number(std::ifstream &f) {
  auto b = read(f, 4);
  std::uint32_t n = 0;
  for (unsigned i = 0; i < 4; ++i)
    n |= std::uint32_t(b[i]) << (8 * i);
  return n;
}
struct Handle {
  marc_transform *p{};
  ~Handle() { marc_transform_destroy(p); }
};
template <class C>
void limits(C &c, std::size_t extra, std::size_t input, std::size_t output) {
  c.max_total_output_size = 2 * F + 1;
  c.max_frame_size = c.max_block_size = F;
  c.max_compressed_payload_size = 2 * F;
  c.max_internal_buffered_bytes = UINT64_C(1) << 30;
  c.max_entropy_table_entries = 2632;
  c.max_expansion_ratio = 1024;
  c.expansion_slack = F;
  c.external_retained_bytes = extra + 4096;
  c.input_capacity_bytes = input;
  c.output_capacity_bytes = output;
}
void decode(const Bytes &wire, const Bytes &raw, bool bad) {
  marc_lzss_position_distance_dynamic_range_64m_decoder_config c{};
  check(marc_lzss_position_distance_dynamic_range_64m_decoder_config_init(&c) ==
        MARC_STATUS_OK);
  Bytes output(65536);
  limits(c, raw.capacity() + wire.capacity() + output.capacity(), 4093,
         output.size());
  marc_lzss_position_distance_dynamic_range_64m_decoder_requirements q{
      sizeof(q), MARC_ABI_VERSION};
  check(
      marc_lzss_position_distance_dynamic_range_64m_decoder_workspace_requirements(
          &c, &q) == MARC_STATUS_OK);
  check(q.token_alignment == 1 && q.record_capacity_bytes == 3 * F &&
        q.token_bytes == 3 * F && q.token_scratch_bytes == 3 * F);
  Bytes serial(q.serialized_bytes), tokens(q.token_bytes),
      scratch(q.token_scratch_bytes), private_raw(q.raw_bytes),
      raw_scratch(q.raw_scratch_bytes);
  marc_lzss_position_distance_dynamic_range_64m_decoder_buffers b{
      sizeof(b),
      MARC_ABI_VERSION,
      0,
      0,
      {serial.data(), serial.size()},
      {tokens.data(), tokens.size()},
      {scratch.data(), scratch.size()},
      {private_raw.data(), private_raw.size()},
      {raw_scratch.data(), raw_scratch.size()}};
  c.max_internal_buffered_bytes = q.minimum_aggregate_bytes;
  Handle decoder;
  check(marc_lzss_position_distance_dynamic_range_64m_create_decoder(
            &c, &b, &decoder.p) == MARC_STATUS_OK);
  std::memset(&c, 0, sizeof(c));
  std::memset(&b, 0, sizeof(b));
  std::size_t pos = 0, produced = 0;
  bool ended = false;
  for (unsigned call = 0; call < 1000000; ++call) {
    auto n = std::min(std::size_t{4093}, wire.size() - pos);
    output.assign(output.size(), 0xa5);
    auto r = marc_transform_process(
        decoder.p, {wire.data() + pos, n}, {output.data(), output.size()},
        pos + n == wire.size() ? MARC_PROCESS_END_INPUT : 0);
    check(r.input_consumed <= n && r.output_produced <= output.size() &&
          r.output_produced <= raw.size() - produced);
    pos += r.input_consumed;
    check(std::equal(output.begin(), output.begin() + r.output_produced,
                     raw.begin() + produced));
    check(std::all_of(output.begin() + r.output_produced, output.end(),
                      [](auto v) { return v == 0xa5; }));
    produced += r.output_produced;
    if (r.status >= 100 || r.status == MARC_STATUS_END_OF_STREAM) {
      check(bad ? r.status >= 100 && produced == F
                : r.status == MARC_STATUS_END_OF_STREAM &&
                      produced == raw.size() && pos == wire.size());
      auto again = marc_transform_process(decoder.p, {nullptr, 0},
                                          {output.data(), output.size()}, 0);
      check(again.status == r.status && !again.output_produced);
      ended = true;
      break;
    }
    check(r.input_consumed || r.output_produced ||
          r.status == MARC_STATUS_NEED_INPUT ||
          r.status == MARC_STATUS_NEED_OUTPUT);
  }
  check(ended);
}
int main(int argc, char **argv) try {
  check(argc == 2);
  std::ifstream file(argv[1], std::ios::binary);
  auto magic = read(file, 8);
  check(std::memcmp(magic.data(), "M64O0001", 8) == 0);
  check(number(file) == 7);
  for (unsigned k = 0; k < 7; ++k) {
    const auto n = number(file), length = number(file), extent = number(file);
    check(n >= F - 1 && n <= 2 * F + 1 &&
          (length == 0 || length == 5 || length == 258));
    auto expected = read(file, extent);
    Bytes raw(n, length ? 0 : 65);
    if (length)
      for (std::size_t i = 0; i < length; ++i)
        raw[i] = raw[n - length + i] = std::uint8_t(1 + i % 255);
    Bytes encoded;
    encoded.reserve(extent);
    Bytes output(n == F + 1 ? 1 : 65536);
    {
      marc_lzss_position_distance_dynamic_range_64m_config c{};
      check(marc_lzss_position_distance_dynamic_range_64m_config_init(&c) ==
            MARC_STATUS_OK);
      c.original_size = n;
      c.frame_size = F;
      limits(c,
             raw.capacity() + expected.capacity() + encoded.capacity() +
                 output.capacity(),
             65521, output.size());
      marc_lzss_position_distance_dynamic_range_64m_resources q{};
      check(marc_lzss_position_distance_dynamic_range_64m_resource_requirements(
                &c, &q) == MARC_STATUS_OK &&
            q.initial_raw_bytes == std::min<std::size_t>(n, F) &&
            q.initial_index_entries == 1048576 + q.initial_raw_bytes);
      Handle encoder;
      check(marc_lzss_position_distance_dynamic_range_64m_create_encoder(
                &c, &encoder.p) == MARC_STATUS_OK);
      std::memset(&c, 0, sizeof(c));
      std::size_t pos = 0;
      bool ended = false;
      for (unsigned call = 0; call < 1000000; ++call) {
        const auto chunk = std::min(std::size_t{65521}, raw.size() - pos);
        auto r = marc_transform_process(
            encoder.p, {raw.data() + pos, chunk},
            {output.data(), output.size()},
            pos + chunk == raw.size() ? MARC_PROCESS_END_INPUT : 0);
        check(r.input_consumed <= chunk && r.output_produced <= output.size() &&
              r.status < 100);
        pos += r.input_consumed;
        check(encoded.size() + r.output_produced <= encoded.capacity());
        encoded.insert(encoded.end(), output.begin(),
                       output.begin() + r.output_produced);
        if (r.status == MARC_STATUS_END_OF_STREAM) {
          check(pos == raw.size());
          ended = true;
          break;
        }
        check(r.input_consumed || r.output_produced ||
              r.status == MARC_STATUS_NEED_INPUT ||
              r.status == MARC_STATUS_NEED_OUTPUT);
      }
      check(ended && encoded == expected);
    }
    Bytes{}.swap(expected);
    Bytes{}.swap(output);
    decode(encoded, raw, false);
    if (n == 2 * F && !length) {
      encoded.back() ^= 1;
      decode(encoded, raw, true);
    }
    std::cout << "public " << n << " far " << length << " PASS\n";
  }
  check(file.peek() == std::char_traits<char>::eof());
  return 0;
} catch (const std::exception &e) {
  std::cerr << e.what() << '\n';
  return 1;
}

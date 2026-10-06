#include "marc/marc.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <vector>
namespace {
void check(bool b) {
  if (!b)
    std::abort();
}
struct Handle {
  marc_transform *p{};
  ~Handle() { marc_transform_destroy(p); }
};
} // namespace
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data,
                                      std::size_t size) {
  if (size < 4 || size > 68)
    return 0;
  auto n = size - 4;
  marc_lzss_position_distance_dynamic_range_64m_config c{};
  check(marc_lzss_position_distance_dynamic_range_64m_config_init(&c) ==
        MARC_STATUS_OK);
  c.original_size = n;
  c.frame_size = 64;
  c.max_frame_size = c.max_block_size = 64;
  c.max_total_output_size = 1048576;
  c.max_compressed_payload_size = 65536;
  c.max_internal_buffered_bytes =
      (data[0] & 1) ? std::uint64_t(data[1]) * 65536 : 67108864;
  c.max_entropy_table_entries = 2632;
  c.max_expansion_ratio = 1048576;
  c.expansion_slack = 1048576;
  c.input_capacity_bytes = 64;
  c.output_capacity_bytes = 128;
  c.external_retained_bytes = 65536;
  if (data[0] & 2)
    c.reserved = 1;
  if (data[0] & 4)
    c.abi_version++;
  Handle enc;
  auto status =
      marc_lzss_position_distance_dynamic_range_64m_create_encoder(&c, &enc.p);
  if (status != MARC_STATUS_OK) {
    check(!enc.p && status >= 100);
    return 0;
  }
  std::array<std::uint8_t, 4096> wire{};
  std::array<std::uint8_t, 128> buffer{};
  std::size_t pos{}, written{};
  bool ended = false;
  for (unsigned call = 0; call < 10000; ++call) {
    auto chunk = std::min<std::size_t>(1 + data[2] % 64, n - pos);
    auto cap = std::size_t(1 + data[3] % 128);
    buffer.fill(0xa5);
    auto r = marc_transform_process(
        enc.p, {data + 4 + pos, chunk}, {buffer.data(), cap},
        pos + chunk == n ? MARC_PROCESS_END_INPUT : 0);
    check(r.input_consumed <= chunk && r.output_produced <= cap &&
          written + r.output_produced <= wire.size());
    check(std::all_of(buffer.begin() + r.output_produced, buffer.end(),
                      [](auto b) { return b == 0xa5; }));
    std::copy_n(buffer.begin(), r.output_produced, wire.begin() + written);
    written += r.output_produced;
    pos += r.input_consumed;
    if (r.status >= 100) {
      check(written <= 112);
      return 0;
    }
    if (r.status == MARC_STATUS_END_OF_STREAM) {
      check(pos == n);
      ended = true;
      break;
    }
    check(r.input_consumed || r.output_produced ||
          r.status == MARC_STATUS_NEED_INPUT ||
          r.status == MARC_STATUS_NEED_OUTPUT);
  }
  check(ended);
  marc_transform_destroy(enc.p);
  enc.p = nullptr;
  bool corrupt = (data[0] & 8) && n;
  if (corrupt)
    wire[written - 1] ^= 1;
  marc_lzss_position_distance_dynamic_range_64m_decoder_config d{};
  check(marc_lzss_position_distance_dynamic_range_64m_decoder_config_init(&d) ==
        MARC_STATUS_OK);
  d.max_total_output_size = c.max_total_output_size;
  d.max_frame_size = d.max_block_size = 64;
  d.max_compressed_payload_size = c.max_compressed_payload_size;
  d.max_internal_buffered_bytes = 67108864;
  d.max_entropy_table_entries = 2632;
  d.max_expansion_ratio = c.max_expansion_ratio;
  d.expansion_slack = c.expansion_slack;
  d.input_capacity_bytes = wire.size();
  d.output_capacity_bytes = buffer.size();
  d.external_retained_bytes = 65536;
  marc_lzss_position_distance_dynamic_range_64m_decoder_requirements q{
      sizeof(q), MARC_ABI_VERSION};
  check(
      marc_lzss_position_distance_dynamic_range_64m_decoder_workspace_requirements(
          &d, &q) == MARC_STATUS_OK);
  std::vector<std::uint8_t> serial(q.serialized_bytes), tokens(q.token_bytes),
      scratch(q.token_scratch_bytes), raw(q.raw_bytes),
      raw_scratch(q.raw_scratch_bytes);
  marc_lzss_position_distance_dynamic_range_64m_decoder_buffers b{
      sizeof(b),
      MARC_ABI_VERSION,
      0,
      0,
      {serial.data(), serial.size()},
      {tokens.data(), tokens.size()},
      {scratch.data(), scratch.size()},
      {raw.data(), raw.size()},
      {raw_scratch.data(), raw_scratch.size()}};
  if (data[0] & 16)
    b.token_scratch = b.tokens;
  Handle dec;
  status = marc_lzss_position_distance_dynamic_range_64m_create_decoder(&d, &b,
                                                                        &dec.p);
  if (status != MARC_STATUS_OK) {
    check((data[0] & 16) && status == MARC_STATUS_INVALID_ARGUMENT && !dec.p);
    return 0;
  }
  pos = 0;
  std::size_t made{};
  ended = false;
  for (unsigned call = 0; call < 10000; ++call) {
    auto chunk = std::min<std::size_t>(1 + data[2] % 64, written - pos);
    auto cap = std::size_t(1 + data[3] % 128);
    buffer.fill(0xa5);
    auto r = marc_transform_process(
        dec.p, {wire.data() + pos, chunk}, {buffer.data(), cap},
        pos + chunk == written ? MARC_PROCESS_END_INPUT : 0);
    check(r.input_consumed <= chunk && r.output_produced <= cap &&
          made + r.output_produced <= n);
    check(std::all_of(buffer.begin() + r.output_produced, buffer.end(),
                      [](auto x) { return x == 0xa5; }));
    check(std::equal(buffer.begin(), buffer.begin() + r.output_produced,
                     data + 4 + made));
    pos += r.input_consumed;
    made += r.output_produced;
    if (r.status >= 100) {
      check(corrupt && !made);
      auto sticky =
          marc_transform_process(dec.p, {nullptr, 0}, {buffer.data(), cap}, 0);
      check(sticky.status == r.status && !sticky.output_produced);
      return 0;
    }
    if (r.status == MARC_STATUS_END_OF_STREAM) {
      check(!corrupt && made == n && pos == written);
      ended = true;
      break;
    }
    check(r.input_consumed || r.output_produced ||
          r.status == MARC_STATUS_NEED_INPUT ||
          r.status == MARC_STATUS_NEED_OUTPUT);
  }
  check(ended);
  return 0;
}

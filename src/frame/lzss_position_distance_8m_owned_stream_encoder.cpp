#include "frame/lzss_position_distance_8m_owned_stream_encoder.hpp"
#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"
#include <algorithm>
#include <cstring>
#include <exception>
#include <limits>
#include <new>
namespace marc::frame::internal {
namespace {
using Code = core::ErrorCode;
using Encoder = LzssPositionDistance8mOwnedStreamEncoder;
constexpr std::size_t callback_working = 128;
constexpr std::size_t call_controls =
    sizeof(TypedContextFrameValidationContext) +
    sizeof(TypedContextStreamHeader) + sizeof(core::DecoderLimits) +
    sizeof(LzssPositionDistance8mOwnerResult) +
    sizeof(LzssPositionDistance8mSerializeResult) +
    sizeof(core::ProcessResult) +
    sizeof(LzssPositionDistance8mAllocatorControls) +
    16 * (sizeof(void *) + sizeof(std::size_t)) +
    4 * sizeof(std::span<std::byte>) + 32 * sizeof(std::size_t);
bool add(std::size_t &n, std::size_t x) noexcept {
  return core::checked_add(n, x, n);
}
} // namespace
LzssPositionDistance8mAllocatorControls
LzssPositionDistance8mExactStreamAllocator::controls() const noexcept {
  return {this, sizeof(*this), callback_working};
}
LzssPositionDistance8mOwnedTokens
LzssPositionDistance8mExactStreamAllocator::tokens(std::size_t n) noexcept {
  return blocks_.tokens(n);
}
LzssPositionDistance8mOwnedBytes
LzssPositionDistance8mExactStreamAllocator::bytes(std::size_t n) noexcept {
  return blocks_.bytes(n);
}
LzssPositionDistance8mOwnedIndex
LzssPositionDistance8mExactStreamAllocator::indices(std::size_t n) noexcept {
  if (!n || n > std::numeric_limits<std::size_t>::max() / sizeof(std::uint32_t))
    return {};
  try {
    auto p = new (std::nothrow) std::uint32_t[n]{};
    return {p, p ? n : 0};
  } catch (const std::bad_alloc &) {
    return {};
  }
}
void LzssPositionDistance8mExactStreamAllocator::release(
    LzssPositionDistance8mOwnedTokens &b) noexcept {
  blocks_.release(b);
}
void LzssPositionDistance8mExactStreamAllocator::release(
    LzssPositionDistance8mOwnedBytes &b) noexcept {
  blocks_.release(b);
}
void LzssPositionDistance8mExactStreamAllocator::release(
    LzssPositionDistance8mOwnedIndex &b) noexcept {
  delete[] b.data;
  b = {};
}
LzssPositionDistance8mAllocatorControls
Encoder::Bridge::controls() const noexcept {
  auto c = source.controls();
  std::size_t working = callback_working;
  if (!add(working, c.bytes) || !add(working, c.working_bytes))
    working = std::numeric_limits<std::size_t>::max();
  return {this, sizeof(*this), working};
}
void Encoder::Bridge::remember(const void *p, std::size_t n) noexcept {
  if (!p)
    return;
  for (auto &r : regions)
    if (!r.data) {
      r = {p, n};
      return;
    }
  // Contract admits at most five current and five candidate blocks.
  std::terminate();
}
void Encoder::Bridge::forget(const void *p) noexcept {
  if (!p)
    return;
  for (auto &r : regions)
    if (r.data == p) {
      r = {};
      return;
    }
  std::terminate();
}
LzssPositionDistance8mOwnedTokens
Encoder::Bridge::tokens(std::size_t n) noexcept {
  auto b = source.tokens(n);
  std::size_t bytes{};
  if (!core::checked_multiply(
          b.capacity, sizeof(dictionary::internal::LzssTypedToken), bytes))
    bytes = std::numeric_limits<std::size_t>::max();
  remember(b.data, bytes);
  return b;
}
LzssPositionDistance8mOwnedBytes
Encoder::Bridge::bytes(std::size_t n) noexcept {
  auto b = source.bytes(n);
  remember(b.data, b.capacity);
  return b;
}
void Encoder::Bridge::release(LzssPositionDistance8mOwnedTokens &b) noexcept {
  auto p = b.data;
  source.release(b);
  forget(p);
}
void Encoder::Bridge::release(LzssPositionDistance8mOwnedBytes &b) noexcept {
  auto p = b.data;
  source.release(b);
  forget(p);
}
bool Encoder::Bridge::bytes_live(std::size_t &out) const noexcept {
  out = 0;
  for (const auto &r : regions)
    if (!add(out, r.bytes))
      return false;
  return true;
}
std::size_t Encoder::working_bytes() noexcept {
  return sizeof(Encoder) + call_controls + callback_working +
         std::max(LzssPositionDistance8mStorageOwner::working_bytes() -
                      sizeof(LzssPositionDistance8mStorageOwner),
                  lzss_position_distance_8m_stream_serialize_working_bytes());
}
bool Encoder::budget(std::size_t in, std::size_t out,
                     std::size_t &total) const noexcept {
  if (!bridge_.bytes_live(total))
    return false;
  for (auto n : {working_bytes(), allocator_controls_.bytes,
                 allocator_controls_.working_bytes, external_, raw_.capacity,
                 index_bytes_, in, out})
    if (!add(total, n))
      return false;
  return total <= limits_.max_internal_buffered_bytes;
}
bool Encoder::disjoint(std::span<const std::byte> in,
                       std::span<std::byte> out) const noexcept {
  const std::array regions{
      Region{in.data(), in.size()},
      Region{out.data(), out.size()},
      Region{this, sizeof(*this)},
      Region{allocator_controls_.data, allocator_controls_.bytes},
      Region{raw_.data, raw_.capacity},
      Region{index_.data, index_bytes_}};
  auto disjoint = [](Region a, Region b) {
    return core::check_buffer_overlap(a.data, a.bytes, b.data, b.bytes) ==
           core::BufferOverlap::disjoint;
  };
  for (std::size_t i = 0; i < regions.size(); ++i) {
    for (std::size_t j = i + 1; j < regions.size(); ++j)
      if (!disjoint(regions[i], regions[j]))
        return false;
    for (const auto &r : bridge_.regions)
      if (!disjoint(regions[i], r))
        return false;
  }
  return true;
}
Encoder::LzssPositionDistance8mOwnedStreamEncoder(
    TypedContextStreamHeader s, core::DecoderLimits l,
    LzssPositionDistance8mStreamAllocator &a, std::size_t extra) noexcept
    : stream_(s), limits_(l), bridge_(a), owner_(bridge_),
      allocator_controls_(a.controls()), external_(extra) {
  auto reject = [&](Code code) { static_cast<void>(fail(code, 0)); };
  if (!allocator_controls_.data || !allocator_controls_.bytes ||
      core::validate_limits(limits_) != core::LimitError::none ||
      !disjoint({}, {})) {
    reject(Code::invalid_argument);
    return;
  }
  const auto raw = static_cast<std::size_t>(
      std::min<std::uint64_t>(s.frame_size, s.original_size));
  std::size_t index{}, total = working_bytes();
  if ((raw &&
       (!core::checked_add(raw, std::size_t{65536}, index) ||
        !core::checked_multiply(index, sizeof(std::uint32_t), index_bytes_))) ||
      !add(total, raw) || !add(total, index_bytes_) ||
      !add(total, allocator_controls_.bytes) ||
      !add(total, allocator_controls_.working_bytes) ||
      !add(total, external_) || total > limits_.max_internal_buffered_bytes) {
    reject(Code::limit_exceeded);
    return;
  }
  const auto local = header_.size() +
                     lzss_position_distance_8m_stream_serialize_working_bytes();
  if (local > total) {
    reject(Code::internal_error);
    return;
  }
  const auto h = serialize_lzss_position_distance_8m_stream_header(
      stream_, limits_, header_, header_written_, total - local);
  if (h.error != LzssPositionDistance8mSerializeError::none) {
    reject(h.error == LzssPositionDistance8mSerializeError::limit_exceeded
               ? Code::limit_exceeded
               : Code::invalid_argument);
    return;
  }
  // Both prospective full requests were admitted before either callback.
  if (raw) {
    raw_ = a.bytes(raw);
    if (!raw_.data || raw_.capacity != raw) {
      reject(!raw_.data && !raw_.capacity ? Code::out_of_memory
                                          : Code::limit_exceeded);
      return;
    }
    index_ = a.indices(index);
    if (!index_.data || index_.capacity != index) {
      reject(!index_.data && !index_.capacity ? Code::out_of_memory
                                              : Code::limit_exceeded);
      return;
    }
  }
  if (!disjoint({}, {}))
    reject(Code::invalid_argument);
}
Encoder::~LzssPositionDistance8mOwnedStreamEncoder() {
  bridge_.source.release(index_);
  bridge_.source.release(raw_);
}
core::ProcessResult Encoder::fail(Code code, std::uint64_t position,
                                  std::size_t used, std::size_t made) noexcept {
  state_ = State::error;
  error_ = {code, position, 0};
  return {used, made, core::StreamStatus::error, error_};
}
core::ProcessResult Encoder::process(std::span<const std::byte> input,
                                     std::span<std::byte> output,
                                     std::uint32_t flags) noexcept {
  using S = core::StreamStatus;
  if (state_ == State::error)
    return {0, 0, S::error, error_};
  if (state_ == State::ended)
    return {0, 0, S::end_of_stream, {}};
  constexpr auto end = core::flag_value(core::ProcessFlags::end_input);
  constexpr auto allowed = end | core::flag_value(core::ProcessFlags::flush);
  if (flags & ~allowed)
    return fail(Code::unsupported, accepted_);
  auto controls = bridge_.source.controls();
  if (controls.data != allocator_controls_.data ||
      controls.bytes != allocator_controls_.bytes ||
      controls.working_bytes != allocator_controls_.working_bytes ||
      !disjoint(input, output))
    return fail(Code::invalid_argument, accepted_);
  std::size_t total{};
  if (!budget(input.size(), output.size(), total))
    return fail(Code::limit_exceeded, accepted_);
  if (end_seen_ && !input.empty())
    return fail(Code::malformed_stream, accepted_);
  std::size_t used{}, made{};
  const bool final = (flags & end) != 0;
  while (true) {
    if (final && used == input.size())
      end_seen_ = true;
    if (state_ == State::header_drain || state_ == State::frame_drain) {
      const auto pending =
          state_ == State::header_drain
              ? std::span<const std::byte>(header_).first(header_written_)
              : owner_.publication();
      const auto n = std::min(pending.size() - drained_, output.size() - made);
      if (n)
        std::memcpy(output.data() + made, pending.data() + drained_, n);
      made += n;
      drained_ += n;
      if (drained_ != pending.size())
        return {used, made, S::need_output, {}};
      if (state_ == State::frame_drain)
        owner_.acknowledge_drained();
      drained_ = collected_ = 0;
      state_ = validated_ == stream_.original_size ? State::awaiting_end
                                                   : State::collecting;
      continue;
    }
    if (state_ == State::awaiting_end) {
      if (used != input.size())
        return fail(Code::malformed_stream, accepted_, used, made);
      if (end_seen_) {
        state_ = State::ended;
        return {used, made, S::end_of_stream, {}};
      }
      return {used, made, used || made ? S::progress : S::need_input, {}};
    }
    const auto needed = static_cast<std::size_t>(std::min<std::uint64_t>(
        stream_.frame_size, stream_.original_size - validated_));
    const auto n = std::min(needed - collected_, input.size() - used);
    std::uint64_t next{};
    if (!core::checked_add(accepted_, static_cast<std::uint64_t>(n), next))
      return fail(Code::limit_exceeded, accepted_, used, made);
    if (n)
      std::memcpy(raw_.data + collected_, input.data() + used, n);
    used += n;
    collected_ += n;
    accepted_ = next;
    if (final && used == input.size())
      end_seen_ = true;
    if (collected_ != needed) {
      if (end_seen_)
        return fail(Code::malformed_stream, accepted_, used, made);
      return {used, made, used || made ? S::progress : S::need_input, {}};
    }
    std::uint64_t next_raw{}, next_seq{};
    if (!core::checked_add(validated_, static_cast<std::uint64_t>(needed),
                           next_raw) ||
        !core::checked_add(sequence_, std::uint64_t{1}, next_seq))
      return fail(Code::limit_exceeded, validated_, used, made);
    // Owner charges its full persistent object, bridge, underlying allocator,
    // old/new generations, index and supplied raw prefix. Remove those controls
    // from this reservation, then add the raw tail and complete call views.
    std::size_t extra = working_bytes() -
                        LzssPositionDistance8mStorageOwner::working_bytes() -
                        sizeof(Bridge) - callback_working;
    if (!add(extra, external_) || !add(extra, raw_.capacity - needed) ||
        !add(extra, input.size()) || !add(extra, output.size()))
      return fail(Code::limit_exceeded, validated_, used, made);
    const TypedContextFrameValidationContext context{stream_, limits_,
                                                     sequence_, validated_};
    const auto r =
        owner_.encode(raw_.view().first(needed), context, index_.view(), extra);
    if (r.error != Code::none)
      return fail(r.error, validated_, used, made);
    validated_ = next_raw;
    sequence_ = next_seq;
    state_ = State::frame_drain;
    drained_ = 0;
  }
}
} // namespace marc::frame::internal

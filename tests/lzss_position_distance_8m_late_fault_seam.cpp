#include "lzss_position_distance_8m_late_fault_seam.hpp"
#include "core/checked_math.hpp"
#include <algorithm>
namespace marc::test::late8m {
namespace {
thread_local Controller *active{};
}
Scope::Scope(Controller &c) noexcept : previous_(active) { active = &c; }
Scope::~Scope() { active = previous_; }
Controller *controller() noexcept { return active; }
std::size_t working_bytes() noexcept {
  // Entire shim controls AND conservative nested real-helper storage. This is
  // added to external reservations; never borrowed from the owner's grant.
  return sizeof(Controller *) + sizeof(Scope) + 32 * sizeof(std::size_t) +
         16 * sizeof(void *) + 8 * sizeof(std::span<std::byte>) +
         2 * sizeof(entropy::internal::LzssPositionDistance8mTokenRangeResult) +
         2 * sizeof(frame::internal::LzssPositionDistance8mSerializeResult) +
         2 * sizeof(frame::internal::TypedContextFrameLayout) +
         2 * sizeof(frame::internal::LzssPositionDistance8mFrameRequirements) +
         sizeof(frame::internal::TypedContextFrameValidationContext) +
         entropy::internal::
             lzss_position_distance_8m_token_range_working_bytes() +
         frame::internal::
             lzss_position_distance_8m_prefix_serialize_working_bytes() +
         sizeof(entropy::internal::LzssPositionDistance8mRangeState);
}
bool selected(Controller *c, std::uint64_t prior, std::size_t raw) noexcept {
  return c && c->prior == prior && c->raw == raw;
}
bool inject(Controller &c, unsigned a, unsigned b = 0) noexcept {
  if (c.mode != a && (!b || c.mode != b))
    return false;
  if (c.injections++)
    c.valid = false;
  return true;
}
} // namespace marc::test::late8m
namespace marc::entropy::internal {
LzssPositionDistance8mTokenRangeResult
test_encode_lzss_position_distance_8m_token_range(
    std::span<const dictionary::internal::LzssTypedToken> tokens,
    const dictionary::internal::LzssParameters &parameters,
    const context::internal::LzssFieldContextValidationContext &counts,
    const core::DecoderLimits &limits, std::span<std::byte> output,
    std::span<std::byte> scratch, ContextualDynamicRangeDescriptor &descriptor,
    std::size_t retained) noexcept {
  auto r = encode_lzss_position_distance_8m_token_range(
      tokens, parameters, counts, limits, output, scratch, descriptor,
      retained);
  auto *c = test::late8m::controller();
  if (!test::late8m::selected(
          c, counts.output_already_committed,
          static_cast<std::size_t>(counts.declared_raw_size)))
    return r;
  ++c->range;
  if (r.details.error != LzssPositionDistance8mTokenRangeError::none ||
      !r.bytes_committed) {
    c->valid = false;
    return r;
  }
  if (test::late8m::inject(*c, 1, 2)) {
    if (c->mode == 1)
      r.details.error = LzssPositionDistance8mTokenRangeError::internal_error;
    else if (!core::checked_add(descriptor.payload_size, std::uint32_t{1},
                                descriptor.payload_size))
      c->valid = false;
  }
  return r;
}
} // namespace marc::entropy::internal
namespace marc::frame::internal {
LzssPositionDistance8mSerializeResult
test_serialize_lzss_position_distance_8m_frame_prefix(
    const TypedContextFrameLayout &layout,
    const TypedContextFrameValidationContext &context,
    std::span<std::byte> output, std::size_t &written,
    std::size_t retained) noexcept {
  auto r = serialize_lzss_position_distance_8m_frame_prefix(
      layout, context, output, written, retained);
  auto *c = test::late8m::controller();
  if (!test::late8m::selected(c, context.output_already_committed,
                              layout.header.uncompressed_size))
    return r;
  ++c->prefix;
  c->valid = c->valid && context.expected_sequence == c->prior / 64;
  if (r.error != LzssPositionDistance8mSerializeError::none || written != 80 ||
      r.bytes_committed != 80 || output.size() < 80) {
    c->valid = false;
    return r;
  }
  if (c->mode >= 3 && c->mode <= 5 && test::late8m::inject(*c, c->mode)) {
    if (c->mode == 3)
      r.error = LzssPositionDistance8mSerializeError::validation_error;
    if (c->mode == 4)
      written = 79;
    if (c->mode == 5)
      output[0] ^= std::byte{1};
  }
  return r;
}
LzssPositionDistance8mPreflightError
test_preflight_lzss_position_distance_8m_frame_prefix(
    std::span<const std::byte> input,
    const TypedContextFrameValidationContext &context,
    TypedContextFrameLayout &layout,
    LzssPositionDistance8mFrameRequirements &requirements,
    std::size_t retained) noexcept {
  auto r = preflight_lzss_position_distance_8m_frame_prefix(
      input, context, layout, requirements, retained);
  auto *c = test::late8m::controller();
  const auto raw = static_cast<std::size_t>(std::min<std::uint64_t>(
      context.stream.frame_size,
      context.stream.original_size - context.output_already_committed));
  if (!test::late8m::selected(c, context.output_already_committed, raw))
    return r;
  ++c->reparse;
  c->parsed_error = r;
  c->valid = c->valid && context.expected_sequence == c->prior / 64;
  if (c->mode == 5) {
    c->valid =
        c->valid && r == LzssPositionDistance8mPreflightError::invalid_magic;
    return r;
  }
  if (r != LzssPositionDistance8mPreflightError::none) {
    c->valid = false;
    return r;
  }
  if (test::late8m::inject(*c, 6) &&
      !core::checked_add(layout.header.sequence, std::uint64_t{1},
                         layout.header.sequence))
    c->valid = false;
  return r;
}
} // namespace marc::frame::internal

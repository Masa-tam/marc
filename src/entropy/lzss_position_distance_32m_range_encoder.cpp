// Private thirty-two-MiB finite scalar operation encoder.
#include "entropy/lzss_position_distance_32m_range_encoder.hpp"

#include "context/lzss_position_distance_32m_context_layout.hpp"
#include "context/lzss_position_distance_32m_field_cursor.hpp"
#include "core/buffer_overlap.hpp"
#include "core/checked_math.hpp"
#include <algorithm>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace marc::entropy::internal {
namespace {

inline constexpr std::uint32_t normalization_threshold = UINT32_C(1) << 24;

class RangeWriter {
public:
  explicit RangeWriter(const std::span<std::byte> output) noexcept
      : output_(output) {}

  [[nodiscard]] bool encode(const std::uint32_t cumulative,
                            const std::uint16_t frequency,
                            const std::uint32_t total) noexcept {
    if (total == 0 || frequency == 0 || cumulative >= total ||
        static_cast<std::uint64_t>(cumulative) + frequency > total ||
        range_ < normalization_threshold) {
      return false;
    }
    range_ /= total;
    if (range_ == 0)
      return false;
    low_ += static_cast<std::uint64_t>(cumulative) * range_;
    range_ *= frequency;
    while (range_ < normalization_threshold) {
      range_ <<= 8;
      if (!shift_low())
        return false;
    }
    return true;
  }

  [[nodiscard]] bool finish() noexcept {
    for (int index = 0; index < 5; ++index) {
      if (!shift_low())
        return false;
    }
    return true;
  }

  [[nodiscard]] std::size_t size() const noexcept { return size_; }

private:
  [[nodiscard]] bool emit(const std::uint8_t value) noexcept {
    if (size_ == std::numeric_limits<std::size_t>::max())
      return false;
    if (!output_.empty()) {
      if (size_ >= output_.size())
        return false;
      output_[size_] = static_cast<std::byte>(value);
    }
    ++size_;
    return true;
  }

  [[nodiscard]] bool shift_low() noexcept {
    const auto low32 = static_cast<std::uint32_t>(low_);
    const auto carry = static_cast<std::uint32_t>(low_ >> 32);
    if (carry > 1)
      return false;
    if (low32 < UINT32_C(0xff000000) || carry != 0) {
      if (!emit(static_cast<std::uint8_t>(cache_ + carry)))
        return false;
      const auto delayed = static_cast<std::uint8_t>(UINT32_C(0xff) + carry);
      for (std::size_t index = 1; index < pending_; ++index) {
        if (!emit(delayed))
          return false;
      }
      cache_ = static_cast<std::uint8_t>(low32 >> 24);
      pending_ = 0;
    }
    if (pending_ == std::numeric_limits<std::size_t>::max())
      return false;
    ++pending_;
    low_ = static_cast<std::uint32_t>(low32 << 8);
    return true;
  }

  std::span<std::byte> output_{};
  std::uint64_t low_{};
  std::uint32_t range_{UINT32_MAX};
  std::uint8_t cache_{};
  std::size_t pending_{1};
  std::size_t size_{};
};

struct Models {
  std::array<std::uint16_t,
             context::internal::lzss_position_distance_32m_frequency_entries>
      frequencies{};
  std::array<std::uint32_t,
             context::internal::lzss_position_distance_32m_context_count>
      totals{};

  Models() noexcept {
    frequencies.fill(1);
    for (std::size_t index = 0; index < totals.size(); ++index) {
      totals[index] =
          context::internal::lzss_position_distance_32m_alphabets[index];
    }
  }

  void update(const std::uint16_t context_id,
              const std::uint32_t symbol) noexcept {
    const auto offset =
        context::internal::lzss_position_distance_32m_offsets[context_id];
    ++frequencies[offset + symbol];
    auto &total = totals[context_id];
    ++total;
    if (total != contextual_dynamic_range_model_total_limit)
      return;
    total = 0;
    const auto alphabet =
        context::internal::lzss_position_distance_32m_alphabets[context_id];
    for (std::size_t index = 0; index < alphabet; ++index) {
      auto &frequency = frequencies[offset + index];
      frequency = static_cast<std::uint16_t>(
          (static_cast<std::uint32_t>(frequency) + 1U) / 2U);
      total += frequency;
    }
  }
};

struct WorkingState {
  Models models{};
  RangeWriter writer;
  context::internal::LzssPositionDistance32mFieldState cursor{};
  ContextualDynamicRangeEncodeResult result{};
  explicit WorkingState(std::span<std::byte> output) noexcept
      : writer(output) {}
};

[[nodiscard]] ContextualDynamicRangeEncodeResult
fail(ContextualDynamicRangeEncodeResult result,
     const ContextualDynamicRangeEncodeError error) noexcept {
  result.error = error;
  return result;
}

[[nodiscard]] bool
add_decisions(const std::uint32_t count,
              ContextualDynamicRangeEncodeResult &result) noexcept {
  std::uint32_t updated{};
  if (!core::checked_add(result.decision_count, count, updated)) {
    result.error = ContextualDynamicRangeEncodeError::arithmetic_overflow;
    return false;
  }
  result.decision_count = updated;
  return true;
}

[[nodiscard]] ContextualDynamicRangeEncodeResult
run(const std::span<const context::internal::ModeledOperation> operations,
    const std::span<std::byte> output) noexcept {
  WorkingState w(output);
  auto &result = w.result;
  auto &models = w.models;
  auto &writer = w.writer;
  auto &cursor = w.cursor;
  for (const auto &operation : operations) {
    result.operation_index = result.operation_count;
    const auto field = lzss_position_distance_32m_next(cursor).field;
    // Grammar validation precedes model indexing and bit shifts.
    if (lzss_position_distance_32m_accept(cursor, operation) !=
        context::internal::LzssFieldContextError::none) {
      return fail(result, ContextualDynamicRangeEncodeError::invalid_symbol);
    }
    if (operation.kind == context::internal::ModeledOperationKind::symbol) {
      // Successful cursor acceptance proves these storage predicates.
      // Retain the explicit checks in the reference implementation.
      if (operation.context_id >=
          context::internal::lzss_position_distance_32m_context_count) {
        return fail(result, ContextualDynamicRangeEncodeError::invalid_context);
      }
      if (operation.alphabet_size !=
          context::internal::lzss_position_distance_32m_alphabets
              [operation.context_id]) {
        return fail(result,
                    ContextualDynamicRangeEncodeError::invalid_alphabet);
      }
      if (operation.value >= operation.alphabet_size) {
        return fail(result, ContextualDynamicRangeEncodeError::invalid_symbol);
      }
      if (operation.bit_count != 0) {
        return fail(result,
                    ContextualDynamicRangeEncodeError::nonzero_unused_field);
      }
      const auto offset = context::internal::lzss_position_distance_32m_offsets
          [operation.context_id];
      std::uint32_t cumulative{};
      for (std::uint32_t symbol = 0; symbol < operation.value; ++symbol) {
        cumulative += models.frequencies[offset + symbol];
      }
      if (!writer.encode(cumulative,
                         models.frequencies[offset + operation.value],
                         models.totals[operation.context_id])) {
        return fail(result, ContextualDynamicRangeEncodeError::internal_error);
      }
      models.update(operation.context_id, operation.value);
      if (!add_decisions(1, result))
        return result;
    } else if (operation.kind ==
               context::internal::ModeledOperationKind::bypass_bits) {
      // The accepted phase fixes a width in 1..25 and bounds its value.
      if (operation.context_id != 0 || operation.alphabet_size != 0) {
        return fail(result,
                    ContextualDynamicRangeEncodeError::nonzero_unused_field);
      }
      if (operation.bit_count == 0 || operation.bit_count > 25) {
        return fail(result,
                    ContextualDynamicRangeEncodeError::invalid_bypass_width);
      }
      if ((operation.value >> operation.bit_count) != 0) {
        return fail(result,
                    ContextualDynamicRangeEncodeError::nonzero_unused_field);
      }
      for (std::uint8_t bit = 0; bit < operation.bit_count; ++bit) {
        const auto value = (operation.value >> bit) & 1U;
        const bool adaptive = field ==
                              context::internal::LzssPositionDistance32mField::
                                  adaptive_distance_extra;
        const auto id = static_cast<std::uint16_t>(24 + bit);
        const auto offset =
            context::internal::lzss_position_distance_32m_offsets[id];
        const auto cumulative =
            adaptive && value != 0 ? models.frequencies[offset] : value;
        const auto frequency =
            adaptive ? models.frequencies[offset + value] : 1;
        const auto total = adaptive ? models.totals[id] : 2;
        if (!writer.encode(adaptive && value == 0 ? 0 : cumulative,
                           static_cast<std::uint16_t>(frequency), total)) {
          return fail(result,
                      ContextualDynamicRangeEncodeError::internal_error);
        }
        if (adaptive)
          models.update(id, value);
      }
      if (!add_decisions(operation.bit_count, result))
        return result;
    } else {
      return fail(result,
                  ContextualDynamicRangeEncodeError::invalid_operation_kind);
    }
    ++result.operation_count;
  }
  result.operation_index = result.operation_count;
  if (lzss_position_distance_32m_finish(cursor) !=
      context::internal::LzssFieldContextError::none) {
    return fail(result, ContextualDynamicRangeEncodeError::invalid_symbol);
  }
  if (!writer.finish()) {
    return fail(result, ContextualDynamicRangeEncodeError::internal_error);
  }
  result.payload_size = writer.size();
  return result;
}

struct Region {
  const void *data;
  std::size_t size;
};
constexpr std::size_t working_bytes =
    sizeof(WorkingState) + 2 * sizeof(LzssPositionDistance32mRangeEncodePlan) +
    sizeof(LzssPositionDistance32mRangeEncodeResult) +
    sizeof(ContextualDynamicRangeDescriptor) + sizeof(std::array<Region, 7>);
} // namespace

std::size_t lzss_position_distance_32m_range_encode_working_bytes() noexcept {
  return working_bytes;
}

LzssPositionDistance32mRangeEncodePlan
query_lzss_position_distance_32m_range_encode(
    std::span<const context::internal::ModeledOperation> ops,
    const core::DecoderLimits &limits, std::size_t output_capacity,
    std::size_t scratch_capacity, std::size_t retained) noexcept {
  using E = ContextualDynamicRangeEncodeError;
  LzssPositionDistance32mRangeEncodePlan q{};
  q.working_state_bytes = working_bytes;
  if (core::validate_limits(limits) != core::LimitError::none ||
      2621 > limits.max_entropy_table_entries ||
      32768 > limits.max_range_model_total) {
    q.error = E::limit_exceeded;
    return q;
  }
  if (ops.empty()) {
    q.error = E::empty_operations;
    return q;
  }
  std::size_t opbytes{}, aggregate{};
  if (!core::checked_multiply(
          ops.size(), sizeof(context::internal::ModeledOperation), opbytes)) {
    q.error = E::arithmetic_overflow;
    return q;
  }
  aggregate = opbytes;
  for (auto n : {output_capacity, scratch_capacity, working_bytes, retained})
    if (!core::checked_add(aggregate, n, aggregate)) {
      q.error = E::arithmetic_overflow;
      return q;
    }
  q.aggregate_bytes = aggregate;
  if (aggregate > limits.max_internal_buffered_bytes || ops.size() > 67108864) {
    q.error = E::limit_exceeded;
    return q;
  }
  q.details = run(ops, {});
  if (q.details.error != E::none) {
    q.error = q.details.error;
    return q;
  }
  if (q.details.payload_size > UINT32_MAX) {
    q.error = E::arithmetic_overflow;
    return q;
  }
  if (q.details.payload_size > limits.max_compressed_payload_size) {
    q.error = E::limit_exceeded;
    return q;
  }
  if (output_capacity < q.details.payload_size ||
      scratch_capacity < q.details.payload_size) {
    q.error = E::payload_output_too_small;
    return q;
  }
  q.descriptor = {q.details.decision_count,
                  static_cast<std::uint32_t>(q.details.payload_size), 49};
  return q;
}

LzssPositionDistance32mRangeEncodeResult
encode_lzss_position_distance_32m_range_operations(
    std::span<const context::internal::ModeledOperation> ops,
    const core::DecoderLimits &limits, std::span<std::byte> output,
    std::span<std::byte> scratch, ContextualDynamicRangeDescriptor &descriptor,
    std::size_t retained) noexcept {
  using E = ContextualDynamicRangeEncodeError;
  LzssPositionDistance32mRangeEncodeResult result{};
  std::size_t opbytes{};
  if (!core::checked_multiply(
          ops.size(), sizeof(context::internal::ModeledOperation), opbytes)) {
    result.details.error = E::arithmetic_overflow;
    return result;
  }
  const std::array regions{Region{ops.data(), opbytes},
                           Region{&limits, sizeof(limits)},
                           Region{output.data(), output.size()},
                           Region{scratch.data(), scratch.size()},
                           Region{&descriptor, sizeof(descriptor)}};
  for (std::size_t i = 0; i < regions.size(); ++i)
    for (std::size_t j = i + 1; j < regions.size(); ++j) {
      const auto alias = core::check_buffer_overlap(
          regions[i].data, regions[i].size, regions[j].data, regions[j].size);
      if (alias != core::BufferOverlap::disjoint) {
        result.details.error = alias == core::BufferOverlap::arithmetic_overflow
                                   ? E::arithmetic_overflow
                                   : E::overlapping_buffers;
        return result;
      }
    }
  const auto q = query_lzss_position_distance_32m_range_encode(
      ops, limits, output.size(), scratch.size(), retained);
  result.details = q.details;
  if (q.error != E::none) {
    result.details.error = q.error;
    return result;
  }
  const auto encoded = run(ops, scratch.first(q.details.payload_size));
  result.details = encoded;
  if (encoded.error != E::none)
    return result;
  if (encoded.operation_count != q.details.operation_count ||
      encoded.decision_count != q.details.decision_count ||
      encoded.payload_size != q.details.payload_size) {
    result.details.error = E::internal_error;
    return result;
  }
  std::copy_n(scratch.begin(), encoded.payload_size, output.begin());
  descriptor = q.descriptor;
  result.bytes_committed = encoded.payload_size;
  return result;
}
} // namespace marc::entropy::internal

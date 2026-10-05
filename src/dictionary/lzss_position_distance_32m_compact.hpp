#ifndef MARC_DICTIONARY_POSITION_DISTANCE32M_COMPACT_HPP
#define MARC_DICTIONARY_POSITION_DISTANCE32M_COMPACT_HPP
#include "core/endian.hpp"
#include "dictionary/lzss_position_distance_32m_indexed.hpp"
namespace marc::dictionary::internal {
struct LzssPositionDistance32mCompactPlan {
  LzssPositionDistance32mParseMetadata details{};
  std::size_t byte_count{}, aggregate_bytes{}, working_state_bytes{};
  LzssPositionDistance32mParseError error{
      LzssPositionDistance32mParseError::none};
};
// Reads canonical byte records; failed reads preserve token and cursor.
// History/count validation belongs to the distinct thirty-two-MiB consumer.
class LzssPositionDistance32mCompactReader {
public:
  explicit LzssPositionDistance32mCompactReader(
      std::span<const std::byte> bytes) noexcept
      : bytes_(bytes) {}
  bool empty() const noexcept { return position_ == bytes_.size(); }
  std::size_t position() const noexcept { return position_; }
  bool read(LzssTypedToken &output) noexcept {
    if (empty())
      return false;
    const auto tag = std::to_integer<unsigned>(bytes_[position_]);
    if (tag > 1)
      return false;
    const auto n = tag == 0 ? 2u : 9u;
    if (bytes_.size() - position_ < n)
      return false;
    LzssTypedToken token{};
    if (tag == 0)
      token.literal = std::to_integer<std::uint8_t>(bytes_[position_ + 1]);
    else {
      token.kind = LzssTypedTokenKind::match;
      if (!core::load_le(bytes_, position_ + 1, token.distance) ||
          !core::load_le(bytes_, position_ + 5, token.length))
        return false;
    }
    output = token;
    position_ += n;
    return true;
  }

private:
  std::span<const std::byte> bytes_;
  std::size_t position_{};
};
[[nodiscard]] std::size_t
lzss_position_distance_32m_compact_working_bytes() noexcept;
// Full raw/index/private byte capacity/state/retained admitted before
// traversal. Query mutates only private index and returns exact counts even for
// capacity0.
[[nodiscard]] LzssPositionDistance32mCompactPlan
query_lzss_position_distance_32m_compact(std::span<const std::byte>,
                                         const LzssParameters &,
                                         const core::DecoderLimits &,
                                         std::size_t private_capacity,
                                         std::span<std::uint32_t> index,
                                         std::size_t retained = 0,
                                         std::uint64_t committed = 0) noexcept;
// Single parse into PRIVATE DISCARDABLE bytes; no typed-token arrays/copy.
// ANY failure means discard bytes; returned partial counts are not publication.
// Full regions/config disjoint, borrowed input stable. No allocation/frame
// commit.
[[nodiscard]] LzssPositionDistance32mCompactPlan
write_lzss_position_distance_32m_compact_private(
    std::span<const std::byte>, const LzssParameters &,
    const core::DecoderLimits &, std::span<std::byte> private_bytes,
    std::span<std::uint32_t> index, std::size_t retained = 0,
    std::uint64_t committed = 0) noexcept;
} // namespace marc::dictionary::internal
#endif

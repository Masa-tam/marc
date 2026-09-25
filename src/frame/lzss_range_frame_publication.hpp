#ifndef MARC_FRAME_LZSS_RANGE_FRAME_PUBLICATION_HPP
#define MARC_FRAME_LZSS_RANGE_FRAME_PUBLICATION_HPP

#include "frame/lzss_short_match_frame_encoder.hpp"
#include <array>
#include <cstring>

namespace marc::frame::internal {

// Called after payload writing. Failure leaves header/descriptor bytes alone;
// payload may already be partially written and must not be consumed on error.
[[nodiscard]] inline LzssShortMatchFrameEncodeResult publish_lzss_range_frame(
    LzssShortMatchFrameEncodeResult result,
    const TypedContextRangeDescriptor& expected,
    const TypedContextRangeDescriptor& encoded,
    const std::array<std::byte, typed_context_frame_header_size>& header,
    const std::array<std::byte, typed_context_range_descriptor_size>& descriptor,
    std::span<std::byte> output) noexcept {
    if (result.entropy.error != entropy::internal::ContextualDynamicRangeEncodeError::none
        || result.entropy.decision_count != result.decision_count
        || result.entropy.payload_size != result.payload_size
        || encoded.decision_count != expected.decision_count
        || encoded.payload_size != expected.payload_size
        || encoded.context_count != expected.context_count
        || output.size() < header.size() + descriptor.size()) {
        result.error = LzssShortMatchFrameEncodeError::internal_error;
        return result;
    }
    std::memcpy(output.data(), header.data(), header.size());
    std::memcpy(output.data() + header.size(), descriptor.data(), descriptor.size());
    return result;
}

} // namespace marc::frame::internal
#endif

#ifndef MARC_TESTS_PREPARED_POSITION_DISTANCE_TEST_ACCESS_HPP
#define MARC_TESTS_PREPARED_POSITION_DISTANCE_TEST_ACCESS_HPP
#include "entropy/lzss_position_distance_range_encoder.hpp"

namespace marc::entropy::internal {
// Test-only metadata faults; borrowed operations remain alive and unchanged.
struct PreparedLzssPositionDistanceEncodeTestAccess {
    static void shorten_payload(PreparedLzssPositionDistanceEncode& value,
                                std::size_t size) noexcept {
        value.plan_.payload_size=size;
        value.descriptor_.payload_size=static_cast<std::uint32_t>(size);
    }
    static void mismatch(PreparedLzssPositionDistanceEncode& value, unsigned field) noexcept {
        switch (field) {
        case 0: ++value.plan_.operation_count; break;
        case 1: ++value.plan_.operation_index; break;
        case 2: ++value.plan_.decision_count; break;
        case 3: ++value.plan_.payload_size; break;
        case 4: ++value.descriptor_.decision_count; break;
        case 5: ++value.descriptor_.payload_size; break;
        case 6: ++value.descriptor_.context_count; break;
        }
    }
};
}
#endif

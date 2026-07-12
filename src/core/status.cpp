#include "core/status.hpp"

namespace marc::core {

bool is_valid(const ProcessResult& result,
              const std::size_t input_size,
              const std::size_t output_size) noexcept {
    if (result.input_consumed > input_size ||
        result.output_produced > output_size) {
        return false;
    }
    if (result.status == StreamStatus::progress &&
        result.input_consumed == 0 && result.output_produced == 0) {
        return false;
    }
    if (result.status == StreamStatus::error) {
        return result.error.code != ErrorCode::none;
    }
    return result.error.code == ErrorCode::none;
}

} // namespace marc::core

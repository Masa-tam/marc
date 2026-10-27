#ifndef MARC_C_API_STATUS_HPP
#define MARC_C_API_STATUS_HPP

#include "core/status.hpp"
#include "marc/marc.h"

namespace marc::c_api {

[[nodiscard]] constexpr marc_status status_for(const core::ErrorCode code) noexcept {
    switch (code) {
    case core::ErrorCode::none: return MARC_STATUS_OK;
    case core::ErrorCode::invalid_argument: return MARC_STATUS_INVALID_ARGUMENT;
    case core::ErrorCode::unsupported: return MARC_STATUS_UNSUPPORTED;
    case core::ErrorCode::limit_exceeded: return MARC_STATUS_LIMIT_EXCEEDED;
    case core::ErrorCode::out_of_memory: return MARC_STATUS_OUT_OF_MEMORY;
    case core::ErrorCode::malformed_stream: return MARC_STATUS_MALFORMED_STREAM;
    case core::ErrorCode::internal_error: return MARC_STATUS_INTERNAL_ERROR;
    }
    return MARC_STATUS_INTERNAL_ERROR;
}

[[nodiscard]] constexpr marc_status status_for(const core::StreamStatus status) noexcept {
    switch (status) {
    case core::StreamStatus::progress: return MARC_STATUS_PROGRESS;
    case core::StreamStatus::need_input: return MARC_STATUS_NEED_INPUT;
    case core::StreamStatus::need_output: return MARC_STATUS_NEED_OUTPUT;
    case core::StreamStatus::end_of_stream: return MARC_STATUS_END_OF_STREAM;
    case core::StreamStatus::error: return MARC_STATUS_INTERNAL_ERROR;
    }
    return MARC_STATUS_INTERNAL_ERROR;
}

// Creation historically admits a narrower set of failure categories than the
// process boundary. Do not turn an impossible creation failure into stream data.
[[nodiscard]] constexpr marc_status factory_status(const core::ErrorCode code) noexcept {
    switch (code) {
    case core::ErrorCode::none: return MARC_STATUS_OK;
    case core::ErrorCode::invalid_argument: return MARC_STATUS_INVALID_ARGUMENT;
    case core::ErrorCode::limit_exceeded: return MARC_STATUS_LIMIT_EXCEEDED;
    case core::ErrorCode::out_of_memory: return MARC_STATUS_OUT_OF_MEMORY;
    default: return MARC_STATUS_INTERNAL_ERROR;
    }
}

[[nodiscard]] constexpr marc_status profile_factory_status(const core::ErrorCode code) noexcept {
    return code == core::ErrorCode::unsupported ? MARC_STATUS_UNSUPPORTED
                                               : factory_status(code);
}

} // namespace marc::c_api

#endif

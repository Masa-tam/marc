#include "c_api/common.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <span>

namespace {
using marc::c_api::status_for;
using namespace marc::c_api::detail;
} // namespace

extern "C" {

uint32_t marc_abi_version(void) noexcept {
    return MARC_ABI_VERSION;
}

const char* marc_version_string(void) noexcept {
    return "0.8.0";
}

const char* marc_status_name(const marc_status status) noexcept {
    switch (status) {
    case MARC_STATUS_OK: return "ok";
    case MARC_STATUS_PROGRESS: return "progress";
    case MARC_STATUS_NEED_INPUT: return "need_input";
    case MARC_STATUS_NEED_OUTPUT: return "need_output";
    case MARC_STATUS_END_OF_STREAM: return "end_of_stream";
    case MARC_STATUS_INVALID_ARGUMENT: return "invalid_argument";
    case MARC_STATUS_UNSUPPORTED: return "unsupported";
    case MARC_STATUS_LIMIT_EXCEEDED: return "limit_exceeded";
    case MARC_STATUS_OUT_OF_MEMORY: return "out_of_memory";
    case MARC_STATUS_MALFORMED_STREAM: return "malformed_stream";
    case MARC_STATUS_INTERNAL_ERROR: return "internal_error";
    default: return "unknown";
    }
}

void marc_transform_destroy(marc_transform* transform) noexcept {
    if (transform != nullptr) {
        delete transform->implementation;
        delete transform;
    }
}

marc_process_result marc_transform_process(
    marc_transform* transform, const marc_const_buffer input,
    const marc_buffer output, const marc_process_flags flags) noexcept {
    marc_process_result result{};
    if (transform == nullptr || transform->implementation == nullptr
        || !valid_buffer(input.data, input.size)
        || !valid_buffer(output.data, output.size)) {
        result.status = MARC_STATUS_INVALID_ARGUMENT;
        return result;
    }
    const auto core_result = transform->implementation->process(
        {reinterpret_cast<const std::byte*>(input.data), input.size},
        {reinterpret_cast<std::byte*>(output.data), output.size}, flags);
    result.input_consumed = core_result.input_consumed;
    result.output_produced = core_result.output_produced;
    result.status = core_result.status == marc::core::StreamStatus::error
        ? status_for(core_result.error.code)
        : status_for(core_result.status);
    result.error_byte_position = core_result.error.byte_position;
    result.error_bit_position = core_result.error.bit_position;
    return result;
}

} // extern "C"

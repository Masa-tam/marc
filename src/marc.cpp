#include "marc/marc.h"

extern "C" {

uint32_t marc_abi_version(void) {
    return MARC_ABI_VERSION;
}

const char* marc_version_string(void) {
    return "0.1.0";
}

const char* marc_status_name(const marc_status status) {
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

}

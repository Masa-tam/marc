#include "core/status.hpp"
#include "marc/marc.h"

#include <cassert>
#include <string_view>

int main() {
    assert(marc_abi_version() == MARC_ABI_VERSION);
    assert(std::string_view{marc_version_string()} == "0.1.0");
    assert(std::string_view{marc_status_name(MARC_STATUS_NEED_INPUT)} == "need_input");

    using marc::core::ErrorCode;
    using marc::core::ProcessResult;
    using marc::core::StreamStatus;

    assert(marc::core::is_valid(
        ProcessResult{1, 0, StreamStatus::progress, {}}, 1, 0));
    assert(!marc::core::is_valid(
        ProcessResult{0, 0, StreamStatus::progress, {}}, 1, 1));
    assert(!marc::core::is_valid(
        ProcessResult{2, 0, StreamStatus::need_input, {}}, 1, 1));
    assert(marc::core::is_valid(
        ProcessResult{0, 0, StreamStatus::error,
                      {ErrorCode::malformed_stream, 4, 3}}, 1, 1));
}

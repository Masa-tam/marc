#include "marc/marc.h"

#include <assert.h>
#include <string.h>

int main(void) {
    marc_process_result result = {0};
    result.status = MARC_STATUS_NEED_INPUT;

    assert(sizeof(marc_status) == sizeof(uint32_t));
    assert(sizeof(marc_direction) == sizeof(uint32_t));
    assert(sizeof(marc_process_flags) == sizeof(uint32_t));
    assert(result.status == MARC_STATUS_NEED_INPUT);
    assert(marc_abi_version() == MARC_ABI_VERSION);
    assert(strcmp(marc_status_name(result.status), "need_input") == 0);
    return 0;
}

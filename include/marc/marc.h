#ifndef MARC_MARC_H
#define MARC_MARC_H

#include <stddef.h>
#include <stdint.h>

#include "marc/export.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MARC_ABI_VERSION UINT32_C(1)

typedef uint32_t marc_status;
#define MARC_STATUS_OK UINT32_C(0)
#define MARC_STATUS_PROGRESS UINT32_C(1)
#define MARC_STATUS_NEED_INPUT UINT32_C(2)
#define MARC_STATUS_NEED_OUTPUT UINT32_C(3)
#define MARC_STATUS_END_OF_STREAM UINT32_C(4)
#define MARC_STATUS_INVALID_ARGUMENT UINT32_C(100)
#define MARC_STATUS_UNSUPPORTED UINT32_C(101)
#define MARC_STATUS_LIMIT_EXCEEDED UINT32_C(102)
#define MARC_STATUS_OUT_OF_MEMORY UINT32_C(103)
#define MARC_STATUS_MALFORMED_STREAM UINT32_C(104)
#define MARC_STATUS_INTERNAL_ERROR UINT32_C(105)

typedef uint32_t marc_direction;
#define MARC_DIRECTION_ENCODE UINT32_C(1)
#define MARC_DIRECTION_DECODE UINT32_C(2)

typedef uint32_t marc_process_flags;
#define MARC_PROCESS_NONE UINT32_C(0)
#define MARC_PROCESS_FLUSH (UINT32_C(1) << 0)
#define MARC_PROCESS_END_INPUT (UINT32_C(1) << 1)
#define MARC_PROCESS_RESET_BLOCK (UINT32_C(1) << 2)

typedef struct marc_buffer {
    uint8_t* data;
    size_t size;
} marc_buffer;

typedef struct marc_const_buffer {
    const uint8_t* data;
    size_t size;
} marc_const_buffer;

typedef struct marc_process_result {
    size_t input_consumed;
    size_t output_produced;
    marc_status status;
    uint64_t error_byte_position;
    uint8_t error_bit_position;
    uint8_t reserved[7];
} marc_process_result;

typedef struct marc_transform marc_transform;

/* ABI-safe library metadata. These functions never throw across the C ABI. */
MARC_API uint32_t marc_abi_version(void);
MARC_API const char* marc_version_string(void);
MARC_API const char* marc_status_name(marc_status status);

/*
 * Transform creation and configuration will be added with the first framed
 * format variant. Handles are always owned and destroyed by marc; the ABI will
 * not expose C++ objects or library-allocated variable-sized result buffers.
 */

#ifdef __cplusplus
}
#endif

#endif

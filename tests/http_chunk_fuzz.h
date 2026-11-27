#ifndef CXNET_HTTP_CHUNK_FUZZ_H
#define CXNET_HTTP_CHUNK_FUZZ_H

#include <stddef.h>
#include <stdint.h>
#include "cxnet.internal.h"

struct cxnet_http_chunk_fuzz_result {
    size_t body_length;
    size_t chunk_remaining;
    size_t chunk_size_value;
    size_t chunk_line_bytes;
    size_t chunk_trailer_bytes;
    size_t chunk_trailer_name_length;
    uint32_t body_hash;
    uint32_t chunk_state;
    uint32_t chunk_extension_state;
    uint32_t chunk_size_has_digit;
    uint32_t chunk_trailer_line_has_data;
    uint32_t chunk_trailer_saw_cr;
    uint32_t chunk_trailer_saw_colon;
    uint32_t accepted;
};

CX_CXNET_API int cxnet_http_fuzz_chunk_input(const uint8_t *data, size_t length,
    size_t fragment_size, struct cxnet_http_chunk_fuzz_result *result);

#endif

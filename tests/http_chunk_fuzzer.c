#include "http_chunk_fuzz.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CXNET_HTTP_CHUNK_FUZZ_HEADER_LIMIT 16384u

static void verify_chunk_extension_cases(void) {
    static const size_t fragment_sizes[] = {1u, 2u, 7u, 31u, (size_t)-1};
    static const uint8_t invalid_empty_name[] =
        "4;=x\r\npong\r\n0\r\n\r\n";
    static const uint8_t invalid_empty_value[] =
        "4;note=\r\npong\r\n0\r\n\r\n";
    static const uint8_t valid_optional_whitespace[] =
        "4 ; note = token \r\npong\r\n0\r\n\r\n";
    static const struct {
        const uint8_t *data;
        size_t length;
        uint32_t accepted;
        const char *name;
    } cases[] = {
        {invalid_empty_name, sizeof(invalid_empty_name) - 1u, 0u,
            "empty extension name"},
        {invalid_empty_value, sizeof(invalid_empty_value) - 1u, 0u,
            "empty extension value"},
        {valid_optional_whitespace, sizeof(valid_optional_whitespace) - 1u,
            1u, "optional extension whitespace"}
    };
    size_t case_index;
    for (case_index = 0u;
         case_index < sizeof(cases) / sizeof(cases[0]); case_index += 1u) {
        struct cxnet_http_chunk_fuzz_result expected;
        size_t fragment_index;
        (void)cxnet_http_fuzz_chunk_input(cases[case_index].data,
            cases[case_index].length, fragment_sizes[0], &expected);
        if (expected.accepted != cases[case_index].accepted) {
            fprintf(stderr,
                "chunk extension case failed: %s accepted=%u expected=%u\n",
                cases[case_index].name, expected.accepted,
                cases[case_index].accepted);
            abort();
        }
        for (fragment_index = 1u;
             fragment_index < sizeof(fragment_sizes) / sizeof(fragment_sizes[0]);
             fragment_index += 1u) {
            struct cxnet_http_chunk_fuzz_result actual;
            (void)cxnet_http_fuzz_chunk_input(cases[case_index].data,
                cases[case_index].length, fragment_sizes[fragment_index],
                &actual);
            if (memcmp(&expected, &actual, sizeof(expected)) != 0) {
                fprintf(stderr,
                    "chunk extension case changed with fragment=%zu: %s\n",
                    fragment_sizes[fragment_index], cases[case_index].name);
                abort();
            }
        }
    }
}

static void verify_chunk_trailer_limit(void) {
    static const size_t fragment_sizes[] = {1u, 2u, 7u, 31u, (size_t)-1};
    static const uint8_t trailer_prefix[] = {'0', '\r', '\n', 'X', ':'};
    static const uint8_t trailer_suffix[] = {'\r', '\n', '\r', '\n'};
    size_t value_length = CXNET_HTTP_CHUNK_FUZZ_HEADER_LIMIT -
        (sizeof(trailer_prefix) - 3u) - sizeof(trailer_suffix);
    size_t input_length = sizeof(trailer_prefix) + value_length +
        sizeof(trailer_suffix);
    uint8_t *input = (uint8_t *)malloc(input_length + 1u);
    size_t over_limit;
    size_t fragment_index;
    if (input == NULL) abort();
    memcpy(input, trailer_prefix, sizeof(trailer_prefix));
    memset(input + sizeof(trailer_prefix), 'x', value_length + 1u);
    for (over_limit = 0u; over_limit <= 1u; over_limit += 1u) {
        size_t current_value_length = value_length + over_limit;
        size_t current_input_length = sizeof(trailer_prefix) +
            current_value_length + sizeof(trailer_suffix);
        struct cxnet_http_chunk_fuzz_result expected;
        memcpy(input + sizeof(trailer_prefix) + current_value_length,
            trailer_suffix, sizeof(trailer_suffix));
        (void)cxnet_http_fuzz_chunk_input(input, current_input_length,
            fragment_sizes[0], &expected);
        if (expected.accepted != (over_limit == 0u ? 1u : 0u) ||
            (over_limit == 0u &&
             (expected.chunk_state != 4u ||
              expected.chunk_trailer_bytes != CXNET_HTTP_CHUNK_FUZZ_HEADER_LIMIT))) {
            fprintf(stderr,
                "chunk trailer limit case failed: over=%zu accepted=%u "
                "state=%u bytes=%zu\n", over_limit, expected.accepted,
                expected.chunk_state, expected.chunk_trailer_bytes);
            abort();
        }
        for (fragment_index = 1u;
             fragment_index < sizeof(fragment_sizes) / sizeof(fragment_sizes[0]);
             fragment_index += 1u) {
            struct cxnet_http_chunk_fuzz_result actual;
            (void)cxnet_http_fuzz_chunk_input(input, current_input_length,
                fragment_sizes[fragment_index], &actual);
            if (memcmp(&expected, &actual, sizeof(expected)) != 0) {
                fprintf(stderr,
                    "chunk trailer limit changed with fragment=%zu over=%zu\n",
                    fragment_sizes[fragment_index], over_limit);
                abort();
            }
        }
    }
    free(input);
}

static void verify_chunk_line_limit(void) {
    static const size_t fragment_sizes[] = {1u, 2u, 7u, 31u, (size_t)-1};
    static const uint8_t line_prefix[] = {'0', ';'};
    static const uint8_t line_suffix[] = {'\r', '\n', '\r', '\n'};
    size_t extension_length = CXNET_HTTP_CHUNK_FUZZ_HEADER_LIMIT -
        sizeof(line_prefix) - 2u;
    size_t input_length = sizeof(line_prefix) + extension_length +
        sizeof(line_suffix);
    uint8_t *input = (uint8_t *)malloc(input_length + 1u);
    size_t over_limit;
    size_t fragment_index;
    if (input == NULL) abort();
    memcpy(input, line_prefix, sizeof(line_prefix));
    memset(input + sizeof(line_prefix), 'a', extension_length + 1u);
    for (over_limit = 0u; over_limit <= 1u; over_limit += 1u) {
        size_t current_extension_length = extension_length + over_limit;
        size_t current_input_length = sizeof(line_prefix) +
            current_extension_length + sizeof(line_suffix);
        struct cxnet_http_chunk_fuzz_result expected;
        memcpy(input + sizeof(line_prefix) + current_extension_length,
            line_suffix, sizeof(line_suffix));
        (void)cxnet_http_fuzz_chunk_input(input, current_input_length,
            fragment_sizes[0], &expected);
        if (expected.accepted != (over_limit == 0u ? 1u : 0u) ||
            (over_limit == 0u && expected.chunk_state != 4u)) {
            fprintf(stderr,
                "chunk line limit case failed: over=%zu accepted=%u "
                "state=%u bytes=%zu\n", over_limit, expected.accepted,
                expected.chunk_state, expected.chunk_line_bytes);
            abort();
        }
        for (fragment_index = 1u;
             fragment_index < sizeof(fragment_sizes) / sizeof(fragment_sizes[0]);
             fragment_index += 1u) {
            struct cxnet_http_chunk_fuzz_result actual;
            (void)cxnet_http_fuzz_chunk_input(input, current_input_length,
                fragment_sizes[fragment_index], &actual);
            if (memcmp(&expected, &actual, sizeof(expected)) != 0) {
                fprintf(stderr,
                    "chunk line limit changed with fragment=%zu over=%zu\n",
                    fragment_sizes[fragment_index], over_limit);
                abort();
            }
        }
    }
    free(input);
}

static void verify_chunk_truncation_case(const uint8_t *complete,
    size_t complete_length, const char *name) {
    size_t prefix_length;
    for (prefix_length = 1u; prefix_length <= complete_length;
         prefix_length += 1u) {
        struct cxnet_http_chunk_fuzz_result expected;
        size_t fragment_size;
        (void)cxnet_http_fuzz_chunk_input(complete, prefix_length, 1u,
            &expected);
        if (!expected.accepted ||
            (prefix_length == complete_length &&
             expected.chunk_state != 4u) ||
            (prefix_length < complete_length &&
             expected.chunk_state == 4u)) {
            fprintf(stderr,
                "chunk truncation boundary has wrong state: case=%s "
                "prefix=%zu/%zu accepted=%u state=%u\n", name,
                prefix_length, complete_length, expected.accepted,
                expected.chunk_state);
            abort();
        }
        for (fragment_size = 2u; fragment_size <= prefix_length;
             fragment_size += 1u) {
            struct cxnet_http_chunk_fuzz_result actual;
            (void)cxnet_http_fuzz_chunk_input(complete, prefix_length,
                fragment_size, &actual);
            if (memcmp(&expected, &actual, sizeof(expected)) != 0) {
                fprintf(stderr,
                    "chunk truncation state changed with fragment=%zu "
                    "case=%s prefix=%zu/%zu\n", fragment_size, name,
                    prefix_length, complete_length);
                abort();
            }
        }
    }
}

static void verify_chunk_truncation_boundaries(void) {
    static const uint8_t one_chunk[] =
        "4;note=token\r\npong\r\n0\r\nX-CX-Trailer: value\r\n\r\n";
    static const uint8_t multiple_chunks[] =
        "4;note=token\r\npong\r\n3;flag\r\nxyz\r\n0\r\n"
        "X-One: 1\r\nX-Two: two\r\n\r\n";
    static const uint8_t empty_body[] = "0\r\n\r\n";
    verify_chunk_truncation_case(one_chunk, sizeof(one_chunk) - 1u,
        "one chunk with extension and trailer");
    verify_chunk_truncation_case(multiple_chunks,
        sizeof(multiple_chunks) - 1u, "multiple chunks and trailers");
    verify_chunk_truncation_case(empty_body, sizeof(empty_body) - 1u,
        "empty body");
}

static void verify_chunk_line_ending_mutations(void) {
    static const size_t fragment_sizes[] = {1u, 2u, 7u, 31u, (size_t)-1};
    static const uint8_t one_chunk[] =
        "4;note=token\r\npong\r\n0\r\nX-CX-Trailer: value\r\n\r\n";
    static const uint8_t multiple_chunks[] =
        "4;note=token\r\npong\r\n3;flag\r\nxyz\r\n0\r\n"
        "X-One: 1\r\nX-Two: two\r\n\r\n";
    static const struct {
        const uint8_t *data;
        size_t length;
        const char *name;
    } cases[] = {
        {one_chunk, sizeof(one_chunk) - 1u,
            "one chunk with extension and trailer"},
        {multiple_chunks, sizeof(multiple_chunks) - 1u,
            "multiple chunks and trailers"}
    };
    size_t case_index;
    for (case_index = 0u;
         case_index < sizeof(cases) / sizeof(cases[0]); case_index += 1u) {
        size_t offset;
        for (offset = 0u; offset < cases[case_index].length; offset += 1u) {
            uint8_t *mutated;
            size_t fragment_index;
            uint8_t original = cases[case_index].data[offset];
            if (original != '\r' && original != '\n') continue;
            mutated = (uint8_t *)malloc(cases[case_index].length);
            if (mutated == NULL) abort();
            memcpy(mutated, cases[case_index].data,
                cases[case_index].length);
            mutated[offset] = original == '\r' ? '\n' : '\r';
            for (fragment_index = 0u;
                 fragment_index < sizeof(fragment_sizes) /
                     sizeof(fragment_sizes[0]); fragment_index += 1u) {
                struct cxnet_http_chunk_fuzz_result result;
                (void)cxnet_http_fuzz_chunk_input(mutated,
                    cases[case_index].length,
                    fragment_sizes[fragment_index], &result);
                if (result.accepted) {
                    fprintf(stderr,
                        "chunk line-ending mutation accepted: case=%s "
                        "offset=%zu original=%02x fragment=%zu\n",
                        cases[case_index].name, offset, original,
                        fragment_sizes[fragment_index]);
                    abort();
                }
            }
            free(mutated);
        }
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    static const size_t fragment_sizes[] = {1u, 2u, 7u, 31u, (size_t)-1};
    static int trailer_limit_checked;
    struct cxnet_http_chunk_fuzz_result expected;
    struct cxnet_http_chunk_fuzz_result actual;
    size_t index;

    if (!trailer_limit_checked) {
        trailer_limit_checked = 1;
        verify_chunk_extension_cases();
        verify_chunk_line_limit();
        verify_chunk_trailer_limit();
        verify_chunk_truncation_boundaries();
        verify_chunk_line_ending_mutations();
    }
    (void)cxnet_http_fuzz_chunk_input(data, size, fragment_sizes[0], &expected);
    for (index = 1u;
         index < sizeof(fragment_sizes) / sizeof(fragment_sizes[0]);
         index += 1u) {
        (void)cxnet_http_fuzz_chunk_input(data, size,
            fragment_sizes[index], &actual);
        if (memcmp(&expected, &actual, sizeof(expected)) != 0) {
            fprintf(stderr,
                "chunk parser changed with fragment=%zu input=%zu: "
                "accepted=%u/%u state=%u/%u body=%zu/%zu "
                "remaining=%zu/%zu size=%zu/%zu line=%zu/%zu "
                "trailer=%zu/%zu name=%zu/%zu hash=%08x/%08x\n",
                fragment_sizes[index], size,
                expected.accepted, actual.accepted,
                expected.chunk_state, actual.chunk_state,
                expected.body_length, actual.body_length,
                expected.chunk_remaining, actual.chunk_remaining,
                expected.chunk_size_value, actual.chunk_size_value,
                expected.chunk_line_bytes, actual.chunk_line_bytes,
                expected.chunk_trailer_bytes, actual.chunk_trailer_bytes,
                expected.chunk_trailer_name_length,
                actual.chunk_trailer_name_length,
                expected.body_hash, actual.body_hash);
            abort();
        }
    }
    return 0;
}

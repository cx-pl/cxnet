#ifndef __CXNET_SOCKET_TEST_RESOLVER_INTERNAL_H__
#define __CXNET_SOCKET_TEST_RESOLVER_INTERNAL_H__

#include <stdint.h>

struct cxnet_resolved_address {
    uint32_t address_family;
    uint32_t word0;
    uint32_t word1;
    uint32_t word2;
    uint32_t word3;
    uint32_t scope_id;
};

#ifdef CXNET_TESTING
#if defined(_WIN32) && defined(CX_CXNET_BUILD)
#define CXNET_TEST_RESOLVER_API __declspec(dllexport)
#elif defined(_WIN32) && !defined(CX_STATIC_LINK)
#define CXNET_TEST_RESOLVER_API __declspec(dllimport)
#else
#define CXNET_TEST_RESOLVER_API
#endif

/* Register before starting lookups. Return nonzero to handle the host and set
   error_code; return zero to continue with the native resolver. */
typedef int (*cxnet_test_resolver_callback)(const void *host,
    struct cxnet_resolved_address *addresses, uint32_t capacity,
    uint32_t *count, int *error_code, void *context);

CXNET_TEST_RESOLVER_API void cxnet_test_resolver_set_callback(
    cxnet_test_resolver_callback callback,
    void *context);
CXNET_TEST_RESOLVER_API int cxnet_test_resolver_try_resolve(const void *host,
    struct cxnet_resolved_address *addresses, uint32_t capacity,
    uint32_t *count, int *error_code);
#endif

#endif

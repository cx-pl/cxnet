#include "SocketScheduler.internal.h"
#include <string.h>

#ifdef CXNET_TESTING
static cxnet_test_resolver_callback cxnet_test_resolver_callback_value;
static void *cxnet_test_resolver_context;

void cxnet_test_resolver_set_callback(cxnet_test_resolver_callback callback,
    void *context) {
    cxnet_test_resolver_context = context;
    cxnet_test_resolver_callback_value = callback;
}

int cxnet_test_resolver_try_resolve(const void *host,
    struct cxnet_resolved_address *addresses, uint32_t capacity,
    uint32_t *count, int *error_code) {
    cxnet_test_resolver_callback callback = cxnet_test_resolver_callback_value;
    if (callback == NULL) return 0;
    return callback(host, addresses, capacity, count, error_code,
        cxnet_test_resolver_context);
}
#endif

uint64_t cxnet_socket_post_on_readiness(uintptr_t handle, int writable,
    cx_action continuation, cx_action shutdown_continuation) {
    return cxnet_socket_post_on_readiness_mask(handle,
        writable ? CXNET_SOCKET_WRITABLE : CXNET_SOCKET_READABLE,
        NULL, continuation, shutdown_continuation);
}

uint64_t cxnet_socket_post_on_readiness_mask(uintptr_t handle,
    uint32_t interest_mask, uint32_t *ready_mask, cx_action continuation,
    cx_action shutdown_continuation) {
    if (interest_mask == 0u ||
        (interest_mask & ~(CXNET_SOCKET_READABLE | CXNET_SOCKET_WRITABLE)) != 0u) {
        return 0;
    }
    struct cxnet_socket_wait *wait = cxcore_System_Memory_Alloc((cx_uint)sizeof(*wait));
    memset(wait, 0, sizeof(*wait));
    wait->handle = handle;
    wait->interest_mask = interest_mask;
    wait->ready_mask = ready_mask;
    if (ready_mask != NULL) *ready_mask = 0u;
    return cx_task_scheduler_post_on_event(wait, cxnet_platform_wait_sockets,
        continuation, shutdown_continuation);
}

int cxnet_socket_cancel_wait(uint64_t registration_id) {
    return cx_task_scheduler_cancel_event_wait(registration_id);
}


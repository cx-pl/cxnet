#ifndef __CXNET_SOCKET_SCHEDULER_INTERNAL_H__
#define __CXNET_SOCKET_SCHEDULER_INTERNAL_H__

#include "../../include/cxnet_socket_runtime.h"
#include "SocketTestResolver.internal.h"

struct cxnet_socket_wait {
    uintptr_t handle;
    int writable;
    uint32_t interest_mask;
    uint32_t *ready_mask;
};

struct cxnet_socket_address {
    uint32_t address_family;
    uint32_t word0;
    uint32_t word1;
    uint32_t word2;
    uint32_t word3;
    uint32_t scope_id;
    uint32_t port;
};

int cxnet_platform_wait_sockets(struct cx_scheduler_wait_entry *entries,
    size_t count, uint32_t timeout_milliseconds);
int cxnet_platform_socket_get_address(uintptr_t handle, int remote,
    struct cxnet_socket_address *address);

#endif // __CXNET_SOCKET_SCHEDULER_INTERNAL_H__

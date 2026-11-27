#ifndef __CXNET_SOCKET_RUNTIME_H__
#define __CXNET_SOCKET_RUNTIME_H__

#include "cxcore.h"

#if defined(CX_CXNET_BUILD)
#define CX_CXNET_RUNTIME_API CX_EXPORT
#elif defined(CX_STATIC_LINK)
#define CX_CXNET_RUNTIME_API
#else
#define CX_CXNET_RUNTIME_API CX_IMPORT
#endif

CX_CXNET_RUNTIME_API uint64_t cxnet_socket_post_on_readiness(uintptr_t handle,
    int writable, cx_action continuation, cx_action shutdown_continuation);
// Interest bits can be combined and replaced by canceling the old registration
// before posting the updated mask. The registration completes only once.
#define CXNET_SOCKET_READABLE 1u
#define CXNET_SOCKET_WRITABLE 2u
#define CXNET_SOCKET_ERROR 4u
// ready_mask is written by the native wait provider before continuation runs.
// Input interest accepts READABLE and/or WRITABLE; output may also include ERROR.
CX_CXNET_RUNTIME_API uint64_t cxnet_socket_post_on_readiness_mask(
    uintptr_t handle, uint32_t interest_mask, uint32_t *ready_mask,
    cx_action continuation, cx_action shutdown_continuation);
CX_CXNET_RUNTIME_API int cxnet_socket_cancel_wait(uint64_t registration_id);

extern struct cxcore_System_Threading_Tasks_TaskHandle *
cxnet_System_Net_Sockets_SocketRuntime_ReadAsync(
    uint64_t handle,
    struct cxcore_System_Collections_Array *buffer,
    cx_uint offset,
    cx_uint count,
    struct cxcore_System_Threading_Tasks_CancellationToken token);
extern struct cxcore_System_Threading_Tasks_TaskHandle *
cxnet_System_Net_Sockets_SocketRuntime_WriteAsync(
    uint64_t handle,
    struct cxcore_System_Collections_Array *buffer,
    cx_uint offset,
    cx_uint count,
    struct cxcore_System_Threading_Tasks_CancellationToken token);
extern cx_uint cxnet_System_Net_Sockets_SocketRuntime_GetFailureKind(
    cx_ptr operation);
extern cx_uint cxnet_System_Net_Sockets_SocketRuntime_GetNativeErrorDomain(
    cx_ptr operation);
extern cx_int cxnet_System_Net_Sockets_SocketRuntime_GetNativeErrorCode(
    cx_ptr operation);
extern cx_int cxnet_System_Net_Sockets_SocketRuntime_SetNonBlocking(
    uint64_t handle);
extern cx_uint cxnet_System_Net_Sockets_SocketRuntime_GetPlatformErrorDomain(void);
extern int cxnet_System_Net_Sockets_SocketRuntime_Close(
    uint64_t handle);

extern cx_ptr cxnet_System_Net_Sockets_SocketRuntime_BeginTransfer(
    uint64_t handle,
    struct cxcore_System_Collections_Array *buffer,
    cx_uint offset,
    cx_uint count,
    struct cxcore_System_Threading_Tasks_CancellationToken token,
    cx_bool writable,
    cx_bool single_completion);
extern void cxnet_System_Net_Sockets_SocketRuntime_StartOperation(
    cx_ptr operation,
    cx_action completed,
    cx_action faulted,
    cx_action canceled);
extern cx_uint cxnet_System_Net_Sockets_SocketRuntime_GetTransferred(
    cx_ptr operation);

#ifdef CXNET_TESTING
cx_ptr cxnet_socket_test_get_pending_operation(uint64_t handle);
void cxnet_socket_test_invoke_late_readiness(
    cx_ptr operation, uint64_t replacement_handle);
#endif
#endif // __CXNET_SOCKET_RUNTIME_H__

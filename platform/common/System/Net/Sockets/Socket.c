#include "cxcore.h"
#include "cxruntime.h"
#include "../../../SocketScheduler.internal.h"
#include "../../../SocketTestAccept.internal.h"

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

extern uint64_t cx_task_scheduler_post_after_cancellable(
    uint32_t milliseconds, cx_action continuation);
extern cx_bool cx_task_scheduler_cancel_timer(uint64_t registration_id);
extern cx_bool cx_task_scheduler_should_marshal_cancellation(void);
struct cx_arena;
extern void cx_task_scheduler_post_cancellation_from_thread(
    cx_action continuation, struct cx_arena *logical_arena);

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <errno.h>
#include <pthread.h>
#include <time.h>
#endif

struct cxnet_System_Net_Sockets_NativeSocketAddress {
    cx_int ErrorCode;
    cx_int AddressFamily;
    cx_uint Word0;
    cx_uint Word1;
    cx_uint Word2;
    cx_uint Word3;
    cx_uint ScopeId;
    cx_uint Port;
};

struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651;
extern CX_CXCORE_API void cx_generic_08601b16b628724bab1c3457b8460d9a869b4993ca641dcf8d8c152437c61f62(
    struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *task,
    cx_uint result);

int cxnet_platform_socket_set_nonblocking(uintptr_t socket_handle);
int cxnet_platform_socket_create(uint32_t address_family, uintptr_t *socket_handle);
int cxnet_platform_socket_bind(
    uintptr_t socket_handle, uint32_t address_family,
    uint32_t word0, uint32_t word1, uint32_t word2, uint32_t word3,
    uint32_t scope_id, uint32_t port);
int cxnet_platform_socket_listen(uintptr_t socket_handle, int backlog);
int cxnet_platform_socket_connect(
    uintptr_t socket_handle, uint32_t address_family,
    uint32_t word0, uint32_t word1, uint32_t word2, uint32_t word3,
    uint32_t scope_id, uint32_t port);
int cxnet_platform_socket_connect_blocking(
    uintptr_t socket_handle, uint32_t address_family,
    uint32_t word0, uint32_t word1, uint32_t word2, uint32_t word3,
    uint32_t scope_id, uint32_t port);
int cxnet_platform_socket_finish_connect(uintptr_t socket_handle);
int cxnet_platform_socket_accept(uintptr_t listener, uintptr_t *accepted_handle);
int cxnet_platform_socket_accept_blocking(uintptr_t listener, uintptr_t *accepted_handle);
int64_t cxnet_platform_socket_transfer_blocking(
    uintptr_t socket_handle, void *buffer, uint32_t count, int writable,
    int *native_error_code);
int cxnet_platform_socket_shutdown(uintptr_t socket_handle, int how);
int cxnet_platform_socket_set_option(uintptr_t socket_handle, int option, int value);
int cxnet_platform_socket_get_option(uintptr_t socket_handle, int option, int *value);
cx_uint cxnet_platform_socket_error_domain(void);
int64_t cxnet_platform_socket_transfer(
    uintptr_t socket_handle,
    void *buffer,
    uint32_t count,
    int writable,
    int *native_error_code);
int cxnet_platform_socket_close(uintptr_t socket_handle);
int cxnet_platform_socket_last_error(void);
int cxnet_platform_resolve(const void *host,
    struct cxnet_resolved_address *addresses,
    uint32_t capacity, uint32_t *count);
static cx_byte *cx_socket_buffer_at(
    struct cxcore_System_Collections_Array *buffer,
    cx_uint index);

#if defined(_MSC_VER)
static __declspec(thread) int cxnet_last_socket_create_error;
#else
static _Thread_local int cxnet_last_socket_create_error;
#endif

uint64_t cxnet_System_Net_Sockets_SocketRuntime_CreateSocket(
    cx_int address_family, cx_int socket_type, cx_int protocol_type) {
    uintptr_t handle = UINTPTR_MAX;
    if (socket_type != 1 || protocol_type != 6) {
        cxnet_last_socket_create_error = 22;
        return UINT64_MAX;
    }
    int error_code = cxnet_platform_socket_create(
        (uint32_t)address_family, &handle);
    cxnet_last_socket_create_error = error_code;
    return error_code == 0 ? (uint64_t)handle : UINT64_MAX;
}

cx_int cxnet_System_Net_Sockets_SocketRuntime_ConnectStart(
    uint64_t handle, cx_int address_family,
    uint32_t word0, uint32_t word1, uint32_t word2, uint32_t word3,
    uint32_t scope_id, uint32_t port) {
    int result = cxnet_platform_socket_connect((uintptr_t)handle,
        (uint32_t)address_family, word0, word1, word2, word3, scope_id, port);
    return result == 0 ? 0 : result == 1 ? 1 : -(cx_int)result;
}

uint64_t cxnet_System_Net_Sockets_SocketRuntime_ScheduleTimer(
    uint32_t milliseconds, cx_action continuation) {
    return cx_task_scheduler_post_after_cancellable(milliseconds, continuation);
}

cx_bool cxnet_System_Net_Sockets_SocketRuntime_CancelTimer(
    uint64_t registration_id) {
    return cx_task_scheduler_cancel_timer(registration_id);
}

struct cxnet_blocking_connect_context {
    uintptr_t handle;
    uint32_t family, word0, word1, word2, word3, scope_id, port;
    int error_code;
};

static void cxnet_blocking_connect_callback(void *value) {
    struct cxnet_blocking_connect_context *operation =
        (struct cxnet_blocking_connect_context *)value;
    operation->error_code = cxnet_platform_socket_connect_blocking(
        operation->handle, operation->family, operation->word0,
        operation->word1, operation->word2, operation->word3,
        operation->scope_id, operation->port);
}

cx_int cxnet_System_Net_Sockets_SocketRuntime_ConnectBlocking(
    uint64_t handle, cx_int address_family,
    uint32_t word0, uint32_t word1, uint32_t word2, uint32_t word3,
    uint32_t scope_id, uint32_t port) {
    struct cxnet_blocking_connect_context context;
    context.handle = (uintptr_t)handle;
    context.family = (uint32_t)address_family;
    context.word0 = word0; context.word1 = word1;
    context.word2 = word2; context.word3 = word3;
    context.scope_id = scope_id; context.port = port;
    context.error_code = 0;
    cx_gc_run_blocking(cxnet_blocking_connect_callback, &context);
    return (cx_int)context.error_code;
}

cx_int cxnet_System_Net_Sockets_SocketRuntime_FinishConnect(uint64_t handle) {
    return (cx_int)cxnet_platform_socket_finish_connect((uintptr_t)handle);
}

static
#if defined(_MSC_VER)
__declspec(thread)
#else
_Thread_local
#endif
int cxnet_last_socket_accept_error;

#ifdef CXNET_TESTING
static void (*cxnet_test_accept_after_native_hook)(void);

CXNET_TEST_ACCEPT_API void cxnet_socket_test_accept_after_native_hook_set(
    void (*hook)(void)) {
    cxnet_test_accept_after_native_hook = hook;
}
#endif

uint64_t cxnet_System_Net_Sockets_SocketRuntime_Accept(uint64_t listener) {
    uintptr_t accepted = UINTPTR_MAX;
    cxnet_last_socket_accept_error = cxnet_platform_socket_accept(
        (uintptr_t)listener, &accepted);
#ifdef CXNET_TESTING
    if (cxnet_last_socket_accept_error == 0 &&
        cxnet_test_accept_after_native_hook != NULL) {
        void (*hook)(void) = cxnet_test_accept_after_native_hook;
        cxnet_test_accept_after_native_hook = NULL;
        hook();
    }
#endif
    return cxnet_last_socket_accept_error == 0
        ? (uint64_t)accepted : UINT64_MAX;
}

cx_int cxnet_System_Net_Sockets_SocketRuntime_GetLastAcceptError(void) {
    return (cx_int)cxnet_last_socket_accept_error;
}

struct cxnet_blocking_accept_context {
    uintptr_t listener;
    uintptr_t accepted;
    int error_code;
};

static void cxnet_blocking_accept_callback(void *value) {
    struct cxnet_blocking_accept_context *operation =
        (struct cxnet_blocking_accept_context *)value;
    operation->error_code = cxnet_platform_socket_accept_blocking(
        operation->listener, &operation->accepted);
}

uint64_t cxnet_System_Net_Sockets_SocketRuntime_AcceptBlocking(uint64_t listener) {
    struct cxnet_blocking_accept_context context;
    context.listener = (uintptr_t)listener;
    context.accepted = UINTPTR_MAX;
    context.error_code = 0;
    cx_gc_run_blocking(cxnet_blocking_accept_callback, &context);
    cxnet_last_socket_accept_error = context.error_code;
    return context.error_code == 0 ? (uint64_t)context.accepted : UINT64_MAX;
}

struct cxnet_System_Net_Sockets_NativeSocketAddress
cxnet_System_Net_Sockets_SocketRuntime_GetSocketAddress(
    uint64_t handle, cx_bool remote) {
    struct cxnet_socket_address native_address = {0};
    struct cxnet_System_Net_Sockets_NativeSocketAddress result = {0};
    int error_code = cxnet_platform_socket_get_address(
        (uintptr_t)handle, remote ? 1 : 0, &native_address);
    result.ErrorCode = (cx_int)error_code;
    result.AddressFamily = (cx_int)native_address.address_family;
    result.Word0 = native_address.word0;
    result.Word1 = native_address.word1;
    result.Word2 = native_address.word2;
    result.Word3 = native_address.word3;
    result.ScopeId = native_address.scope_id;
    result.Port = native_address.port;
    return result;
}

cx_int cxnet_System_Net_Sockets_SocketRuntime_GetLastCreateError(void) {
    return (cx_int)cxnet_last_socket_create_error;
}

int cxnet_System_Net_Sockets_SocketRuntime_Bind(
    uint64_t handle, cx_int address_family,
    uint32_t word0, uint32_t word1, uint32_t word2, uint32_t word3,
    uint32_t scope_id, uint32_t port) {
    return (cx_int)cxnet_platform_socket_bind((uintptr_t)handle,
        (uint32_t)address_family, word0, word1, word2, word3, scope_id, port);
}

int cxnet_System_Net_Sockets_SocketRuntime_Listen(
    uint64_t handle, cx_int backlog) {
    return (cx_int)cxnet_platform_socket_listen((uintptr_t)handle, (int)backlog);
}

int cxnet_System_Net_Sockets_SocketRuntime_Shutdown(
    uint64_t handle, cx_int how) {
    return (cx_int)cxnet_platform_socket_shutdown((uintptr_t)handle, (int)how);
}

int cxnet_System_Net_Sockets_SocketRuntime_SetOption(
    uint64_t handle, cx_int option, cx_int value) {
    return (cx_int)cxnet_platform_socket_set_option(
        (uintptr_t)handle, (int)option, (int)value);
}

cx_int cxnet_System_Net_Sockets_SocketRuntime_GetOption(
    uint64_t handle, cx_int option) {
    int value = 0;
    int error_code = cxnet_platform_socket_get_option(
        (uintptr_t)handle, (int)option, &value);
    return error_code == 0 ? (cx_int)value : -(cx_int)error_code;
}

struct cxnet_blocking_transfer_context {
    uintptr_t handle;
    void *buffer;
    uint32_t count;
    int writable;
    int64_t transferred;
    int error_code;
};

static void cxnet_blocking_transfer_callback(void *value) {
    struct cxnet_blocking_transfer_context *operation =
        (struct cxnet_blocking_transfer_context *)value;
    operation->transferred = cxnet_platform_socket_transfer_blocking(
        operation->handle, operation->buffer, operation->count,
        operation->writable, &operation->error_code);
}

int64_t cxnet_System_Net_Sockets_SocketRuntime_TransferBlocking(
    uint64_t handle,
    struct cxcore_System_Collections_Array *buffer,
    cx_uint offset,
    cx_uint count,
    cx_bool writable) {
    struct cxnet_blocking_transfer_context context;
    struct cx_gc_root_handle *root;
    cx_uint length = buffer == NULL ? 0u :
        cxcore_System_Collections_Array_Length___const_get(buffer);
    if (buffer == NULL || offset > length || count > length - offset) return -22;
    context.handle = (uintptr_t)handle;
    context.buffer = cx_socket_buffer_at(buffer, offset);
    context.count = count;
    context.writable = writable ? 1 : 0;
    context.transferred = -1;
    context.error_code = 0;
    root = cx_gc_add_root(&buffer, (cx_uint)sizeof(buffer));
    cx_gc_run_blocking(cxnet_blocking_transfer_callback, &context);
    cx_gc_remove_root(root);
    if (context.transferred < 0) return -(int64_t)context.error_code;
    return context.transferred;
}

#define CXNET_DNS_CAPACITY 64u
#define CXNET_DNS_RECORD_SIZE 24u

struct cxnet_dns_resolution_context {
    const void *host;
    struct cxnet_resolved_address addresses[CXNET_DNS_CAPACITY];
    uint32_t count;
    int error_code;
};

static void cxnet_dns_resolution_callback(void *value) {
    struct cxnet_dns_resolution_context *context =
        (struct cxnet_dns_resolution_context *)value;
    context->error_code = cxnet_platform_resolve(context->host,
        context->addresses, CXNET_DNS_CAPACITY, &context->count);
}

static void cxnet_dns_write_uint32(cx_byte *destination, uint32_t value) {
    destination[0] = (cx_byte)(value & 0xffu);
    destination[1] = (cx_byte)((value >> 8) & 0xffu);
    destination[2] = (cx_byte)((value >> 16) & 0xffu);
    destination[3] = (cx_byte)((value >> 24) & 0xffu);
}

cx_int cxnet_System_Net_Dns_ResolveNative(
    struct cxcore_System_String *host,
    struct cxcore_System_Collections_Array *output) {
    struct cxnet_dns_resolution_context context;
    struct cx_gc_root_handle *host_root;
    struct cx_gc_root_handle *output_root;
    void *native_host;
    size_t native_host_size;
    cx_uint output_length;
    cx_uint index;
    cx_byte *destination;
    if (host == NULL || output == NULL || host->_data == NULL || host->_length == 0u) {
        return -22;
    }
    if (memchr(host->_data, 0, host->_length) != NULL) return -22;
#if defined(_WIN32)
    {
        struct cxcore_System_WideString *wide =
            cxcore_System_String_ToWideString(host);
        if (wide == NULL || wide->_data == NULL || wide->_length == 0u ||
            (size_t)wide->_length > SIZE_MAX / sizeof(wchar_t) - 1u) {
            return -22;
        }
        native_host_size = ((size_t)wide->_length + 1u) * sizeof(wchar_t);
        native_host = malloc(native_host_size);
        if (native_host == NULL) return -12;
        memcpy(native_host, wide->_data, (size_t)wide->_length * sizeof(wchar_t));
        ((wchar_t *)native_host)[wide->_length] = L'\0';
    }
#else
    native_host_size = (size_t)host->_length + 1u;
    native_host = malloc(native_host_size);
    if (native_host == NULL) return -12;
    memcpy(native_host, host->_data, host->_length);
    ((char *)native_host)[host->_length] = '\0';
#endif
    output_length = cxcore_System_Collections_Array_Length___const_get(output);
    if (output_length < 4u + CXNET_DNS_CAPACITY * CXNET_DNS_RECORD_SIZE) {
        free(native_host);
        return -22;
    }
    memset(&context, 0, sizeof(context));
    context.host = native_host;
    host_root = cx_gc_add_root(&host, (cx_uint)sizeof(host));
    output_root = cx_gc_add_root(&output, (cx_uint)sizeof(output));
    cx_gc_run_blocking(cxnet_dns_resolution_callback, &context);
    cx_gc_remove_root(output_root);
    cx_gc_remove_root(host_root);
    free(native_host);
    destination = (cx_byte *)cx_array_at(output, 0u, (cx_uint)sizeof(cx_byte));
    if (context.error_code != 0) {
        cxnet_dns_write_uint32(destination, UINT32_MAX);
        cxnet_dns_write_uint32(destination + 4u,
            (uint32_t)context.error_code);
        return 0;
    }
    cxnet_dns_write_uint32(destination, context.count);
    for (index = 0u; index < context.count; ++index) {
        const struct cxnet_resolved_address *address = &context.addresses[index];
        cx_byte *record = destination + 4u + index * CXNET_DNS_RECORD_SIZE;
        cxnet_dns_write_uint32(record, address->address_family);
        cxnet_dns_write_uint32(record + 4u, address->word0);
        cxnet_dns_write_uint32(record + 8u, address->word1);
        cxnet_dns_write_uint32(record + 12u, address->word2);
        cxnet_dns_write_uint32(record + 16u, address->word3);
        cxnet_dns_write_uint32(record + 20u, address->scope_id);
    }
    return (cx_int)context.count;
}

struct cxnet_dns_async_state {
    struct cxnet_dns_async_state *self;
    void *host;
    struct cxcore_System_Collections_Array *output;
    struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *completion;
    struct cx_gc_root_handle *self_root;
    struct cx_gc_root_handle *output_root;
    struct cx_gc_root_handle *completion_root;
    // 0 = worker active, 1 = result ready for the scheduler, 2 = scheduler
    // abandoned the wait and the worker owns final cleanup.
    volatile long completed;
    uint64_t wait_registration;
};

static long cxnet_dns_async_state_load(
    struct cxnet_dns_async_state *state) {
#ifdef _WIN32
    return InterlockedCompareExchange(&state->completed, 0, 0);
#else
    return __atomic_load_n(&state->completed, __ATOMIC_ACQUIRE);
#endif
}

static int cxnet_dns_async_state_transition(
    struct cxnet_dns_async_state *state, long expected, long replacement) {
#ifdef _WIN32
    return InterlockedCompareExchange(&state->completed, replacement,
        expected) == expected;
#else
    long observed = expected;
    return __atomic_compare_exchange_n(&state->completed, &observed,
        replacement, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
#endif
}

static void cxnet_dns_async_release_state(
    struct cxnet_dns_async_state *state) {
    struct cx_gc_root_handle *completion_root = state->completion_root;
    struct cx_gc_root_handle *output_root = state->output_root;
    struct cx_gc_root_handle *self_root = state->self_root;
    void *native_host = state->host;
    state->completion = NULL;
    state->output = NULL;
    state->completion_root = NULL;
    state->output_root = NULL;
    state->self_root = NULL;
    state->host = NULL;
    state->self = NULL;
    free(native_host);
    if (completion_root != NULL) cx_gc_remove_root(completion_root);
    if (output_root != NULL) cx_gc_remove_root(output_root);
    if (self_root != NULL) cx_gc_remove_root(self_root);
}

static void cxnet_dns_write_result(
    struct cxnet_dns_async_state *state,
    int error_code,
    const struct cxnet_resolved_address *addresses,
    uint32_t count) {
    cx_byte *destination = (cx_byte *)state->output->_data;
    uint32_t index;
    if (error_code != 0) {
        cxnet_dns_write_uint32(destination, UINT32_MAX);
        cxnet_dns_write_uint32(destination + 4u, (uint32_t)error_code);
        return;
    }
    cxnet_dns_write_uint32(destination, count);
    for (index = 0u; index < count; ++index) {
        const struct cxnet_resolved_address *address = &addresses[index];
        cx_byte *record = destination + 4u + index * CXNET_DNS_RECORD_SIZE;
        cxnet_dns_write_uint32(record, address->address_family);
        cxnet_dns_write_uint32(record + 4u, address->word0);
        cxnet_dns_write_uint32(record + 8u, address->word1);
        cxnet_dns_write_uint32(record + 12u, address->word2);
        cxnet_dns_write_uint32(record + 16u, address->word3);
        cxnet_dns_write_uint32(record + 20u, address->scope_id);
    }
}

static void cxnet_dns_async_worker(void *value) {
    struct cxnet_dns_async_state *state =
        (struct cxnet_dns_async_state *)value;
    struct cxnet_resolved_address addresses[CXNET_DNS_CAPACITY];
    uint32_t count = 0u;
    int error_code;
    cx_gc_attach_thread();
    // Resolution only touches the rooted native host and stack-owned results.
    // Keep the blocking resolver outside the GC's attached-thread rendezvous
    // so scheduler shutdown and collections can make progress while DNS waits.
    cx_gc_detach_thread();
    error_code = cxnet_platform_resolve(state->host, addresses,
        CXNET_DNS_CAPACITY, &count);
    cx_gc_attach_thread();
    cxnet_dns_write_result(state, error_code, addresses, count);
    if (cxnet_dns_async_state_transition(state, 0, 1)) {
        cx_gc_detach_thread();
        return;
    }
    if (cxnet_dns_async_state_load(state) == 2) {
        cxnet_dns_async_release_state(state);
    }
    cx_gc_detach_thread();
}

static int cxnet_dns_async_wait(struct cx_scheduler_wait_entry *entries,
    size_t count, uint32_t timeout_milliseconds) {
    uint64_t waited_milliseconds = 0u;
    for (;;) {
        size_t index;
        cx_bool any_ready = CX_FALSE;
        for (index = 0u; index < count; ++index) {
            struct cxnet_dns_async_state *state =
                (struct cxnet_dns_async_state *)entries[index].context;
            entries[index].ready =
                cxnet_dns_async_state_load(state) == 1;
            if (entries[index].ready) any_ready = CX_TRUE;
        }
        if (any_ready || timeout_milliseconds == 0u) return 0;
        if (timeout_milliseconds != UINT32_MAX &&
            waited_milliseconds >= timeout_milliseconds) return 0;
#ifdef _WIN32
        Sleep(1u);
#else
        {
            struct timespec delay = {0, 1000000L};
            (void)nanosleep(&delay, NULL);
        }
#endif
        if (waited_milliseconds != UINT64_MAX) ++waited_milliseconds;
    }
}

static void cxnet_dns_async_complete(void *environment) {
    struct cxnet_dns_async_state *state =
        (struct cxnet_dns_async_state *)environment;
    struct cx_exception_frame frame;
    volatile cx_ptr failure = CX_NULL;
    if (cxnet_dns_async_state_load(state) != 1) return;
    cx_exception_push(&frame);
    if (setjmp(frame.environment) == 0) {
        cx_generic_08601b16b628724bab1c3457b8460d9a869b4993ca641dcf8d8c152437c61f62(
            state->completion, 1u);
        cx_exception_pop(&frame);
    } else {
        failure = cx_exception_current();
        cx_exception_clear();
        cx_exception_pop(&frame);
    }
    cxnet_dns_async_release_state(state);
    if (failure != CX_NULL) cx_exception_throw(failure, __FILE__, __LINE__);
}

static void cxnet_dns_async_shutdown(void *environment) {
    struct cxnet_dns_async_state *state =
        (struct cxnet_dns_async_state *)environment;
    if (cxnet_dns_async_state_transition(state, 0, 2)) return;
    if (cxnet_dns_async_state_load(state) == 1) {
        cxnet_dns_async_complete(state);
    }
}

#ifdef _WIN32
static DWORD WINAPI cxnet_dns_async_thread(void *value) {
    cxnet_dns_async_worker(value);
    return 0;
}
#else
static void *cxnet_dns_async_thread(void *value) {
    cxnet_dns_async_worker(value);
    return NULL;
}
#endif

cx_int cxnet_System_Net_Dns_ResolveNativeAsync(
    struct cxcore_System_String *host,
    struct cxcore_System_Collections_Array *output,
    struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *completion) {
    struct cxnet_dns_async_state *state;
    struct cx_exception_frame wait_frame;
    volatile cx_ptr wait_failure = CX_NULL;
    size_t host_size;
    if (host == NULL || output == NULL || completion == NULL ||
        host->_data == NULL || host->_length == 0u ||
        memchr(host->_data, 0, host->_length) != NULL) {
#ifdef _WIN32
        return WSAEINVAL;
#else
        return EINVAL;
#endif
    }
    if (cxcore_System_Collections_Array_Length___const_get(output) <
            4u + CXNET_DNS_CAPACITY * CXNET_DNS_RECORD_SIZE) {
#ifdef _WIN32
        return WSAEINVAL;
#else
        return EINVAL;
#endif
    }
    state = (struct cxnet_dns_async_state *)cxcore_System_Memory_Alloc(
        (cx_uint)sizeof(*state));
    if (state == NULL) {
#ifdef _WIN32
        return ERROR_NOT_ENOUGH_MEMORY;
#else
        return ENOMEM;
#endif
    }
    memset(state, 0, sizeof(*state));
    state->self = state;
    state->self_root = cx_gc_add_root(&state->self,
        (cx_uint)sizeof(state->self));
#ifdef _WIN32
    {
        struct cxcore_System_WideString *wide =
            cxcore_System_String_ToWideString(host);
        if (wide == NULL || wide->_data == NULL || wide->_length == 0u ||
            (size_t)wide->_length > SIZE_MAX / sizeof(wchar_t) - 1u) {
            cx_gc_remove_root(state->self_root);
            state->self = NULL;
            return WSAEINVAL;
        }
        host_size = ((size_t)wide->_length + 1u) * sizeof(wchar_t);
        state->host = malloc(host_size);
        if (state->host == NULL) {
            cx_gc_remove_root(state->self_root);
            state->self = NULL;
            return ERROR_NOT_ENOUGH_MEMORY;
        }
        memcpy(state->host, wide->_data,
            (size_t)wide->_length * sizeof(wchar_t));
        ((wchar_t *)state->host)[wide->_length] = L'\0';
    }
#else
    host_size = (size_t)host->_length + 1u;
    state->host = malloc(host_size);
    if (state->host == NULL) {
        cx_gc_remove_root(state->self_root);
        state->self = NULL;
        return ENOMEM;
    }
    memcpy(state->host, host->_data, host->_length);
    ((char *)state->host)[host->_length] = '\0';
#endif
    state->output = output;
    state->completion = completion;
    state->output_root = cx_gc_add_root(&state->output,
        (cx_uint)sizeof(state->output));
    state->completion_root = cx_gc_add_root(&state->completion,
        (cx_uint)sizeof(state->completion));
    cx_exception_push(&wait_frame);
    if (setjmp(wait_frame.environment) == 0) {
    state->wait_registration = cx_task_scheduler_post_on_event(
        (cx_ptr)state, cxnet_dns_async_wait,
        (cx_action){ .__function = cxnet_dns_async_complete,
            .__environment = state, .__target = (cx_ptr)state },
        (cx_action){ .__function = cxnet_dns_async_shutdown,
            .__environment = state, .__target = (cx_ptr)state });
        cx_exception_pop(&wait_frame);
    } else {
        wait_failure = cx_exception_current();
        cx_exception_clear();
        cx_exception_pop(&wait_frame);
    }
    if (wait_failure != CX_NULL) {
        cxnet_dns_async_release_state(state);
        cx_exception_throw(wait_failure, __FILE__, __LINE__);
    }
    if (state->wait_registration == 0u) {
        cxnet_dns_async_release_state(state);
#ifdef _WIN32
        return WSAEINVAL;
#else
        return EINVAL;
#endif
    }
#ifdef _WIN32
    {
        HANDLE thread = CreateThread(NULL, 0,
            cxnet_dns_async_thread, state, 0, NULL);
        if (thread == NULL) {
            int error_code = (int)GetLastError();
            (void)cx_task_scheduler_cancel_event_wait(state->wait_registration);
            cxnet_dns_async_release_state(state);
            return error_code;
        }
        CloseHandle(thread);
    }
#else
    {
        pthread_t thread;
        int error_code = pthread_create(&thread, NULL,
            cxnet_dns_async_thread, state);
        if (error_code != 0) {
            (void)cx_task_scheduler_cancel_event_wait(state->wait_registration);
            cxnet_dns_async_release_state(state);
            return error_code;
        }
        (void)pthread_detach(thread);
    }
#endif
    return 0;
}

#include <string.h>

extern CX_CXCORE_API struct cxcore_System_Object *cxcore_System_Threading_Tasks_TaskRuntime_GetReusableSourceNativeWaitState(
        struct cxcore_System_Object *source);
extern CX_CXCORE_API void cxcore_System_Threading_Tasks_TaskRuntime_SetReusableSourceNativeWaitState(
        struct cxcore_System_Object *source,
        struct cxcore_System_Object *item);

struct cx_socket_operation {
    // A deferred close must not cancel operations created after handle reuse.
    uint64_t operation_id;
    struct cxcore_System_Collections_Array *buffer;
    struct cxcore_System_Threading_Tasks_CancellationToken token;
    struct cxcore_System_Threading_Tasks_CancellationTokenRegistration
        cancellation_registration;
    struct cxnet_socket_wait wait;
    cx_uint offset;
    cx_uint count;
    union {
        uint64_t registration_id;
        cx_ptr reusable_item;
    } readiness;
    int finished;
    int single_completion;
    int wait_only;
    int completion_kind;
    cx_uint failure_kind;
    cx_uint native_error_domain;
    cx_int native_error_code;
    cx_uint transferred;
    cx_action completed;
    struct cx_socket_operation *next;
    union {
        struct {
            cx_action faulted;
            cx_action canceled;
        } task;
        cx_uint reusable_version;
    } completion_storage;
};

#ifdef _WIN32
static volatile LONG64 cx_socket_operation_sequence;

static uint64_t cx_socket_next_operation_id(void) {
    return (uint64_t)InterlockedIncrement64(&cx_socket_operation_sequence);
}

static uint64_t cx_socket_current_operation_id(void) {
    return (uint64_t)InterlockedCompareExchange64(
        &cx_socket_operation_sequence, 0, 0);
}
#else
static uint64_t cx_socket_operation_sequence;

static uint64_t cx_socket_next_operation_id(void) {
    return __atomic_add_fetch(&cx_socket_operation_sequence, 1u,
        __ATOMIC_RELAXED);
}

static uint64_t cx_socket_current_operation_id(void) {
    return __atomic_load_n(&cx_socket_operation_sequence, __ATOMIC_RELAXED);
}
#endif

static struct cx_socket_operation *cx_socket_operations;
static cx_bool cx_socket_roots_registered;
static const struct cx_gc_root_region cx_socket_roots[] = {
    {&cx_socket_operations, (cx_uint)sizeof(cx_socket_operations)},
};

static void cx_socket_readiness_callback(void *environment);
static void cx_socket_cancel_callback(void *environment);

static cx_byte *cx_socket_buffer_at(
    struct cxcore_System_Collections_Array *buffer,
    cx_uint index) {
    return (cx_byte *)cx_array_at(buffer, index, (cx_uint)sizeof(cx_byte));
}

static void cx_socket_register_roots(void) {
    if (!cx_socket_roots_registered) {
        cx_gc_register_static_roots(
            cx_socket_roots,
            (cx_uint)(sizeof(cx_socket_roots) / sizeof(cx_socket_roots[0])));
        cx_socket_roots_registered = CX_TRUE;
    }
}

static void cx_socket_store_reference(cx_ptr *destination, cx_ptr value) {
    (void)cx_gc_store_reference(destination, value);
}

static void cx_socket_store_action(cx_action *destination, cx_action value) {
    destination->__function = value.__function;
    cx_socket_store_reference(
        (cx_ptr *)&destination->__environment, (cx_ptr)value.__environment);
    cx_socket_store_reference(
        (cx_ptr *)&destination->__target, value.__target);
    destination->__method = value.__method;
    cx_socket_store_reference(
        (cx_ptr *)&destination->__invocation_list,
        (cx_ptr)value.__invocation_list);
}

static void cx_socket_store_cancellation_token(
    struct cxcore_System_Threading_Tasks_CancellationToken *destination,
    struct cxcore_System_Threading_Tasks_CancellationToken value) {
    cx_socket_store_reference(
        (cx_ptr *)&destination->_source, (cx_ptr)value._source);
}

static void cx_socket_store_cancellation_registration(
    struct cxcore_System_Threading_Tasks_CancellationTokenRegistration *destination,
    struct cxcore_System_Threading_Tasks_CancellationTokenRegistration value) {
    cx_socket_store_reference(
        (cx_ptr *)&destination->_source, (cx_ptr)value._source);
    destination->_registrationId = value._registrationId;
}

static void cx_socket_complete_result(
    struct cx_socket_operation *operation,
    cx_uint transferred) {
    operation->completion_kind = 1;
    operation->transferred = transferred;
}

static void cx_socket_complete_fault(struct cx_socket_operation *operation) {
    operation->completion_kind = 3;
}

static void cx_socket_unlink_operation(struct cx_socket_operation *operation) {
    struct cx_socket_operation **cursor = &cx_socket_operations;
    while (*cursor != NULL) {
        if (*cursor == operation) {
            cx_socket_store_reference((cx_ptr *)cursor,
                (cx_ptr)operation->next);
            cx_socket_store_reference((cx_ptr *)&operation->next, NULL);
            break;
        }
        cursor = &(*cursor)->next;
    }
}

static void cx_socket_finish_operation(
    struct cx_socket_operation *operation,
    int completion_kind,
    cx_uint transferred) {
    if (operation == NULL || operation->finished) {
        return;
    }
    cx_action completion = {0};
    struct cx_exception_frame frame;
    volatile cx_ptr failure = CX_NULL;
    volatile int callback_failed = 0;

    operation->finished = 1;
    if (operation->single_completion && operation->completion_kind != 4 &&
        operation->readiness.reusable_item != NULL) {
        cx_task_scheduler_cancel_reusable_event_wait(
            operation->readiness.reusable_item);
        cx_socket_store_reference(
            &operation->readiness.reusable_item, NULL);
    } else if (operation->readiness.registration_id != 0) {
        cxnet_socket_cancel_wait(
            operation->readiness.registration_id);
        operation->readiness.registration_id = 0;
    }
    cx_exception_push(&frame);
    if (setjmp(frame.environment) == 0) {
        cxcore_System_Threading_Tasks_CancellationTokenRegistration_Dispose(
            &operation->cancellation_registration);
        cx_exception_pop(&frame);
    } else {
        if (failure == CX_NULL) failure = cx_exception_current();
        cx_exception_clear();
        cx_exception_pop(&frame);
        cx_socket_store_reference(
            (cx_ptr *)&operation->cancellation_registration._source,
            CX_NULL);
        operation->cancellation_registration._registrationId = 0u;
        callback_failed = 1;
    }
    if (completion_kind == 1) {
        cx_socket_complete_result(operation, transferred);
    } else if (completion_kind == 2) {
        operation->completion_kind = 2;
    } else if (completion_kind == 3) {
        cx_socket_complete_fault(operation);
    }
    cx_socket_unlink_operation(operation);
    if (operation->single_completion) {
        completion = operation->completed;
    } else if (operation->completion_kind == 1) {
        completion = operation->completed;
    } else if (operation->completion_kind == 2) {
        completion = operation->completion_storage.task.canceled;
    } else if (operation->completion_kind == 3) {
        completion = operation->completion_storage.task.faulted;
    }
    if (completion.__function != NULL) {
        cx_exception_push(&frame);
        if (setjmp(frame.environment) == 0) {
            completion.__function(completion.__environment);
            cx_exception_pop(&frame);
        } else {
            if (failure == CX_NULL) failure = cx_exception_current();
            cx_exception_clear();
            cx_exception_pop(&frame);
            callback_failed = 1;
        }
    }
    cx_socket_store_action(&operation->completed, (cx_action){0});
    if (!operation->single_completion) {
        cx_socket_store_action(
            &operation->completion_storage.task.faulted, (cx_action){0});
        cx_socket_store_action(
            &operation->completion_storage.task.canceled, (cx_action){0});
    }
    cx_socket_store_reference((cx_ptr *)&operation->buffer, NULL);
    cx_socket_store_reference((cx_ptr *)&operation->token._source, NULL);
    if (callback_failed) {
        cx_exception_throw(failure, __FILE__, __LINE__);
    }
}

static uint64_t cx_socket_wait_for_operation(
    struct cx_socket_operation *operation,
    struct cxcore_System_Object *source) {
    cx_action continuation = {0};
    continuation.__function = cx_socket_readiness_callback;
    continuation.__environment = operation;
    continuation.__target = (cx_ptr)operation;
    cx_action shutdown_continuation = {0};
    shutdown_continuation.__function = cx_socket_cancel_callback;
    shutdown_continuation.__environment = operation;
    shutdown_continuation.__target = (cx_ptr)operation;
    if (source != NULL) {
        cx_ptr cached_item = (cx_ptr)cxcore_System_Threading_Tasks_TaskRuntime_GetReusableSourceNativeWaitState(source);
        cx_task_scheduler_post_on_event_reusable(
            &operation->wait, cxnet_platform_wait_sockets, continuation,
            shutdown_continuation, &cached_item);
        cxcore_System_Threading_Tasks_TaskRuntime_SetReusableSourceNativeWaitState(source,
                (struct cxcore_System_Object *)cached_item);
        cx_socket_store_reference(
            &operation->readiness.reusable_item, cached_item);
        return 0;
    }
    operation->readiness.registration_id = cx_task_scheduler_post_on_event(
        &operation->wait, cxnet_platform_wait_sockets, continuation,
        shutdown_continuation);
    return operation->readiness.registration_id;
}

static void cx_socket_cancel_callback(void *environment) {
    cx_socket_finish_operation(
        (struct cx_socket_operation *)environment, 2, 0);
}

static void cx_socket_readiness_callback(void *environment) {
    struct cx_socket_operation *operation =
        (struct cx_socket_operation *)environment;
    int64_t transferred;
    int native_error_code = 0;
    if (operation == NULL || operation->finished) {
        return;
    }
    if (operation->single_completion && operation->completion_kind != 4) {
        cx_socket_store_reference(
            &operation->readiness.reusable_item, NULL);
    } else {
        operation->readiness.registration_id = 0;
    }
    if (operation->wait_only) {
        cx_socket_finish_operation(operation, 1, 1u);
        return;
    }
    transferred = cxnet_platform_socket_transfer(
        operation->wait.handle,
        cx_socket_buffer_at(operation->buffer, operation->offset),
        operation->count,
        operation->wait.writable,
        &native_error_code);
    if (transferred == -2) {
        operation->completion_kind = 4;
        operation->readiness.registration_id =
            cx_socket_wait_for_operation(operation, NULL);
        return;
    }
    if (transferred < 0) {
        operation->native_error_domain = cxnet_platform_socket_error_domain();
        operation->native_error_code = native_error_code;
        cx_socket_finish_operation(operation, 3, 0);
        return;
    }
    if ((uint64_t)transferred > (uint64_t)operation->count) {
        operation->failure_kind = 3u;
        cx_socket_finish_operation(operation, 3, 0);
        return;
    }
    cx_socket_finish_operation(operation, 1, (cx_uint)transferred);
}

#ifdef CXNET_TESTING
cx_ptr cxnet_socket_test_get_pending_operation(uint64_t handle) {
    struct cx_socket_operation *operation = cx_socket_operations;
    while (operation != NULL) {
        if (!operation->finished &&
            operation->wait.handle == (uintptr_t)handle) {
            return (cx_ptr)operation;
        }
        operation = operation->next;
    }
    return CX_NULL;
}

void cxnet_socket_test_invoke_late_readiness(
    cx_ptr operation_value, uint64_t replacement_handle) {
    struct cx_socket_operation *operation =
        (struct cx_socket_operation *)operation_value;
    if (operation == NULL) return;
    operation->wait.handle = (uintptr_t)replacement_handle;
    cx_socket_readiness_callback(operation);
}
#endif

static struct cx_socket_operation *
cx_socket_begin_transfer(
    uint64_t handle,
    struct cxcore_System_Collections_Array *buffer,
    cx_uint offset,
    cx_uint count,
    struct cxcore_System_Threading_Tasks_CancellationToken token,
    int writable,
    int single_completion) {
    struct cx_socket_operation *operation;
    size_t allocation_size = single_completion
        ? offsetof(struct cx_socket_operation, completion_storage) + sizeof(cx_uint)
        : sizeof(*operation);
    cx_socket_register_roots();
    cx_uint buffer_length = buffer == NULL ? 0 :
        cxcore_System_Collections_Array_Length___const_get(buffer);
    operation = (struct cx_socket_operation *)cxcore_System_Memory_Alloc((cx_uint)allocation_size);
    memset(operation, 0, allocation_size);
    operation->operation_id = cx_socket_next_operation_id();
    cx_socket_store_reference((cx_ptr *)&operation->buffer, (cx_ptr)buffer);
    cx_socket_store_cancellation_token(&operation->token, token);
    operation->wait.handle = (uintptr_t)handle;
    operation->offset = offset;
    operation->count = count;
    operation->wait.writable = writable;
    operation->wait.interest_mask = writable
        ? CXNET_SOCKET_WRITABLE : CXNET_SOCKET_READABLE;
    operation->single_completion = single_completion;
    cx_socket_store_reference(
        (cx_ptr *)&operation->next, (cx_ptr)cx_socket_operations);
    cx_socket_store_reference(
        (cx_ptr *)&cx_socket_operations, (cx_ptr)operation);
    if (buffer == NULL || offset > buffer_length ||
        count > buffer_length - offset) {
        operation->failure_kind = buffer == NULL ? 1u : 2u;
        cx_socket_complete_fault(operation);
    } else if (count == 0) {
        cx_socket_complete_result(operation, 0);
    }
    return operation;
}

cx_ptr cxnet_System_Net_Sockets_SocketRuntime_BeginTransfer(
    uint64_t handle,
    struct cxcore_System_Collections_Array *buffer,
    cx_uint offset,
    cx_uint count,
    struct cxcore_System_Threading_Tasks_CancellationToken token,
    cx_bool writable,
    cx_bool single_completion) {
    return (cx_ptr)cx_socket_begin_transfer(
        handle, buffer, offset, count, token, writable ? 1 : 0,
        single_completion ? 1 : 0);
}

cx_ptr cxnet_System_Net_Sockets_SocketRuntime_BeginWait(
    uint64_t handle,
    struct cxcore_System_Threading_Tasks_CancellationToken token,
    cx_bool writable) {
    struct cx_socket_operation *operation;
    cx_socket_register_roots();
    operation = (struct cx_socket_operation *)cxcore_System_Memory_Alloc(
        (cx_uint)sizeof(*operation));
    memset(operation, 0, sizeof(*operation));
    operation->operation_id = cx_socket_next_operation_id();
    cx_socket_store_cancellation_token(&operation->token, token);
    operation->wait.handle = (uintptr_t)handle;
    operation->wait.writable = writable ? 1 : 0;
    operation->wait.interest_mask = writable
        ? CXNET_SOCKET_WRITABLE : CXNET_SOCKET_READABLE;
    operation->wait_only = 1;
    cx_socket_store_reference((cx_ptr *)&operation->next, (cx_ptr)cx_socket_operations);
    cx_socket_store_reference((cx_ptr *)&cx_socket_operations, (cx_ptr)operation);
    return (cx_ptr)operation;
}

static void cx_socket_start_operation(
    struct cx_socket_operation *operation,
    cx_action completed,
    cx_action faulted,
    cx_action canceled,
    int single_completion,
    struct cxcore_System_Object *source) {
    cx_action cancellation_callback = {0};
    int64_t transferred;
    int native_error_code = 0;
    if (operation == NULL || operation->finished) {
        return;
    }
    operation->single_completion = single_completion;
    cx_socket_store_action(&operation->completed, completed);
    if (!single_completion) {
        cx_socket_store_action(
            &operation->completion_storage.task.faulted, faulted);
        cx_socket_store_action(
            &operation->completion_storage.task.canceled, canceled);
    }
    if (operation->completion_kind != 0) {
        cx_socket_finish_operation(
            operation, operation->completion_kind, operation->transferred);
        return;
    }
    if (operation->token._source != NULL &&
        cxcore_System_Threading_Tasks_CancellationToken_IsCancellationRequested___const_get(&operation->token)) {
        cx_socket_finish_operation(operation, 2, 0);
        return;
    }
    cancellation_callback.__function = cx_socket_cancel_callback;
    cancellation_callback.__environment = operation;
    cancellation_callback.__target = (cx_ptr)operation;
    cx_socket_store_cancellation_registration(
        &operation->cancellation_registration,
        cxcore_System_Threading_Tasks_CancellationToken_Register(
            &operation->token, cancellation_callback));
    if (operation->finished) {
        return;
    }
    if (operation->wait_only) {
        operation->readiness.registration_id =
            cx_socket_wait_for_operation(operation, NULL);
        return;
    }
    transferred = cxnet_platform_socket_transfer(
        operation->wait.handle,
        cx_socket_buffer_at(operation->buffer, operation->offset),
        operation->count,
        operation->wait.writable,
        &native_error_code);
    if (transferred >= 0) {
        if ((uint64_t)transferred > (uint64_t)operation->count) {
            operation->failure_kind = 3u;
            cx_socket_finish_operation(operation, 3, 0);
            return;
        }
        cx_socket_finish_operation(operation, 1, (cx_uint)transferred);
        return;
    }
    if (transferred != -2) {
        operation->native_error_domain = cxnet_platform_socket_error_domain();
        operation->native_error_code = native_error_code;
        cx_socket_finish_operation(operation, 3, 0);
        return;
    }
    operation->readiness.registration_id =
        cx_socket_wait_for_operation(operation, source);
}

void cxnet_System_Net_Sockets_SocketRuntime_StartOperation(
    cx_ptr operation_value,
    cx_action completed,
    cx_action faulted,
    cx_action canceled) {
    cx_socket_start_operation(
        (struct cx_socket_operation *)operation_value,
        completed, faulted, canceled, 0, NULL);
}

void cxnet_System_Net_Sockets_SocketRuntime_StartReusableOperation(
    cx_ptr operation_value,
    struct cxcore_System_Object *source,
    cx_uint version,
    cx_action completed) {
    struct cx_socket_operation *operation =
        (struct cx_socket_operation *)operation_value;
    if (operation == NULL) {
        return;
    }
    operation->completion_storage.reusable_version = version;
    cx_socket_start_operation(
        operation,
        completed, (cx_action){0}, (cx_action){0}, 1, source);
}

cx_uint cxnet_System_Net_Sockets_SocketRuntime_GetTransferred(
    cx_ptr operation_value) {
    struct cx_socket_operation *operation =
        (struct cx_socket_operation *)operation_value;
    return operation == NULL ? 0u : operation->transferred;
}

cx_uint cxnet_System_Net_Sockets_SocketRuntime_GetFailureKind(
    cx_ptr operation_value) {
    struct cx_socket_operation *operation =
        (struct cx_socket_operation *)operation_value;
    return operation == NULL ? 0u : operation->failure_kind;
}

cx_uint cxnet_System_Net_Sockets_SocketRuntime_GetNativeErrorDomain(
    cx_ptr operation_value) {
    struct cx_socket_operation *operation =
        (struct cx_socket_operation *)operation_value;
    return operation == NULL ? 0u : operation->native_error_domain;
}

cx_int cxnet_System_Net_Sockets_SocketRuntime_GetNativeErrorCode(
    cx_ptr operation_value) {
    struct cx_socket_operation *operation =
        (struct cx_socket_operation *)operation_value;
    return operation == NULL ? 0 : operation->native_error_code;
}

cx_uint cxnet_System_Net_Sockets_SocketRuntime_GetCompletionKind(
    cx_ptr operation_value) {
    struct cx_socket_operation *operation =
        (struct cx_socket_operation *)operation_value;
    return operation == NULL ? 3u : (cx_uint)operation->completion_kind;
}

cx_uint cxnet_System_Net_Sockets_SocketRuntime_GetCompletionVersion(
    cx_ptr operation_value) {
    struct cx_socket_operation *operation =
        (struct cx_socket_operation *)operation_value;
    return operation == NULL ? 0u : operation->completion_storage.reusable_version;
}

cx_int cxnet_System_Net_Sockets_SocketRuntime_SetNonBlocking(
    uint64_t handle) {
    return (cx_int)cxnet_platform_socket_set_nonblocking((uintptr_t)handle);
}

cx_uint cxnet_System_Net_Sockets_SocketRuntime_GetPlatformErrorDomain(void) {
    return cxnet_platform_socket_error_domain();
}

#ifdef CXNET_TESTING
static cx_bool cxnet_socket_fail_next_close;

CX_CXNET_RUNTIME_API void cxnet_System_Net_Sockets_SocketRuntime_TestFailNextClose(void) {
    cxnet_socket_fail_next_close = 1;
}
#endif

#if defined(_MSC_VER)
#define CXNET_SOCKET_NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#define CXNET_SOCKET_NOINLINE __attribute__((noinline))
#else
#define CXNET_SOCKET_NOINLINE
#endif

static CXNET_SOCKET_NOINLINE cx_ptr cxnet_socket_finish_operation_capturing_failure(
    struct cx_socket_operation *operation) {
    struct cx_exception_frame frame;
    volatile cx_ptr failure = CX_NULL;
    cx_exception_push(&frame);
    if (setjmp(frame.environment) == 0) {
        cx_socket_finish_operation(operation, 2, 0);
        cx_exception_pop(&frame);
    } else {
        failure = cx_exception_current();
        cx_exception_clear();
        cx_exception_pop(&frame);
    }
    return (cx_ptr)failure;
}

#undef CXNET_SOCKET_NOINLINE

struct cx_socket_close_request {
    uintptr_t handle;
    uint64_t max_operation_id;
};

static void cx_socket_cancel_closed_handle(void *environment) {
    struct cx_socket_close_request *request =
        (struct cx_socket_close_request *)environment;
    struct cx_socket_operation *operation = cx_socket_operations;
    cx_ptr failure = CX_NULL;
    while (operation != NULL) {
        struct cx_socket_operation *next = operation->next;
        if (operation->wait.handle == request->handle &&
            operation->operation_id <= request->max_operation_id) {
            cx_ptr operation_failure =
                cxnet_socket_finish_operation_capturing_failure(operation);
            if (failure == CX_NULL) failure = operation_failure;
        }
        operation = next;
    }
    if (failure != CX_NULL) {
        cx_exception_throw(failure, __FILE__, __LINE__);
    }
}

int cxnet_System_Net_Sockets_SocketRuntime_Close(uint64_t handle) {
    volatile cx_ptr failure = CX_NULL;
    int close_succeeded;
    int close_error;

    if (cx_task_scheduler_should_marshal_cancellation()) {
        {
            struct cx_exception_frame frame;
            cx_exception_push(&frame);
            if (setjmp(frame.environment) == 0) {
                struct cx_socket_close_request *request =
                    (struct cx_socket_close_request *)
                        cxcore_System_Memory_Alloc((cx_uint)sizeof(*request));
                cx_action continuation = {0};
                request->handle = (uintptr_t)handle;
                request->max_operation_id = cx_socket_current_operation_id();
                continuation.__function = cx_socket_cancel_closed_handle;
                continuation.__environment = request;
                continuation.__target = (cx_ptr)request;
                cx_task_scheduler_post_cancellation_from_thread(
                    continuation, NULL);
                cx_exception_pop(&frame);
            } else {
                failure = cx_exception_current();
                cx_exception_clear();
                cx_exception_pop(&frame);
            }
        }
    } else {
        struct cx_socket_operation *operation = cx_socket_operations;
        while (operation != NULL) {
            struct cx_socket_operation *next = operation->next;
            if (operation->wait.handle == (uintptr_t)handle) {
                cx_ptr operation_failure =
                    cxnet_socket_finish_operation_capturing_failure(operation);
                if (failure == CX_NULL) failure = operation_failure;
            }
            operation = next;
        }
    }
    close_succeeded = cxnet_platform_socket_close((uintptr_t)handle);
    close_error = close_succeeded ? 0 : cxnet_platform_socket_last_error();
#ifdef CXNET_TESTING
    if (cxnet_socket_fail_next_close) {
        cxnet_socket_fail_next_close = 0;
        close_error =
#ifdef _WIN32
            10014;
#else
            5;
#endif
    }
#endif
    if (failure != CX_NULL) {
        cx_exception_throw((cx_ptr)failure, __FILE__, __LINE__);
    }
    return close_error;
}

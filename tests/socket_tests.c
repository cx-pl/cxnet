#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cxcore.h"
#include "cxruntime.h"
#include "cxnet_socket_runtime.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
static SOCKET sockets[2] = { INVALID_SOCKET, INVALID_SOCKET };
static int create_socket_pair(void) {
    WSADATA data;
    SOCKET listener;
    struct sockaddr_in address;
    int address_length = (int)sizeof(address);
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return 0;
    listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET) return 0;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(listener, (const struct sockaddr *)&address, sizeof(address)) != 0 ||
        listen(listener, 1) != 0 ||
        getsockname(listener, (struct sockaddr *)&address, &address_length) != 0) {
        closesocket(listener);
        return 0;
    }
    sockets[0] = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockets[0] == INVALID_SOCKET ||
        connect(sockets[0], (const struct sockaddr *)&address, sizeof(address)) != 0) {
        closesocket(listener);
        return 0;
    }
    sockets[1] = accept(listener, NULL, NULL);
    closesocket(listener);
    return sockets[1] != INVALID_SOCKET;
}
static int create_reset_pair(void) { return create_socket_pair(); }
static void close_pair(void) {
    if (sockets[0] != INVALID_SOCKET) closesocket(sockets[0]);
    if (sockets[1] != INVALID_SOCKET) closesocket(sockets[1]);
    WSACleanup();
}
static int send_bytes(const void *buffer, int count) {
    return send(sockets[1], (const char *)buffer, count, 0);
}
static int send_primary(const void *buffer, int count) {
    return send(sockets[0], (const char *)buffer, count, 0);
}
static int receive_peer(void *buffer, int count) {
    return recv(sockets[1], (char *)buffer, count, 0);
}
static int set_peer_nonblocking(void) {
    u_long enabled = 1;
    return ioctlsocket(sockets[1], FIONBIO, &enabled) == 0;
}
static void close_peer(void) {
    if (sockets[1] != INVALID_SOCKET) {
        closesocket(sockets[1]);
        sockets[1] = INVALID_SOCKET;
    }
}
static void reset_peer(void) {
    struct linger reset = {1, 0};
    if (sockets[1] != INVALID_SOCKET) {
        (void)setsockopt(sockets[1], SOL_SOCKET, SO_LINGER,
            (const char *)&reset, (int)sizeof(reset));
        closesocket(sockets[1]);
        sockets[1] = INVALID_SOCKET;
    }
}
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
static int sockets[2] = {-1, -1};
static int create_socket_pair(void) {
    return socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0;
}
static int create_reset_pair(void) {
    int listener;
    struct sockaddr_in address;
    socklen_t address_length = (socklen_t)sizeof(address);
    listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) return 0;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(listener, (const struct sockaddr *)&address, sizeof(address)) != 0 ||
        listen(listener, 1) != 0 ||
        getsockname(listener, (struct sockaddr *)&address, &address_length) != 0) {
        close(listener);
        return 0;
    }
    sockets[0] = socket(AF_INET, SOCK_STREAM, 0);
    if (sockets[0] < 0 || connect(sockets[0], (const struct sockaddr *)&address,
            sizeof(address)) != 0) {
        if (sockets[0] >= 0) close(sockets[0]);
        sockets[0] = -1;
        close(listener);
        return 0;
    }
    sockets[1] = accept(listener, NULL, NULL);
    close(listener);
    if (sockets[1] < 0) {
        close(sockets[0]);
        sockets[0] = -1;
        return 0;
    }
    return 1;
}
static void close_pair(void) {
    if (sockets[0] >= 0) close(sockets[0]);
    if (sockets[1] >= 0) close(sockets[1]);
}
static int send_bytes(const void *buffer, int count) {
    return (int)send(sockets[1], buffer, (size_t)count, 0);
}
static int send_primary(const void *buffer, int count) {
    return (int)send(sockets[0], buffer, (size_t)count, 0);
}
static int receive_peer(void *buffer, int count) {
    return (int)recv(sockets[1], buffer, (size_t)count, 0);
}
static int set_peer_nonblocking(void) {
    int flags = fcntl(sockets[1], F_GETFL, 0);
    return flags >= 0 && fcntl(sockets[1], F_SETFL, flags | O_NONBLOCK) == 0;
}
static void close_peer(void) {
    if (sockets[1] >= 0) {
        close(sockets[1]);
        sockets[1] = -1;
    }
}
static void reset_peer(void) {
    struct linger reset = {1, 0};
    if (sockets[1] >= 0) {
        (void)setsockopt(sockets[1], SOL_SOCKET, SO_LINGER,
            &reset, (socklen_t)sizeof(reset));
        close(sockets[1]);
        sockets[1] = -1;
    }
}
#endif

struct cxnet_buffer_observer {
    struct cxcore_System_Collections_Array *buffer;
};
static struct cxnet_buffer_observer *unmanaged_buffer_observer;
static cx_uint late_read_first_continuations;
static cx_uint late_read_canceled_continuations;
static cx_uint late_read_closed_continuations;
static cx_uint observed_failure_kind;
static cx_uint observed_failure_domain;
static cx_int observed_failure_code;
static struct cxcore_System_Threading_Tasks_CancellationTokenSource
    *cancel_during_callback_source;
static cx_uint cancel_during_callback_count;

static void late_read_first_completed(void *environment) {
    (void)environment;
    ++late_read_first_continuations;
}

static void late_read_canceled_completed(void *environment) {
    (void)environment;
    ++late_read_canceled_continuations;
}

static void late_read_closed_completed(void *environment) {
    (void)environment;
    ++late_read_closed_continuations;
}

static void cancel_source_during_task_callback(void *environment) {
    (void)environment;
    ++cancel_during_callback_count;
    cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(
        cancel_during_callback_source);
}

static void run_task_callback_during_source_cancel(void *environment) {
    (void)environment;
    if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
        ++cancel_during_callback_count;
    }
}

static struct cxcore_System_Collections_Array *new_buffer(cx_uint length) {
    return (struct cxcore_System_Collections_Array *)cx_array_new(
        length, (cx_uint)sizeof(cx_byte));
}

struct cx_socket_test_task {
    struct cxcore_System_Threading_Tasks_TaskCompletionSourceVoid *completion;
    struct cxcore_System_Threading_Tasks_TaskHandle *task;
    cx_uint transferred;
    cx_ptr operation;
};

static void cx_socket_test_complete(void *environment) {
    struct cx_socket_test_task *test_task =
        (struct cx_socket_test_task *)environment;
    test_task->transferred = cxnet_System_Net_Sockets_SocketRuntime_GetTransferred(
            test_task->operation);
    cxcore_System_Threading_Tasks_TaskHandle_SetResult(
        test_task->task);
    cx_gc_store_reference((cx_ptr *)&test_task->task->_state->_value,
                          test_task);
}

static void cx_socket_test_fault(void *environment) {
    struct cx_socket_test_task *test_task =
        (struct cx_socket_test_task *)environment;
    struct cxcore_System_Exception *exception =
        (struct cxcore_System_Exception *)cxcore_System_Memory_Alloc(
            (cx_uint)sizeof(*exception));
    observed_failure_kind =
        cxnet_System_Net_Sockets_SocketRuntime_GetFailureKind(
            test_task->operation);
    observed_failure_domain =
        cxnet_System_Net_Sockets_SocketRuntime_GetNativeErrorDomain(
            test_task->operation);
    observed_failure_code =
        cxnet_System_Net_Sockets_SocketRuntime_GetNativeErrorCode(
            test_task->operation);
    cxcore_System_Exception___constructor__3(exception);
    cxcore_System_Threading_Tasks_TaskHandle_SetFault(
        test_task->task, exception);
}

static void cx_socket_test_cancel(void *environment) {
    struct cx_socket_test_task *test_task =
        (struct cx_socket_test_task *)environment;
    cxcore_System_Threading_Tasks_TaskHandle_SetCanceled(
        test_task->task);
}

static struct cxcore_System_Threading_Tasks_TaskHandle *
cx_socket_test_begin_transfer(
    uint64_t handle,
    struct cxcore_System_Collections_Array *buffer,
    cx_uint offset,
    cx_uint count,
    struct cxcore_System_Threading_Tasks_CancellationToken token,
    cx_bool writable) {
    struct cx_socket_test_task *test_task =
        (struct cx_socket_test_task *)cxcore_System_Memory_Alloc(
            (cx_uint)sizeof(*test_task));
    cx_action completed = {0};
    cx_action faulted = {0};
    cx_action canceled = {0};
    memset(test_task, 0, sizeof(*test_task));
    test_task->completion =
        (struct cxcore_System_Threading_Tasks_TaskCompletionSourceVoid *)
            cxcore_System_Memory_Alloc(
                (cx_uint)sizeof(*test_task->completion));
    cxcore_System_Threading_Tasks_TaskCompletionSourceVoid___constructor(
        test_task->completion);
    test_task->task =
        cxcore_System_Threading_Tasks_TaskCompletionSourceVoid_Task___const_get(
            test_task->completion);
    test_task->operation = cxnet_System_Net_Sockets_SocketRuntime_BeginTransfer(
            handle, buffer, offset, count, token, writable, CX_FALSE);
    completed.__function = cx_socket_test_complete;
    completed.__environment = test_task;
    completed.__target = (cx_ptr)test_task;
    faulted.__function = cx_socket_test_fault;
    faulted.__environment = test_task;
    faulted.__target = (cx_ptr)test_task;
    canceled.__function = cx_socket_test_cancel;
    canceled.__environment = test_task;
    canceled.__target = (cx_ptr)test_task;
    cxnet_System_Net_Sockets_SocketRuntime_StartOperation(
        test_task->operation, completed, faulted, canceled);
    return test_task->task;
}

struct cxcore_System_Threading_Tasks_TaskHandle *
cxnet_System_Net_Sockets_SocketRuntime_ReadAsync(
    uint64_t handle,
    struct cxcore_System_Collections_Array *buffer,
    cx_uint offset,
    cx_uint count,
    struct cxcore_System_Threading_Tasks_CancellationToken token) {
    return cx_socket_test_begin_transfer(
        handle, buffer, offset, count, token, CX_FALSE);
}

struct cxcore_System_Threading_Tasks_TaskHandle *
cxnet_System_Net_Sockets_SocketRuntime_WriteAsync(
    uint64_t handle,
    struct cxcore_System_Collections_Array *buffer,
    cx_uint offset,
    cx_uint count,
    struct cxcore_System_Threading_Tasks_CancellationToken token) {
    return cx_socket_test_begin_transfer(
        handle, buffer, offset, count, token, CX_TRUE);
}

static int task_completed(struct cxcore_System_Threading_Tasks_TaskHandle *task) {
    return task != NULL && task->_state != NULL && task->_state->_isCompleted;
}

static cx_uint task_result(
    struct cxcore_System_Threading_Tasks_TaskHandle *task) {
    struct cx_socket_test_task *test_task;
    if (task == NULL || task->_state == NULL || task->_state->_value == NULL)
        return UINT32_MAX;
    test_task = (struct cx_socket_test_task *)task->_state->_value;
    return test_task->transferred;
}

static int task_canceled(
    struct cxcore_System_Threading_Tasks_TaskHandle *task) {
    return task_completed(task) && task->_state->_isCanceled;
}

static void throw_socket_completion_exception(void *environment) {
    cx_exception_throw(environment, __FILE__, __LINE__);
}

static int test_completion_callback_exception_identity(void) {
    struct cx_exception_frame frame;
    struct cxcore_System_Exception *exception =
        (struct cxcore_System_Exception *)cxcore_System_Memory_Alloc((cx_uint)sizeof(*exception));
    cx_action completed = {0};
    cx_ptr operation;
    volatile cx_bool caught = CX_FALSE;
    cxcore_System_Exception___constructor(exception, NULL);
    completed.__function = throw_socket_completion_exception;
    completed.__environment = exception;
    completed.__target = (cx_ptr)exception;
    operation = cxnet_System_Net_Sockets_SocketRuntime_BeginTransfer(
        (uint64_t)sockets[0], new_buffer(0u), 0u, 0u,
        cxcore_System_Threading_Tasks_CancellationToken_None___const_get(), CX_FALSE, CX_FALSE);
    cx_exception_push(&frame);
    if (setjmp(frame.environment) == 0) {
        cxnet_System_Net_Sockets_SocketRuntime_StartOperation(
            operation, completed, (cx_action){0}, (cx_action){0});
    } else {
        caught = cx_exception_current() == (cx_ptr)exception;
    }
    cx_exception_pop(&frame);
    cx_exception_clear();
    if (!caught) {
        fputs("socket completion callback did not preserve the thrown exception\n", stderr);
        return 0;
    }
    return 1;
}

static struct cxcore_System_Threading_Tasks_CancellationToken
create_token(struct cxcore_System_Threading_Tasks_CancellationTokenSource **source) {
    *source = (struct cxcore_System_Threading_Tasks_CancellationTokenSource *)
        cxcore_System_Memory_Alloc((cx_uint)sizeof(**source));
    cxcore_System_Threading_Tasks_CancellationTokenSource___constructor(*source);
    return cxcore_System_Threading_Tasks_CancellationTokenSource_Token___const_get(*source);
}

static int test_pending_read_and_eof(void) {
    struct cxcore_System_Collections_Array *buffer = new_buffer(9);
    struct cxcore_System_Threading_Tasks_TaskHandle *task;
    const char message[] = "hello";
    int sent;
    cx_bool completed_before;
    cx_bool scheduler_ran;
    cx_bool completed_after;
    if (cxnet_System_Net_Sockets_SocketRuntime_SetNonBlocking(
            (uint64_t)sockets[0]) != 0) return 0;
    task = cxnet_System_Net_Sockets_SocketRuntime_ReadAsync(
        (uint64_t)sockets[0], buffer, 1u, 8u,
        cxcore_System_Threading_Tasks_CancellationToken_None___const_get());
    completed_before = task_completed(task);
    sent = send_bytes(message, 5);
    scheduler_ran = cxcore_System_Threading_Tasks_TaskScheduler_RunOne();
    completed_after = task_completed(task);
    if (completed_before || sent != 5 || !scheduler_ran ||
        !completed_after || task_result(task) != 5u ||
        memcmp(cx_array_at(buffer, 1, (cx_uint)sizeof(cx_byte)), message, 5) != 0) {
        fprintf(stderr,
            "pending socket read mismatch (initial=%d, sent=%d, scheduler=%d, completed=%d, result=%u, data=%.*s)\n",
            completed_before, sent, scheduler_ran, completed_after,
            (unsigned)task_result(task), 5,
            (const char *)cx_array_at(buffer, 1, (cx_uint)sizeof(cx_byte)));
        return 0;
    }
    close_peer();
    task = cxnet_System_Net_Sockets_SocketRuntime_ReadAsync(
        (uint64_t)sockets[0], buffer, 0u, 8u,
        cxcore_System_Threading_Tasks_CancellationToken_None___const_get());
    if (!task_completed(task) || task_result(task) != 0u) {
        fputs("peer EOF did not complete a socket read with zero bytes\n", stderr);
        return 0;
    }
    return 1;
}

#ifdef _MSC_VER
__declspec(noinline)
#else
__attribute__((noinline))
#endif
static struct cxcore_System_Threading_Tasks_TaskHandle *
start_read_with_unrooted_local_buffer(void) {
    struct cxcore_System_Collections_Array *buffer = new_buffer(1);
    struct cxcore_System_Threading_Tasks_TaskHandle *task;
    unmanaged_buffer_observer =
        (struct cxnet_buffer_observer *)malloc(sizeof(*unmanaged_buffer_observer));
    if (unmanaged_buffer_observer == NULL) return NULL;
    unmanaged_buffer_observer->buffer = buffer;
    task = cxnet_System_Net_Sockets_SocketRuntime_ReadAsync(
        (uint64_t)sockets[0], buffer, 0u, 1u,
        cxcore_System_Threading_Tasks_CancellationToken_None___const_get());
    buffer = NULL;
    return task;
}

#ifdef _MSC_VER
__declspec(noinline)
#else
__attribute__((noinline))
#endif
static void collect_after_clearing_stack(void) {
    volatile uintptr_t cleared[8192];
    size_t index;
    for (index = 0; index < sizeof(cleared) / sizeof(cleared[0]); ++index) {
        cleared[index] = 0;
    }
    cx_gc_collect();
}

static int test_pending_buffer_survives_collection(void) {
    struct cxcore_System_Threading_Tasks_TaskHandle *task;
    const char byte = 'g';
    if (cxnet_System_Net_Sockets_SocketRuntime_SetNonBlocking(
            (uint64_t)sockets[0]) != 0) return 0;
    task = start_read_with_unrooted_local_buffer();
    if (task == NULL || task_completed(task)) return 0;
    collect_after_clearing_stack();
    if (send_bytes(&byte, 1) != 1 ||
        !cxcore_System_Threading_Tasks_TaskScheduler_RunOne() ||
        !task_completed(task) || task_result(task) != 1u ||
        unmanaged_buffer_observer == NULL || unmanaged_buffer_observer->buffer == NULL ||
        * (cx_byte *)cx_array_at(
            unmanaged_buffer_observer->buffer, 0, (cx_uint)sizeof(cx_byte)) !=
            (cx_byte)byte) {
        fputs("pending socket operation did not keep its buffer alive across collection\n", stderr);
        return 0;
    }
    free(unmanaged_buffer_observer);
    unmanaged_buffer_observer = NULL;
    return 1;
}

static int test_immediate_write(void) {
    struct cxcore_System_Collections_Array *buffer = new_buffer(4);
    struct cxcore_System_Threading_Tasks_TaskHandle *task;
    memcpy(cx_array_at(buffer, 0, (cx_uint)sizeof(cx_byte)), "data", 4);
    if (cxnet_System_Net_Sockets_SocketRuntime_SetNonBlocking(
            (uint64_t)sockets[0]) != 0) return 0;
    task = cxnet_System_Net_Sockets_SocketRuntime_WriteAsync(
        (uint64_t)sockets[0], buffer, 0u, 4u,
        cxcore_System_Threading_Tasks_CancellationToken_None___const_get());
    if (!task_completed(task) || task_result(task) != 4u) {
        fputs("immediate socket write did not report its transferred byte count\n", stderr);
        return 0;
    }
    return 1;
}

static int test_zero_length_transfers(void) {
    struct cxcore_System_Collections_Array *buffer = new_buffer(0u);
    struct cxcore_System_Threading_Tasks_TaskHandle *read_task;
    struct cxcore_System_Threading_Tasks_TaskHandle *write_task;
    read_task = cxnet_System_Net_Sockets_SocketRuntime_ReadAsync(
        (uint64_t)sockets[0], buffer, 0u, 0u,
        cxcore_System_Threading_Tasks_CancellationToken_None___const_get());
    write_task = cxnet_System_Net_Sockets_SocketRuntime_WriteAsync(
        (uint64_t)sockets[0], buffer, 0u, 0u,
        cxcore_System_Threading_Tasks_CancellationToken_None___const_get());
    if (!task_completed(read_task) || task_result(read_task) != 0u ||
        !task_completed(write_task) || task_result(write_task) != 0u ||
        read_task->_state->_isFaulted || write_task->_state->_isFaulted ||
        task_canceled(read_task) || task_canceled(write_task)) {
        fputs("zero-length socket transfers did not complete with zero bytes\n", stderr);
        return 0;
    }
    return 1;
}

static int test_peer_reset_faults_read(void) {
    struct cxcore_System_Collections_Array *buffer = new_buffer(1u);
    struct cxcore_System_Threading_Tasks_TaskHandle *task;
    if (cxnet_System_Net_Sockets_SocketRuntime_SetNonBlocking(
            (uint64_t)sockets[0]) != 0) return 0;
    reset_peer();
    task = cxnet_System_Net_Sockets_SocketRuntime_ReadAsync(
        (uint64_t)sockets[0], buffer, 0u, 1u,
        cxcore_System_Threading_Tasks_CancellationToken_None___const_get());
    if (!task_completed(task) || !task->_state->_isFaulted ||
        task->_state->_isCanceled || observed_failure_kind != 0u ||
        observed_failure_domain == 0u || observed_failure_code == 0) {
        fputs("peer reset did not fault the socket read task\n", stderr);
        return 0;
    }
    return 1;
}

static int test_pending_read_peer_reset_faults(void) {
    struct cxcore_System_Collections_Array *buffer = new_buffer(1u);
    struct cxcore_System_Threading_Tasks_TaskHandle *task;
    cx_uint attempts;
    if (cxnet_System_Net_Sockets_SocketRuntime_SetNonBlocking(
            (uint64_t)sockets[0]) != 0) return 0;
    task = cxnet_System_Net_Sockets_SocketRuntime_ReadAsync(
        (uint64_t)sockets[0], buffer, 0u, 1u,
        cxcore_System_Threading_Tasks_CancellationToken_None___const_get());
    if (task == NULL || task_completed(task)) {
        fputs("socket read did not remain pending before peer reset\n", stderr);
        return 0;
    }
    reset_peer();
    for (attempts = 0u; attempts < 16u && !task_completed(task); ++attempts) {
        (void)cxcore_System_Threading_Tasks_TaskScheduler_RunOne();
    }
    if (!task_completed(task) || !task->_state->_isFaulted ||
        task_canceled(task) || observed_failure_kind != 0u ||
        observed_failure_domain == 0u || observed_failure_code == 0) {
        fputs("pending socket read did not fault after peer reset\n", stderr);
        return 0;
    }
    if (cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
        fputs("peer reset completed the pending read more than once\n", stderr);
        return 0;
    }
    return 1;
}

static int test_pending_write_peer_reset_faults(void) {
    struct cxcore_System_Collections_Array *buffer = new_buffer(1u);
    struct cxcore_System_Threading_Tasks_TaskHandle *task;
    char filler[4096] = {0};
    int total_filled = 0;
    cx_uint attempts;
    if (cxnet_System_Net_Sockets_SocketRuntime_SetNonBlocking(
            (uint64_t)sockets[0]) != 0) return 0;
    while (send_primary(filler, (int)sizeof(filler)) > 0) {
        total_filled += 1;
    }
    if (total_filled == 0) {
        fputs("could not saturate the socket before peer reset\n", stderr);
        return 0;
    }
    *(cx_byte *)cx_array_at(buffer, 0u, (cx_uint)sizeof(cx_byte)) =
        (cx_byte)'r';
    task = cxnet_System_Net_Sockets_SocketRuntime_WriteAsync(
        (uint64_t)sockets[0], buffer, 0u, 1u,
        cxcore_System_Threading_Tasks_CancellationToken_None___const_get());
    if (task == NULL || task_completed(task)) {
        fputs("socket write did not remain pending before peer reset\n", stderr);
        return 0;
    }
    reset_peer();
    for (attempts = 0u; attempts < 16u && !task_completed(task); ++attempts) {
        (void)cxcore_System_Threading_Tasks_TaskScheduler_RunOne();
    }
    if (!task_completed(task) || !task->_state->_isFaulted ||
        task_canceled(task) || observed_failure_kind != 0u ||
        observed_failure_domain == 0u || observed_failure_code == 0) {
        fputs("pending socket write did not fault after peer reset\n", stderr);
        return 0;
    }
    if (cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
        fputs("peer reset left duplicate socket write completion work\n", stderr);
        return 0;
    }
    return 1;
}

static int test_pending_write_buffer_survives_collection(void) {
    struct cxcore_System_Collections_Array *buffer;
    struct cxcore_System_Threading_Tasks_TaskHandle *task;
    char filler[4096] = {0};
    char drained[4096];
    int total_filled = 0;
    int received;
    if (cxnet_System_Net_Sockets_SocketRuntime_SetNonBlocking(
            (uint64_t)sockets[0]) != 0 || !set_peer_nonblocking()) return 0;
    while ((received = send_primary(filler, (int)sizeof(filler))) > 0) {
        total_filled += received;
    }
    if (total_filled == 0) {
        fputs("could not fill the nonblocking socket send buffer\n", stderr);
        return 0;
    }
    buffer = new_buffer(1u);
    *(cx_byte *)cx_array_at(buffer, 0u, (cx_uint)sizeof(cx_byte)) = (cx_byte)'w';
    unmanaged_buffer_observer =
        (struct cxnet_buffer_observer *)malloc(sizeof(*unmanaged_buffer_observer));
    if (unmanaged_buffer_observer == NULL) return 0;
    unmanaged_buffer_observer->buffer = buffer;
    task = cxnet_System_Net_Sockets_SocketRuntime_WriteAsync(
        (uint64_t)sockets[0], buffer, 0u, 1u,
        cxcore_System_Threading_Tasks_CancellationToken_None___const_get());
    if (task_completed(task)) {
        free(unmanaged_buffer_observer);
        unmanaged_buffer_observer = NULL;
        fputs("single-byte write did not remain pending on a full socket\n", stderr);
        return 0;
    }
    buffer = NULL;
    collect_after_clearing_stack();
    while (receive_peer(drained, (int)sizeof(drained)) > 0) { }
    if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne() ||
        !task_completed(task) || task->_state->_isFaulted || task_canceled(task) ||
        task_result(task) != 1u || unmanaged_buffer_observer->buffer == NULL ||
        *(cx_byte *)cx_array_at(unmanaged_buffer_observer->buffer, 0u,
            (cx_uint)sizeof(cx_byte)) != (cx_byte)'w') {
        fputs("pending socket write lost its buffer or reported an invalid count after collection\n", stderr);
        free(unmanaged_buffer_observer);
        unmanaged_buffer_observer = NULL;
        return 0;
    }
    free(unmanaged_buffer_observer);
    unmanaged_buffer_observer = NULL;
    return 1;
}

static int test_simultaneous_read_write_readiness(void) {
    struct cxcore_System_Collections_Array *read_buffer = new_buffer(1u);
    struct cxcore_System_Collections_Array *write_buffer = new_buffer(1u);
    struct cxcore_System_Threading_Tasks_TaskHandle *read_task;
    struct cxcore_System_Threading_Tasks_TaskHandle *write_task;
    char filler[4096] = {0};
    char drained[4096];
    const char incoming = 'i';
    cx_uint attempt;
    int sent;
    *(cx_byte *)cx_array_at(write_buffer, 0u, (cx_uint)sizeof(cx_byte)) = (cx_byte)'o';
    if (cxnet_System_Net_Sockets_SocketRuntime_SetNonBlocking(
            (uint64_t)sockets[0]) != 0 || !set_peer_nonblocking()) return 0;
    while (send_primary(filler, (int)sizeof(filler)) > 0) { }
    write_task = cxnet_System_Net_Sockets_SocketRuntime_WriteAsync(
        (uint64_t)sockets[0], write_buffer, 0u, 1u,
        cxcore_System_Threading_Tasks_CancellationToken_None___const_get());
    read_task = cxnet_System_Net_Sockets_SocketRuntime_ReadAsync(
        (uint64_t)sockets[0], read_buffer, 0u, 1u,
        cxcore_System_Threading_Tasks_CancellationToken_None___const_get());
    sent = send_bytes(&incoming, 1);
    while (receive_peer(drained, (int)sizeof(drained)) > 0) { }
    if (task_completed(read_task) || task_completed(write_task) || sent != 1) {
        fputs("simultaneous socket read/write operations did not both start pending\n", stderr);
        return 0;
    }
    for (attempt = 0u; attempt < 4u &&
            (!task_completed(read_task) || !task_completed(write_task)); ++attempt) {
        (void)cxcore_System_Threading_Tasks_TaskScheduler_RunOne();
    }
    if (!task_completed(read_task) || task_canceled(read_task) ||
        task_result(read_task) != 1u ||
        *(cx_byte *)cx_array_at(read_buffer, 0u, (cx_uint)sizeof(cx_byte)) !=
            (cx_byte)incoming ||
        !task_completed(write_task) || task_canceled(write_task) ||
        task_result(write_task) != 1u ||
        *(cx_byte *)cx_array_at(write_buffer, 0u, (cx_uint)sizeof(cx_byte)) !=
            (cx_byte)'o') {
        fputs("simultaneous socket read/write readiness lost a completion or byte\n", stderr);
        return 0;
    }
    return 1;
}

static int test_partial_write(void) {
    const cx_uint requested = 1024u * 1024u;
    struct cxcore_System_Collections_Array *buffer = new_buffer(requested);
    struct cxcore_System_Threading_Tasks_TaskHandle *task;
    if (cxnet_System_Net_Sockets_SocketRuntime_SetNonBlocking(
            (uint64_t)sockets[0]) != 0) return 0;
    task = cxnet_System_Net_Sockets_SocketRuntime_WriteAsync(
        (uint64_t)sockets[0], buffer, 0u, requested,
        cxcore_System_Threading_Tasks_CancellationToken_None___const_get());
    if (!task_completed(task) &&
        !cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
        fputs("pending partial-write operation did not wake for socket writability\n", stderr);
        return 0;
    }
    if (!task_completed(task) || task->_state->_isFaulted || task_canceled(task) ||
        task_result(task) == 0u || task_result(task) > requested
#ifndef _WIN32
        || task_result(task) >= requested
#endif
        ) {
        fprintf(stderr,
            "single-transfer socket write returned an invalid byte count (completed=%d, faulted=%d, canceled=%d, count=%u)\n",
            task_completed(task), task->_state->_isFaulted, task_canceled(task),
            (unsigned)task_result(task));
        return 0;
    }
    return 1;
}

static int test_invalid_range_faults(void) {
    struct cxcore_System_Collections_Array *buffer = new_buffer(4);
    struct cxcore_System_Threading_Tasks_TaskHandle *task =
        cxnet_System_Net_Sockets_SocketRuntime_ReadAsync(
            (uint64_t)sockets[0], buffer, 3u, 2u,
            cxcore_System_Threading_Tasks_CancellationToken_None___const_get());
    if (!task_completed(task) || !task->_state->_isFaulted ||
        observed_failure_kind != 2u || observed_failure_domain != 0u ||
        observed_failure_code != 0) {
        fputs("invalid socket buffer range did not fault its task\n", stderr);
        return 0;
    }
    return 1;
}

static int test_cancellation(void) {
    struct cxcore_System_Collections_Array *buffer = new_buffer(1);
    struct cxcore_System_Threading_Tasks_CancellationTokenSource *source;
    struct cxcore_System_Threading_Tasks_CancellationToken token = create_token(&source);
    struct cxcore_System_Threading_Tasks_TaskHandle *task;
    if (cxnet_System_Net_Sockets_SocketRuntime_SetNonBlocking(
            (uint64_t)sockets[0]) != 0) return 0;
    task = cxnet_System_Net_Sockets_SocketRuntime_ReadAsync(
        (uint64_t)sockets[0], buffer, 0u, 1u, token);
    if (task_completed(task)) return 0;
    cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(source);
    if (!task_canceled(task)) {
        fputs("canceling a pending socket read did not cancel its task\n", stderr);
        return 0;
    }
    return 1;
}

static int test_cancel_after_success(void) {
    struct cxcore_System_Collections_Array *buffer = new_buffer(1);
    struct cxcore_System_Threading_Tasks_CancellationTokenSource *source;
    struct cxcore_System_Threading_Tasks_CancellationToken token = create_token(&source);
    struct cxcore_System_Threading_Tasks_TaskHandle *task;
    const char byte = 's';
    cx_action continuation = {0};
    late_read_first_continuations = 0;
    if (cxnet_System_Net_Sockets_SocketRuntime_SetNonBlocking(
            (uint64_t)sockets[0]) != 0) return 0;
    task = cxnet_System_Net_Sockets_SocketRuntime_ReadAsync(
        (uint64_t)sockets[0], buffer, 0u, 1u, token);
    continuation.__function = late_read_first_completed;
    cxcore_System_Threading_Tasks_TaskState_OnCompleted(
        task->_state, continuation);
    if (task_completed(task) || send_bytes(&byte, 1) != 1 ||
        !cxcore_System_Threading_Tasks_TaskScheduler_RunOne() ||
        !task_completed(task) || task_canceled(task) || task_result(task) != 1u) {
        fputs("socket read did not complete before late cancellation\n", stderr);
        return 0;
    }
    cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(source);
    if (!task_completed(task) || task_canceled(task) || task_result(task) != 1u ||
        cxcore_System_Threading_Tasks_TaskScheduler_Run() != 1u ||
        late_read_first_continuations != 1u ||
        *(cx_byte *)cx_array_at(buffer, 0u, (cx_uint)sizeof(cx_byte)) != (cx_byte)byte) {
        fputs("late cancellation changed a successful read or delivered its continuation more than once\n", stderr);
        return 0;
    }
    return 1;
}

static int test_cancel_during_callback(void) {
    struct cxcore_System_Collections_Array *buffer = new_buffer(1);
    struct cxcore_System_Threading_Tasks_CancellationTokenSource *source;
    struct cxcore_System_Threading_Tasks_CancellationToken token = create_token(&source);
    struct cxcore_System_Threading_Tasks_TaskHandle *task;
    cx_action continuation = {0};
    cx_action nested_scheduler = {0};
    cancel_during_callback_source = source;
    cancel_during_callback_count = 0u;
    if (cxnet_System_Net_Sockets_SocketRuntime_SetNonBlocking(
            (uint64_t)sockets[0]) != 0) return 0;
    task = cxnet_System_Net_Sockets_SocketRuntime_ReadAsync(
        (uint64_t)sockets[0], buffer, 0u, 1u, token);
    if (task_completed(task)) return 0;
    continuation.__function = cancel_source_during_task_callback;
    cxcore_System_Threading_Tasks_TaskState_OnCompleted(
        task->_state, continuation);
    nested_scheduler.__function = run_task_callback_during_source_cancel;
    (void)cxcore_System_Threading_Tasks_CancellationTokenSource_Register(
        source, nested_scheduler);
    cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(source);
    cancel_during_callback_source = NULL;
    if (!task_canceled(task) || cancel_during_callback_count != 1u ||
        *(cx_byte *)cx_array_at(buffer, 0u, (cx_uint)sizeof(cx_byte)) != 0u ||
        cxcore_System_Threading_Tasks_TaskScheduler_Run() != 0u) {
        fputs("reentrant cancellation during a task callback changed completion state\n", stderr);
        return 0;
    }
    return 1;
}

static int test_canceled_queued_read_event(void) {
    struct cxcore_System_Collections_Array *first_buffer = new_buffer(1);
    struct cxcore_System_Collections_Array *canceled_buffer = new_buffer(1);
    struct cxcore_System_Collections_Array *closed_buffer = new_buffer(1);
    struct cxcore_System_Threading_Tasks_CancellationTokenSource
        *first_source;
    struct cxcore_System_Threading_Tasks_CancellationTokenSource
        *canceled_source;
    struct cxcore_System_Threading_Tasks_CancellationToken
        first_token = create_token(&first_source);
    struct cxcore_System_Threading_Tasks_CancellationToken
        canceled_token = create_token(&canceled_source);
    struct cxcore_System_Threading_Tasks_TaskHandle *first_task;
    struct cxcore_System_Threading_Tasks_TaskHandle *canceled_task;
    struct cxcore_System_Threading_Tasks_TaskHandle *closed_task;
    cx_action first_continuation = {0};
    cx_action canceled_continuation = {0};
    cx_action closed_continuation = {0};
    cx_bool scheduler_ran;
    cx_uint drained_callbacks;
    int sent;
    const char data[] = "ab";
    late_read_first_continuations = 0;
    late_read_canceled_continuations = 0;
    late_read_closed_continuations = 0;
    if (cxnet_System_Net_Sockets_SocketRuntime_SetNonBlocking(
            (uint64_t)sockets[0]) != 0) return 0;
    // Registration is newest-first in the scheduler. Queue all three events so
    // the first read completes in RunOne while the other two remain pending.
    closed_task = cxnet_System_Net_Sockets_SocketRuntime_ReadAsync(
        (uint64_t)sockets[0], closed_buffer, 0u, 1u,
        cxcore_System_Threading_Tasks_CancellationToken_None___const_get());
    canceled_task = cxnet_System_Net_Sockets_SocketRuntime_ReadAsync(
        (uint64_t)sockets[0], canceled_buffer, 0u, 1u, canceled_token);
    first_task = cxnet_System_Net_Sockets_SocketRuntime_ReadAsync(
        (uint64_t)sockets[0], first_buffer, 0u, 1u, first_token);
    if (task_completed(first_task) || task_completed(canceled_task) ||
        task_completed(closed_task)) {
        fputs("concurrent late-event reads did not remain pending\n", stderr);
        return 0;
    }
    first_continuation.__function = late_read_first_completed;
    canceled_continuation.__function = late_read_canceled_completed;
    closed_continuation.__function = late_read_closed_completed;
    cxcore_System_Threading_Tasks_TaskState_OnCompleted(
        first_task->_state, first_continuation);
    cxcore_System_Threading_Tasks_TaskState_OnCompleted(
        canceled_task->_state, canceled_continuation);
    cxcore_System_Threading_Tasks_TaskState_OnCompleted(
        closed_task->_state, closed_continuation);
    sent = send_bytes(data, 2);
    scheduler_ran = cxcore_System_Threading_Tasks_TaskScheduler_RunOne();
    if (sent != 2 || !scheduler_ran ||
        !task_completed(first_task) || task_result(first_task) != 1u ||
        task_completed(canceled_task) ||
        task_completed(closed_task) ||
        late_read_first_continuations != 0u ||
        late_read_canceled_continuations != 0u ||
        late_read_closed_continuations != 0u) {
        fprintf(stderr,
            "socket poll interleave mismatch (sent=%d, ran=%d, first_completed=%d, first_result=%u, canceled_completed=%d, closed_completed=%d, first_callbacks=%u, canceled_callbacks=%u, closed_callbacks=%u)\n",
            sent, scheduler_ran, task_completed(first_task),
            task_completed(first_task) ? (unsigned)task_result(first_task) : UINT32_MAX,
            task_completed(canceled_task), task_completed(closed_task),
            (unsigned)late_read_first_continuations,
            (unsigned)late_read_canceled_continuations,
            (unsigned)late_read_closed_continuations);
        return 0;
    }
    cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(
        canceled_source);
    if (!task_canceled(canceled_task) ||
        late_read_first_continuations != 0u ||
        late_read_canceled_continuations != 0u ||
        *(cx_byte *)cx_array_at(canceled_buffer, 0u, (cx_uint)sizeof(cx_byte)) != 0u) {
        fputs("canceling a queued socket event did not preserve the canceled task\n", stderr);
        return 0;
    }

    // Closing the socket must also cancel and remove another already-queued
    // readiness event, and must not leave its callback pointing at freed state.
    cxnet_System_Net_Sockets_SocketRuntime_Close((uint64_t)sockets[0]);
#ifdef _WIN32
    sockets[0] = INVALID_SOCKET;
#else
    sockets[0] = -1;
#endif
    if (!task_canceled(closed_task) || late_read_closed_continuations != 0u ||
        *(cx_byte *)cx_array_at(closed_buffer, 0u, (cx_uint)sizeof(cx_byte)) != 0u) {
        fputs("closing a socket with a queued readiness event did not cancel its task\n", stderr);
        return 0;
    }

    // The readiness event has already moved to the scheduler's ready queue.
    // Cancellation must remove that queued event before unlinking the operation.
    // Collect before draining to catch any stale callback environment.
    cx_gc_collect();
    drained_callbacks = cxcore_System_Threading_Tasks_TaskScheduler_Run();
    if (drained_callbacks != 3u ||
        !task_completed(first_task) || task_result(first_task) != 1u ||
        !task_canceled(canceled_task) ||
        !task_canceled(closed_task) ||
        late_read_first_continuations != 1u ||
        late_read_canceled_continuations != 1u ||
        late_read_closed_continuations != 1u ||
        *(cx_byte *)cx_array_at(canceled_buffer, 0u, (cx_uint)sizeof(cx_byte)) != 0u ||
        *(cx_byte *)cx_array_at(closed_buffer, 0u, (cx_uint)sizeof(cx_byte)) != 0u) {
        fprintf(stderr,
            "late socket-event drain mismatch (callbacks=%u, first_completed=%d, first_result=%u, canceled=%d, closed=%d, first_callbacks=%u, canceled_callbacks=%u, closed_callbacks=%u, canceled_byte=%u, closed_byte=%u)\n",
            (unsigned)drained_callbacks, task_completed(first_task),
            (unsigned)task_result(first_task), task_canceled(canceled_task),
            task_canceled(closed_task),
            (unsigned)late_read_first_continuations,
            (unsigned)late_read_canceled_continuations,
            (unsigned)late_read_closed_continuations,
            (unsigned)*(cx_byte *)cx_array_at(
                canceled_buffer, 0u, (cx_uint)sizeof(cx_byte)),
            (unsigned)*(cx_byte *)cx_array_at(
                closed_buffer, 0u, (cx_uint)sizeof(cx_byte)));
        return 0;
    }
    return 1;
}

static int test_close_pending_operation(void) {
    struct cxcore_System_Collections_Array *buffer = new_buffer(1);
    struct cxcore_System_Threading_Tasks_TaskHandle *task;
    if (cxnet_System_Net_Sockets_SocketRuntime_SetNonBlocking(
            (uint64_t)sockets[0]) != 0) return 0;
    task = cxnet_System_Net_Sockets_SocketRuntime_ReadAsync(
        (uint64_t)sockets[0], buffer, 0u, 1u,
        cxcore_System_Threading_Tasks_CancellationToken_None___const_get());
    if (task_completed(task)) return 0;
    cxnet_System_Net_Sockets_SocketRuntime_Close((uint64_t)sockets[0]);
    sockets[0] =
#ifdef _WIN32
        INVALID_SOCKET;
#else
        -1;
#endif
    if (!task_canceled(task)) {
        fputs("closing a socket did not cancel its pending operation\n", stderr);
        return 0;
    }
    return 1;
}

static int test_late_readiness_after_handle_reuse(void) {
    struct cxcore_System_Collections_Array *old_buffer = new_buffer(1);
    struct cxcore_System_Collections_Array *new_buffer_value = new_buffer(1);
    struct cxcore_System_Threading_Tasks_TaskHandle *old_task;
    struct cxcore_System_Threading_Tasks_TaskHandle *new_task;
    struct cx_gc_root_handle *old_operation_root;
    cx_ptr old_operation;
    uint64_t old_handle;
    const char byte = 'r';
    if (cxnet_System_Net_Sockets_SocketRuntime_SetNonBlocking(
            (uint64_t)sockets[0]) != 0) return 0;
    old_handle = (uint64_t)sockets[0];
    old_task = cxnet_System_Net_Sockets_SocketRuntime_ReadAsync(
        old_handle, old_buffer, 0u, 1u,
        cxcore_System_Threading_Tasks_CancellationToken_None___const_get());
    if (task_completed(old_task)) return 0;
    old_operation = cxnet_socket_test_get_pending_operation(old_handle);
    old_operation_root = cx_gc_add_root(
        &old_operation, (cx_uint)sizeof(old_operation));
    if (old_operation == CX_NULL || old_operation_root == NULL) return 0;
    cxnet_System_Net_Sockets_SocketRuntime_Close(old_handle);
#ifdef _WIN32
    sockets[0] = INVALID_SOCKET;
#else
    sockets[0] = -1;
#endif
    close_pair();
    if (!task_canceled(old_task)) {
        fputs("closing the old socket did not cancel its original operation\n", stderr);
        return 0;
    }
    if (!create_socket_pair()) return 0;
    if (cxnet_System_Net_Sockets_SocketRuntime_SetNonBlocking(
            (uint64_t)sockets[0]) != 0) return 0;
    new_task = cxnet_System_Net_Sockets_SocketRuntime_ReadAsync(
        (uint64_t)sockets[0], new_buffer_value, 0u, 1u,
        cxcore_System_Threading_Tasks_CancellationToken_None___const_get());
    if (task_completed(new_task) || send_bytes(&byte, 1) != 1) return 0;
    // Model a delayed readiness callback whose native handle value has been
    // recycled for the new registration. The test hook retargets the finished
    // operation to that handle and dispatches the old callback immediately.
    cxnet_socket_test_invoke_late_readiness(
        old_operation, (uint64_t)sockets[0]);
    if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne() ||
        !task_completed(new_task) || task_canceled(new_task) ||
        task_result(new_task) != 1u || !task_canceled(old_task) ||
        *(cx_byte *)cx_array_at(old_buffer, 0u, (cx_uint)sizeof(cx_byte)) != 0u ||
        *(cx_byte *)cx_array_at(new_buffer_value, 0u, (cx_uint)sizeof(cx_byte)) != (cx_byte)byte ||
        cxcore_System_Threading_Tasks_TaskScheduler_Run() != 0u) {
        fputs("late readiness for a closed operation affected the new registration\n", stderr);
        return 0;
    }
    cx_gc_remove_root(old_operation_root);
    return 1;
}

int main(void) {
    cx_uint iteration;
    cx_gc_check_thread();
    if (!create_socket_pair()) {
        fputs("could not create local socket pair\n", stderr);
        return 1;
    }
    if (!test_pending_read_and_eof()) return 1;
    close_pair();
    if (!create_socket_pair() || !test_immediate_write()) return 1;
    close_pair();
    if (!create_socket_pair() || !test_zero_length_transfers()) return 1;
    close_pair();
    if (!create_reset_pair() || !test_peer_reset_faults_read()) return 1;
    close_pair();
    if (!create_reset_pair() || !test_pending_read_peer_reset_faults()) return 1;
    close_pair();
    if (!create_reset_pair() || !test_pending_write_peer_reset_faults()) return 1;
    close_pair();
    if (!create_socket_pair() || !test_pending_write_buffer_survives_collection()) return 1;
    close_pair();
    if (!create_socket_pair() || !test_simultaneous_read_write_readiness()) return 1;
    close_pair();
    if (!create_socket_pair() || !test_pending_buffer_survives_collection()) return 1;
    close_pair();
    if (!create_socket_pair() || !test_partial_write()) return 1;
    close_pair();
    if (!create_socket_pair() || !test_invalid_range_faults()) return 1;
    close_pair();
    if (!create_socket_pair() || !test_completion_callback_exception_identity()) return 1;
    close_pair();
    if (!create_socket_pair() || !test_cancellation()) return 1;
    close_pair();
    if (!create_socket_pair() || !test_cancel_after_success()) return 1;
    close_pair();
    if (!create_socket_pair() || !test_cancel_during_callback()) return 1;
    close_pair();
    // Repeat the readiness/cancel/close interleave so stale queued callback
    // environments and operation-list links are exercised across many teardown
    // cycles, with a collection before every late scheduler drain.
    for (iteration = 0; iteration < 32u; ++iteration) {
        if (!create_socket_pair() || !test_canceled_queued_read_event()) return 1;
        close_pair();
    }
    if (!create_socket_pair() || !test_close_pending_operation()) return 1;
    close_pair();
    if (!create_socket_pair() || !test_late_readiness_after_handle_reuse()) return 1;
    close_pair();
    cx_gc_collect();
    return 0;
}

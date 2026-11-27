#include "cxnet.internal.h"
#include "SocketTestResolver.internal.h"
#include "SocketTestAccept.internal.h"
#include <setjmp.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sched.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#endif

extern void cxnet_System_Net_Sockets_SocketRuntime_TestFailNextClose(void);
static volatile long disposal_resolver_started;
static uint32_t reentrant_socket_dispose_count;
static uint32_t reentrant_socket_dispose_return_count;
static struct cxcore_System_Threading_Tasks_CancellationTokenSource *
    accept_after_native_cancel_source;
static uint32_t accept_after_native_hook_calls;

struct socket_cross_thread_dispose {
    struct cxnet_System_Net_Sockets_Socket *socket;
    struct cxnet_System_Net_Sockets_NetworkStream *network_stream;
    volatile int ready;
    volatile int start;
    volatile int completed;
    volatile int failed;
};

struct socket_peer_payload_drain {
    uintptr_t peer;
    size_t filled_bytes;
    size_t trailing_bytes;
    volatile int completed;
    volatile int failed;
};

struct socket_peer_cancel_race_drain {
    uintptr_t peer;
    size_t filled_bytes;
    volatile int start;
    volatile int completed;
    volatile int failed;
};

struct socket_peer_readiness_race_send {
    uintptr_t peer;
    volatile int start;
    volatile int completed;
    volatile int failed;
};

struct socket_write_cancel_race_context {
    struct cxcore_System_Threading_Tasks_CancellationTokenSource *source;
    volatile uint32_t cancellation_fired;
};

#ifdef _WIN32
static DWORD WINAPI socket_cross_thread_dispose_worker(void *argument);
#else
static void *socket_cross_thread_dispose_worker(void *argument);
#endif
static void socket_dispose_race_store(volatile int *location, int value);
static int socket_dispose_race_load(volatile int *location);
static int socket_prefill_send_buffer(uint64_t handle, uintptr_t peer_handle,
    size_t *filled_bytes);
static int stream_peer_observes_payload_then_eof(uintptr_t peer_handle,
    size_t expected_bytes);
static int socket_write_task_completed(
    const struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *generic_task,
    const struct cxcore_System_Threading_Tasks_TaskHandle *task_handle);
static int socket_write_task_faulted(
    const struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *generic_task,
    const struct cxcore_System_Threading_Tasks_TaskHandle *task_handle);
static int socket_write_task_canceled(
    const struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *generic_task,
    const struct cxcore_System_Threading_Tasks_TaskHandle *task_handle);
static struct cxcore_System_Exception *socket_write_task_exception(
    const struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *generic_task,
    const struct cxcore_System_Threading_Tasks_TaskHandle *task_handle);
#ifdef _WIN32
static DWORD WINAPI socket_peer_payload_drain_worker(void *argument);
#else
static void *socket_peer_payload_drain_worker(void *argument);
#endif
#ifdef _WIN32
static DWORD WINAPI socket_peer_cancel_race_drain_worker(void *argument);
static DWORD WINAPI socket_peer_readiness_race_send_worker(void *argument);
#else
static void *socket_peer_cancel_race_drain_worker(void *argument);
static void *socket_peer_readiness_race_send_worker(void *argument);
#endif

static int test_resolver_callback(const void *host,
    struct cxnet_resolved_address *addresses, uint32_t capacity,
    uint32_t *count, int *error_code, void *context) {
    int failure = 0;
    int delayed = 0;
    int disposal_delay = 0;
    int fallback = 0;
    (void)context;
#ifdef _WIN32
    if (wcscmp((const wchar_t *)host,
            L"cxnet-test-resolution-failure.invalid") == 0) failure = 1;
    else if (wcscmp((const wchar_t *)host,
            L"cxnet-test-resolution-delay.invalid") == 0) delayed = 1;
    else if (wcscmp((const wchar_t *)host,
            L"cxnet-test-resolution-dispose.invalid") == 0) {
        delayed = 1;
        disposal_delay = 1;
    }
    else if (wcscmp((const wchar_t *)host,
            L"cxnet-test-resolution-fallback.invalid") == 0) fallback = 1;
#else
    if (strcmp((const char *)host,
            "cxnet-test-resolution-failure.invalid") == 0) failure = 1;
    else if (strcmp((const char *)host,
            "cxnet-test-resolution-delay.invalid") == 0) delayed = 1;
    else if (strcmp((const char *)host,
            "cxnet-test-resolution-dispose.invalid") == 0) {
        delayed = 1;
        disposal_delay = 1;
    }
    else if (strcmp((const char *)host,
            "cxnet-test-resolution-fallback.invalid") == 0) fallback = 1;
#endif
    if (!failure && !delayed && !fallback) return 0;
    if (failure) {
#ifdef _WIN32
        *error_code = WSAHOST_NOT_FOUND;
#else
        *error_code = EAI_AGAIN;
#endif
        return 1;
    }
    if (delayed) {
        if (disposal_delay) {
#ifdef _WIN32
            InterlockedIncrement((volatile LONG *)&disposal_resolver_started);
#else
            __sync_add_and_fetch(&disposal_resolver_started, 1);
#endif
        }
#ifdef _WIN32
        Sleep(150u);
#else
        struct timespec delay = {0, 150000000L};
        nanosleep(&delay, NULL);
#endif
    }
    if (capacity < (fallback ? 2u : 1u)) {
#ifdef _WIN32
        *error_code = WSAEFAULT;
#else
        *error_code = EAI_OVERFLOW;
#endif
        return 1;
    }
    memset(addresses, 0, (fallback ? 2u : 1u) * sizeof(*addresses));
    addresses[0].address_family = 2u;
    addresses[0].word0 = fallback ? 0x7f000002u : 0x7f000001u;
    *count = 1u;
    if (fallback) {
        addresses[1].address_family = 2u;
        addresses[1].word0 = 0x7f000001u;
        *count = 2u;
    }
    *error_code = 0;
    return 1;
}

extern void cxcore_System_String___constructor__2(
    struct cxcore_System_String *value, cx_uint length, cx_ptr data);

static void cancel_token_on_scheduler(void *environment) {
    cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(
        (struct cxcore_System_Threading_Tasks_CancellationTokenSource *)environment);
}

static void cancel_after_native_accept(void) {
    accept_after_native_hook_calls += 1u;
    cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(
        accept_after_native_cancel_source);
}

static uint32_t connect_race_completion_count;
static uint32_t connect_race_cancel_count;
static uint32_t accept_race_completion_count;
static uint32_t accept_race_cancel_count;
static uint32_t socket_write_cancel_race_completion_count;
static uint32_t socket_read_cancel_race_completion_count;

static void socket_write_cancel_race_cancel(void *environment) {
    struct socket_write_cancel_race_context *context =
        (struct socket_write_cancel_race_context *)environment;
    cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(
        context->source);
    context->cancellation_fired += 1u;
}

static void socket_write_cancel_race_completed(void *environment) {
    (void)environment;
    socket_write_cancel_race_completion_count += 1u;
}

static void socket_read_cancel_race_completed(void *environment) {
    (void)environment;
    socket_read_cancel_race_completion_count += 1u;
}

static int create_reset_socket(
    uint64_t *connected_handle, uint64_t *reset_peer_handle) {
#ifdef _WIN32
    WSADATA startup_data;
    SOCKET listener = INVALID_SOCKET;
    SOCKET connected = INVALID_SOCKET;
    SOCKET reset_peer = INVALID_SOCKET;
    struct sockaddr_in address;
    int address_length = (int)sizeof(address);
    struct linger reset = {1, 0};
    if (WSAStartup(MAKEWORD(2, 2), &startup_data) != 0) return 0;
    listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET) goto failed;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(listener, (const struct sockaddr *)&address, sizeof(address)) != 0 ||
        listen(listener, 1) != 0 ||
        getsockname(listener, (struct sockaddr *)&address, &address_length) != 0) {
        goto failed;
    }
    connected = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (connected == INVALID_SOCKET ||
        connect(connected, (const struct sockaddr *)&address, sizeof(address)) != 0) {
        goto failed;
    }
    reset_peer = accept(listener, NULL, NULL);
    if (reset_peer == INVALID_SOCKET) goto failed;
    closesocket(listener);
    listener = INVALID_SOCKET;
    if (setsockopt(reset_peer, SOL_SOCKET, SO_LINGER, (const char *)&reset,
            (int)sizeof(reset)) != 0) goto failed;
    *reset_peer_handle = (uint64_t)(uintptr_t)reset_peer;
    *connected_handle = (uint64_t)(uintptr_t)connected;
    return 1;
failed:
    if (listener != INVALID_SOCKET) closesocket(listener);
    if (connected != INVALID_SOCKET) closesocket(connected);
    if (reset_peer != INVALID_SOCKET) closesocket(reset_peer);
    WSACleanup();
    return 0;
#else
    int listener = -1;
    int connected = -1;
    int reset_peer = -1;
    struct sockaddr_in address;
    socklen_t address_length = (socklen_t)sizeof(address);
    struct linger reset = {1, 0};
    listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener < 0) goto failed;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(listener, (const struct sockaddr *)&address, sizeof(address)) != 0 ||
        listen(listener, 1) != 0 ||
        getsockname(listener, (struct sockaddr *)&address, &address_length) != 0) {
        goto failed;
    }
    connected = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (connected < 0 || connect(connected, (const struct sockaddr *)&address,
            sizeof(address)) != 0) goto failed;
    reset_peer = accept(listener, NULL, NULL);
    if (reset_peer < 0) goto failed;
    close(listener);
    listener = -1;
    if (setsockopt(reset_peer, SOL_SOCKET, SO_LINGER, &reset,
            (socklen_t)sizeof(reset)) != 0) goto failed;
    *reset_peer_handle = (uint64_t)(uintptr_t)reset_peer;
    *connected_handle = (uint64_t)(uintptr_t)connected;
    return 1;
failed:
    if (listener >= 0) close(listener);
    if (connected >= 0) close(connected);
    if (reset_peer >= 0) close(reset_peer);
    return 0;
#endif
}

static int test_managed_read_reports_peer_reset(int use_network_stream) {
    uint64_t connected_handle = 0u;
    uint64_t reset_peer_handle = 0u;
    struct cxnet_System_Net_Sockets_Socket *socket =
        (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Sockets_Socket___typeinfo.Size);
    struct cxnet_System_Net_Sockets_NetworkStream *stream = NULL;
    struct cxcore_System_Collections_Array *buffer = NULL;
    struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *task = NULL;
    struct cxcore_System_Threading_Tasks_CancellationToken cancellation_token = {0};
    struct cx_gc_root_handle *socket_root = cx_gc_add_root(
        &socket, (cx_uint)sizeof(socket));
    struct cx_gc_root_handle *stream_root = NULL;
    struct cx_gc_root_handle *buffer_root = NULL;
    struct cx_gc_root_handle *task_root = NULL;
    uint32_t steps = 0u;
    int passed = 0;
    if (!create_reset_socket(&connected_handle, &reset_peer_handle)) {
        fputs("could not create a TCP peer-reset fixture\n", stderr);
        cx_gc_remove_root(socket_root);
        return 0;
    }
    cxnet_System_Net_Sockets_Socket___constructor(socket,
        (cx_ulong)connected_handle);
    if (use_network_stream) {
        stream = (struct cxnet_System_Net_Sockets_NetworkStream *)
            cxcore_System_Memory_Alloc((cx_uint)sizeof(*stream));
        stream_root = cx_gc_add_root(&stream, (cx_uint)sizeof(stream));
        if (stream == NULL || stream_root == NULL) {
            fputs("could not allocate a NetworkStream for peer-reset test\n",
                stderr);
            goto cleanup;
        }
        cxnet_System_Net_Sockets_NetworkStream___constructor__2(
            stream, socket, 0);
    }
    buffer = cx_array_new(1u, (cx_uint)sizeof(cx_byte));
    buffer_root = cx_gc_add_root(&buffer, (cx_uint)sizeof(buffer));
    task = use_network_stream ?
        cxnet_System_Net_Sockets_NetworkStream_ReadAsync(
            stream, buffer, 0u, 1u, cancellation_token) :
        cxnet_System_Net_Sockets_Socket_ReadAsync(socket, buffer, 0u, 1u);
    task_root = cx_gc_add_root(&task, (cx_uint)sizeof(task));
    if (task == NULL || task_root == NULL || task->_isCompleted) {
        fprintf(stderr,
            "managed %s.ReadAsync did not remain pending before forced GC\n",
            use_network_stream ? "NetworkStream" : "Socket");
        goto cleanup;
    }
    cx_gc_collect();
#ifdef _WIN32
    closesocket((SOCKET)(uintptr_t)reset_peer_handle);
#else
    close((int)(uintptr_t)reset_peer_handle);
#endif
    reset_peer_handle = 0u;
    while (!task->_isCompleted && steps < 100000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (task->_isCompleted && task->_isFaulted && !task->_isCanceled &&
        cxcore_System_Exception_Matches(task->_exception,
            cxnet_System_Net_Sockets_SocketException___typeinfo) &&
        cxnet_System_Net_Sockets_SocketException_SocketErrorCode___const_get(
            (struct cxnet_System_Net_Sockets_SocketException *)task->_exception) ==
            cxnet_System_Net_Sockets_SocketError_ConnectionReset &&
        cxcore_System_IO_IOException_NativeErrorCode___const_get(
            (struct cxcore_System_IO_IOException *)task->_exception) != 0 &&
        cxcore_System_IO_IOException_NativeErrorDomain___const_get(
            (struct cxcore_System_IO_IOException *)task->_exception) != NULL) {
        passed = 1;
    } else {
        fprintf(stderr,
            "managed %s.ReadAsync did not preserve peer-reset identity\n",
            use_network_stream ? "NetworkStream" : "Socket");
    }
cleanup:
#ifdef _WIN32
    if (reset_peer_handle != 0u) {
        closesocket((SOCKET)(uintptr_t)reset_peer_handle);
    }
#else
    if (reset_peer_handle != 0u) close((int)(uintptr_t)reset_peer_handle);
#endif
    if (stream != NULL && stream_root != NULL) {
        cxnet_System_Net_Sockets_NetworkStream_Dispose(stream);
    }
    cxnet_System_Net_Sockets_Socket_Dispose(socket);
#ifdef _WIN32
    WSACleanup();
#endif
    if (task_root != NULL) cx_gc_remove_root(task_root);
    if (buffer_root != NULL) cx_gc_remove_root(buffer_root);
    if (stream_root != NULL) cx_gc_remove_root(stream_root);
    cx_gc_remove_root(socket_root);
    return passed;
}

static void on_connect_race_completed(void *environment) {
    (void)environment;
    connect_race_completion_count++;
}

static void cancel_connect_race_source(void *environment) {
    connect_race_cancel_count++;
    cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(
        (struct cxcore_System_Threading_Tasks_CancellationTokenSource *)environment);
}

static void on_accept_race_completed(void *environment) {
    (void)environment;
    accept_race_completion_count++;
}

static void cancel_accept_race_source(void *environment) {
    accept_race_cancel_count++;
    cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(
        (struct cxcore_System_Threading_Tasks_CancellationTokenSource *)environment);
}

static const uint32_t connect_cancel_delays_ms[] = {
    0u, 1u, 2u, 3u, 5u, 8u, 13u, 21u,
    34u, 0u, 2u, 5u, 8u, 13u, 21u, 34u
};

static int test_connect_cancellation_race(uint32_t cancel_delay_ms) {
    struct cxnet_System_Net_Sockets_Socket *listener =
        (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Sockets_Socket___typeinfo.Size);
    struct cxnet_System_Net_Sockets_Socket *client =
        (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Sockets_Socket___typeinfo.Size);
    struct cxnet_System_Net_IPEndPoint *endpoint =
        (struct cxnet_System_Net_IPEndPoint *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_IPEndPoint___typeinfo.Size);
    struct cxcore_System_Threading_Tasks_CancellationTokenSource *source =
        (struct cxcore_System_Threading_Tasks_CancellationTokenSource *)
            cxcore_System_Memory_Alloc(
                cxcore_System_Threading_Tasks_CancellationTokenSource___typeinfo.Size);
    struct cxcore_System_Threading_Tasks_CancellationToken token;
    struct cxcore_System_Threading_Tasks_TaskHandle *task;
    struct cx_gc_root_handle *listener_root = cx_gc_add_root(
        &listener, (cx_uint)sizeof(listener));
    struct cx_gc_root_handle *client_root = cx_gc_add_root(
        &client, (cx_uint)sizeof(client));
    struct cx_gc_root_handle *endpoint_root = cx_gc_add_root(
        &endpoint, (cx_uint)sizeof(endpoint));
    struct cx_gc_root_handle *source_root = cx_gc_add_root(
        &source, (cx_uint)sizeof(source));
    struct cx_gc_root_handle *task_root;
    cx_action cancel_action = {0};
    cx_action completion_action = {0};
    uint32_t steps = 0u;
    int passed = 0;

    cxnet_System_Net_Sockets_Socket___constructor__2(listener,
        cxnet_System_Net_AddressFamily_InterNetwork,
        cxnet_System_Net_Sockets_SocketType_Stream,
        cxnet_System_Net_Sockets_ProtocolType_Tcp);
    cxnet_System_Net_IPEndPoint___constructor__2(endpoint,
        cxnet_System_Net_IPAddress_Loopback___get(), 0);
    cxnet_System_Net_Sockets_Socket_Bind(listener,
        (struct cxnet_System_Net_EndPoint *)endpoint);
    cxnet_System_Net_Sockets_Socket_Listen__2(listener, 1);
    endpoint = cxnet_System_Net_Sockets_Socket_GetLocalEndPoint(listener);
    cxnet_System_Net_Sockets_Socket___constructor__2(client,
        cxnet_System_Net_AddressFamily_InterNetwork,
        cxnet_System_Net_Sockets_SocketType_Stream,
        cxnet_System_Net_Sockets_ProtocolType_Tcp);
    cxcore_System_Threading_Tasks_CancellationTokenSource___constructor(source);
    token = cxcore_System_Threading_Tasks_CancellationTokenSource_Token___const_get(
        source);
    connect_race_completion_count = 0u;
    connect_race_cancel_count = 0u;
    task = cxnet_System_Net_Sockets_Socket_ConnectAsync__6(client,
        (struct cxnet_System_Net_EndPoint *)endpoint, token);
    task_root = cx_gc_add_root(&task, (cx_uint)sizeof(task));
    completion_action.__function = on_connect_race_completed;
    completion_action.__environment = NULL;
    completion_action.__target = NULL;
    cxcore_System_Threading_Tasks_TaskHandle_OnCompleted(task,
        completion_action);
    cancel_action.__function = cancel_connect_race_source;
    cancel_action.__environment = source;
    cancel_action.__target = (cx_ptr)source;
    cxcore_System_Threading_Tasks_TaskScheduler_PostAfter(cancel_delay_ms,
        cancel_action);
    cx_gc_collect();
    while ((!cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(task) ||
            connect_race_cancel_count == 0u ||
            connect_race_completion_count == 0u) && steps < 100000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }

    if (cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(task) &&
        connect_race_cancel_count == 1u &&
        connect_race_completion_count == 1u &&
        !task->_state->_isFaulted) {
        if (cxcore_System_Threading_Tasks_TaskHandle_IsCanceled___const_get(task)) {
            passed = !cxnet_System_Net_Sockets_Socket_Connected___const_get(client);
        } else {
            cxcore_System_Threading_Tasks_TaskHandle_GetResult(task);
            passed = cxnet_System_Net_Sockets_Socket_Connected___const_get(client);
        }
    }
    cxnet_System_Net_Sockets_Socket_Dispose(client);
    cxnet_System_Net_Sockets_Socket_Dispose(listener);
    cx_gc_remove_root(task_root);
    cx_gc_remove_root(source_root);
    cx_gc_remove_root(endpoint_root);
    cx_gc_remove_root(client_root);
    cx_gc_remove_root(listener_root);
    if (!passed) {
        fprintf(stderr,
            "connect/cancel race failed delay=%u completed=%d canceled=%d faulted=%d connected=%d taskCallbacks=%u cancelCallbacks=%u steps=%u\n",
            (unsigned)cancel_delay_ms,
            cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(task),
            cxcore_System_Threading_Tasks_TaskHandle_IsCanceled___const_get(task),
            task->_state->_isFaulted,
            cxnet_System_Net_Sockets_Socket_Connected___const_get(client),
            (unsigned)connect_race_completion_count,
            (unsigned)connect_race_cancel_count,
            (unsigned)steps);
    }
    return passed;
}

static int test_accept_cancellation_race(uint32_t cancel_delay_ms) {
    struct cxnet_System_Net_Sockets_Socket *listener =
        (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Sockets_Socket___typeinfo.Size);
    struct cxnet_System_Net_Sockets_Socket *client =
        (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Sockets_Socket___typeinfo.Size);
    struct cxnet_System_Net_Sockets_Socket *accepted = NULL;
    struct cxnet_System_Net_IPEndPoint *endpoint =
        (struct cxnet_System_Net_IPEndPoint *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_IPEndPoint___typeinfo.Size);
    struct cxcore_System_Threading_Tasks_CancellationTokenSource *source =
        (struct cxcore_System_Threading_Tasks_CancellationTokenSource *)
            cxcore_System_Memory_Alloc(
                cxcore_System_Threading_Tasks_CancellationTokenSource___typeinfo.Size);
    struct cxcore_System_Threading_Tasks_CancellationToken token;
    struct cxcore_System_Threading_Tasks_Task *accept_task;
    struct cxcore_System_Threading_Tasks_TaskHandle *connect_task;
    struct cx_gc_root_handle *listener_root = cx_gc_add_root(
        &listener, (cx_uint)sizeof(listener));
    struct cx_gc_root_handle *client_root = cx_gc_add_root(
        &client, (cx_uint)sizeof(client));
    struct cx_gc_root_handle *accepted_root = cx_gc_add_root(
        &accepted, (cx_uint)sizeof(accepted));
    struct cx_gc_root_handle *endpoint_root = cx_gc_add_root(
        &endpoint, (cx_uint)sizeof(endpoint));
    struct cx_gc_root_handle *source_root = cx_gc_add_root(
        &source, (cx_uint)sizeof(source));
    struct cx_gc_root_handle *accept_task_root;
    struct cx_gc_root_handle *connect_task_root;
    cx_action cancel_action = {0};
    cx_action completion_action = {0};
    struct cxcore_System_Collections_Array send_buffer = {0};
    struct cxcore_System_Collections_Array receive_buffer = {0};
    cx_byte sent = (cx_byte)'R';
    cx_byte received = 0u;
    uint32_t steps = 0u;
    int passed = 0;

    cxnet_System_Net_Sockets_Socket___constructor__2(listener,
        cxnet_System_Net_AddressFamily_InterNetwork,
        cxnet_System_Net_Sockets_SocketType_Stream,
        cxnet_System_Net_Sockets_ProtocolType_Tcp);
    cxnet_System_Net_IPEndPoint___constructor__2(endpoint,
        cxnet_System_Net_IPAddress_Loopback___get(), 0);
    cxnet_System_Net_Sockets_Socket_Bind(listener,
        (struct cxnet_System_Net_EndPoint *)endpoint);
    cxnet_System_Net_Sockets_Socket_Listen__2(listener, 2);
    endpoint = cxnet_System_Net_Sockets_Socket_GetLocalEndPoint(listener);
    cxnet_System_Net_Sockets_Socket___constructor__2(client,
        cxnet_System_Net_AddressFamily_InterNetwork,
        cxnet_System_Net_Sockets_SocketType_Stream,
        cxnet_System_Net_Sockets_ProtocolType_Tcp);
    cxcore_System_Threading_Tasks_CancellationTokenSource___constructor(source);
    token = cxcore_System_Threading_Tasks_CancellationTokenSource_Token___const_get(
        source);
    send_buffer._length = 1u;
    send_buffer._data = &sent;
    receive_buffer._length = 1u;
    receive_buffer._data = &received;
    accept_race_completion_count = 0u;
    accept_race_cancel_count = 0u;
    accept_task = cxnet_System_Net_Sockets_Socket_AcceptAsync__2(listener, token);
    accept_task_root = cx_gc_add_root(&accept_task, (cx_uint)sizeof(accept_task));
    completion_action.__function = on_accept_race_completed;
    completion_action.__environment = NULL;
    completion_action.__target = NULL;
    cxcore_System_Threading_Tasks_Task_OnCompleted(accept_task,
        completion_action);
    connect_task = cxnet_System_Net_Sockets_Socket_ConnectAsync__5(client,
        (struct cxnet_System_Net_EndPoint *)endpoint);
    connect_task_root = cx_gc_add_root(&connect_task,
        (cx_uint)sizeof(connect_task));
    cancel_action.__function = cancel_accept_race_source;
    cancel_action.__environment = source;
    cancel_action.__target = (cx_ptr)source;
    cxcore_System_Threading_Tasks_TaskScheduler_PostAfter(cancel_delay_ms,
        cancel_action);
    cx_gc_collect();
    while ((!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(accept_task) ||
            !cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(connect_task) ||
            accept_race_cancel_count == 0u || accept_race_completion_count == 0u) &&
        steps < 100000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }

    if (cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(accept_task) &&
        cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(connect_task) &&
        accept_race_cancel_count == 1u && accept_race_completion_count == 1u &&
        !accept_task->_isFaulted && !connect_task->_state->_isFaulted &&
        !connect_task->_state->_isCanceled) {
        cxcore_System_Threading_Tasks_TaskHandle_GetResult(connect_task);
        if (cxcore_System_Threading_Tasks_Task_IsCanceled___const_get(accept_task)) {
            struct cxcore_System_Threading_Tasks_Task *reused_accept =
                cxnet_System_Net_Sockets_Socket_AcceptAsync(listener);
            struct cx_gc_root_handle *reused_accept_root = cx_gc_add_root(
                &reused_accept, (cx_uint)sizeof(reused_accept));
            uint32_t reuse_steps = 0u;
            while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(
                    reused_accept) && reuse_steps < 100000u) {
                if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                    Sleep(1u);
#else
                    usleep(1000u);
#endif
                }
                reuse_steps++;
            }
            if (cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(
                    reused_accept) && !reused_accept->_isFaulted &&
                !reused_accept->_isCanceled) {
                accepted = cx_generic_7c4851112331385bf3ec935e4daabb246ea0a9a28971c93643ab4b4268d85138(
                    (const struct cxcore_System_Threading_Tasks_Task *)reused_accept);
            }
            cx_gc_remove_root(reused_accept_root);
        } else {
            accepted = cx_generic_7c4851112331385bf3ec935e4daabb246ea0a9a28971c93643ab4b4268d85138(
                (const struct cxcore_System_Threading_Tasks_Task *)accept_task);
        }
        passed = accepted != NULL &&
            cxnet_System_Net_Sockets_Socket_Connected___const_get(client) &&
            cxnet_System_Net_Sockets_Socket_Connected___const_get(accepted) &&
            cxnet_System_Net_Sockets_Socket_Send(client, &send_buffer, 0u, 1u) == 1 &&
            cxnet_System_Net_Sockets_Socket_Receive(accepted,
                &receive_buffer, 0u, 1u) == 1 && received == sent;
    }

    if (!passed) {
        fprintf(stderr,
            "accept/cancel race failed delay=%u accept=%d canceled=%d faulted=%d connect=%d/%d connected=%d/%d callbacks=%u/%u steps=%u\n",
            (unsigned)cancel_delay_ms,
            cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(accept_task),
            cxcore_System_Threading_Tasks_Task_IsCanceled___const_get(accept_task),
            accept_task->_isFaulted,
            cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(connect_task),
            connect_task->_state->_isFaulted,
            cxnet_System_Net_Sockets_Socket_Connected___const_get(client),
            accepted != NULL && cxnet_System_Net_Sockets_Socket_Connected___const_get(accepted),
            (unsigned)accept_race_completion_count,
            (unsigned)accept_race_cancel_count,
            (unsigned)steps);
    }
    if (accepted != NULL) cxnet_System_Net_Sockets_Socket_Dispose(accepted);
    cxnet_System_Net_Sockets_Socket_Dispose(client);
    cxnet_System_Net_Sockets_Socket_Dispose(listener);
    cx_gc_remove_root(connect_task_root);
    cx_gc_remove_root(accept_task_root);
    cx_gc_remove_root(source_root);
    cx_gc_remove_root(endpoint_root);
    cx_gc_remove_root(accepted_root);
    cx_gc_remove_root(client_root);
    cx_gc_remove_root(listener_root);
    return passed;
}

static struct cxcore_System_String *make_string(const char *text) {
    size_t length = strlen(text);
    cx_ptr data = cxcore_System_Memory_Alloc((cx_uint)length);
    struct cxcore_System_String *value =
        (struct cxcore_System_String *)cxcore_System_Memory_Alloc(
            cxcore_System_String___typeinfo.Size);
    memcpy(data, text, length);
    cxcore_System_String___constructor__2(value, (cx_uint)length, data);
    return value;
}

static uint32_t hostname_completion_throw_count;

static void throw_hostname_completion(void *environment) {
    struct cxcore_System_InvalidOperationException *exception;
    (void)environment;
    hostname_completion_throw_count++;
    exception = (struct cxcore_System_InvalidOperationException *)
        cxcore_System_Memory_Alloc(
            cxcore_System_InvalidOperationException___typeinfo.Size);
    cxcore_System_InvalidOperationException___constructor(exception);
    CX_THROW(exception);
}

static void reenter_socket_dispose_then_throw(void *environment) {
    struct cxcore_System_InvalidOperationException *exception;
    reentrant_socket_dispose_count++;
    cxnet_System_Net_Sockets_Socket_Dispose(
        (struct cxnet_System_Net_Sockets_Socket *)environment);
    reentrant_socket_dispose_return_count++;
    exception = (struct cxcore_System_InvalidOperationException *)
        cxcore_System_Memory_Alloc(
            cxcore_System_InvalidOperationException___typeinfo.Size);
    cxcore_System_InvalidOperationException___constructor(exception);
    CX_THROW(exception);
}

static int test_hostname_connect_throwing_completion(
    struct cxnet_System_Net_Sockets_Socket *listener, cx_ushort port) {
    struct cxnet_System_Net_Sockets_Socket *client =
        (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Sockets_Socket___typeinfo.Size);
    struct cxcore_System_Threading_Tasks_CancellationTokenSource *source =
        (struct cxcore_System_Threading_Tasks_CancellationTokenSource *)
            cxcore_System_Memory_Alloc(
                cxcore_System_Threading_Tasks_CancellationTokenSource___typeinfo.Size);
    struct cxcore_System_Threading_Tasks_TaskHandle *task;
    struct cxnet_System_Net_Sockets_Socket *accepted = NULL;
    struct cx_gc_root_handle *client_root = cx_gc_add_root(
        &client, (cx_uint)sizeof(client));
    struct cx_gc_root_handle *source_root = cx_gc_add_root(
        &source, (cx_uint)sizeof(source));
    struct cx_gc_root_handle *task_root;
    struct cxcore_System_Threading_Tasks_CancellationToken token;
    cx_action completion_action = {0};
    uint32_t steps = 0u;
    uint32_t observed_exceptions = 0u;
    int passed = 0;

    cxnet_System_Net_Sockets_Socket___constructor__2(client,
        cxnet_System_Net_AddressFamily_InterNetwork,
        cxnet_System_Net_Sockets_SocketType_Stream,
        cxnet_System_Net_Sockets_ProtocolType_Tcp);
    client->_connectTimeoutMilliseconds = 5000u;
    cxcore_System_Threading_Tasks_CancellationTokenSource___constructor(source);
    token = cxcore_System_Threading_Tasks_CancellationTokenSource_Token___const_get(
        source);
    hostname_completion_throw_count = 0u;
    task = cxnet_System_Net_Sockets_Socket_ConnectAsync__4(
        client, make_string("localhost"), (cx_int)port, token);
    task_root = cx_gc_add_root(&task, (cx_uint)sizeof(task));
    completion_action.__function = throw_hostname_completion;
    completion_action.__environment = NULL;
    completion_action.__target = NULL;
    cxcore_System_Threading_Tasks_TaskHandle_OnCompleted(task,
        completion_action);
    cx_gc_collect();
    while ((!task->_state->_isCompleted ||
            hostname_completion_throw_count == 0u ||
            observed_exceptions == 0u) && steps < 100000u) {
        struct cx_exception_frame frame;
        cx_exception_push(&frame);
        if (setjmp(frame.environment) == 0) {
            cx_bool ran = cxcore_System_Threading_Tasks_TaskScheduler_RunOne();
            cx_exception_pop(&frame);
            if (!ran) {
#ifdef _WIN32
                Sleep(1u);
#else
                usleep(1000u);
#endif
            }
        } else {
            struct cxcore_System_Exception *exception =
                (struct cxcore_System_Exception *)cx_exception_current();
            cx_exception_pop(&frame);
            if (cxcore_System_Exception_Matches(exception,
                    cxcore_System_InvalidOperationException___typeinfo)) {
                observed_exceptions++;
            }
            cx_exception_clear();
        }
        steps++;
    }

    if (task->_state->_isCompleted && !task->_state->_isFaulted &&
        !task->_state->_isCanceled && hostname_completion_throw_count == 1u &&
        observed_exceptions == 1u &&
        cxnet_System_Net_Sockets_Socket_Connected___const_get(client) &&
        client->_hostConnectCompletion == NULL &&
        client->_hostConnectCancellation == NULL &&
        client->_hostConnectTimer == 0u &&
        client->_hostConnectCallerRegistration._source == NULL &&
        source->_callbackCount == 0u) {
        cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(source);
        accepted = cxnet_System_Net_Sockets_Socket_Accept(listener);
        passed = cxnet_System_Net_Sockets_Socket_Connected___const_get(client) &&
            source->_callbackCount == 0u && accepted != NULL &&
            cxnet_System_Net_Sockets_Socket_Connected___const_get(accepted);
    }
    if (!passed) {
        fprintf(stderr,
            "hostname connect completion cleanup failed completed=%d faulted=%d canceled=%d connected=%d callbacks=%u exceptions=%u callerRegistrations=%u timer=%llu steps=%u\n",
            task->_state->_isCompleted, task->_state->_isFaulted,
            task->_state->_isCanceled,
            cxnet_System_Net_Sockets_Socket_Connected___const_get(client),
            (unsigned)hostname_completion_throw_count,
            (unsigned)observed_exceptions,
            (unsigned)source->_callbackCount,
            (unsigned long long)client->_hostConnectTimer,
            (unsigned)steps);
    }
    if (accepted != NULL) cxnet_System_Net_Sockets_Socket_Dispose(accepted);
    cxnet_System_Net_Sockets_Socket_Dispose(client);
    cx_gc_remove_root(task_root);
    cx_gc_remove_root(source_root);
    cx_gc_remove_root(client_root);
    return passed;
}

static int test_hostname_connect_dispose_during_resolution(void) {
    struct socket_cross_thread_dispose dispose_race = {0};
    struct cxnet_System_Net_Sockets_Socket *client =
        (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Sockets_Socket___typeinfo.Size);
    struct cxcore_System_Threading_Tasks_CancellationTokenSource *source =
        (struct cxcore_System_Threading_Tasks_CancellationTokenSource *)
            cxcore_System_Memory_Alloc(
                cxcore_System_Threading_Tasks_CancellationTokenSource___typeinfo.Size);
    struct cxcore_System_Threading_Tasks_TaskHandle *task;
    struct cxcore_System_Threading_Tasks_CancellationToken token;
    struct cx_gc_root_handle *client_root = cx_gc_add_root(
        &client, (cx_uint)sizeof(client));
    struct cx_gc_root_handle *source_root = cx_gc_add_root(
        &source, (cx_uint)sizeof(source));
    struct cx_gc_root_handle *task_root;
    uint32_t steps = 0u;
    uint32_t callbacks_before_cancel;
    int resolver_started = 0;
    int state_valid;
    int passed = 0;

    cxnet_System_Net_Sockets_Socket___constructor__2(client,
        cxnet_System_Net_AddressFamily_InterNetwork,
        cxnet_System_Net_Sockets_SocketType_Stream,
        cxnet_System_Net_Sockets_ProtocolType_Tcp);
    client->_connectTimeoutMilliseconds = 5000u;
    cxcore_System_Threading_Tasks_CancellationTokenSource___constructor(source);
    token = cxcore_System_Threading_Tasks_CancellationTokenSource_Token___const_get(
        source);
    disposal_resolver_started = 0;
    task = cxnet_System_Net_Sockets_Socket_ConnectAsync__4(
        client, make_string("cxnet-test-resolution-dispose.invalid"), 80, token);
    task_root = cx_gc_add_root(&task, (cx_uint)sizeof(task));
    while (steps < 5000u) {
#ifdef _WIN32
        if (InterlockedCompareExchange(
                (volatile LONG *)&disposal_resolver_started, 0, 0) != 0) {
#else
        if (__sync_val_compare_and_swap(&disposal_resolver_started, 0, 0) != 0) {
#endif
            resolver_started = 1;
            break;
        }
#ifdef _WIN32
        Sleep(1u);
#else
        usleep(1000u);
#endif
        steps++;
    }
    steps = 0u;
#ifdef _WIN32
    {
        HANDLE worker;
        dispose_race.socket = client;
        worker = CreateThread(NULL, 0, socket_cross_thread_dispose_worker,
            &dispose_race, 0, NULL);
        if (worker == NULL) {
            fputs("could not start hostname-connect dispose worker\n", stderr);
            cxnet_System_Net_Sockets_Socket_Dispose(client);
            cx_gc_remove_root(task_root);
            cx_gc_remove_root(source_root);
            cx_gc_remove_root(client_root);
            return 0;
        }
        while (!socket_dispose_race_load(&dispose_race.ready)) Sleep(0u);
        socket_dispose_race_store(&dispose_race.start, 1);
        WaitForSingleObject(worker, INFINITE);
        CloseHandle(worker);
    }
#else
    {
        pthread_t worker;
        dispose_race.socket = client;
        if (pthread_create(&worker, NULL, socket_cross_thread_dispose_worker,
                &dispose_race) != 0) {
            fputs("could not start hostname-connect dispose worker\n", stderr);
            cxnet_System_Net_Sockets_Socket_Dispose(client);
            cx_gc_remove_root(task_root);
            cx_gc_remove_root(source_root);
            cx_gc_remove_root(client_root);
            return 0;
        }
        while (!socket_dispose_race_load(&dispose_race.ready)) sched_yield();
        socket_dispose_race_store(&dispose_race.start, 1);
        pthread_join(worker, NULL);
    }
#endif
    cx_gc_collect();
    while (steps < 300u) {
        cxcore_System_Threading_Tasks_TaskScheduler_RunOne();
#ifdef _WIN32
        Sleep(1u);
#else
        usleep(1000u);
#endif
        steps++;
    }
    callbacks_before_cancel = source->_callbackCount;
    state_valid = resolver_started &&
        !socket_dispose_race_load(&dispose_race.failed) &&
        socket_dispose_race_load(&dispose_race.completed) &&
        task->_state->_isCompleted &&
        task->_state->_isFaulted &&
        !task->_state->_isCanceled &&
        cxcore_System_Exception_Matches(task->_state->_exception,
            cxcore_System_ObjectDisposedException___typeinfo) &&
        !cxnet_System_Net_Sockets_Socket_Connected___const_get(client) &&
        client->_hostConnectCompletion == NULL &&
        client->_hostConnectCancellation == NULL &&
        client->_hostConnectTimer == 0u &&
        client->_hostConnectCallerRegistration._source == NULL &&
        callbacks_before_cancel == 0u;
    cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(source);
    if (state_valid) {
        cxnet_System_Net_Sockets_Socket_Dispose(client);
        passed = task->_state->_isFaulted &&
            cxcore_System_Exception_Matches(task->_state->_exception,
                cxcore_System_ObjectDisposedException___typeinfo) &&
            source->_callbackCount == 0u;
    }
    if (!passed) {
        fprintf(stderr,
            "hostname connect disposal during DNS failed completed=%d faulted=%d canceled=%d callbacks-before-cancel=%u callbacks-after-cancel=%u timer=%llu steps=%u\n",
            task->_state->_isCompleted, task->_state->_isFaulted,
            task->_state->_isCanceled, (unsigned)callbacks_before_cancel,
            (unsigned)source->_callbackCount,
            (unsigned long long)client->_hostConnectTimer, (unsigned)steps);
    }
    cx_gc_remove_root(task_root);
    cx_gc_remove_root(source_root);
    cx_gc_remove_root(client_root);
    return passed;
}

static int test_socket_dispose_reentrant_throwing_completion(void) {
    struct cxnet_System_Net_Sockets_Socket *client =
        (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Sockets_Socket___typeinfo.Size);
    struct cxcore_System_Threading_Tasks_CancellationTokenSource *source =
        (struct cxcore_System_Threading_Tasks_CancellationTokenSource *)
            cxcore_System_Memory_Alloc(
                cxcore_System_Threading_Tasks_CancellationTokenSource___typeinfo.Size);
    struct cxcore_System_Threading_Tasks_TaskHandle *task;
    struct cxcore_System_Threading_Tasks_CancellationToken token;
    struct cx_gc_root_handle *client_root = cx_gc_add_root(
        &client, (cx_uint)sizeof(client));
    struct cx_gc_root_handle *source_root = cx_gc_add_root(
        &source, (cx_uint)sizeof(source));
    struct cx_gc_root_handle *task_root;
    cx_action completion_action = {0};
    uint32_t steps = 0u;
    uint32_t observed_exceptions = 0u;
    int resolver_started = 0;
    int passed = 0;

    cxnet_System_Net_Sockets_Socket___constructor__2(client,
        cxnet_System_Net_AddressFamily_InterNetwork,
        cxnet_System_Net_Sockets_SocketType_Stream,
        cxnet_System_Net_Sockets_ProtocolType_Tcp);
    client->_connectTimeoutMilliseconds = 5000u;
    cxcore_System_Threading_Tasks_CancellationTokenSource___constructor(source);
    token = cxcore_System_Threading_Tasks_CancellationTokenSource_Token___const_get(
        source);
    disposal_resolver_started = 0;
    reentrant_socket_dispose_count = 0u;
    reentrant_socket_dispose_return_count = 0u;
    task = cxnet_System_Net_Sockets_Socket_ConnectAsync__4(
        client, make_string("cxnet-test-resolution-dispose.invalid"), 80, token);
    task_root = cx_gc_add_root(&task, (cx_uint)sizeof(task));
    while (steps < 5000u) {
#ifdef _WIN32
        if (InterlockedCompareExchange(
                (volatile LONG *)&disposal_resolver_started, 0, 0) != 0) {
#else
        if (__sync_val_compare_and_swap(&disposal_resolver_started, 0, 0) != 0) {
#endif
            resolver_started = 1;
            break;
        }
#ifdef _WIN32
        Sleep(1u);
#else
        usleep(1000u);
#endif
        steps++;
    }

    completion_action.__function = reenter_socket_dispose_then_throw;
    completion_action.__environment = client;
    completion_action.__target = NULL;
    cxcore_System_Threading_Tasks_TaskHandle_OnCompleted(task,
        completion_action);
    {
        struct cx_exception_frame frame;
        cx_exception_push(&frame);
        if (setjmp(frame.environment) == 0) {
            cxnet_System_Net_Sockets_Socket_Dispose(client);
            cx_exception_pop(&frame);
        } else {
            struct cxcore_System_Exception *exception =
                (struct cxcore_System_Exception *)cx_exception_current();
            cx_exception_pop(&frame);
            if (cxcore_System_Exception_Matches(exception,
                    cxcore_System_InvalidOperationException___typeinfo)) {
                observed_exceptions++;
            }
            cx_exception_clear();
        }
    }
    cx_gc_collect();
    steps = 0u;
    while ((reentrant_socket_dispose_count == 0u ||
            observed_exceptions == 0u || steps < 250u) && steps < 1000u) {
        struct cx_exception_frame frame;
        cx_exception_push(&frame);
        if (setjmp(frame.environment) == 0) {
            cxcore_System_Threading_Tasks_TaskScheduler_RunOne();
            cx_exception_pop(&frame);
        } else {
            struct cxcore_System_Exception *exception =
                (struct cxcore_System_Exception *)cx_exception_current();
            cx_exception_pop(&frame);
            if (cxcore_System_Exception_Matches(exception,
                    cxcore_System_InvalidOperationException___typeinfo)) {
                observed_exceptions++;
            }
            cx_exception_clear();
        }
#ifdef _WIN32
        Sleep(1u);
#else
        usleep(1000u);
#endif
        steps++;
    }

    cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(source);
    passed = resolver_started && task->_state->_isCompleted &&
        task->_state->_isFaulted && !task->_state->_isCanceled &&
        cxcore_System_Exception_Matches(task->_state->_exception,
            cxcore_System_ObjectDisposedException___typeinfo) &&
        client->_disposed &&
        client->_nativeHandle == cxcore_System_ULong_MaxValue &&
        client->_hostConnectCompletion == NULL &&
        client->_hostConnectCancellation == NULL &&
        client->_hostConnectTimer == 0u &&
        client->_hostConnectCallerRegistration._source == NULL &&
        source->_callbackCount == 0u &&
        reentrant_socket_dispose_count == 1u &&
        reentrant_socket_dispose_return_count == 1u &&
        observed_exceptions == 1u;
    cxnet_System_Net_Sockets_Socket_Dispose(client);
    if (!passed) {
        fprintf(stderr,
            "socket disposal reentrant completion failed resolver=%d completed=%d faulted=%d canceled=%d disposed=%d callback=%u returned=%u exceptions=%u registrations=%u steps=%u\n",
            resolver_started, task->_state->_isCompleted,
            task->_state->_isFaulted, task->_state->_isCanceled,
            client->_disposed, (unsigned)reentrant_socket_dispose_count,
            (unsigned)reentrant_socket_dispose_return_count,
            (unsigned)observed_exceptions, (unsigned)source->_callbackCount,
            (unsigned)steps);
    }
    cx_gc_remove_root(task_root);
    cx_gc_remove_root(source_root);
    cx_gc_remove_root(client_root);
    return passed;
}

static int test_endpoint_connect_failure_reuse(cx_ushort refused_port) {
    struct cxnet_System_Net_Sockets_Socket *client =
        (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Sockets_Socket___typeinfo.Size);
    struct cxnet_System_Net_Sockets_Socket *listener =
        (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Sockets_Socket___typeinfo.Size);
    struct cxnet_System_Net_Sockets_Socket *accepted = NULL;
    struct cxnet_System_Net_IPEndPoint *refused_endpoint =
        (struct cxnet_System_Net_IPEndPoint *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_IPEndPoint___typeinfo.Size);
    struct cxnet_System_Net_IPEndPoint *bind_endpoint =
        (struct cxnet_System_Net_IPEndPoint *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_IPEndPoint___typeinfo.Size);
    struct cxnet_System_Net_IPEndPoint *retry_endpoint = NULL;
    struct cxcore_System_Threading_Tasks_TaskHandle *task;
    struct cxcore_System_Threading_Tasks_TaskHandle *duplicate_connect_task = NULL;
    struct cx_gc_root_handle *client_root = cx_gc_add_root(
        &client, (cx_uint)sizeof(client));
    struct cx_gc_root_handle *listener_root = cx_gc_add_root(
        &listener, (cx_uint)sizeof(listener));
    struct cx_gc_root_handle *accepted_root = cx_gc_add_root(
        &accepted, (cx_uint)sizeof(accepted));
    struct cx_gc_root_handle *refused_endpoint_root = cx_gc_add_root(
        &refused_endpoint, (cx_uint)sizeof(refused_endpoint));
    struct cx_gc_root_handle *bind_endpoint_root = cx_gc_add_root(
        &bind_endpoint, (cx_uint)sizeof(bind_endpoint));
    struct cx_gc_root_handle *retry_endpoint_root = NULL;
    struct cx_gc_root_handle *task_root;
    struct cx_gc_root_handle *duplicate_connect_task_root = NULL;
    uint32_t steps = 0u;
    int listener_initialized = 0;
    int passed = 0;

    cxnet_System_Net_Sockets_Socket___constructor__2(client,
        cxnet_System_Net_AddressFamily_InterNetwork,
        cxnet_System_Net_Sockets_SocketType_Stream,
        cxnet_System_Net_Sockets_ProtocolType_Tcp);
    cxnet_System_Net_IPEndPoint___constructor__2(refused_endpoint,
        cxnet_System_Net_IPAddress_Loopback___get(), (cx_int)refused_port);
    task = cxnet_System_Net_Sockets_Socket_ConnectAsync__5(
        client, (struct cxnet_System_Net_EndPoint *)refused_endpoint);
    task_root = cx_gc_add_root(&task, (cx_uint)sizeof(task));
    while (!task->_state->_isCompleted && steps < 100000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (task->_state->_isCompleted && task->_state->_isFaulted &&
        !task->_state->_isCanceled &&
        cxcore_System_Exception_Matches(task->_state->_exception,
            cxnet_System_Net_Sockets_SocketException___typeinfo) &&
        cxnet_System_Net_Sockets_SocketException_SocketErrorCode___const_get(
            (struct cxnet_System_Net_Sockets_SocketException *)
                task->_state->_exception) ==
            cxnet_System_Net_Sockets_SocketError_ConnectionRefused &&
        !cxnet_System_Net_Sockets_Socket_Connected___const_get(client) &&
        client->_state == 0u) {
        cxnet_System_Net_Sockets_Socket___constructor__2(listener,
            cxnet_System_Net_AddressFamily_InterNetwork,
            cxnet_System_Net_Sockets_SocketType_Stream,
            cxnet_System_Net_Sockets_ProtocolType_Tcp);
        listener_initialized = 1;
        cxnet_System_Net_IPEndPoint___constructor__2(bind_endpoint,
            cxnet_System_Net_IPAddress_Loopback___get(), 0);
        cxnet_System_Net_Sockets_Socket_Bind(listener,
            (struct cxnet_System_Net_EndPoint *)bind_endpoint);
        cxnet_System_Net_Sockets_Socket_Listen__2(listener, 1);
        retry_endpoint = cxnet_System_Net_Sockets_Socket_GetLocalEndPoint(
            listener);
        retry_endpoint_root = cx_gc_add_root(&retry_endpoint,
            (cx_uint)sizeof(retry_endpoint));
        struct cx_exception_frame frame;
        cx_exception_push(&frame);
        if (setjmp(frame.environment) == 0) {
            cxnet_System_Net_Sockets_Socket_Connect(client,
                (struct cxnet_System_Net_EndPoint *)retry_endpoint);
            cx_exception_pop(&frame);
            accepted = cxnet_System_Net_Sockets_Socket_Accept(listener);
            passed = cxnet_System_Net_Sockets_Socket_Connected___const_get(
                    client) && accepted != NULL &&
                cxnet_System_Net_Sockets_Socket_Connected___const_get(accepted);
            if (passed) {
                cx_byte sent_value = (cx_byte)'C';
                cx_byte received_value = 0u;
                struct cxcore_System_Collections_Array sent = {0};
                struct cxcore_System_Collections_Array received = {0};
                duplicate_connect_task =
                    cxnet_System_Net_Sockets_Socket_ConnectAsync__5(client,
                        (struct cxnet_System_Net_EndPoint *)retry_endpoint);
                duplicate_connect_task_root = cx_gc_add_root(
                    &duplicate_connect_task,
                    (cx_uint)sizeof(duplicate_connect_task));
                sent._length = 1u;
                sent._data = &sent_value;
                received._length = 1u;
                received._data = &received_value;
                passed = duplicate_connect_task->_state->_isCompleted &&
                    duplicate_connect_task->_state->_isFaulted &&
                    !duplicate_connect_task->_state->_isCanceled &&
                    cxcore_System_Exception_Matches(
                        duplicate_connect_task->_state->_exception,
                        cxcore_System_InvalidOperationException___typeinfo) &&
                    cxnet_System_Net_Sockets_Socket_Connected___const_get(client) &&
                    cxnet_System_Net_Sockets_Socket_Connected___const_get(accepted) &&
                    cxnet_System_Net_Sockets_Socket_Send(client, &sent, 0u, 1u) == 1 &&
                    cxnet_System_Net_Sockets_Socket_Receive(accepted,
                        &received, 0u, 1u) == 1 && received_value == sent_value;
            }
        } else {
            cx_exception_pop(&frame);
            cx_exception_clear();
        }
    }
    if (!passed) {
        fprintf(stderr,
            "endpoint ConnectAsync failure/retry or duplicate-connect state regression failed completed=%d faulted=%d canceled=%d state=%u steps=%u\n",
            task->_state->_isCompleted, task->_state->_isFaulted,
            task->_state->_isCanceled, (unsigned)client->_state,
            (unsigned)steps);
    }
    if (accepted != NULL) cxnet_System_Net_Sockets_Socket_Dispose(accepted);
    cxnet_System_Net_Sockets_Socket_Dispose(client);
    if (listener_initialized) cxnet_System_Net_Sockets_Socket_Dispose(listener);
    if (duplicate_connect_task_root != NULL)
        cx_gc_remove_root(duplicate_connect_task_root);
    if (retry_endpoint_root != NULL) cx_gc_remove_root(retry_endpoint_root);
    cx_gc_remove_root(task_root);
    cx_gc_remove_root(bind_endpoint_root);
    cx_gc_remove_root(refused_endpoint_root);
    cx_gc_remove_root(accepted_root);
    cx_gc_remove_root(listener_root);
    cx_gc_remove_root(client_root);
    return passed;
}

static int run_endpoint_connect_failure_reuse_only(void) {
    struct cxnet_System_Net_Sockets_Socket *listener =
        (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Sockets_Socket___typeinfo.Size);
    struct cxnet_System_Net_IPEndPoint *bind_endpoint =
        (struct cxnet_System_Net_IPEndPoint *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_IPEndPoint___typeinfo.Size);
    struct cxnet_System_Net_IPEndPoint *local_endpoint = NULL;
    struct cx_gc_root_handle *listener_root = cx_gc_add_root(
        &listener, (cx_uint)sizeof(listener));
    struct cx_gc_root_handle *bind_endpoint_root = cx_gc_add_root(
        &bind_endpoint, (cx_uint)sizeof(bind_endpoint));
    struct cx_gc_root_handle *local_endpoint_root = NULL;
    cx_ushort refused_port;
    int passed;
    if (listener == NULL || bind_endpoint == NULL || listener_root == NULL ||
        bind_endpoint_root == NULL) return 0;
    cxnet_System_Net_Sockets_Socket___constructor__2(listener,
        cxnet_System_Net_AddressFamily_InterNetwork,
        cxnet_System_Net_Sockets_SocketType_Stream,
        cxnet_System_Net_Sockets_ProtocolType_Tcp);
    cxnet_System_Net_IPEndPoint___constructor__2(bind_endpoint,
        cxnet_System_Net_IPAddress_Loopback___get(), 0);
    cxnet_System_Net_Sockets_Socket_Bind(listener,
        (struct cxnet_System_Net_EndPoint *)bind_endpoint);
    cxnet_System_Net_Sockets_Socket_Listen__2(listener, 1);
    local_endpoint = (struct cxnet_System_Net_IPEndPoint *)
        cxnet_System_Net_Sockets_Socket_GetLocalEndPoint(listener);
    local_endpoint_root = cx_gc_add_root(&local_endpoint,
        (cx_uint)sizeof(local_endpoint));
    if (local_endpoint == NULL || local_endpoint_root == NULL) {
        cxnet_System_Net_Sockets_Socket_Dispose(listener);
        return 0;
    }
    refused_port = cxnet_System_Net_IPEndPoint_Port___const_get(local_endpoint);
    cxnet_System_Net_Sockets_Socket_Dispose(listener);
    passed = test_endpoint_connect_failure_reuse(refused_port);
    cx_gc_remove_root(local_endpoint_root);
    cx_gc_remove_root(bind_endpoint_root);
    cx_gc_remove_root(listener_root);
    return passed;
}

static int run_endpoint_connect_duplicate_pending_only(void) {
    struct cxnet_System_Net_Sockets_Socket *listener =
        (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Sockets_Socket___typeinfo.Size);
    struct cxnet_System_Net_Sockets_Socket *client =
        (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Sockets_Socket___typeinfo.Size);
    struct cxnet_System_Net_Sockets_Socket *accepted = NULL;
    struct cxnet_System_Net_IPEndPoint *bind_endpoint =
        (struct cxnet_System_Net_IPEndPoint *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_IPEndPoint___typeinfo.Size);
    struct cxnet_System_Net_IPEndPoint *endpoint = NULL;
    struct cxcore_System_Threading_Tasks_TaskHandle *first_task = NULL;
    struct cxcore_System_Threading_Tasks_TaskHandle *duplicate_task = NULL;
    struct cx_gc_root_handle *listener_root = NULL;
    struct cx_gc_root_handle *client_root = NULL;
    struct cx_gc_root_handle *accepted_root = NULL;
    struct cx_gc_root_handle *bind_endpoint_root = NULL;
    struct cx_gc_root_handle *endpoint_root = NULL;
    struct cx_gc_root_handle *first_task_root = NULL;
    struct cx_gc_root_handle *duplicate_task_root = NULL;
    uint32_t steps = 0u;
    int listener_initialized = 0;
    int client_initialized = 0;
    int passed = 0;

    if (listener == NULL || client == NULL || bind_endpoint == NULL) goto cleanup;
    listener_root = cx_gc_add_root(&listener, (cx_uint)sizeof(listener));
    client_root = cx_gc_add_root(&client, (cx_uint)sizeof(client));
    accepted_root = cx_gc_add_root(&accepted, (cx_uint)sizeof(accepted));
    bind_endpoint_root = cx_gc_add_root(&bind_endpoint,
        (cx_uint)sizeof(bind_endpoint));
    if (listener_root == NULL || client_root == NULL || accepted_root == NULL ||
        bind_endpoint_root == NULL) goto cleanup;

    cxnet_System_Net_Sockets_Socket___constructor__2(listener,
        cxnet_System_Net_AddressFamily_InterNetwork,
        cxnet_System_Net_Sockets_SocketType_Stream,
        cxnet_System_Net_Sockets_ProtocolType_Tcp);
    listener_initialized = 1;
    cxnet_System_Net_IPEndPoint___constructor__2(bind_endpoint,
        cxnet_System_Net_IPAddress_Loopback___get(), 0);
    cxnet_System_Net_Sockets_Socket_Bind(listener,
        (struct cxnet_System_Net_EndPoint *)bind_endpoint);
    cxnet_System_Net_Sockets_Socket_Listen__2(listener, 1);
    endpoint = (struct cxnet_System_Net_IPEndPoint *)
        cxnet_System_Net_Sockets_Socket_GetLocalEndPoint(listener);
    endpoint_root = cx_gc_add_root(&endpoint, (cx_uint)sizeof(endpoint));
    if (endpoint == NULL || endpoint_root == NULL) goto cleanup;

    cxnet_System_Net_Sockets_Socket___constructor__2(client,
        cxnet_System_Net_AddressFamily_InterNetwork,
        cxnet_System_Net_Sockets_SocketType_Stream,
        cxnet_System_Net_Sockets_ProtocolType_Tcp);
    client_initialized = 1;
    first_task = cxnet_System_Net_Sockets_Socket_ConnectAsync__5(client,
        (struct cxnet_System_Net_EndPoint *)endpoint);
    first_task_root = cx_gc_add_root(&first_task,
        (cx_uint)sizeof(first_task));
    if (first_task == NULL || first_task_root == NULL ||
        first_task->_state->_isCompleted) {
        fputs("first endpoint ConnectAsync did not remain pending\n", stderr);
        goto cleanup;
    }

    duplicate_task = cxnet_System_Net_Sockets_Socket_ConnectAsync__5(client,
        (struct cxnet_System_Net_EndPoint *)endpoint);
    duplicate_task_root = cx_gc_add_root(&duplicate_task,
        (cx_uint)sizeof(duplicate_task));
    if (duplicate_task == NULL || duplicate_task_root == NULL ||
        !duplicate_task->_state->_isCompleted ||
        !duplicate_task->_state->_isFaulted ||
        duplicate_task->_state->_isCanceled ||
        !cxcore_System_Exception_Matches(duplicate_task->_state->_exception,
            cxcore_System_InvalidOperationException___typeinfo)) {
        fputs("duplicate pending endpoint ConnectAsync was not rejected\n",
            stderr);
        goto cleanup;
    }

    cx_gc_collect();
    while (!first_task->_state->_isCompleted && steps < 100000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps += 1u;
    }
    if (!first_task->_state->_isCompleted ||
        first_task->_state->_isFaulted || first_task->_state->_isCanceled ||
        !cxnet_System_Net_Sockets_Socket_Connected___const_get(client)) {
        fputs("first endpoint connect did not survive duplicate rejection\n",
            stderr);
        goto cleanup;
    }
    accepted = cxnet_System_Net_Sockets_Socket_Accept(listener);
    if (accepted == NULL ||
        !cxnet_System_Net_Sockets_Socket_Connected___const_get(accepted)) {
        fputs("listener did not accept the original pending connect\n", stderr);
        goto cleanup;
    }
    {
        cx_byte sent_value = (cx_byte)'P';
        cx_byte received_value = 0u;
        struct cxcore_System_Collections_Array sent = {0};
        struct cxcore_System_Collections_Array received = {0};
        sent._length = 1u;
        sent._data = &sent_value;
        received._length = 1u;
        received._data = &received_value;
        passed = cxnet_System_Net_Sockets_Socket_Send(client, &sent, 0u, 1u) ==
                1 &&
            cxnet_System_Net_Sockets_Socket_Receive(accepted, &received, 0u,
                1u) == 1 && received_value == sent_value;
    }
    if (!passed) {
        fprintf(stderr,
            "pending duplicate-connect state regression failed steps=%u state=%u connected=%d\n",
            (unsigned)steps, (unsigned)client->_state,
            cxnet_System_Net_Sockets_Socket_Connected___const_get(client));
    }

cleanup:
    if (accepted != NULL) cxnet_System_Net_Sockets_Socket_Dispose(accepted);
    if (client_initialized) cxnet_System_Net_Sockets_Socket_Dispose(client);
    if (listener_initialized)
        cxnet_System_Net_Sockets_Socket_Dispose(listener);
    if (duplicate_task_root != NULL) cx_gc_remove_root(duplicate_task_root);
    if (first_task_root != NULL) cx_gc_remove_root(first_task_root);
    if (endpoint_root != NULL) cx_gc_remove_root(endpoint_root);
    if (bind_endpoint_root != NULL) cx_gc_remove_root(bind_endpoint_root);
    if (accepted_root != NULL) cx_gc_remove_root(accepted_root);
    if (client_root != NULL) cx_gc_remove_root(client_root);
    if (listener_root != NULL) cx_gc_remove_root(listener_root);
    return passed;
}

static int test_concurrent_pending_accepts(void) {
    struct cxnet_System_Net_Sockets_Socket *listener =
        (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Sockets_Socket___typeinfo.Size);
    struct cxnet_System_Net_Sockets_Socket *client_a =
        (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Sockets_Socket___typeinfo.Size);
    struct cxnet_System_Net_Sockets_Socket *client_b =
        (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Sockets_Socket___typeinfo.Size);
    struct cxnet_System_Net_Sockets_Socket *accepted_a = NULL;
    struct cxnet_System_Net_Sockets_Socket *accepted_b = NULL;
    struct cxnet_System_Net_IPEndPoint *bind_endpoint =
        (struct cxnet_System_Net_IPEndPoint *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_IPEndPoint___typeinfo.Size);
    struct cxnet_System_Net_IPEndPoint *endpoint = NULL;
    struct cxcore_System_Threading_Tasks_CancellationTokenSource *accept_cancel =
        (struct cxcore_System_Threading_Tasks_CancellationTokenSource *)
            cxcore_System_Memory_Alloc(
                cxcore_System_Threading_Tasks_CancellationTokenSource___typeinfo.Size);
    struct cxcore_System_Threading_Tasks_CancellationToken accept_cancel_token;
    struct cxcore_System_Threading_Tasks_Task *accept_a = NULL;
    struct cxcore_System_Threading_Tasks_Task *accept_b = NULL;
    struct cxcore_System_Threading_Tasks_TaskHandle *connect_a = NULL;
    struct cxcore_System_Threading_Tasks_TaskHandle *connect_b = NULL;
    struct cx_gc_root_handle *listener_root = cx_gc_add_root(
        &listener, (cx_uint)sizeof(listener));
    struct cx_gc_root_handle *client_a_root = cx_gc_add_root(
        &client_a, (cx_uint)sizeof(client_a));
    struct cx_gc_root_handle *client_b_root = cx_gc_add_root(
        &client_b, (cx_uint)sizeof(client_b));
    struct cx_gc_root_handle *accepted_a_root = cx_gc_add_root(
        &accepted_a, (cx_uint)sizeof(accepted_a));
    struct cx_gc_root_handle *accepted_b_root = cx_gc_add_root(
        &accepted_b, (cx_uint)sizeof(accepted_b));
    struct cx_gc_root_handle *bind_endpoint_root = cx_gc_add_root(
        &bind_endpoint, (cx_uint)sizeof(bind_endpoint));
    struct cx_gc_root_handle *accept_cancel_root = cx_gc_add_root(
        &accept_cancel, (cx_uint)sizeof(accept_cancel));
    struct cx_gc_root_handle *endpoint_root;
    struct cx_gc_root_handle *accept_a_root = cx_gc_add_root(
        &accept_a, (cx_uint)sizeof(accept_a));
    struct cx_gc_root_handle *accept_b_root = cx_gc_add_root(
        &accept_b, (cx_uint)sizeof(accept_b));
    struct cx_gc_root_handle *connect_a_root = cx_gc_add_root(
        &connect_a, (cx_uint)sizeof(connect_a));
    struct cx_gc_root_handle *connect_b_root = cx_gc_add_root(
        &connect_b, (cx_uint)sizeof(connect_b));
    uint32_t steps = 0u;
    int passed = 0;

    cxnet_System_Net_Sockets_Socket___constructor__2(listener,
        cxnet_System_Net_AddressFamily_InterNetwork,
        cxnet_System_Net_Sockets_SocketType_Stream,
        cxnet_System_Net_Sockets_ProtocolType_Tcp);
    cxnet_System_Net_IPEndPoint___constructor__2(bind_endpoint,
        cxnet_System_Net_IPAddress_Loopback___get(), 0);
    cxnet_System_Net_Sockets_Socket_Bind(listener,
        (struct cxnet_System_Net_EndPoint *)bind_endpoint);
    cxnet_System_Net_Sockets_Socket_Listen__2(listener, 4);
    endpoint = cxnet_System_Net_Sockets_Socket_GetLocalEndPoint(listener);
    endpoint_root = cx_gc_add_root(&endpoint, (cx_uint)sizeof(endpoint));
    cxcore_System_Threading_Tasks_CancellationTokenSource___constructor(
        accept_cancel);
    accept_cancel_token =
        cxcore_System_Threading_Tasks_CancellationTokenSource_Token___const_get(
            accept_cancel);
    accept_a = cxnet_System_Net_Sockets_Socket_AcceptAsync__2(listener,
        accept_cancel_token);
    accept_b = cxnet_System_Net_Sockets_Socket_AcceptAsync(listener);
    if (cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(accept_a) ||
        cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(accept_b)) {
        fputs("two fresh accepts did not remain pending together\n", stderr);
        return 1;
    }
    cx_gc_collect();
    cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(accept_cancel);
    steps = 0u;
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(accept_a) &&
        steps < 100000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (!cxcore_System_Threading_Tasks_Task_IsCanceled___const_get(accept_a) ||
        cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(accept_b)) {
        fputs("canceling one pending accept affected its peer waiter\n", stderr);
        return 1;
    }
    cxnet_System_Net_Sockets_Socket___constructor__2(client_a,
        cxnet_System_Net_AddressFamily_InterNetwork,
        cxnet_System_Net_Sockets_SocketType_Stream,
        cxnet_System_Net_Sockets_ProtocolType_Tcp);
    cxnet_System_Net_Sockets_Socket___constructor__2(client_b,
        cxnet_System_Net_AddressFamily_InterNetwork,
        cxnet_System_Net_Sockets_SocketType_Stream,
        cxnet_System_Net_Sockets_ProtocolType_Tcp);
    connect_a = cxnet_System_Net_Sockets_Socket_ConnectAsync__5(client_a,
        (struct cxnet_System_Net_EndPoint *)endpoint);
    cx_gc_collect();
    while ((!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(accept_b) ||
            !cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(connect_a)) &&
        steps < 100000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(accept_b) ||
        accept_b->_isFaulted || accept_b->_isCanceled ||
        !cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(connect_a) ||
        connect_a->_state->_isFaulted || connect_a->_state->_isCanceled) {
        fputs("surviving accept did not take the first client\n", stderr);
        return 1;
    }
    cxcore_System_Threading_Tasks_TaskHandle_GetResult(connect_a);
    accepted_b = cx_generic_7c4851112331385bf3ec935e4daabb246ea0a9a28971c93643ab4b4268d85138(
        (const struct cxcore_System_Threading_Tasks_Task *)accept_b);
    if (accepted_b == NULL ||
        !cxnet_System_Net_Sockets_Socket_Connected___const_get(accepted_b)) {
        fputs("surviving concurrent accept returned no connected socket\n", stderr);
        return 1;
    }
    accept_a = cxnet_System_Net_Sockets_Socket_AcceptAsync(listener);
    if (cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(accept_a)) {
        fputs("reused listener accept completed before the second client\n", stderr);
        return 1;
    }
    connect_b = cxnet_System_Net_Sockets_Socket_ConnectAsync__5(client_b,
        (struct cxnet_System_Net_EndPoint *)endpoint);
    cx_gc_collect();
    steps = 0u;
    while ((!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(accept_a) ||
            !cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(connect_b)) &&
        steps < 100000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(accept_a) &&
        !accept_a->_isFaulted && !accept_a->_isCanceled &&
        cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(connect_b) &&
        !connect_b->_state->_isFaulted && !connect_b->_state->_isCanceled) {
        cxcore_System_Threading_Tasks_TaskHandle_GetResult(connect_b);
        accepted_a = cx_generic_7c4851112331385bf3ec935e4daabb246ea0a9a28971c93643ab4b4268d85138(
            (const struct cxcore_System_Threading_Tasks_Task *)accept_a);
        passed = accepted_a != NULL && accepted_a != accepted_b &&
            cxnet_System_Net_Sockets_Socket_Connected___const_get(accepted_a) &&
            cxnet_System_Net_Sockets_Socket_Connected___const_get(accepted_b);
    }
    if (passed) {
        accept_a = cxnet_System_Net_Sockets_Socket_AcceptAsync(listener);
        accept_b = cxnet_System_Net_Sockets_Socket_AcceptAsync(listener);
        if (cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(accept_a) ||
            cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(accept_b)) {
            fputs("two more accepts did not remain pending before listener disposal\n",
                stderr);
            passed = 0;
        } else {
            cx_gc_collect();
            cxnet_System_Net_Sockets_Socket_Dispose(listener);
            cxnet_System_Net_Sockets_Socket_Dispose(listener);
            steps = 0u;
            while ((!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(accept_a) ||
                    !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(accept_b)) &&
                steps < 100000u) {
                if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                    Sleep(1u);
#else
                    usleep(1000u);
#endif
                }
                steps++;
            }
            passed = cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(accept_a) &&
                cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(accept_b) &&
                accept_a->_isFaulted && !accept_a->_isCanceled &&
                accept_b->_isFaulted && !accept_b->_isCanceled &&
                cxcore_System_Exception_Matches(accept_a->_exception,
                    cxcore_System_ObjectDisposedException___typeinfo) &&
                cxcore_System_Exception_Matches(accept_b->_exception,
                    cxcore_System_ObjectDisposedException___typeinfo) &&
                cxnet_System_Net_Sockets_Socket_Connected___const_get(client_a) &&
                cxnet_System_Net_Sockets_Socket_Connected___const_get(client_b) &&
                cxnet_System_Net_Sockets_Socket_Connected___const_get(accepted_a) &&
                cxnet_System_Net_Sockets_Socket_Connected___const_get(accepted_b);
        }
    }
    if (!passed) {
        fprintf(stderr, "concurrent accept lifecycle failed: steps=%u accept=%d/%d connect=%d/%d\n",
            (unsigned)steps,
            cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(accept_a),
            cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(accept_b),
            cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(connect_a),
            cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(connect_b));
    }
    if (accepted_a != NULL) cxnet_System_Net_Sockets_Socket_Dispose(accepted_a);
    if (accepted_b != NULL) cxnet_System_Net_Sockets_Socket_Dispose(accepted_b);
    cxnet_System_Net_Sockets_Socket_Dispose(client_a);
    cxnet_System_Net_Sockets_Socket_Dispose(client_b);
    cxnet_System_Net_Sockets_Socket_Dispose(listener);
    cx_gc_remove_root(connect_b_root);
    cx_gc_remove_root(connect_a_root);
    cx_gc_remove_root(accept_b_root);
    cx_gc_remove_root(accept_a_root);
    cx_gc_remove_root(endpoint_root);
    cx_gc_remove_root(accept_cancel_root);
    cx_gc_remove_root(bind_endpoint_root);
    cx_gc_remove_root(accepted_b_root);
    cx_gc_remove_root(accepted_a_root);
    cx_gc_remove_root(client_b_root);
    cx_gc_remove_root(client_a_root);
    cx_gc_remove_root(listener_root);
    return passed ? 0 : 1;
}

static int test_concurrent_accepts_succeed(void) {
    struct cxnet_System_Net_Sockets_Socket *listener =
        (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Sockets_Socket___typeinfo.Size);
    struct cxnet_System_Net_Sockets_Socket *client_a =
        (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Sockets_Socket___typeinfo.Size);
    struct cxnet_System_Net_Sockets_Socket *client_b =
        (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Sockets_Socket___typeinfo.Size);
    struct cxnet_System_Net_Sockets_Socket *accepted_a = NULL;
    struct cxnet_System_Net_Sockets_Socket *accepted_b = NULL;
    struct cxnet_System_Net_IPEndPoint *bind_endpoint =
        (struct cxnet_System_Net_IPEndPoint *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_IPEndPoint___typeinfo.Size);
    struct cxnet_System_Net_IPEndPoint *endpoint = NULL;
    struct cxcore_System_Threading_Tasks_Task *accept_a = NULL;
    struct cxcore_System_Threading_Tasks_Task *accept_b = NULL;
    struct cxcore_System_Threading_Tasks_TaskHandle *connect_a = NULL;
    struct cxcore_System_Threading_Tasks_TaskHandle *connect_b = NULL;
    struct cx_gc_root_handle *listener_root = cx_gc_add_root(
        &listener, (cx_uint)sizeof(listener));
    struct cx_gc_root_handle *client_a_root = cx_gc_add_root(
        &client_a, (cx_uint)sizeof(client_a));
    struct cx_gc_root_handle *client_b_root = cx_gc_add_root(
        &client_b, (cx_uint)sizeof(client_b));
    struct cx_gc_root_handle *accepted_a_root = cx_gc_add_root(
        &accepted_a, (cx_uint)sizeof(accepted_a));
    struct cx_gc_root_handle *accepted_b_root = cx_gc_add_root(
        &accepted_b, (cx_uint)sizeof(accepted_b));
    struct cx_gc_root_handle *bind_endpoint_root = cx_gc_add_root(
        &bind_endpoint, (cx_uint)sizeof(bind_endpoint));
    struct cx_gc_root_handle *endpoint_root = NULL;
    struct cx_gc_root_handle *accept_a_root = cx_gc_add_root(
        &accept_a, (cx_uint)sizeof(accept_a));
    struct cx_gc_root_handle *accept_b_root = cx_gc_add_root(
        &accept_b, (cx_uint)sizeof(accept_b));
    struct cx_gc_root_handle *connect_a_root = cx_gc_add_root(
        &connect_a, (cx_uint)sizeof(connect_a));
    struct cx_gc_root_handle *connect_b_root = cx_gc_add_root(
        &connect_b, (cx_uint)sizeof(connect_b));
    struct cxcore_System_Collections_Array send_a = {0};
    struct cxcore_System_Collections_Array send_b = {0};
    struct cxcore_System_Collections_Array receive_a = {0};
    struct cxcore_System_Collections_Array receive_b = {0};
    cx_byte send_a_value = (cx_byte)'A';
    cx_byte send_b_value = (cx_byte)'B';
    cx_byte receive_a_value = 0u;
    cx_byte receive_b_value = 0u;
    uint32_t steps = 0u;
    int passed = 0;

    send_a._length = 1u;
    send_a._data = &send_a_value;
    send_b._length = 1u;
    send_b._data = &send_b_value;
    receive_a._length = 1u;
    receive_a._data = &receive_a_value;
    receive_b._length = 1u;
    receive_b._data = &receive_b_value;

    cxnet_System_Net_Sockets_Socket___constructor__2(listener,
        cxnet_System_Net_AddressFamily_InterNetwork,
        cxnet_System_Net_Sockets_SocketType_Stream,
        cxnet_System_Net_Sockets_ProtocolType_Tcp);
    cxnet_System_Net_IPEndPoint___constructor__2(bind_endpoint,
        cxnet_System_Net_IPAddress_Loopback___get(), 0);
    cxnet_System_Net_Sockets_Socket_Bind(listener,
        (struct cxnet_System_Net_EndPoint *)bind_endpoint);
    cxnet_System_Net_Sockets_Socket_Listen__2(listener, 4);
    endpoint = cxnet_System_Net_Sockets_Socket_GetLocalEndPoint(listener);
    endpoint_root = cx_gc_add_root(&endpoint, (cx_uint)sizeof(endpoint));
    cxnet_System_Net_Sockets_Socket___constructor__2(client_a,
        cxnet_System_Net_AddressFamily_InterNetwork,
        cxnet_System_Net_Sockets_SocketType_Stream,
        cxnet_System_Net_Sockets_ProtocolType_Tcp);
    cxnet_System_Net_Sockets_Socket___constructor__2(client_b,
        cxnet_System_Net_AddressFamily_InterNetwork,
        cxnet_System_Net_Sockets_SocketType_Stream,
        cxnet_System_Net_Sockets_ProtocolType_Tcp);

    accept_a = cxnet_System_Net_Sockets_Socket_AcceptAsync(listener);
    accept_b = cxnet_System_Net_Sockets_Socket_AcceptAsync(listener);
    if (cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(accept_a) ||
        cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(accept_b)) {
        fputs("concurrent accepts completed before either client connected\n",
            stderr);
        goto cleanup;
    }
    connect_a = cxnet_System_Net_Sockets_Socket_ConnectAsync__5(client_a,
        (struct cxnet_System_Net_EndPoint *)endpoint);
    connect_b = cxnet_System_Net_Sockets_Socket_ConnectAsync__5(client_b,
        (struct cxnet_System_Net_EndPoint *)endpoint);
    cx_gc_collect();
    while ((!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(accept_a) ||
            !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(accept_b) ||
            !cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(connect_a) ||
            !cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(connect_b)) &&
        steps < 100000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(accept_a) ||
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(accept_b) ||
        accept_a->_isFaulted || accept_a->_isCanceled ||
        accept_b->_isFaulted || accept_b->_isCanceled ||
        !cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(connect_a) ||
        connect_a->_state->_isFaulted || connect_a->_state->_isCanceled ||
        !cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(connect_b) ||
        connect_b->_state->_isFaulted || connect_b->_state->_isCanceled) {
        fputs("two pending accepts did not complete for two clients\n", stderr);
        goto cleanup;
    }
    cxcore_System_Threading_Tasks_TaskHandle_GetResult(connect_a);
    cxcore_System_Threading_Tasks_TaskHandle_GetResult(connect_b);
    accepted_a = cx_generic_7c4851112331385bf3ec935e4daabb246ea0a9a28971c93643ab4b4268d85138(
        (const struct cxcore_System_Threading_Tasks_Task *)accept_a);
    accepted_b = cx_generic_7c4851112331385bf3ec935e4daabb246ea0a9a28971c93643ab4b4268d85138(
        (const struct cxcore_System_Threading_Tasks_Task *)accept_b);
    if (accepted_a == NULL || accepted_b == NULL || accepted_a == accepted_b ||
        !cxnet_System_Net_Sockets_Socket_Connected___const_get(accepted_a) ||
        !cxnet_System_Net_Sockets_Socket_Connected___const_get(accepted_b) ||
        cxnet_System_Net_Sockets_Socket_Send(client_a, &send_a, 0u, 1u) != 1 ||
        cxnet_System_Net_Sockets_Socket_Send(client_b, &send_b, 0u, 1u) != 1 ||
        cxnet_System_Net_Sockets_Socket_Receive(accepted_a, &receive_a, 0u, 1u) != 1 ||
        cxnet_System_Net_Sockets_Socket_Receive(accepted_b, &receive_b, 0u, 1u) != 1 ||
        receive_a_value == receive_b_value ||
        !((receive_a_value == (cx_byte)'A' && receive_b_value == (cx_byte)'B') ||
          (receive_a_value == (cx_byte)'B' && receive_b_value == (cx_byte)'A'))) {
        fputs("concurrent accepts did not return distinct usable connections\n",
            stderr);
        goto cleanup;
    }
    passed = 1;

cleanup:
    if (accepted_a != NULL) cxnet_System_Net_Sockets_Socket_Dispose(accepted_a);
    if (accepted_b != NULL) cxnet_System_Net_Sockets_Socket_Dispose(accepted_b);
    if (client_a != NULL) cxnet_System_Net_Sockets_Socket_Dispose(client_a);
    if (client_b != NULL) cxnet_System_Net_Sockets_Socket_Dispose(client_b);
    if (listener != NULL) cxnet_System_Net_Sockets_Socket_Dispose(listener);
    if (connect_b_root != NULL) cx_gc_remove_root(connect_b_root);
    if (connect_a_root != NULL) cx_gc_remove_root(connect_a_root);
    if (accept_b_root != NULL) cx_gc_remove_root(accept_b_root);
    if (accept_a_root != NULL) cx_gc_remove_root(accept_a_root);
    if (endpoint_root != NULL) cx_gc_remove_root(endpoint_root);
    if (bind_endpoint_root != NULL) cx_gc_remove_root(bind_endpoint_root);
    if (accepted_b_root != NULL) cx_gc_remove_root(accepted_b_root);
    if (accepted_a_root != NULL) cx_gc_remove_root(accepted_a_root);
    if (client_b_root != NULL) cx_gc_remove_root(client_b_root);
    if (client_a_root != NULL) cx_gc_remove_root(client_a_root);
    if (listener_root != NULL) cx_gc_remove_root(listener_root);
    return passed;
}

static int create_stream_read_pair(uint64_t *connected_handle,
    uintptr_t *peer_handle) {
#ifdef _WIN32
    WSADATA startup_data;
    SOCKET listener = INVALID_SOCKET;
    SOCKET connected = INVALID_SOCKET;
    SOCKET peer = INVALID_SOCKET;
    struct sockaddr_in address;
    int address_length = (int)sizeof(address);
    if (WSAStartup(MAKEWORD(2, 2), &startup_data) != 0) return 0;
    listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET) goto failed;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(listener, (const struct sockaddr *)&address, sizeof(address)) != 0 ||
        listen(listener, 1) != 0 ||
        getsockname(listener, (struct sockaddr *)&address, &address_length) != 0)
        goto failed;
    connected = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (connected == INVALID_SOCKET ||
        connect(connected, (const struct sockaddr *)&address, sizeof(address)) != 0)
        goto failed;
    peer = accept(listener, NULL, NULL);
    if (peer == INVALID_SOCKET) goto failed;
    closesocket(listener);
    *connected_handle = (uint64_t)(uintptr_t)connected;
    *peer_handle = (uintptr_t)peer;
    return 1;
failed:
    if (listener != INVALID_SOCKET) closesocket(listener);
    if (connected != INVALID_SOCKET) closesocket(connected);
    if (peer != INVALID_SOCKET) closesocket(peer);
    WSACleanup();
    return 0;
#else
    int listener = -1;
    int connected = -1;
    int peer = -1;
    struct sockaddr_in address;
    socklen_t address_length = (socklen_t)sizeof(address);
    listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener < 0) goto failed;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(listener, (const struct sockaddr *)&address, sizeof(address)) != 0 ||
        listen(listener, 1) != 0 ||
        getsockname(listener, (struct sockaddr *)&address, &address_length) != 0)
        goto failed;
    connected = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (connected < 0 || connect(connected, (const struct sockaddr *)&address,
            sizeof(address)) != 0) goto failed;
    peer = accept(listener, NULL, NULL);
    if (peer < 0) goto failed;
    close(listener);
    *connected_handle = (uint64_t)(uintptr_t)connected;
    *peer_handle = (uintptr_t)peer;
    return 1;
failed:
    if (listener >= 0) close(listener);
    if (connected >= 0) close(connected);
    if (peer >= 0) close(peer);
    return 0;
#endif
}

static void close_stream_read_peer(uintptr_t peer_handle) {
#ifdef _WIN32
    closesocket((SOCKET)peer_handle);
    WSACleanup();
#else
    close((int)peer_handle);
#endif
}

static int stream_peer_observes_eof(uintptr_t peer_handle) {
    char byte;
#ifdef _WIN32
    return recv((SOCKET)peer_handle, &byte, 1, 0) == 0;
#else
    return recv((int)peer_handle, &byte, 1, 0) == 0;
#endif
}

static int stream_peer_observes_close_or_reset(uintptr_t peer_handle) {
    char byte;
#ifdef _WIN32
    int result = recv((SOCKET)peer_handle, &byte, 1, 0);
    int error_code = result == SOCKET_ERROR ? WSAGetLastError() : 0;
    return result == 0 || error_code == WSAECONNRESET ||
        error_code == WSAECONNABORTED;
#else
    return recv((int)peer_handle, &byte, 1, 0) == 0;
#endif
}

struct socket_dispose_read_race {
    struct cxnet_System_Net_Sockets_Socket *socket;
    struct cxnet_System_Net_Sockets_NetworkStream *network_stream;
    int dispose_network_stream;
    struct cxcore_System_Collections_Array *buffer;
    struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *task;
    volatile int ready;
    volatile int start;
    volatile int completed;
    volatile int threw_exception;
    volatile int failed;
};

static void socket_dispose_race_store(volatile int *location, int value) {
#ifdef _WIN32
    InterlockedExchange((volatile LONG *)location, (LONG)value);
#else
    __sync_lock_test_and_set(location, value);
#endif
}

static int socket_dispose_race_load(volatile int *location) {
#ifdef _WIN32
    return (int)InterlockedCompareExchange((volatile LONG *)location, 0, 0);
#else
    return __sync_val_compare_and_swap(location, 0, 0);
#endif
}

#ifdef _WIN32
static DWORD WINAPI socket_peer_payload_drain_worker(void *argument) {
#else
static void *socket_peer_payload_drain_worker(void *argument) {
#endif
    struct socket_peer_payload_drain *drain =
        (struct socket_peer_payload_drain *)argument;
    unsigned char bytes[4096];
    size_t received = 0u;
#ifdef _WIN32
    DWORD timeout = 5000u;
    (void)setsockopt((SOCKET)drain->peer, SOL_SOCKET, SO_RCVTIMEO,
        (const char *)&timeout, (int)sizeof(timeout));
#else
    struct timeval timeout = {5, 0};
    (void)setsockopt((int)drain->peer, SOL_SOCKET, SO_RCVTIMEO,
        &timeout, (socklen_t)sizeof(timeout));
#endif
    while (received < drain->filled_bytes + drain->trailing_bytes) {
        size_t remaining = drain->filled_bytes + drain->trailing_bytes - received;
        size_t requested = remaining < sizeof(bytes) ? remaining : sizeof(bytes);
#ifdef _WIN32
        int amount = recv((SOCKET)drain->peer, (char *)bytes,
            (int)requested, 0);
#else
        ssize_t amount = recv((int)drain->peer, bytes, requested, 0);
#endif
        size_t index;
        if (amount <= 0 || (size_t)amount > remaining) {
            socket_dispose_race_store(&drain->failed, 1);
            break;
        }
        for (index = 0u; index < (size_t)amount; index++) {
            unsigned char expected = received + index < drain->filled_bytes
                ? 0xa7u : 0x5au;
            if (bytes[index] != expected) {
                socket_dispose_race_store(&drain->failed, 1);
                break;
            }
        }
        if (socket_dispose_race_load(&drain->failed)) break;
        received += (size_t)amount;
    }
    if (!socket_dispose_race_load(&drain->failed)) {
        unsigned char reply = 0x36u;
#ifdef _WIN32
        int sent = send((SOCKET)drain->peer, (const char *)&reply, 1, 0);
#else
        ssize_t sent = send((int)drain->peer, (const char *)&reply, 1, 0);
#endif
        if (sent != 1) socket_dispose_race_store(&drain->failed, 1);
    }
    socket_dispose_race_store(&drain->completed, 1);
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

#ifdef _WIN32
static DWORD WINAPI socket_cross_thread_dispose_worker(void *argument) {
#else
static void *socket_cross_thread_dispose_worker(void *argument) {
#endif
    struct socket_cross_thread_dispose *race =
        (struct socket_cross_thread_dispose *)argument;
    cx_bool attached = cx_gc_enter_thread();
    if (!attached) {
        socket_dispose_race_store(&race->failed, 1);
        socket_dispose_race_store(&race->ready, 1);
        socket_dispose_race_store(&race->completed, 1);
#ifdef _WIN32
        return 0;
#else
        return NULL;
#endif
    }
    socket_dispose_race_store(&race->ready, 1);
    while (!socket_dispose_race_load(&race->start)) {
#ifdef _WIN32
        Sleep(0u);
#else
        sched_yield();
#endif
    }
    {
        struct cx_exception_frame frame;
        cx_exception_push(&frame);
        if (setjmp(frame.environment) == 0) {
            if (race->network_stream != NULL) {
                cxnet_System_Net_Sockets_NetworkStream_Dispose(
                    race->network_stream);
            } else {
                cxnet_System_Net_Sockets_Socket_Dispose(race->socket);
            }
            cx_exception_pop(&frame);
        } else {
            socket_dispose_race_store(&race->failed, 1);
            cx_exception_clear();
            cx_exception_pop(&frame);
        }
    }
    cx_gc_leave_thread(attached);
    socket_dispose_race_store(&race->completed, 1);
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

static int test_endpoint_connect_cross_thread_dispose(void) {
    uint32_t iteration;
    uint32_t pending_iterations = 0u;
    for (iteration = 0u; iteration < 24u; iteration++) {
        struct cxnet_System_Net_Sockets_Socket *listener =
            (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
                cxnet_System_Net_Sockets_Socket___typeinfo.Size);
        struct cxnet_System_Net_Sockets_Socket *client =
            (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
                cxnet_System_Net_Sockets_Socket___typeinfo.Size);
        struct cxnet_System_Net_IPEndPoint *bind_endpoint =
            (struct cxnet_System_Net_IPEndPoint *)cxcore_System_Memory_Alloc(
                cxnet_System_Net_IPEndPoint___typeinfo.Size);
        struct cxnet_System_Net_IPEndPoint *remote_endpoint = NULL;
        struct cxcore_System_Threading_Tasks_TaskHandle *task = NULL;
        struct socket_cross_thread_dispose dispose_race = {0};
        struct cx_gc_root_handle *listener_root = NULL;
        struct cx_gc_root_handle *client_root = NULL;
        struct cx_gc_root_handle *bind_endpoint_root = NULL;
        struct cx_gc_root_handle *remote_endpoint_root = NULL;
        struct cx_gc_root_handle *task_root = NULL;
        cx_action completion_action = {0};
        uint32_t steps = 0u;
        int connect_was_pending = 0;
        int listener_initialized = 0;
        int client_initialized = 0;
        int passed = 0;
        #ifdef _WIN32
        HANDLE worker = NULL;
        #else
        pthread_t worker;
        int worker_started = 0;
        #endif

        if (listener == NULL || client == NULL || bind_endpoint == NULL)
            goto cleanup;
        listener_root = cx_gc_add_root(&listener,
            (cx_uint)sizeof(listener));
        client_root = cx_gc_add_root(&client, (cx_uint)sizeof(client));
        bind_endpoint_root = cx_gc_add_root(&bind_endpoint,
            (cx_uint)sizeof(bind_endpoint));
        if (listener_root == NULL || client_root == NULL ||
            bind_endpoint_root == NULL) goto cleanup;
        cxnet_System_Net_Sockets_Socket___constructor__2(listener,
            cxnet_System_Net_AddressFamily_InterNetwork,
            cxnet_System_Net_Sockets_SocketType_Stream,
            cxnet_System_Net_Sockets_ProtocolType_Tcp);
        listener_initialized = 1;
        cxnet_System_Net_IPEndPoint___constructor__2(bind_endpoint,
            cxnet_System_Net_IPAddress_Loopback___get(), 0);
        cxnet_System_Net_Sockets_Socket_Bind(listener,
            (struct cxnet_System_Net_EndPoint *)bind_endpoint);
        cxnet_System_Net_Sockets_Socket_Listen__2(listener, 1);
        remote_endpoint = cxnet_System_Net_Sockets_Socket_GetLocalEndPoint(
            listener);
        remote_endpoint_root = cx_gc_add_root(&remote_endpoint,
            (cx_uint)sizeof(remote_endpoint));
        if (remote_endpoint == NULL || remote_endpoint_root == NULL)
            goto cleanup;
        cxnet_System_Net_Sockets_Socket___constructor__2(client,
            cxnet_System_Net_AddressFamily_InterNetwork,
            cxnet_System_Net_Sockets_SocketType_Stream,
            cxnet_System_Net_Sockets_ProtocolType_Tcp);
        client_initialized = 1;
        task = cxnet_System_Net_Sockets_Socket_ConnectAsync__5(client,
            (struct cxnet_System_Net_EndPoint *)remote_endpoint);
        task_root = cx_gc_add_root(&task, (cx_uint)sizeof(task));
        if (task == NULL || task_root == NULL) goto cleanup;
        connect_was_pending = !task->_state->_isCompleted;
        if (connect_was_pending) pending_iterations++;
        connect_race_completion_count = 0u;
        completion_action.__function = on_connect_race_completed;
        completion_action.__environment = NULL;
        completion_action.__target = NULL;
        cxcore_System_Threading_Tasks_TaskHandle_OnCompleted(task,
            completion_action);

        dispose_race.socket = client;
        #ifdef _WIN32
        worker = CreateThread(NULL, 0,
            socket_cross_thread_dispose_worker, &dispose_race, 0, NULL);
        if (worker == NULL) goto cleanup;
        #else
        if (pthread_create(&worker, NULL,
                socket_cross_thread_dispose_worker, &dispose_race) != 0)
            goto cleanup;
        worker_started = 1;
        #endif
        while (!socket_dispose_race_load(&dispose_race.ready)) {
            #ifdef _WIN32
            Sleep(0u);
            #else
            sched_yield();
            #endif
        }
        socket_dispose_race_store(&dispose_race.start, 1);
        cx_gc_collect();
        while ((!task->_state->_isCompleted ||
                !socket_dispose_race_load(&dispose_race.completed) ||
                connect_race_completion_count == 0u) && steps < 100000u) {
            if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
                #ifdef _WIN32
                Sleep(1u);
                #else
                usleep(1000u);
                #endif
            }
            steps++;
        }
        #ifdef _WIN32
        WaitForSingleObject(worker, INFINITE);
        CloseHandle(worker);
        worker = NULL;
        #else
        pthread_join(worker, NULL);
        worker_started = 0;
        #endif
        for (steps = 0u; steps < 8u; steps++) {
            if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) break;
        }
        if (task->_state->_isCompleted && !task->_state->_isCanceled &&
            !task->_state->_isFaulted) {
            cxcore_System_Threading_Tasks_TaskHandle_GetResult(task);
            passed = !socket_dispose_race_load(&dispose_race.failed) &&
                socket_dispose_race_load(&dispose_race.completed) &&
                connect_race_completion_count == 1u && client->_disposed &&
                !cxnet_System_Net_Sockets_Socket_Connected___const_get(client);
        } else {
            int disposed_error = task->_state->_isFaulted &&
                !task->_state->_isCanceled &&
                cxcore_System_Exception_Matches(task->_state->_exception,
                    cxcore_System_ObjectDisposedException___typeinfo);
            int canceled = task->_state->_isCanceled &&
                !task->_state->_isFaulted;
            passed = !socket_dispose_race_load(&dispose_race.failed) &&
                socket_dispose_race_load(&dispose_race.completed) &&
                connect_race_completion_count == 1u && client->_disposed &&
                !cxnet_System_Net_Sockets_Socket_Connected___const_get(client) &&
                (disposed_error || canceled);
        }
        if (!passed) {
            fprintf(stderr,
                "endpoint connect/dispose race failed at iteration %u (pending=%d completed=%d canceled=%d faulted=%d disposed=%d connected=%d callback=%u workerFailed=%d workerDone=%d steps=%u)\n",
                (unsigned)iteration, connect_was_pending,
                task->_state->_isCompleted, task->_state->_isCanceled,
                task->_state->_isFaulted, client->_disposed,
                cxnet_System_Net_Sockets_Socket_Connected___const_get(client),
                (unsigned)connect_race_completion_count,
                socket_dispose_race_load(&dispose_race.failed),
                socket_dispose_race_load(&dispose_race.completed),
                (unsigned)steps);
        }

cleanup:
        #ifdef _WIN32
        if (worker != NULL) {
            socket_dispose_race_store(&dispose_race.start, 1);
            WaitForSingleObject(worker, INFINITE);
            CloseHandle(worker);
        }
        #else
        if (worker_started) {
            socket_dispose_race_store(&dispose_race.start, 1);
            pthread_join(worker, NULL);
        }
        #endif
        if (client_initialized)
            cxnet_System_Net_Sockets_Socket_Dispose(client);
        if (listener_initialized)
            cxnet_System_Net_Sockets_Socket_Dispose(listener);
        if (task_root != NULL) cx_gc_remove_root(task_root);
        if (remote_endpoint_root != NULL)
            cx_gc_remove_root(remote_endpoint_root);
        if (bind_endpoint_root != NULL)
            cx_gc_remove_root(bind_endpoint_root);
        if (client_root != NULL) cx_gc_remove_root(client_root);
        if (listener_root != NULL) cx_gc_remove_root(listener_root);
        if (!passed) return 0;
    }
    if (pending_iterations == 0u) {
        fputs("endpoint connect/dispose race did not observe a pending connect\n",
            stderr);
        return 0;
    }
    return 1;
}

#ifdef _WIN32
static DWORD WINAPI socket_dispose_read_worker(void *argument) {
#else
static void *socket_dispose_read_worker(void *argument) {
#endif
    struct socket_dispose_read_race *race =
        (struct socket_dispose_read_race *)argument;
    cx_bool attached = cx_gc_enter_thread();
    if (!attached) {
        socket_dispose_race_store(&race->failed, 1);
        socket_dispose_race_store(&race->ready, 1);
        socket_dispose_race_store(&race->completed, 1);
#ifdef _WIN32
        return 0;
#else
        return NULL;
#endif
    }
    socket_dispose_race_store(&race->ready, 1);
    while (!socket_dispose_race_load(&race->start)) {
#ifdef _WIN32
        Sleep(0u);
#else
        sched_yield();
#endif
    }
    if (race->dispose_network_stream) {
        struct cx_exception_frame frame;
        cx_exception_push(&frame);
        if (setjmp(frame.environment) == 0) {
            cxnet_System_Net_Sockets_NetworkStream_Dispose(
                race->network_stream);
            cx_exception_pop(&frame);
        } else {
            socket_dispose_race_store(&race->failed, 1);
            cx_exception_pop(&frame);
            cx_exception_clear();
        }
        cx_gc_leave_thread(attached);
        socket_dispose_race_store(&race->completed, 1);
#ifdef _WIN32
        return 0;
#else
        return NULL;
#endif
    }
    {
        struct cx_exception_frame frame;
        cx_exception_push(&frame);
        if (setjmp(frame.environment) == 0) {
            if (race->network_stream != NULL) {
                struct cxcore_System_Threading_Tasks_CancellationToken token = {0};
                race->task = cxnet_System_Net_Sockets_NetworkStream_ReadAsync(
                    race->network_stream, race->buffer, 0u, 1u, token);
            } else {
                race->task = cxnet_System_Net_Sockets_Socket_ReadAsync(
                    race->socket, race->buffer, 0u, 1u);
            }
            cx_exception_pop(&frame);
            if (race->task == NULL) {
                socket_dispose_race_store(&race->failed, 1);
            }
        } else {
            struct cxcore_System_Exception *exception =
                (struct cxcore_System_Exception *)cx_exception_current();
            cx_exception_pop(&frame);
            (void)exception;
            socket_dispose_race_store(&race->threw_exception, 1);
            cx_exception_clear();
        }
    }
    cx_gc_leave_thread(attached);
    socket_dispose_race_store(&race->completed, 1);
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

static int send_stream_read_byte(uintptr_t peer_handle, cx_byte value);

static int test_managed_socket_read_dispose_race(int use_network_stream) {
    int owns_socket = use_network_stream == 1;
    int is_nonowning_stream_race = use_network_stream == 2;
    uint32_t iteration;
    for (iteration = 0u; iteration < 24u; iteration++) {
        uint64_t connected_handle = 0u;
        uintptr_t peer_handle = (uintptr_t)0;
        struct socket_dispose_read_race race = {0};
        struct cx_gc_root_handle *stream_root = NULL;
        struct cx_gc_root_handle *socket_root;
        struct cx_gc_root_handle *buffer_root;
        struct cx_gc_root_handle *task_root;
        uint32_t steps = 0u;
#ifdef _WIN32
        HANDLE worker;
#else
        pthread_t worker;
#endif
        if (!create_stream_read_pair(&connected_handle, &peer_handle)) {
            fputs("could not create managed socket read/dispose race pair\n",
                stderr);
            return 0;
        }
        race.socket = (struct cxnet_System_Net_Sockets_Socket *)
            cxcore_System_Memory_Alloc(
                cxnet_System_Net_Sockets_Socket___typeinfo.Size);
        cxnet_System_Net_Sockets_Socket___constructor(race.socket,
            (cx_ulong)connected_handle);
        if (use_network_stream) {
            race.network_stream = (struct cxnet_System_Net_Sockets_NetworkStream *)
                cxcore_System_Memory_Alloc((cx_uint)sizeof(
                    struct cxnet_System_Net_Sockets_NetworkStream));
            cxnet_System_Net_Sockets_NetworkStream___constructor__2(
                race.network_stream, race.socket, owns_socket);
            stream_root = cx_gc_add_root(&race.network_stream,
                (cx_uint)sizeof(race.network_stream));
            race.dispose_network_stream = 1;
        }
        race.buffer = cx_array_new(1u, (cx_uint)sizeof(cx_byte));
        socket_root = cx_gc_add_root(&race.socket, (cx_uint)sizeof(race.socket));
        buffer_root = cx_gc_add_root(&race.buffer, (cx_uint)sizeof(race.buffer));
        task_root = cx_gc_add_root(&race.task, (cx_uint)sizeof(race.task));
        if (socket_root == NULL || buffer_root == NULL || task_root == NULL ||
            (use_network_stream && stream_root == NULL)) {
            fputs("could not root managed socket read race state\n", stderr);
            return 0;
        }
#ifdef _WIN32
        worker = CreateThread(NULL, 0, socket_dispose_read_worker,
            &race, 0, NULL);
        if (worker == NULL) {
#else
        if (pthread_create(&worker, NULL, socket_dispose_read_worker,
                &race) != 0) {
#endif
            fputs("could not start managed socket read/dispose race worker\n",
                stderr);
            return 0;
        }
        while (!socket_dispose_race_load(&race.ready)) {
#ifdef _WIN32
            Sleep(0u);
#else
            sched_yield();
#endif
        }
        socket_dispose_race_store(&race.start, 1);
        if (use_network_stream) {
            struct cx_exception_frame frame;
            cx_exception_push(&frame);
            if (setjmp(frame.environment) == 0) {
                struct cxcore_System_Threading_Tasks_CancellationToken token = {0};
                race.task = cxnet_System_Net_Sockets_NetworkStream_ReadAsync(
                    race.network_stream, race.buffer, 0u, 1u, token);
                cx_exception_pop(&frame);
                if (race.task == NULL) {
                    socket_dispose_race_store(&race.failed, 1);
                }
            } else {
                cx_exception_pop(&frame);
                socket_dispose_race_store(&race.threw_exception, 1);
                cx_exception_clear();
            }
        }
#ifdef _WIN32
        WaitForSingleObject(worker, INFINITE);
        CloseHandle(worker);
#else
        pthread_join(worker, NULL);
#endif
        if (!use_network_stream) {
            cxnet_System_Net_Sockets_Socket_Dispose(race.socket);
        }
        if (is_nonowning_stream_race) {
            cx_byte expected = (cx_byte)'R';
            if (socket_dispose_race_load(&race.failed) ||
                !socket_dispose_race_load(&race.completed) ||
                (race.task == NULL &&
                    !socket_dispose_race_load(&race.threw_exception)) ||
                (race.task != NULL &&
                    socket_dispose_race_load(&race.threw_exception))) {
                fprintf(stderr,
                    "non-owning NetworkStream read/dispose race had an invalid admission result at iteration %u\n",
                    (unsigned)iteration);
                return 0;
            }
            if (!send_stream_read_byte(peer_handle, expected)) {
                fputs("could not release the non-owning NetworkStream read race\n",
                    stderr);
                return 0;
            }
            if (race.task != NULL && race.task->_isFaulted &&
                race.task->_exception != NULL &&
                !cxcore_System_Exception_Matches(race.task->_exception,
                    cxcore_System_ObjectDisposedException___typeinfo) &&
                !cxcore_System_Exception_Matches(race.task->_exception,
                    cxcore_System_IO_IOException___typeinfo)) {
                fputs("non-owning NetworkStream disposal produced an unrelated read fault\n",
                    stderr);
                return 0;
            }
            if (race.task == NULL || race.task->_isCanceled ||
                race.task->_isFaulted) {
                race.task = cxnet_System_Net_Sockets_Socket_ReadAsync(
                    race.socket, race.buffer, 0u, 1u);
            }
            while (race.task != NULL && !race.task->_isCompleted &&
                steps < 100000u) {
                if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                    Sleep(1u);
#else
                    usleep(1000u);
#endif
                }
                steps++;
            }
            if (race.task == NULL || !race.task->_isCompleted ||
                race.task->_isFaulted || race.task->_isCanceled ||
                race.task->_result != 1u ||
                ((cx_byte *)race.buffer->_data)[0] != expected ||
                !cxnet_System_Net_Sockets_Socket_Connected___const_get(
                    race.socket)) {
                fprintf(stderr,
                    "non-owning NetworkStream disposal corrupted its socket read at iteration %u task=%p completed=%d faulted=%d canceled=%d result=%u byte=%u connected=%d threw=%d steps=%u\n",
                    (unsigned)iteration, (void *)race.task,
                    race.task != NULL && race.task->_isCompleted,
                    race.task != NULL && race.task->_isFaulted,
                    race.task != NULL && race.task->_isCanceled,
                    race.task != NULL ? (unsigned)race.task->_result : 0u,
                    race.buffer != NULL ?
                        (unsigned)((cx_byte *)race.buffer->_data)[0] : 0u,
                    cxnet_System_Net_Sockets_Socket_Connected___const_get(
                        race.socket),
                    socket_dispose_race_load(&race.threw_exception),
                    (unsigned)steps);
                return 0;
            }
            cxnet_System_Net_Sockets_Socket_Dispose(race.socket);
            if (!stream_peer_observes_eof(peer_handle)) {
                fprintf(stderr,
                    "non-owning NetworkStream race closed the peer before socket disposal at iteration %u\n",
                    (unsigned)iteration);
                return 0;
            }
            close_stream_read_peer(peer_handle);
            cx_gc_remove_root(task_root);
            cx_gc_remove_root(buffer_root);
            cx_gc_remove_root(stream_root);
            cx_gc_remove_root(socket_root);
            continue;
        }
        if (socket_dispose_race_load(&race.failed) ||
            !socket_dispose_race_load(&race.completed) ||
            (use_network_stream &&
                (race.task == NULL ||
                 socket_dispose_race_load(&race.threw_exception))) ||
            (race.task == NULL &&
                !socket_dispose_race_load(&race.threw_exception)) ||
            (race.task != NULL &&
                socket_dispose_race_load(&race.threw_exception))) {
            fprintf(stderr,
                "managed %s read/dispose race had an invalid outcome at iteration %u (failed=%d completed=%d task=%p threw_exception=%d)\n",
                use_network_stream ? "NetworkStream" : "socket",
                (unsigned)iteration,
                socket_dispose_race_load(&race.failed),
                socket_dispose_race_load(&race.completed),
                (void *)race.task,
                socket_dispose_race_load(&race.threw_exception));
            return 0;
        }
        if (race.task != NULL) {
            while (!race.task->_isCompleted && steps < 100000u) {
                if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                    Sleep(1u);
#else
                    usleep(1000u);
#endif
                }
                steps++;
            }
            if (!race.task->_isCompleted ||
                (!race.task->_isCanceled &&
                 !(use_network_stream && race.task->_isFaulted &&
                   race.task->_exception != NULL &&
                   (cxcore_System_Exception_Matches(race.task->_exception,
                        cxcore_System_ObjectDisposedException___typeinfo) ||
                    cxcore_System_Exception_Matches(race.task->_exception,
                        cxcore_System_IO_IOException___typeinfo))))) {
                const struct cxcore_System_String *exception_name = NULL;
                if (race.task->_exception != NULL) {
                    struct cxcore_System_Reflection_TypeInfo exception_type =
                        cxcore_System_Exception_RuntimeType___const_get(
                            race.task->_exception);
                    exception_name = exception_type.Name;
                }
                fprintf(stderr,
                    "disposing managed %s produced an invalid raced-read task at iteration %u canceled=%d faulted=%d exception=%.*s object-disposed=%d io=%d\n",
                    use_network_stream ? "NetworkStream" : "socket",
                    (unsigned)iteration, race.task->_isCanceled,
                    race.task->_isFaulted,
                    exception_name != NULL ? (int)exception_name->_length : 0,
                    exception_name != NULL ?
                        (const char *)exception_name->_data : "",
                    race.task->_exception != NULL &&
                        cxcore_System_Exception_Matches(race.task->_exception,
                            cxcore_System_ObjectDisposedException___typeinfo),
                    race.task->_exception != NULL &&
                        cxcore_System_Exception_Matches(race.task->_exception,
                            cxcore_System_IO_IOException___typeinfo));
                return 0;
            }
        }
        if (cxnet_System_Net_Sockets_Socket_Connected___const_get(race.socket)) {
            fprintf(stderr,
                "managed %s disposal left its handle active at iteration %u\n",
                use_network_stream ? "NetworkStream" : "socket",
                (unsigned)iteration);
            return 0;
        }
        if (!stream_peer_observes_eof(peer_handle)) {
            fprintf(stderr,
                "managed %s read/dispose race left its peer open at iteration %u\n",
                use_network_stream ? "NetworkStream" : "socket",
                (unsigned)iteration);
            return 0;
        }
        close_stream_read_peer(peer_handle);
        cxnet_System_Net_Sockets_Socket_Dispose(race.socket);
        cx_gc_remove_root(task_root);
        cx_gc_remove_root(buffer_root);
        if (stream_root != NULL) cx_gc_remove_root(stream_root);
        cx_gc_remove_root(socket_root);
    }
    return 1;
}

static int test_managed_socket_write_dispose_start_race(
    int use_network_stream) {
    uint32_t iteration;
    for (iteration = 0u; iteration < 24u; iteration++) {
        uint64_t connected_handle = 0u;
        uintptr_t peer_handle = (uintptr_t)0;
        size_t filled_bytes = 0u;
        struct cxnet_System_Net_Sockets_Socket *socket = NULL;
        struct cxnet_System_Net_Sockets_NetworkStream *network_stream = NULL;
        struct cxcore_System_Collections_Array *buffer = NULL;
        struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *task = NULL;
        struct cxcore_System_Threading_Tasks_TaskHandle *stream_task = NULL;
        struct socket_cross_thread_dispose dispose_race = {0};
        struct cx_gc_root_handle *socket_root = NULL;
        struct cx_gc_root_handle *buffer_root = NULL;
        struct cx_gc_root_handle *task_root = NULL;
        struct cx_gc_root_handle *network_stream_root = NULL;
        struct cx_gc_root_handle *stream_task_root = NULL;
        struct cxcore_System_Threading_Tasks_CancellationToken no_cancellation = {0};
        uint32_t steps = 0u;
        int native_handle_open = 0;
        int socket_initialized = 0;
        int disposed_exception = 0;
        int unexpected_exception = 0;
        int worker_started = 0;
        int passed = 0;
#ifdef _WIN32
        HANDLE worker = NULL;
#else
        pthread_t worker;
#endif

        if (!create_stream_read_pair(&connected_handle, &peer_handle)) {
            fputs("could not create a managed socket write/dispose race pair\n",
                stderr);
            return 0;
        }
        native_handle_open = 1;
        if (!socket_prefill_send_buffer(connected_handle, peer_handle,
                &filled_bytes)) {
            fputs("could not saturate the send buffer before write/dispose race\n",
                stderr);
            goto cleanup;
        }
        socket = (struct cxnet_System_Net_Sockets_Socket *)
            cxcore_System_Memory_Alloc(
                cxnet_System_Net_Sockets_Socket___typeinfo.Size);
        buffer = cx_array_new(1u, (cx_uint)sizeof(cx_byte));
        if (socket == NULL || buffer == NULL) goto cleanup;
        socket_root = cx_gc_add_root(&socket, (cx_uint)sizeof(socket));
        buffer_root = cx_gc_add_root(&buffer, (cx_uint)sizeof(buffer));
        task_root = cx_gc_add_root(&task, (cx_uint)sizeof(task));
        if (socket_root == NULL || buffer_root == NULL || task_root == NULL)
            goto cleanup;
        cxnet_System_Net_Sockets_Socket___constructor(socket,
            (cx_ulong)connected_handle);
        socket_initialized = 1;
        native_handle_open = 0;
        dispose_race.socket = socket;
        if (use_network_stream) {
            network_stream =
                (struct cxnet_System_Net_Sockets_NetworkStream *)
                cxcore_System_Memory_Alloc(
                    cxnet_System_Net_Sockets_NetworkStream___typeinfo.Size);
            if (network_stream == NULL) goto cleanup;
            network_stream_root = cx_gc_add_root(&network_stream,
                (cx_uint)sizeof(network_stream));
            if (network_stream_root == NULL) goto cleanup;
            cxnet_System_Net_Sockets_NetworkStream___constructor__2(
                network_stream, socket, use_network_stream == 1);
            dispose_race.network_stream = network_stream;
        }
        ((cx_byte *)buffer->_data)[0] = 0x5au;

#ifdef _WIN32
        worker = CreateThread(NULL, 0, socket_cross_thread_dispose_worker,
            &dispose_race, 0, NULL);
        if (worker == NULL) {
#else
        if (pthread_create(&worker, NULL, socket_cross_thread_dispose_worker,
                &dispose_race) != 0) {
#endif
            fputs("could not start managed socket write/dispose race worker\n",
                stderr);
            goto cleanup;
        }
        worker_started = 1;
        while (!socket_dispose_race_load(&dispose_race.ready)) {
#ifdef _WIN32
            Sleep(0u);
#else
            sched_yield();
#endif
        }
        socket_dispose_race_store(&dispose_race.start, 1);
        {
            struct cx_exception_frame frame;
            cx_exception_push(&frame);
            if (setjmp(frame.environment) == 0) {
                if (use_network_stream) {
                    stream_task =
                        cxnet_System_Net_Sockets_NetworkStream_WriteAsync(
                            network_stream, buffer, 0u, 1u,
                            no_cancellation);
                    stream_task_root = cx_gc_add_root(&stream_task,
                        (cx_uint)sizeof(stream_task));
                } else {
                    task = cxnet_System_Net_Sockets_Socket_WriteAsync(socket,
                        buffer, 0u, 1u);
                }
                cx_exception_pop(&frame);
            } else {
                struct cxcore_System_Exception *exception =
                    (struct cxcore_System_Exception *)cx_exception_current();
                disposed_exception = exception != NULL &&
                    cxcore_System_Exception_Matches(exception,
                        cxcore_System_ObjectDisposedException___typeinfo);
                unexpected_exception = !disposed_exception;
                cx_exception_pop(&frame);
                cx_exception_clear();
            }
        }
#ifdef _WIN32
        WaitForSingleObject(worker, INFINITE);
        CloseHandle(worker);
#else
        pthread_join(worker, NULL);
#endif
        worker_started = 0;
        if (socket_dispose_race_load(&dispose_race.failed) ||
            !socket_dispose_race_load(&dispose_race.completed) ||
            unexpected_exception ||
            (task == NULL && stream_task == NULL && !disposed_exception) ||
            ((task != NULL || stream_task != NULL) && disposed_exception) ||
            (use_network_stream &&
                (stream_task != NULL && stream_task_root == NULL))) {
            fprintf(stderr,
                "managed %s write/dispose race had an invalid admission result at iteration %u\n",
                use_network_stream ? "NetworkStream" : "socket",
                (unsigned)iteration);
            goto cleanup;
        }
        if (use_network_stream == 2) {
            struct socket_peer_payload_drain drain = {0};
            int write_admitted = stream_task != NULL &&
                !socket_write_task_faulted(task, stream_task);
#ifdef _WIN32
            HANDLE drain_worker;
#else
            pthread_t drain_worker;
#endif
            drain.peer = peer_handle;
            drain.filled_bytes = filled_bytes;
            drain.trailing_bytes = write_admitted ? 1u : 0u;
#ifdef _WIN32
            drain_worker = CreateThread(NULL, 0,
                socket_peer_payload_drain_worker, &drain, 0, NULL);
            if (drain_worker == NULL) goto cleanup;
#else
            if (pthread_create(&drain_worker, NULL,
                    socket_peer_payload_drain_worker, &drain) != 0)
                goto cleanup;
#endif
            if (write_admitted) {
                cx_gc_collect();
                while (!socket_write_task_completed(task, stream_task) &&
                    steps < 100000u) {
                    if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                        Sleep(1u);
#else
                        usleep(1000u);
#endif
                    }
                    steps++;
                }
            }
#ifdef _WIN32
            WaitForSingleObject(drain_worker, INFINITE);
            CloseHandle(drain_worker);
#else
            pthread_join(drain_worker, NULL);
#endif
            if (!drain.completed || drain.failed) {
                fputs("non-owning NetworkStream write race corrupted peer bytes\n",
                    stderr);
                goto cleanup;
            }
            if (write_admitted) {
                if (!socket_write_task_completed(task, stream_task) ||
                    socket_write_task_canceled(task, stream_task) ||
                    socket_write_task_faulted(task, stream_task)) {
                    fputs("admitted non-owning NetworkStream write did not complete successfully\n",
                        stderr);
                    goto cleanup;
                }
                cxcore_System_Threading_Tasks_TaskHandle_GetResult(stream_task);
            } else if (stream_task != NULL) {
                struct cxcore_System_Exception *write_exception =
                    socket_write_task_exception(task, stream_task);
                if (!socket_write_task_faulted(task, stream_task) ||
                    write_exception == NULL ||
                    !cxcore_System_Exception_Matches(write_exception,
                        cxcore_System_ObjectDisposedException___typeinfo)) {
                    fputs("rejected non-owning NetworkStream write had the wrong failure\n",
                        stderr);
                    goto cleanup;
                }
            } else if (!disposed_exception) {
                fputs("non-owning NetworkStream write was neither admitted nor rejected by disposal\n",
                    stderr);
                goto cleanup;
            }
            if (socket->_disposed ||
                !cxnet_System_Net_Sockets_Socket_Connected___const_get(socket)) {
                fputs("disposing a non-owning NetworkStream closed its socket\n",
                    stderr);
                goto cleanup;
            }
            {
                if (cxnet_System_Net_Sockets_Socket_Receive(socket,
                        buffer, 0u, 1u) != 1 ||
                    ((cx_byte *)buffer->_data)[0] != 0x36u) {
                    fputs("non-owning NetworkStream disposal broke reverse traffic\n",
                        stderr);
                    goto cleanup;
                }
            }
            cxnet_System_Net_Sockets_Socket_Dispose(socket);
            if (!stream_peer_observes_payload_then_eof(peer_handle, 0u)) {
                fputs("non-owning NetworkStream race left its socket peer open\n",
                    stderr);
                goto cleanup;
            }
            passed = 1;
            goto cleanup;
        }
        if (task != NULL || stream_task != NULL) {
            struct cxcore_System_Exception *write_exception = NULL;
            int write_was_canceled;
            int write_faulted_with_disposal;
            cx_gc_collect();
            while (!socket_write_task_completed(task, stream_task) &&
                steps < 100000u) {
                if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                    Sleep(1u);
#else
                    usleep(1000u);
#endif
                }
                steps++;
            }
            write_exception = socket_write_task_exception(task, stream_task);
            write_was_canceled =
                socket_write_task_canceled(task, stream_task);
            write_faulted_with_disposal =
                use_network_stream &&
                socket_write_task_faulted(task, stream_task) &&
                write_exception != NULL &&
                (cxcore_System_Exception_Matches(write_exception,
                    cxcore_System_ObjectDisposedException___typeinfo) ||
                 cxcore_System_Exception_Matches(write_exception,
                    cxcore_System_IO_IOException___typeinfo));
            if (!socket_write_task_completed(task, stream_task) ||
                (!write_was_canceled && !write_faulted_with_disposal)) {
                fprintf(stderr,
                    "disposing managed %s produced an invalid raced-write result at iteration %u completed=%d canceled=%d faulted=%d exception=%p object-disposed=%d socket=%d io=%d canceled-exception=%d\n",
                    use_network_stream ? "NetworkStream" : "socket",
                    (unsigned)iteration,
                    socket_write_task_completed(task, stream_task),
                    write_was_canceled,
                    socket_write_task_faulted(task, stream_task),
                    (void *)write_exception,
                    write_exception != NULL &&
                        cxcore_System_Exception_Matches(write_exception,
                            cxcore_System_ObjectDisposedException___typeinfo),
                    write_exception != NULL &&
                        cxcore_System_Exception_Matches(write_exception,
                            cxnet_System_Net_Sockets_SocketException___typeinfo),
                    write_exception != NULL &&
                        cxcore_System_Exception_Matches(write_exception,
                            cxcore_System_IO_IOException___typeinfo),
                    write_exception != NULL &&
                        cxcore_System_Exception_Matches(write_exception,
                            cxcore_System_Threading_Tasks_OperationCanceledException___typeinfo));
                goto cleanup;
            }
        }
        if (cxnet_System_Net_Sockets_Socket_Connected___const_get(socket) ||
            !stream_peer_observes_payload_then_eof(peer_handle, filled_bytes)) {
            fprintf(stderr,
                "managed %s write/dispose race left the handle or peer active at iteration %u\n",
                use_network_stream ? "NetworkStream" : "socket",
                (unsigned)iteration);
            goto cleanup;
        }
        passed = 1;
cleanup:
        if (worker_started) {
            socket_dispose_race_store(&dispose_race.start, 1);
#ifdef _WIN32
            WaitForSingleObject(worker, INFINITE);
            CloseHandle(worker);
#else
            pthread_join(worker, NULL);
#endif
        }
        if (socket_initialized) {
            cxnet_System_Net_Sockets_Socket_Dispose(socket);
        } else if (native_handle_open) {
#ifdef _WIN32
            closesocket((SOCKET)(uintptr_t)connected_handle);
#else
            close((int)connected_handle);
#endif
        }
        if (network_stream != NULL)
            cxnet_System_Net_Sockets_NetworkStream_Dispose(network_stream);
        if (task != NULL || stream_task != NULL) {
            steps = 0u;
            while (!socket_write_task_completed(task, stream_task) &&
                steps < 100000u) {
                if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                    Sleep(1u);
#else
                    usleep(1000u);
#endif
                }
                steps++;
            }
        }
        if (peer_handle != (uintptr_t)0) close_stream_read_peer(peer_handle);
        if (task_root != NULL) cx_gc_remove_root(task_root);
        if (stream_task_root != NULL) cx_gc_remove_root(stream_task_root);
        if (network_stream_root != NULL)
            cx_gc_remove_root(network_stream_root);
        if (buffer_root != NULL) cx_gc_remove_root(buffer_root);
        if (socket_root != NULL) cx_gc_remove_root(socket_root);
        if (!passed) return 0;
    }
    return 1;
}

static int test_managed_socket_multiple_accept_dispose(void) {
    struct cxnet_System_Net_Sockets_Socket *listener =
        (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Sockets_Socket___typeinfo.Size);
    struct cxnet_System_Net_IPEndPoint *bind_endpoint =
        (struct cxnet_System_Net_IPEndPoint *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_IPEndPoint___typeinfo.Size);
    struct cxcore_System_Threading_Tasks_Task *accept_a = NULL;
    struct cxcore_System_Threading_Tasks_Task *accept_b = NULL;
    struct cx_gc_root_handle *listener_root = cx_gc_add_root(
        &listener, (cx_uint)sizeof(listener));
    struct cx_gc_root_handle *bind_endpoint_root = cx_gc_add_root(
        &bind_endpoint, (cx_uint)sizeof(bind_endpoint));
    struct cx_gc_root_handle *accept_a_root = cx_gc_add_root(
        &accept_a, (cx_uint)sizeof(accept_a));
    struct cx_gc_root_handle *accept_b_root = cx_gc_add_root(
        &accept_b, (cx_uint)sizeof(accept_b));
    uint32_t steps = 0u;
    int passed = 0;

    cxnet_System_Net_Sockets_Socket___constructor__2(listener,
        cxnet_System_Net_AddressFamily_InterNetwork,
        cxnet_System_Net_Sockets_SocketType_Stream,
        cxnet_System_Net_Sockets_ProtocolType_Tcp);
    cxnet_System_Net_IPEndPoint___constructor__2(bind_endpoint,
        cxnet_System_Net_IPAddress_Loopback___get(), 0);
    cxnet_System_Net_Sockets_Socket_Bind(listener,
        (struct cxnet_System_Net_EndPoint *)bind_endpoint);
    cxnet_System_Net_Sockets_Socket_Listen__2(listener, 4);
    accept_a = cxnet_System_Net_Sockets_Socket_AcceptAsync(listener);
    accept_b = cxnet_System_Net_Sockets_Socket_AcceptAsync(listener);
    if (cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(accept_a) ||
        cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(accept_b)) {
        fputs("fresh concurrent accepts completed before listener disposal\n",
            stderr);
        goto cleanup;
    }

    cx_gc_collect();
    cxnet_System_Net_Sockets_Socket_Dispose(listener);
    cxnet_System_Net_Sockets_Socket_Dispose(listener);
    while ((!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(accept_a) ||
            !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(accept_b)) &&
        steps < 100000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(accept_a) ||
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(accept_b) ||
        !accept_a->_isFaulted || !accept_b->_isFaulted ||
        accept_a->_isCanceled || accept_b->_isCanceled ||
        !cxcore_System_Exception_Matches(accept_a->_exception,
            cxcore_System_ObjectDisposedException___typeinfo) ||
        !cxcore_System_Exception_Matches(accept_b->_exception,
            cxcore_System_ObjectDisposedException___typeinfo)) {
        fputs("disposing listener did not fault every pending accept with ObjectDisposedException\n",
            stderr);
        goto cleanup;
    }
    passed = 1;

cleanup:
    cxnet_System_Net_Sockets_Socket_Dispose(listener);
    cx_gc_remove_root(accept_b_root);
    cx_gc_remove_root(accept_a_root);
    cx_gc_remove_root(bind_endpoint_root);
    cx_gc_remove_root(listener_root);
    return passed;
}

static int test_managed_socket_cancel_after_native_accept(void) {
    struct cxnet_System_Net_Sockets_Socket *listener = NULL;
    struct cxnet_System_Net_Sockets_Socket *client = NULL;
    struct cxnet_System_Net_Sockets_Socket *client_after_cancel = NULL;
    struct cxnet_System_Net_Sockets_Socket *accepted_after_cancel = NULL;
    struct cxnet_System_Net_IPEndPoint *bind_endpoint = NULL;
    struct cxnet_System_Net_IPEndPoint *endpoint = NULL;
    struct cxcore_System_Threading_Tasks_CancellationTokenSource *source = NULL;
    struct cxcore_System_Threading_Tasks_CancellationToken token;
    struct cxcore_System_Threading_Tasks_Task *accept_task = NULL;
    struct cxcore_System_Threading_Tasks_Task *accept_after_cancel_task = NULL;
    struct cx_gc_root_handle *listener_root = NULL;
    struct cx_gc_root_handle *client_root = NULL;
    struct cx_gc_root_handle *client_after_cancel_root = NULL;
    struct cx_gc_root_handle *accepted_after_cancel_root = NULL;
    struct cx_gc_root_handle *bind_endpoint_root = NULL;
    struct cx_gc_root_handle *endpoint_root = NULL;
    struct cx_gc_root_handle *source_root = NULL;
    struct cx_gc_root_handle *accept_task_root = NULL;
    struct cx_gc_root_handle *accept_after_cancel_task_root = NULL;
    struct cxcore_System_Collections_Array sent = {0};
    struct cxcore_System_Collections_Array received = {0};
    cx_byte sent_byte = (cx_byte)'A';
    cx_byte received_byte = 0u;
    cx_byte eof_byte = 0u;
    int passed = 0;

    listener = (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
        cxnet_System_Net_Sockets_Socket___typeinfo.Size);
    listener_root = cx_gc_add_root(&listener, (cx_uint)sizeof(listener));
    bind_endpoint = (struct cxnet_System_Net_IPEndPoint *)cxcore_System_Memory_Alloc(
        cxnet_System_Net_IPEndPoint___typeinfo.Size);
    bind_endpoint_root = cx_gc_add_root(&bind_endpoint,
        (cx_uint)sizeof(bind_endpoint));
    cxnet_System_Net_Sockets_Socket___constructor__2(listener,
        cxnet_System_Net_AddressFamily_InterNetwork,
        cxnet_System_Net_Sockets_SocketType_Stream,
        cxnet_System_Net_Sockets_ProtocolType_Tcp);
    cxnet_System_Net_IPEndPoint___constructor__2(bind_endpoint,
        cxnet_System_Net_IPAddress_Loopback___get(), 0);
    cxnet_System_Net_Sockets_Socket_Bind(listener,
        (struct cxnet_System_Net_EndPoint *)bind_endpoint);
    cxnet_System_Net_Sockets_Socket_Listen__2(listener, 4);
    endpoint = cxnet_System_Net_Sockets_Socket_GetLocalEndPoint(listener);
    endpoint_root = cx_gc_add_root(&endpoint, (cx_uint)sizeof(endpoint));

    client = (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
        cxnet_System_Net_Sockets_Socket___typeinfo.Size);
    client_root = cx_gc_add_root(&client, (cx_uint)sizeof(client));
    cxnet_System_Net_Sockets_Socket___constructor__2(client,
        cxnet_System_Net_AddressFamily_InterNetwork,
        cxnet_System_Net_Sockets_SocketType_Stream,
        cxnet_System_Net_Sockets_ProtocolType_Tcp);
    cxnet_System_Net_Sockets_Socket_Connect(client,
        (struct cxnet_System_Net_EndPoint *)endpoint);

    source = (struct cxcore_System_Threading_Tasks_CancellationTokenSource *)
        cxcore_System_Memory_Alloc(
            cxcore_System_Threading_Tasks_CancellationTokenSource___typeinfo.Size);
    source_root = cx_gc_add_root(&source, (cx_uint)sizeof(source));
    cxcore_System_Threading_Tasks_CancellationTokenSource___constructor(source);
    token = cxcore_System_Threading_Tasks_CancellationTokenSource_Token___const_get(
        source);
    accept_after_native_cancel_source = source;
    accept_after_native_hook_calls = 0u;
    cxnet_socket_test_accept_after_native_hook_set(cancel_after_native_accept);
    accept_task = cxnet_System_Net_Sockets_Socket_AcceptAsync__2(listener, token);
    accept_task_root = cx_gc_add_root(&accept_task, (cx_uint)sizeof(accept_task));
    cxnet_socket_test_accept_after_native_hook_set(NULL);
    accept_after_native_cancel_source = NULL;
    if (accept_after_native_hook_calls != 1u ||
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(accept_task) ||
        !cxcore_System_Threading_Tasks_Task_IsCanceled___const_get(accept_task) ||
        accept_task->_isFaulted) {
        fprintf(stderr,
            "AcceptAsync post-native cancellation failed: hooks=%u completed=%d canceled=%d faulted=%d\n",
            accept_after_native_hook_calls,
            cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(accept_task),
            cxcore_System_Threading_Tasks_Task_IsCanceled___const_get(accept_task),
            accept_task->_isFaulted);
        goto cleanup;
    }
    received._length = 1u;
    received._data = &eof_byte;
    if (cxnet_System_Net_Sockets_Socket_Receive(client, &received, 0u, 1u) != 0) {
        fputs("canceled AcceptAsync left its newly accepted socket open\n", stderr);
        goto cleanup;
    }

    client_after_cancel =
        (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Sockets_Socket___typeinfo.Size);
    client_after_cancel_root = cx_gc_add_root(&client_after_cancel,
        (cx_uint)sizeof(client_after_cancel));
    cxnet_System_Net_Sockets_Socket___constructor__2(client_after_cancel,
        cxnet_System_Net_AddressFamily_InterNetwork,
        cxnet_System_Net_Sockets_SocketType_Stream,
        cxnet_System_Net_Sockets_ProtocolType_Tcp);
    cxnet_System_Net_Sockets_Socket_Connect(client_after_cancel,
        (struct cxnet_System_Net_EndPoint *)endpoint);
    accept_after_cancel_task =
        cxnet_System_Net_Sockets_Socket_AcceptAsync(listener);
    accept_after_cancel_task_root = cx_gc_add_root(&accept_after_cancel_task,
        (cx_uint)sizeof(accept_after_cancel_task));
    if (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(
            accept_after_cancel_task) ||
        cxcore_System_Threading_Tasks_Task_IsCanceled___const_get(
            accept_after_cancel_task) ||
        accept_after_cancel_task->_isFaulted) {
        fputs("listener did not accept a connection after canceled accept\n",
            stderr);
        goto cleanup;
    }
    accepted_after_cancel = cx_generic_7c4851112331385bf3ec935e4daabb246ea0a9a28971c93643ab4b4268d85138(
        (const struct cxcore_System_Threading_Tasks_Task *)
            accept_after_cancel_task);
    accepted_after_cancel_root = cx_gc_add_root(&accepted_after_cancel,
        (cx_uint)sizeof(accepted_after_cancel));
    sent._length = 1u;
    sent._data = &sent_byte;
    received._data = &received_byte;
    if (accepted_after_cancel == NULL ||
        cxnet_System_Net_Sockets_Socket_Send(client_after_cancel, &sent, 0u, 1u) != 1 ||
        cxnet_System_Net_Sockets_Socket_Receive(
            accepted_after_cancel, &received, 0u, 1u) != 1 ||
        received_byte != sent_byte) {
        fputs("listener was unusable after canceled accept cleanup\n", stderr);
        goto cleanup;
    }
    passed = 1;

cleanup:
    cxnet_socket_test_accept_after_native_hook_set(NULL);
    accept_after_native_cancel_source = NULL;
    if (accepted_after_cancel != NULL)
        cxnet_System_Net_Sockets_Socket_Dispose(accepted_after_cancel);
    if (client_after_cancel != NULL)
        cxnet_System_Net_Sockets_Socket_Dispose(client_after_cancel);
    if (client != NULL) cxnet_System_Net_Sockets_Socket_Dispose(client);
    if (listener != NULL) cxnet_System_Net_Sockets_Socket_Dispose(listener);
    if (accept_after_cancel_task_root != NULL)
        cx_gc_remove_root(accept_after_cancel_task_root);
    if (accept_task_root != NULL) cx_gc_remove_root(accept_task_root);
    if (source_root != NULL) cx_gc_remove_root(source_root);
    if (endpoint_root != NULL) cx_gc_remove_root(endpoint_root);
    if (bind_endpoint_root != NULL) cx_gc_remove_root(bind_endpoint_root);
    if (client_after_cancel_root != NULL)
        cx_gc_remove_root(client_after_cancel_root);
    if (client_root != NULL) cx_gc_remove_root(client_root);
    if (listener_root != NULL) cx_gc_remove_root(listener_root);
    if (accepted_after_cancel_root != NULL)
        cx_gc_remove_root(accepted_after_cancel_root);
    return passed;
}

static int test_managed_socket_receive_shutdown(void) {
    uint32_t iteration;
    for (iteration = 0u; iteration < 24u; iteration++) {
        uint64_t connected_handle = 0u;
        uintptr_t peer_handle = (uintptr_t)0;
        struct cxnet_System_Net_Sockets_Socket *socket;
        struct cxcore_System_Collections_Array *buffer;
        struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *task = NULL;
        struct cx_gc_root_handle *socket_root;
        struct cx_gc_root_handle *buffer_root;
        struct cx_gc_root_handle *task_root;
        uint32_t steps = 0u;
        if (!create_stream_read_pair(&connected_handle, &peer_handle)) {
            fputs("could not create receive-shutdown pair\n",
                stderr);
            return 0;
        }
        socket = (struct cxnet_System_Net_Sockets_Socket *)
            cxcore_System_Memory_Alloc(
                cxnet_System_Net_Sockets_Socket___typeinfo.Size);
        cxnet_System_Net_Sockets_Socket___constructor(socket,
            (cx_ulong)connected_handle);
        buffer = cx_array_new(1u, (cx_uint)sizeof(cx_byte));
        socket_root = cx_gc_add_root(&socket, (cx_uint)sizeof(socket));
        buffer_root = cx_gc_add_root(&buffer, (cx_uint)sizeof(buffer));
        task_root = cx_gc_add_root(&task, (cx_uint)sizeof(task));
        if (socket_root == NULL || buffer_root == NULL || task_root == NULL) {
            fputs("could not root receive-shutdown test state\n", stderr);
            return 0;
        }
        cx_gc_collect();
        cxnet_System_Net_Sockets_Socket_Shutdown(socket,
            cxnet_System_Net_Sockets_SocketShutdown_Receive);
        task = cxnet_System_Net_Sockets_Socket_ReadAsync(socket, buffer,
            0u, 1u);
        while (!task->_isCompleted && steps < 1000u) {
            if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                Sleep(1u);
#else
                usleep(1000u);
#endif
            }
            steps++;
        }
#ifdef _WIN32
        if (!task->_isCompleted || !task->_isFaulted || task->_isCanceled ||
            !cxcore_System_Exception_Matches(task->_exception,
                cxnet_System_Net_Sockets_SocketException___typeinfo) ||
            cxnet_System_Net_Sockets_SocketException_SocketErrorCode___const_get(
                (const struct cxnet_System_Net_Sockets_SocketException *)
                    task->_exception) !=
                cxnet_System_Net_Sockets_SocketError_Shutdown ||
            !cxnet_System_Net_Sockets_Socket_Connected___const_get(socket)) {
            fputs("Winsock receive after Shutdown(Receive) did not report Shutdown\n",
                stderr);
#else
        if (!task->_isCompleted || task->_isFaulted || task->_isCanceled ||
            task->_result != 0u ||
            !cxnet_System_Net_Sockets_Socket_Connected___const_get(socket)) {
            fputs("POSIX receive after Shutdown(Receive) did not return EOF\n",
                stderr);
#endif
            return 0;
        }

        task = cxnet_System_Net_Sockets_Socket_ReadAsync(socket, buffer,
            0u, 1u);
        steps = 0u;
        while (!task->_isCompleted && steps < 1000u) {
            if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                Sleep(1u);
#else
                usleep(1000u);
#endif
            }
            steps++;
        }
#ifdef _WIN32
        if (!task->_isCompleted || !task->_isFaulted || task->_isCanceled ||
            !cxcore_System_Exception_Matches(task->_exception,
                cxnet_System_Net_Sockets_SocketException___typeinfo) ||
            cxnet_System_Net_Sockets_SocketException_SocketErrorCode___const_get(
                (const struct cxnet_System_Net_Sockets_SocketException *)
                    task->_exception) !=
                cxnet_System_Net_Sockets_SocketError_Shutdown) {
            fputs("repeated Winsock receive after shutdown did not report Shutdown\n",
                stderr);
#else
        if (!task->_isCompleted || task->_isFaulted || task->_isCanceled ||
            task->_result != 0u) {
            fputs("repeated POSIX receive after shutdown did not return EOF\n",
                stderr);
#endif
            return 0;
        }
        cxnet_System_Net_Sockets_Socket_Dispose(socket);
        if (!stream_peer_observes_eof(peer_handle)) {
            fputs("disposing the half-shut socket did not close its peer\n",
                stderr);
            return 0;
        }
        close_stream_read_peer(peer_handle);
        cx_gc_remove_root(task_root);
        cx_gc_remove_root(buffer_root);
        cx_gc_remove_root(socket_root);
    }
    return 1;
}

static int test_managed_socket_pending_receive_shutdown(
    int use_network_stream) {
    uint32_t iteration;
    for (iteration = 0u; iteration < 24u; iteration++) {
        uint64_t connected_handle = 0u;
        uintptr_t peer_handle = (uintptr_t)0;
        struct cxnet_System_Net_Sockets_Socket *socket = NULL;
        struct cxnet_System_Net_Sockets_NetworkStream *stream = NULL;
        struct cxcore_System_Collections_Array *buffer = NULL;
        struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *task = NULL;
        struct cx_gc_root_handle *socket_root = NULL;
        struct cx_gc_root_handle *stream_root = NULL;
        struct cx_gc_root_handle *buffer_root = NULL;
        struct cx_gc_root_handle *task_root = NULL;
        uint32_t steps = 0u;
        if (!create_stream_read_pair(&connected_handle, &peer_handle)) {
            fputs("could not create pending receive-shutdown pair\n", stderr);
            return 0;
        }
        socket = (struct cxnet_System_Net_Sockets_Socket *)
            cxcore_System_Memory_Alloc(
                cxnet_System_Net_Sockets_Socket___typeinfo.Size);
        if (use_network_stream) {
            stream = (struct cxnet_System_Net_Sockets_NetworkStream *)
                cxcore_System_Memory_Alloc((cx_uint)sizeof(*stream));
        }
        cxnet_System_Net_Sockets_Socket___constructor(socket,
            (cx_ulong)connected_handle);
        if (stream != NULL) {
            cxnet_System_Net_Sockets_NetworkStream___constructor__2(
                stream, socket, 0);
        }
        buffer = cx_array_new(1u, (cx_uint)sizeof(cx_byte));
        socket_root = cx_gc_add_root(&socket, (cx_uint)sizeof(socket));
        if (stream != NULL) {
            stream_root = cx_gc_add_root(&stream, (cx_uint)sizeof(stream));
        }
        buffer_root = cx_gc_add_root(&buffer, (cx_uint)sizeof(buffer));
        task_root = cx_gc_add_root(&task, (cx_uint)sizeof(task));
        if (socket_root == NULL ||
            (use_network_stream && (stream == NULL || stream_root == NULL)) ||
            buffer_root == NULL || task_root == NULL) {
            fputs("could not root pending receive-shutdown state\n", stderr);
            return 0;
        }
        cx_gc_collect();
        if (stream != NULL) {
            struct cxcore_System_Threading_Tasks_CancellationToken token = {0};
            task = cxnet_System_Net_Sockets_NetworkStream_ReadAsync(
                stream, buffer, 0u, 1u, token);
        } else {
            task = cxnet_System_Net_Sockets_Socket_ReadAsync(socket, buffer,
                0u, 1u);
        }
        if (task->_isCompleted) {
            fputs("receive completed before pending receive shutdown\n", stderr);
            return 0;
        }
        cxnet_System_Net_Sockets_Socket_Shutdown(socket,
            cxnet_System_Net_Sockets_SocketShutdown_Receive);
#ifdef _WIN32
        {
            const char wake_byte = 'x';
            if (send((SOCKET)peer_handle, &wake_byte, 1, 0) != 1) {
                fputs("could not wake pending Winsock receive after shutdown\n",
                    stderr);
                return 0;
            }
        }
#endif
        while (!task->_isCompleted && steps < 1000u) {
            if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                Sleep(1u);
#else
                usleep(1000u);
#endif
            }
            steps++;
        }
#ifdef _WIN32
        if (!task->_isCompleted || !task->_isFaulted || task->_isCanceled ||
            !cxcore_System_Exception_Matches(task->_exception,
                cxnet_System_Net_Sockets_SocketException___typeinfo) ||
            cxnet_System_Net_Sockets_SocketException_SocketErrorCode___const_get(
                (const struct cxnet_System_Net_Sockets_SocketException *)
                    task->_exception) !=
                cxnet_System_Net_Sockets_SocketError_ConnectionAborted) {
            fputs("pending Winsock receive after shutdown did not report ConnectionAborted\n",
                stderr);
            return 0;
        }
#else
        if (!task->_isCompleted || task->_isFaulted || task->_isCanceled ||
            task->_result != 0u) {
            fprintf(stderr,
                "pending POSIX receive after shutdown did not return EOF (iteration=%u completed=%d faulted=%d canceled=%d result=%u connected=%d steps=%u stream=%d)\n",
                (unsigned)iteration, task->_isCompleted, task->_isFaulted,
                task->_isCanceled, (unsigned)task->_result,
                cxnet_System_Net_Sockets_Socket_Connected___const_get(socket),
                (unsigned)steps, stream != NULL);
            return 0;
        }
#endif
        if (!cxnet_System_Net_Sockets_Socket_Connected___const_get(socket)) {
            fputs("receive shutdown changed the connected socket state\n", stderr);
            return 0;
        }
        if (stream != NULL) {
            cxnet_System_Net_Sockets_NetworkStream_Dispose(stream);
        }
        cxnet_System_Net_Sockets_Socket_Dispose(socket);
        if (!stream_peer_observes_close_or_reset(peer_handle)) {
            fputs("disposing the half-shut socket did not close or reset its peer\n",
                stderr);
            return 0;
        }
        close_stream_read_peer(peer_handle);
        cx_gc_remove_root(task_root);
        cx_gc_remove_root(buffer_root);
        if (stream_root != NULL) cx_gc_remove_root(stream_root);
        cx_gc_remove_root(socket_root);
    }
    return 1;
}

static int test_managed_socket_rejects_concurrent_reads(void) {
    uint64_t connected_handle = 0u;
    uintptr_t peer_handle = (uintptr_t)0;
    struct cxnet_System_Net_Sockets_Socket *socket = NULL;
    struct cxcore_System_Collections_Array *buffer = NULL;
    struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *first_task = NULL;
    struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *second_task = NULL;
    struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *send_task = NULL;
    struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *second_send_task = NULL;
    struct cx_gc_root_handle *socket_root = NULL;
    struct cx_gc_root_handle *buffer_root = NULL;
    struct cx_gc_root_handle *task_root = NULL;
    struct cx_gc_root_handle *send_task_root = NULL;
    struct cx_gc_root_handle *second_send_task_root = NULL;
    uint32_t steps = 0u;
    int rejected = 0;
    int passed = 0;
    if (!create_stream_read_pair(&connected_handle, &peer_handle)) {
        fputs("could not create concurrent managed socket read pair\n", stderr);
        return 0;
    }
    socket = (struct cxnet_System_Net_Sockets_Socket *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Sockets_Socket___typeinfo.Size);
    buffer = cx_array_new(1u, (cx_uint)sizeof(cx_byte));
    if (socket == NULL || buffer == NULL) goto cleanup;
    socket_root = cx_gc_add_root(&socket, (cx_uint)sizeof(socket));
    buffer_root = cx_gc_add_root(&buffer, (cx_uint)sizeof(buffer));
    if (socket_root == NULL || buffer_root == NULL) goto cleanup;
    cxnet_System_Net_Sockets_Socket___constructor(socket,
        (cx_ulong)connected_handle);
    connected_handle = 0u;
    first_task = cxnet_System_Net_Sockets_Socket_ReadAsync(
        socket, buffer, 0u, 1u);
    task_root = cx_gc_add_root(&first_task, (cx_uint)sizeof(first_task));
    if (first_task == NULL || task_root == NULL || first_task->_isCompleted) {
        fputs("first managed socket read was not left pending\n", stderr);
        goto cleanup;
    }
    send_task = cxnet_System_Net_Sockets_Socket_WriteAsync(
        socket, buffer, 0u, 1u);
    send_task_root = cx_gc_add_root(&send_task, (cx_uint)sizeof(send_task));
    if (send_task == NULL || send_task_root == NULL ||
        !send_task->_isCompleted || send_task->_isCanceled ||
        send_task->_isFaulted) {
        fputs("a pending receive prevented an independent socket send\n",
            stderr);
        goto cleanup;
    }
    second_send_task = cxnet_System_Net_Sockets_Socket_WriteAsync(
        socket, buffer, 0u, 1u);
    second_send_task_root = cx_gc_add_root(&second_send_task,
        (cx_uint)sizeof(second_send_task));
    if (second_send_task == NULL || second_send_task_root == NULL ||
        !second_send_task->_isCompleted || second_send_task->_isCanceled ||
        second_send_task->_isFaulted) {
        fputs("completed socket send did not release its send slot\n", stderr);
        goto cleanup;
    }
    {
        cx_byte received = 1u;
#ifdef _WIN32
        int received_count = recv((SOCKET)peer_handle,
            (char *)&received, 1, 0);
#else
        ssize_t received_count = recv((int)peer_handle,
            (char *)&received, 1, 0);
#endif
        if (received_count != 1 || received != 0u) {
            fputs("independent socket send did not reach its peer\n", stderr);
            goto cleanup;
        }
    }
    {
        cx_byte received = 1u;
#ifdef _WIN32
        int received_count = recv((SOCKET)peer_handle,
            (char *)&received, 1, 0);
#else
        ssize_t received_count = recv((int)peer_handle,
            (char *)&received, 1, 0);
#endif
        if (received_count != 1 || received != 0u) {
            fputs("second independent socket send did not reach its peer\n",
                stderr);
            goto cleanup;
        }
    }
    {
        struct cx_exception_frame frame;
        cx_exception_push(&frame);
        if (setjmp(frame.environment) == 0) {
            second_task = cxnet_System_Net_Sockets_Socket_ReadAsync(
                socket, buffer, 0u, 1u);
            cx_exception_pop(&frame);
        } else {
            rejected = cxcore_System_Exception_Matches(
                (struct cxcore_System_Exception *)cx_exception_current(),
                cxcore_System_InvalidOperationException___typeinfo);
            cx_exception_pop(&frame);
            cx_exception_clear();
        }
    }
    if (!rejected || second_task != NULL) {
        fputs("a second pending socket receive was not rejected\n", stderr);
        goto cleanup;
    }
    {
        cx_byte incoming = 0x33u;
#ifdef _WIN32
        int sent = send((SOCKET)peer_handle, (const char *)&incoming, 1, 0);
#else
        ssize_t sent = send((int)peer_handle, (const char *)&incoming, 1, 0);
#endif
        if (sent != 1) {
            fputs("could not complete the original pending socket read\n",
                stderr);
            goto cleanup;
        }
    }
    while (!first_task->_isCompleted && steps < 100000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (!first_task->_isCompleted || first_task->_isCanceled ||
        first_task->_isFaulted || ((cx_byte *)buffer->_data)[0] != 0x33u) {
        fputs("original socket read did not complete with peer data\n", stderr);
        goto cleanup;
    }
    second_task = cxnet_System_Net_Sockets_Socket_ReadAsync(
        socket, buffer, 0u, 1u);
    struct cx_gc_root_handle *second_task_root = cx_gc_add_root(
        &second_task, (cx_uint)sizeof(second_task));
    if (second_task == NULL || second_task_root == NULL ||
        second_task->_isCompleted) {
        fputs("successful socket read did not release its receive slot\n",
            stderr);
        if (second_task_root != NULL) cx_gc_remove_root(second_task_root);
        goto cleanup;
    }
    cxnet_System_Net_Sockets_Socket_Dispose(socket);
    steps = 0u;
    while (!second_task->_isCompleted && steps < 100000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (!second_task->_isCompleted || !second_task->_isCanceled ||
        second_task->_isFaulted || !stream_peer_observes_eof(peer_handle)) {
        fputs("disposing the socket did not cancel the next pending read\n",
            stderr);
        cx_gc_remove_root(second_task_root);
        goto cleanup;
    }
    cx_gc_remove_root(second_task_root);
    passed = 1;
cleanup:
    if (socket != NULL) cxnet_System_Net_Sockets_Socket_Dispose(socket);
    if (connected_handle != 0u) {
#ifdef _WIN32
        closesocket((SOCKET)(uintptr_t)connected_handle);
#else
        close((int)connected_handle);
#endif
    }
    if (peer_handle != (uintptr_t)0) close_stream_read_peer(peer_handle);
    if (second_send_task_root != NULL) cx_gc_remove_root(second_send_task_root);
    if (send_task_root != NULL) cx_gc_remove_root(send_task_root);
    if (task_root != NULL) cx_gc_remove_root(task_root);
    if (buffer_root != NULL) cx_gc_remove_root(buffer_root);
    if (socket_root != NULL) cx_gc_remove_root(socket_root);
    return passed;
}

static int test_network_stream_rejects_concurrent_reads(void) {
    uint64_t connected_handle = 0u;
    uintptr_t peer_handle = (uintptr_t)0;
    struct cxnet_System_Net_Sockets_Socket *socket = NULL;
    struct cxnet_System_Net_Sockets_NetworkStream *stream = NULL;
    struct cxcore_System_Collections_Array *buffer = NULL;
    struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *first_task = NULL;
    struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *second_task = NULL;
    struct cxcore_System_Threading_Tasks_CancellationToken token = {0};
    struct cx_gc_root_handle *socket_root = NULL;
    struct cx_gc_root_handle *stream_root = NULL;
    struct cx_gc_root_handle *buffer_root = NULL;
    struct cx_gc_root_handle *first_task_root = NULL;
    struct cx_gc_root_handle *second_task_root = NULL;
    uint32_t steps = 0u;
    int rejected = 0;
    int passed = 0;

    if (!create_stream_read_pair(&connected_handle, &peer_handle)) {
        fputs("could not create concurrent NetworkStream read pair\n", stderr);
        return 0;
    }
    socket = (struct cxnet_System_Net_Sockets_Socket *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Sockets_Socket___typeinfo.Size);
    stream = (struct cxnet_System_Net_Sockets_NetworkStream *)
        cxcore_System_Memory_Alloc(
            (cx_uint)sizeof(struct cxnet_System_Net_Sockets_NetworkStream));
    buffer = cx_array_new(1u, (cx_uint)sizeof(cx_byte));
    if (socket == NULL || stream == NULL || buffer == NULL) goto cleanup;
    socket_root = cx_gc_add_root(&socket, (cx_uint)sizeof(socket));
    stream_root = cx_gc_add_root(&stream, (cx_uint)sizeof(stream));
    buffer_root = cx_gc_add_root(&buffer, (cx_uint)sizeof(buffer));
    if (socket_root == NULL || stream_root == NULL || buffer_root == NULL)
        goto cleanup;
    cxnet_System_Net_Sockets_Socket___constructor(socket,
        (cx_ulong)connected_handle);
    connected_handle = 0u;
    cxnet_System_Net_Sockets_NetworkStream___constructor__2(stream, socket, 0);
    first_task = cxnet_System_Net_Sockets_NetworkStream_ReadAsync(
        stream, buffer, 0u, 1u, token);
    first_task_root = cx_gc_add_root(&first_task, (cx_uint)sizeof(first_task));
    if (first_task == NULL || first_task_root == NULL ||
        first_task->_isCompleted) {
        fputs("first NetworkStream read was not left pending\n", stderr);
        goto cleanup;
    }
    cx_gc_collect();
    if (first_task->_isCompleted) {
        fputs("forced GC completed the pending NetworkStream read\n", stderr);
        goto cleanup;
    }
    second_task = cxnet_System_Net_Sockets_NetworkStream_ReadAsync(
        stream, buffer, 0u, 1u, token);
    second_task_root = cx_gc_add_root(&second_task, (cx_uint)sizeof(second_task));
    rejected = second_task != NULL && second_task_root != NULL &&
        second_task->_isCompleted && second_task->_isFaulted &&
        !second_task->_isCanceled &&
        cxcore_System_Exception_Matches(second_task->_exception,
            cxcore_System_InvalidOperationException___typeinfo);
    if (!rejected) {
        fputs("a second pending NetworkStream read did not return a faulted task\n",
            stderr);
        goto cleanup;
    }
    cx_gc_remove_root(second_task_root);
    second_task_root = NULL;
    second_task = NULL;
    {
        const cx_byte incoming = 0x39u;
#ifdef _WIN32
        int sent = send((SOCKET)peer_handle, (const char *)&incoming, 1, 0);
#else
        ssize_t sent = send((int)peer_handle, (const char *)&incoming, 1, 0);
#endif
        if (sent != 1) {
            fputs("could not complete the original NetworkStream read\n",
                stderr);
            goto cleanup;
        }
    }
    while (!first_task->_isCompleted && steps < 100000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (!first_task->_isCompleted || first_task->_isCanceled ||
        first_task->_isFaulted || ((cx_byte *)buffer->_data)[0] != 0x39u) {
        fputs("original NetworkStream read did not complete with peer data\n",
            stderr);
        goto cleanup;
    }
    second_task = cxnet_System_Net_Sockets_NetworkStream_ReadAsync(
        stream, buffer, 0u, 1u, token);
    second_task_root = cx_gc_add_root(&second_task, (cx_uint)sizeof(second_task));
    if (second_task == NULL || second_task_root == NULL ||
        second_task->_isCompleted) {
        fputs("successful NetworkStream read did not release its receive slot\n",
            stderr);
        goto cleanup;
    }
    cxnet_System_Net_Sockets_Socket_Dispose(socket);
    steps = 0u;
    while (!second_task->_isCompleted && steps < 100000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    passed = second_task->_isCompleted && second_task->_isCanceled &&
        !second_task->_isFaulted &&
        !cxnet_System_Net_Sockets_NetworkStream_CanRead___get(stream) &&
        stream_peer_observes_eof(peer_handle);
    if (!passed)
        fputs("socket disposal did not cancel the reused NetworkStream read\n",
            stderr);
cleanup:
    if (stream != NULL) cxnet_System_Net_Sockets_NetworkStream_Dispose(stream);
    if (socket != NULL) cxnet_System_Net_Sockets_Socket_Dispose(socket);
    if (connected_handle != 0u) {
#ifdef _WIN32
        closesocket((SOCKET)(uintptr_t)connected_handle);
#else
        close((int)connected_handle);
#endif
    }
    if (peer_handle != (uintptr_t)0) close_stream_read_peer(peer_handle);
    if (second_task_root != NULL) cx_gc_remove_root(second_task_root);
    if (first_task_root != NULL) cx_gc_remove_root(first_task_root);
    if (buffer_root != NULL) cx_gc_remove_root(buffer_root);
    if (stream_root != NULL) cx_gc_remove_root(stream_root);
    if (socket_root != NULL) cx_gc_remove_root(socket_root);
    return passed;
}

static int task_handle_faults_with_invalid_operation(
    struct cxcore_System_Threading_Tasks_TaskHandle *task) {
    struct cx_exception_frame frame;
    int matches = 0;
    cx_exception_push(&frame);
    if (setjmp(frame.environment) == 0) {
        cxcore_System_Threading_Tasks_TaskHandle_GetResult(task);
        cx_exception_pop(&frame);
    } else {
        matches = cxcore_System_Exception_Matches(
            (struct cxcore_System_Exception *)cx_exception_current(),
            cxcore_System_InvalidOperationException___typeinfo);
        cx_exception_pop(&frame);
        cx_exception_clear();
    }
    return matches;
}

static int test_network_stream_rejects_concurrent_writes(void) {
    uint64_t connected_handle = 0u;
    uintptr_t peer_handle = (uintptr_t)0;
    size_t filled_bytes = 0u;
    struct cxnet_System_Net_Sockets_Socket *socket = NULL;
    struct cxnet_System_Net_Sockets_NetworkStream *stream = NULL;
    struct cxcore_System_Collections_Array *buffer = NULL;
    struct cxcore_System_Threading_Tasks_TaskHandle *first_task = NULL;
    struct cxcore_System_Threading_Tasks_TaskHandle *second_task = NULL;
    struct cxcore_System_Threading_Tasks_CancellationToken token = {0};
    struct cx_gc_root_handle *socket_root = NULL;
    struct cx_gc_root_handle *stream_root = NULL;
    struct cx_gc_root_handle *buffer_root = NULL;
    struct cx_gc_root_handle *first_task_root = NULL;
    struct cx_gc_root_handle *second_task_root = NULL;
    struct socket_peer_payload_drain drain = {0};
    uint32_t steps = 0u;
    int worker_started = 0;
    int passed = 0;
#ifdef _WIN32
    HANDLE worker = NULL;
#else
    pthread_t worker;
#endif

    if (!create_stream_read_pair(&connected_handle, &peer_handle) ||
        !socket_prefill_send_buffer(connected_handle, peer_handle,
            &filled_bytes)) {
        fputs("could not create saturated NetworkStream write fixture\n",
            stderr);
        goto cleanup;
    }
    socket = (struct cxnet_System_Net_Sockets_Socket *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Sockets_Socket___typeinfo.Size);
    stream = (struct cxnet_System_Net_Sockets_NetworkStream *)
        cxcore_System_Memory_Alloc(
            (cx_uint)sizeof(struct cxnet_System_Net_Sockets_NetworkStream));
    buffer = cx_array_new(1u, (cx_uint)sizeof(cx_byte));
    if (socket == NULL || stream == NULL || buffer == NULL) goto cleanup;
    socket_root = cx_gc_add_root(&socket, (cx_uint)sizeof(socket));
    stream_root = cx_gc_add_root(&stream, (cx_uint)sizeof(stream));
    buffer_root = cx_gc_add_root(&buffer, (cx_uint)sizeof(buffer));
    if (socket_root == NULL || stream_root == NULL || buffer_root == NULL)
        goto cleanup;
    cxnet_System_Net_Sockets_Socket___constructor(socket,
        (cx_ulong)connected_handle);
    connected_handle = 0u;
    cxnet_System_Net_Sockets_NetworkStream___constructor__2(stream, socket, 0);
    ((cx_byte *)buffer->_data)[0] = 0x5au;
    first_task = cxnet_System_Net_Sockets_NetworkStream_WriteAsync(
        stream, buffer, 0u, 1u, token);
    first_task_root = cx_gc_add_root(&first_task, (cx_uint)sizeof(first_task));
    if (first_task == NULL || first_task_root == NULL ||
        cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(
            first_task)) {
        fputs("first NetworkStream write was not left pending\n", stderr);
        goto cleanup;
    }
    cx_gc_collect();
    if (cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(
            first_task)) {
        fputs("forced GC completed the pending NetworkStream write\n", stderr);
        goto cleanup;
    }
    second_task = cxnet_System_Net_Sockets_NetworkStream_WriteAsync(
        stream, buffer, 0u, 1u, token);
    second_task_root = cx_gc_add_root(&second_task,
        (cx_uint)sizeof(second_task));
    if (second_task == NULL || second_task_root == NULL ||
        !cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(
            second_task) ||
        !cxcore_System_Threading_Tasks_TaskHandle_IsFaulted___const_get(
            second_task) ||
        cxcore_System_Threading_Tasks_TaskHandle_IsCanceled___const_get(
            second_task) ||
        !task_handle_faults_with_invalid_operation(second_task)) {
        fputs("a second pending NetworkStream write did not return a faulted task\n",
            stderr);
        goto cleanup;
    }
    cx_gc_remove_root(second_task_root);
    second_task_root = NULL;
    second_task = NULL;

    drain.peer = peer_handle;
    drain.filled_bytes = filled_bytes;
    drain.trailing_bytes = 1u;
#ifdef _WIN32
    worker = CreateThread(NULL, 0, socket_peer_payload_drain_worker,
        &drain, 0, NULL);
    if (worker == NULL) goto cleanup;
#else
    if (pthread_create(&worker, NULL, socket_peer_payload_drain_worker,
            &drain) != 0) goto cleanup;
#endif
    worker_started = 1;
    while ((!cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(
                first_task) ||
            !socket_dispose_race_load(&drain.completed)) && steps < 100000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
#ifdef _WIN32
    WaitForSingleObject(worker, INFINITE);
    CloseHandle(worker);
#else
    pthread_join(worker, NULL);
#endif
    worker_started = 0;
    if (!socket_dispose_race_load(&drain.completed) ||
        socket_dispose_race_load(&drain.failed) ||
        !cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(
            first_task) ||
        cxcore_System_Threading_Tasks_TaskHandle_IsFaulted___const_get(
            first_task) ||
        cxcore_System_Threading_Tasks_TaskHandle_IsCanceled___const_get(
            first_task) ||
        !cxnet_System_Net_Sockets_Socket_Connected___const_get(socket)) {
        fputs("original NetworkStream write did not complete after peer drain\n",
            stderr);
        goto cleanup;
    }
    second_task = cxnet_System_Net_Sockets_NetworkStream_WriteAsync(
        stream, buffer, 0u, 1u, token);
    second_task_root = cx_gc_add_root(&second_task,
        (cx_uint)sizeof(second_task));
    steps = 0u;
    while (second_task != NULL &&
        !cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(
            second_task) &&
        steps < 100000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    passed = second_task != NULL && second_task_root != NULL &&
        cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(
            second_task) &&
        !cxcore_System_Threading_Tasks_TaskHandle_IsFaulted___const_get(
            second_task) &&
        !cxcore_System_Threading_Tasks_TaskHandle_IsCanceled___const_get(
            second_task);
    if (!passed)
        fputs("NetworkStream send slot was not reusable after write completion\n",
            stderr);
cleanup:
    if (worker_started) {
#ifdef _WIN32
        WaitForSingleObject(worker, INFINITE);
        CloseHandle(worker);
#else
        pthread_join(worker, NULL);
#endif
    }
    if (stream != NULL) cxnet_System_Net_Sockets_NetworkStream_Dispose(stream);
    if (socket != NULL) cxnet_System_Net_Sockets_Socket_Dispose(socket);
    if (connected_handle != 0u) {
#ifdef _WIN32
        closesocket((SOCKET)(uintptr_t)connected_handle);
#else
        close((int)connected_handle);
#endif
    }
    if (peer_handle != (uintptr_t)0) close_stream_read_peer(peer_handle);
    if (second_task_root != NULL) cx_gc_remove_root(second_task_root);
    if (first_task_root != NULL) cx_gc_remove_root(first_task_root);
    if (buffer_root != NULL) cx_gc_remove_root(buffer_root);
    if (stream_root != NULL) cx_gc_remove_root(stream_root);
    if (socket_root != NULL) cx_gc_remove_root(socket_root);
    return passed;
}

static int socket_prefill_send_buffer(uint64_t handle, uintptr_t peer_handle,
    size_t *filled_bytes) {
    char payload[4096];
    int send_buffer_size = 4096;
    int receive_buffer_size = 4096;
    memset(payload, 0xa7, sizeof(payload));
    *filled_bytes = 0u;
#ifdef _WIN32
    {
        SOCKET socket_handle = (SOCKET)(uintptr_t)handle;
        SOCKET peer_socket = (SOCKET)peer_handle;
        u_long nonblocking = 1u;
        if (setsockopt(peer_socket, SOL_SOCKET, SO_RCVBUF,
                (const char *)&receive_buffer_size,
                (int)sizeof(receive_buffer_size)) != 0 ||
            setsockopt(socket_handle, SOL_SOCKET, SO_SNDBUF,
                (const char *)&send_buffer_size,
                (int)sizeof(send_buffer_size)) != 0 ||
            ioctlsocket(socket_handle, FIONBIO, &nonblocking) != 0) return 0;
        for (;;) {
            int amount = send(socket_handle, payload, (int)sizeof(payload), 0);
            if (amount > 0) {
                *filled_bytes += (size_t)amount;
                continue;
            }
            if (amount < 0 && WSAGetLastError() == WSAEWOULDBLOCK) break;
            return 0;
        }
    }
#else
    {
        int socket_handle = (int)handle;
        int flags = fcntl(socket_handle, F_GETFL, 0);
        if (setsockopt((int)peer_handle, SOL_SOCKET, SO_RCVBUF,
                &receive_buffer_size,
                (socklen_t)sizeof(receive_buffer_size)) != 0 ||
            flags < 0 || setsockopt(socket_handle, SOL_SOCKET, SO_SNDBUF,
                &send_buffer_size, (socklen_t)sizeof(send_buffer_size)) != 0 ||
            fcntl(socket_handle, F_SETFL, flags | O_NONBLOCK) != 0) return 0;
        for (;;) {
            ssize_t amount = send(socket_handle, payload, sizeof(payload), 0);
            if (amount > 0) {
                *filled_bytes += (size_t)amount;
                continue;
            }
            if (amount < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
            return 0;
        }
    }
#endif
    return *filled_bytes != 0u;
}

static int socket_write_task_completed(
    const struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *generic_task,
    const struct cxcore_System_Threading_Tasks_TaskHandle *task_handle);
static int socket_write_task_faulted(
    const struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *generic_task,
    const struct cxcore_System_Threading_Tasks_TaskHandle *task_handle);
static int socket_write_task_canceled(
    const struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *generic_task,
    const struct cxcore_System_Threading_Tasks_TaskHandle *task_handle);
static struct cxcore_System_Exception *socket_write_task_exception(
    const struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *generic_task,
    const struct cxcore_System_Threading_Tasks_TaskHandle *task_handle);

static int test_managed_pending_write_peer_reset(int use_network_stream) {
    uint64_t connected_handle = 0u;
    uint64_t reset_peer_handle = 0u;
    size_t filled_bytes = 0u;
    struct cxnet_System_Net_Sockets_Socket *socket = NULL;
    struct cxnet_System_Net_Sockets_NetworkStream *stream = NULL;
    struct cxcore_System_Collections_Array *buffer = NULL;
    struct cxcore_System_Threading_Tasks_TaskHandle *stream_task = NULL;
    struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *socket_task = NULL;
    struct cxcore_System_Threading_Tasks_CancellationToken cancellation_token = {0};
    struct cx_gc_root_handle *socket_root = NULL;
    struct cx_gc_root_handle *stream_root = NULL;
    struct cx_gc_root_handle *buffer_root = NULL;
    struct cx_gc_root_handle *stream_task_root = NULL;
    struct cx_gc_root_handle *socket_task_root = NULL;
    uint32_t steps = 0u;
    int native_handle_open = 0;
    int passed = 0;

    if (!create_reset_socket(&connected_handle, &reset_peer_handle)) {
        fputs("could not create a pending managed-write reset pair\n", stderr);
        return 0;
    }
    native_handle_open = 1;
    if (!socket_prefill_send_buffer(connected_handle,
            (uintptr_t)reset_peer_handle, &filled_bytes) || filled_bytes == 0u) {
        fputs("could not saturate the socket before managed-write reset\n",
            stderr);
        goto cleanup;
    }
    socket = (struct cxnet_System_Net_Sockets_Socket *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Sockets_Socket___typeinfo.Size);
    if (use_network_stream) {
        stream = (struct cxnet_System_Net_Sockets_NetworkStream *)
            cxcore_System_Memory_Alloc((cx_uint)sizeof(*stream));
    }
    buffer = cx_array_new(1u, (cx_uint)sizeof(cx_byte));
    if (socket == NULL || (use_network_stream && stream == NULL) ||
        buffer == NULL) goto cleanup;
    socket_root = cx_gc_add_root(&socket, (cx_uint)sizeof(socket));
    if (stream != NULL)
        stream_root = cx_gc_add_root(&stream, (cx_uint)sizeof(stream));
    buffer_root = cx_gc_add_root(&buffer, (cx_uint)sizeof(buffer));
    if (socket_root == NULL || (use_network_stream && stream_root == NULL) ||
        buffer_root == NULL)
        goto cleanup;
    cxnet_System_Net_Sockets_Socket___constructor(socket,
        (cx_ulong)connected_handle);
    native_handle_open = 0;
    if (stream != NULL)
        cxnet_System_Net_Sockets_NetworkStream___constructor__2(stream,
            socket, 0);
    ((cx_byte *)buffer->_data)[0] = 0x5au;
    if (stream != NULL) {
        stream_task = cxnet_System_Net_Sockets_NetworkStream_WriteAsync(
            stream, buffer, 0u, 1u, cancellation_token);
        stream_task_root = cx_gc_add_root(&stream_task,
            (cx_uint)sizeof(stream_task));
    } else {
        socket_task = cxnet_System_Net_Sockets_Socket_WriteAsync(
            socket, buffer, 0u, 1u);
        socket_task_root = cx_gc_add_root(&socket_task,
            (cx_uint)sizeof(socket_task));
    }
    if (socket_write_task_completed(socket_task, stream_task) ||
        (stream != NULL && (stream_task == NULL || stream_task_root == NULL)) ||
        (stream == NULL && (socket_task == NULL || socket_task_root == NULL))) {
        fprintf(stderr, "managed %s.WriteAsync did not remain pending before reset\n",
            use_network_stream ? "NetworkStream" : "Socket");
        goto cleanup;
    }
    cx_gc_collect();
    if (socket_write_task_completed(socket_task, stream_task)) {
        fprintf(stderr, "forced GC changed a pending managed %s write\n",
            use_network_stream ? "NetworkStream" : "Socket");
        goto cleanup;
    }
#ifdef _WIN32
    closesocket((SOCKET)(uintptr_t)reset_peer_handle);
#else
    close((int)(uintptr_t)reset_peer_handle);
#endif
    reset_peer_handle = 0u;
    while (!socket_write_task_completed(socket_task, stream_task) &&
        steps < 100000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (socket_write_task_completed(socket_task, stream_task) &&
        socket_write_task_faulted(socket_task, stream_task) &&
        !socket_write_task_canceled(socket_task, stream_task) &&
        cxcore_System_Exception_Matches(
            socket_write_task_exception(socket_task, stream_task),
            cxnet_System_Net_Sockets_SocketException___typeinfo) &&
        cxnet_System_Net_Sockets_SocketException_SocketErrorCode___const_get(
            (struct cxnet_System_Net_Sockets_SocketException *)
                socket_write_task_exception(socket_task, stream_task)) ==
            cxnet_System_Net_Sockets_SocketError_ConnectionReset &&
        cxcore_System_IO_IOException_NativeErrorCode___const_get(
            (struct cxcore_System_IO_IOException *)
                socket_write_task_exception(socket_task, stream_task)) != 0 &&
        cxcore_System_IO_IOException_NativeErrorDomain___const_get(
            (struct cxcore_System_IO_IOException *)
                socket_write_task_exception(socket_task, stream_task)) != NULL) {
        passed = 1;
    } else {
        fprintf(stderr,
            "pending managed %s.WriteAsync did not preserve peer-reset identity\n",
            use_network_stream ? "NetworkStream" : "Socket");
    }
cleanup:
#ifdef _WIN32
    if (reset_peer_handle != 0u)
        closesocket((SOCKET)(uintptr_t)reset_peer_handle);
#else
    if (reset_peer_handle != 0u) close((int)(uintptr_t)reset_peer_handle);
#endif
    if (socket_write_task_completed(socket_task, stream_task) == 0 &&
        (socket_task != NULL || stream_task != NULL)) {
        steps = 0u;
        while (!socket_write_task_completed(socket_task, stream_task) &&
            steps < 100000u) {
            if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                Sleep(1u);
#else
                usleep(1000u);
#endif
            }
            steps++;
        }
    }
    if (stream != NULL && stream_root != NULL)
        cxnet_System_Net_Sockets_NetworkStream_Dispose(stream);
    if (!native_handle_open && socket != NULL)
        cxnet_System_Net_Sockets_Socket_Dispose(socket);
    else if (native_handle_open && connected_handle != 0u) {
#ifdef _WIN32
        closesocket((SOCKET)(uintptr_t)connected_handle);
#else
        close((int)(uintptr_t)connected_handle);
#endif
    }
#ifdef _WIN32
    WSACleanup();
#endif
    if (stream_task_root != NULL) cx_gc_remove_root(stream_task_root);
    if (socket_task_root != NULL) cx_gc_remove_root(socket_task_root);
    if (buffer_root != NULL) cx_gc_remove_root(buffer_root);
    if (stream_root != NULL) cx_gc_remove_root(stream_root);
    if (socket_root != NULL) cx_gc_remove_root(socket_root);
    return passed;
}

static int stream_peer_observes_payload_then_eof(uintptr_t peer_handle,
    size_t expected_bytes) {
    char payload[4096];
    size_t received_bytes = 0u;
#ifdef _WIN32
    DWORD timeout = 5000u;
    (void)setsockopt((SOCKET)peer_handle, SOL_SOCKET, SO_RCVTIMEO,
        (const char *)&timeout, (int)sizeof(timeout));
#else
    struct timeval timeout = {5, 0};
    (void)setsockopt((int)peer_handle, SOL_SOCKET, SO_RCVTIMEO,
        &timeout, (socklen_t)sizeof(timeout));
#endif
    for (;;) {
#ifdef _WIN32
        int amount = recv((SOCKET)peer_handle, payload, (int)sizeof(payload), 0);
#else
        ssize_t amount = recv((int)peer_handle, payload, sizeof(payload), 0);
#endif
        size_t index;
        if (amount == 0) return received_bytes == expected_bytes;
        if (amount < 0 || (size_t)amount > expected_bytes - received_bytes)
            return 0;
        for (index = 0u; index < (size_t)amount; index++) {
            if ((unsigned char)payload[index] != 0xa7u) return 0;
        }
        received_bytes += (size_t)amount;
    }
}

static int stream_peer_reads_expected(uintptr_t peer_handle,
    unsigned char expected_value, size_t expected_bytes) {
    char buffer[4096];
    size_t received_bytes = 0u;
#ifdef _WIN32
    DWORD timeout = 5000u;
    (void)setsockopt((SOCKET)peer_handle, SOL_SOCKET, SO_RCVTIMEO,
        (const char *)&timeout, (int)sizeof(timeout));
#else
    struct timeval timeout = {5, 0};
    (void)setsockopt((int)peer_handle, SOL_SOCKET, SO_RCVTIMEO,
        &timeout, (socklen_t)sizeof(timeout));
#endif
    while (received_bytes < expected_bytes) {
        size_t requested = expected_bytes - received_bytes;
        size_t index;
#ifdef _WIN32
        int amount;
#else
        ssize_t amount;
#endif
        if (requested > sizeof(buffer)) requested = sizeof(buffer);
#ifdef _WIN32
        amount = recv((SOCKET)peer_handle, buffer, (int)requested, 0);
#else
        amount = recv((int)peer_handle, buffer, requested, 0);
#endif
        if (amount <= 0 || (size_t)amount > requested) return 0;
        for (index = 0u; index < (size_t)amount; index++) {
            if ((unsigned char)buffer[index] != expected_value) return 0;
        }
        received_bytes += (size_t)amount;
    }
    return 1;
}

#ifdef _WIN32
static DWORD WINAPI socket_peer_cancel_race_drain_worker(void *argument) {
#else
static void *socket_peer_cancel_race_drain_worker(void *argument) {
#endif
    struct socket_peer_cancel_race_drain *drain =
        (struct socket_peer_cancel_race_drain *)argument;
    while (!socket_dispose_race_load(&drain->start)) {
#ifdef _WIN32
        Sleep(0u);
#else
        sched_yield();
#endif
    }
    if (!stream_peer_reads_expected(drain->peer, 0xa7u,
            drain->filled_bytes)) {
        socket_dispose_race_store(&drain->failed, 1);
    }
    socket_dispose_race_store(&drain->completed, 1);
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

#ifdef _WIN32
static DWORD WINAPI socket_peer_readiness_race_send_worker(void *argument) {
#else
static void *socket_peer_readiness_race_send_worker(void *argument) {
#endif
    struct socket_peer_readiness_race_send *send_state =
        (struct socket_peer_readiness_race_send *)argument;
    const unsigned char incoming = 0x36u;
    while (!socket_dispose_race_load(&send_state->start)) {
#ifdef _WIN32
        Sleep(0u);
#else
        sched_yield();
#endif
    }
#ifdef _WIN32
    if (send((SOCKET)send_state->peer, (const char *)&incoming, 1, 0) != 1)
#else
    if (send((int)send_state->peer, (const char *)&incoming, 1, 0) != 1)
#endif
        socket_dispose_race_store(&send_state->failed, 1);
    socket_dispose_race_store(&send_state->completed, 1);
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

static int test_managed_socket_saturated_write(void) {
    const uint32_t write_length = 1024u * 1024u;
    uint64_t connected_handle = 0u;
    uintptr_t peer_handle = (uintptr_t)0;
    size_t filled_bytes = 0u;
    size_t initially_drained;
    struct cxnet_System_Net_Sockets_Socket *socket = NULL;
    struct cxcore_System_Collections_Array *buffer = NULL;
    struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *task = NULL;
    struct cx_gc_root_handle *socket_root = NULL;
    struct cx_gc_root_handle *buffer_root = NULL;
    struct cx_gc_root_handle *task_root = NULL;
    uint32_t steps = 0u;
    uint32_t transferred = 0u;
    int native_handle_open = 0;
    int socket_initialized = 0;
    int passed = 0;

    if (!create_stream_read_pair(&connected_handle, &peer_handle)) {
        fputs("could not create a managed saturated-write socket pair\n", stderr);
        return 0;
    }
    native_handle_open = 1;
    if (!socket_prefill_send_buffer(connected_handle, peer_handle,
            &filled_bytes)) {
        fputs("could not saturate the socket before short-write test\n",
            stderr);
        goto cleanup;
    }
    socket = (struct cxnet_System_Net_Sockets_Socket *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Sockets_Socket___typeinfo.Size);
    buffer = cx_array_new(write_length, (cx_uint)sizeof(cx_byte));
    if (socket == NULL || buffer == NULL) goto cleanup;
    socket_root = cx_gc_add_root(&socket, (cx_uint)sizeof(socket));
    buffer_root = cx_gc_add_root(&buffer, (cx_uint)sizeof(buffer));
    if (socket_root == NULL || buffer_root == NULL) goto cleanup;
    memset(buffer->_data, 0x5a, write_length);
    cxnet_System_Net_Sockets_Socket___constructor(socket,
        (cx_ulong)connected_handle);
    socket_initialized = 1;
    native_handle_open = 0;
    task = cxnet_System_Net_Sockets_Socket_WriteAsync(socket, buffer, 0u,
        write_length);
    task_root = cx_gc_add_root(&task, (cx_uint)sizeof(task));
    if (task == NULL || task_root == NULL || task->_isCompleted) {
        fputs("saturated Socket.WriteAsync did not remain pending\n", stderr);
        goto cleanup;
    }
    cx_gc_collect();
    if (task->_isCompleted) {
        fputs("forced GC changed a pending saturated socket write\n", stderr);
        goto cleanup;
    }
    initially_drained = filled_bytes / 2u;
    if (initially_drained < 4096u) {
        initially_drained = filled_bytes < 4096u ? filled_bytes : 4096u;
    }
    if (!stream_peer_reads_expected(peer_handle, 0xa7u, initially_drained)) {
        fputs("peer could not free the requested socket send capacity\n",
            stderr);
        goto cleanup;
    }
    while (!task->_isCompleted && steps < 100000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (!task->_isCompleted || task->_isCanceled || task->_isFaulted) {
        fputs("pending Socket.WriteAsync did not complete successfully\n",
            stderr);
        goto cleanup;
    }
    transferred = cx_generic_32f22edd4c9ba40c64ed2658909ec8fb1d6e85d2c4276d46eebd01030eefa80f(task);
    if (transferred == 0u || transferred > write_length
#ifndef _WIN32
        || transferred == write_length
#endif
        ) {
        fprintf(stderr,
            "Socket.WriteAsync returned an invalid transfer count: %u/%u\n",
            (unsigned)transferred, (unsigned)write_length);
        goto cleanup;
    }
    if (!stream_peer_reads_expected(peer_handle, 0xa7u,
            filled_bytes - initially_drained) ||
        !stream_peer_reads_expected(peer_handle, 0x5au, transferred)) {
        fputs("saturated Socket.WriteAsync payload or ordering was incorrect\n",
            stderr);
        goto cleanup;
    }
    cxnet_System_Net_Sockets_Socket_Dispose(socket);
    if (!stream_peer_observes_eof(peer_handle)) {
        fputs("peer did not observe EOF after short-write cleanup\n", stderr);
        goto cleanup;
    }
    passed = 1;
cleanup:
    if (socket_initialized) {
        cxnet_System_Net_Sockets_Socket_Dispose(socket);
    } else if (native_handle_open) {
#ifdef _WIN32
        closesocket((SOCKET)(uintptr_t)connected_handle);
#else
        close((int)connected_handle);
#endif
    }
    if (peer_handle != (uintptr_t)0) close_stream_read_peer(peer_handle);
    if (task_root != NULL) cx_gc_remove_root(task_root);
    if (buffer_root != NULL) cx_gc_remove_root(buffer_root);
    if (socket_root != NULL) cx_gc_remove_root(socket_root);
    return passed;
}

static int test_managed_socket_rejects_concurrent_writes(void) {
    uint64_t connected_handle = 0u;
    uintptr_t peer_handle = (uintptr_t)0;
    size_t filled_bytes = 0u;
    struct cxnet_System_Net_Sockets_Socket *socket = NULL;
    struct cxcore_System_Collections_Array *buffer = NULL;
    struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *first_task = NULL;
    struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *read_task = NULL;
    struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *retry_task = NULL;
    struct cx_gc_root_handle *socket_root = NULL;
    struct cx_gc_root_handle *buffer_root = NULL;
    struct cx_gc_root_handle *first_task_root = NULL;
    struct cx_gc_root_handle *read_task_root = NULL;
    struct cx_gc_root_handle *retry_task_root = NULL;
    struct socket_peer_payload_drain drain = {0};
    uint32_t steps = 0u;
    int native_handle_open = 0;
    int socket_initialized = 0;
    int worker_started = 0;
    int rejected = 0;
    int passed = 0;
#ifdef _WIN32
    HANDLE worker = NULL;
#else
    pthread_t worker;
#endif

    if (!create_stream_read_pair(&connected_handle, &peer_handle)) {
        fputs("could not create concurrent managed socket write pair\n",
            stderr);
        return 0;
    }
    native_handle_open = 1;
    if (!socket_prefill_send_buffer(connected_handle, peer_handle,
            &filled_bytes)) {
        fputs("could not saturate socket for concurrent-write rejection\n",
            stderr);
        goto cleanup;
    }
    socket = (struct cxnet_System_Net_Sockets_Socket *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Sockets_Socket___typeinfo.Size);
    buffer = cx_array_new(1u, (cx_uint)sizeof(cx_byte));
    if (socket == NULL || buffer == NULL) goto cleanup;
    socket_root = cx_gc_add_root(&socket, (cx_uint)sizeof(socket));
    buffer_root = cx_gc_add_root(&buffer, (cx_uint)sizeof(buffer));
    if (socket_root == NULL || buffer_root == NULL) goto cleanup;
    ((cx_byte *)buffer->_data)[0] = 0x5au;
    cxnet_System_Net_Sockets_Socket___constructor(socket,
        (cx_ulong)connected_handle);
    native_handle_open = 0;
    socket_initialized = 1;

    first_task = cxnet_System_Net_Sockets_Socket_WriteAsync(socket, buffer,
        0u, 1u);
    first_task_root = cx_gc_add_root(&first_task,
        (cx_uint)sizeof(first_task));
    if (first_task == NULL || first_task_root == NULL ||
        first_task->_isCompleted) {
        fputs("first saturated Socket.WriteAsync was not left pending\n",
            stderr);
        goto cleanup;
    }
    cx_gc_collect();
    if (first_task->_isCompleted) {
        fputs("forced GC completed a pending saturated socket write\n",
            stderr);
        goto cleanup;
    }

    {
        struct cx_exception_frame frame;
        cx_exception_push(&frame);
        if (setjmp(frame.environment) == 0) {
            retry_task = cxnet_System_Net_Sockets_Socket_WriteAsync(socket,
                buffer, 0u, 1u);
            cx_exception_pop(&frame);
        } else {
            rejected = cxcore_System_Exception_Matches(
                (struct cxcore_System_Exception *)cx_exception_current(),
                cxcore_System_InvalidOperationException___typeinfo);
            cx_exception_pop(&frame);
            cx_exception_clear();
        }
    }
    if (!rejected || retry_task != NULL || first_task->_isCompleted) {
        fputs("a second pending socket write was not rejected while preserving the first\n",
            stderr);
        goto cleanup;
    }

    drain.peer = peer_handle;
    drain.filled_bytes = filled_bytes;
    drain.trailing_bytes = 1u;
#ifdef _WIN32
    worker = CreateThread(NULL, 0, socket_peer_payload_drain_worker,
        &drain, 0, NULL);
    if (worker == NULL) goto cleanup;
#else
    if (pthread_create(&worker, NULL, socket_peer_payload_drain_worker,
            &drain) != 0) goto cleanup;
#endif
    worker_started = 1;
    while ((!first_task->_isCompleted ||
            !socket_dispose_race_load(&drain.completed)) && steps < 100000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
#ifdef _WIN32
    WaitForSingleObject(worker, INFINITE);
    CloseHandle(worker);
#else
    pthread_join(worker, NULL);
#endif
    worker_started = 0;
    if (!socket_dispose_race_load(&drain.completed) ||
        socket_dispose_race_load(&drain.failed) || !first_task->_isCompleted ||
        first_task->_isCanceled || first_task->_isFaulted ||
        !cxnet_System_Net_Sockets_Socket_Connected___const_get(socket)) {
        fputs("the original socket write did not complete after peer drain\n",
            stderr);
        goto cleanup;
    }

    read_task = cxnet_System_Net_Sockets_Socket_ReadAsync(socket, buffer,
        0u, 1u);
    read_task_root = cx_gc_add_root(&read_task, (cx_uint)sizeof(read_task));
    steps = 0u;
    while (read_task != NULL && !read_task->_isCompleted &&
        steps < 100000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (read_task == NULL || read_task_root == NULL ||
        !read_task->_isCompleted || read_task->_isCanceled ||
        read_task->_isFaulted ||
        cx_generic_32f22edd4c9ba40c64ed2658909ec8fb1d6e85d2c4276d46eebd01030eefa80f(read_task) != 1u ||
        ((cx_byte *)buffer->_data)[0] != 0x36u) {
        fputs("could not consume the peer reply before socket disposal\n",
            stderr);
        goto cleanup;
    }
    cx_gc_remove_root(read_task_root);
    read_task_root = NULL;
    read_task = NULL;
    ((cx_byte *)buffer->_data)[0] = 0x5au;

    retry_task = cxnet_System_Net_Sockets_Socket_WriteAsync(socket, buffer,
        0u, 1u);
    retry_task_root = cx_gc_add_root(&retry_task,
        (cx_uint)sizeof(retry_task));
    steps = 0u;
    while (retry_task != NULL && !retry_task->_isCompleted &&
        steps < 100000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (retry_task == NULL || retry_task_root == NULL ||
        !retry_task->_isCompleted || retry_task->_isCanceled ||
        retry_task->_isFaulted ||
        !stream_peer_reads_expected(peer_handle, 0x5au, 1u)) {
        fputs("socket send slot was not reusable after the pending write completed\n",
            stderr);
        goto cleanup;
    }
    cxnet_System_Net_Sockets_Socket_Dispose(socket);
    if (!stream_peer_observes_eof(peer_handle)) {
        fputs("peer did not observe EOF after concurrent-write check\n",
            stderr);
        goto cleanup;
    }
    passed = 1;
cleanup:
    if (socket_initialized) {
        cxnet_System_Net_Sockets_Socket_Dispose(socket);
    } else if (native_handle_open) {
#ifdef _WIN32
        closesocket((SOCKET)(uintptr_t)connected_handle);
#else
        close((int)connected_handle);
#endif
    }
    if (worker_started) {
#ifdef _WIN32
        WaitForSingleObject(worker, INFINITE);
        CloseHandle(worker);
#else
        pthread_join(worker, NULL);
#endif
    }
    if (peer_handle != (uintptr_t)0) close_stream_read_peer(peer_handle);
    if (retry_task_root != NULL) cx_gc_remove_root(retry_task_root);
    if (read_task_root != NULL) cx_gc_remove_root(read_task_root);
    if (first_task_root != NULL) cx_gc_remove_root(first_task_root);
    if (buffer_root != NULL) cx_gc_remove_root(buffer_root);
    if (socket_root != NULL) cx_gc_remove_root(socket_root);
    return passed;
}

static int test_managed_socket_pending_write_cancel(void) {
    uint64_t connected_handle = 0u;
    uintptr_t peer_handle = (uintptr_t)0;
    size_t filled_bytes = 0u;
    struct cxnet_System_Net_Sockets_Socket *socket = NULL;
    struct cxcore_System_Collections_Array *buffer = NULL;
    struct cxcore_System_Threading_Tasks_CancellationTokenSource *source = NULL;
    struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *task = NULL;
    struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *retry_task = NULL;
    struct cx_gc_root_handle *socket_root = NULL;
    struct cx_gc_root_handle *buffer_root = NULL;
    struct cx_gc_root_handle *source_root = NULL;
    struct cx_gc_root_handle *task_root = NULL;
    struct cx_gc_root_handle *retry_task_root = NULL;
    uint32_t steps = 0u;
    int native_handle_open = 0;
    int socket_initialized = 0;
    int passed = 0;

    if (!create_stream_read_pair(&connected_handle, &peer_handle)) {
        fputs("could not create a managed write-cancellation socket pair\n",
            stderr);
        return 0;
    }
    native_handle_open = 1;
    if (!socket_prefill_send_buffer(connected_handle, peer_handle,
            &filled_bytes)) {
        fputs("could not saturate the socket before write cancellation\n",
            stderr);
        goto cleanup;
    }
    socket = (struct cxnet_System_Net_Sockets_Socket *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Sockets_Socket___typeinfo.Size);
    buffer = cx_array_new(1u, (cx_uint)sizeof(cx_byte));
    source = (struct cxcore_System_Threading_Tasks_CancellationTokenSource *)
        cxcore_System_Memory_Alloc(
            cxcore_System_Threading_Tasks_CancellationTokenSource___typeinfo.Size);
    if (socket == NULL || buffer == NULL || source == NULL) goto cleanup;
    socket_root = cx_gc_add_root(&socket, (cx_uint)sizeof(socket));
    buffer_root = cx_gc_add_root(&buffer, (cx_uint)sizeof(buffer));
    source_root = cx_gc_add_root(&source, (cx_uint)sizeof(source));
    if (socket_root == NULL || buffer_root == NULL || source_root == NULL)
        goto cleanup;
    cxcore_System_Threading_Tasks_CancellationTokenSource___constructor(source);
    cxnet_System_Net_Sockets_Socket___constructor(socket,
        (cx_ulong)connected_handle);
    socket_initialized = 1;
    native_handle_open = 0;
    ((cx_byte *)buffer->_data)[0] = 0x5au;
    task = cxnet_System_Net_Sockets_Socket_WriteAsync__2(socket, buffer, 0u,
        1u,
        cxcore_System_Threading_Tasks_CancellationTokenSource_Token___const_get(
            source));
    task_root = cx_gc_add_root(&task, (cx_uint)sizeof(task));
    if (task == NULL || task_root == NULL || task->_isCompleted) {
        fputs("saturated Socket.WriteAsync completed before cancellation\n",
            stderr);
        goto cleanup;
    }
    cx_gc_collect();
    if (task->_isCompleted) {
        fputs("forced GC changed a pending cancellable socket write\n",
            stderr);
        goto cleanup;
    }
    cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(source);
    while (!task->_isCompleted && steps < 100000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (!task->_isCompleted || !task->_isCanceled || task->_isFaulted ||
        source->_callbackCount != 0u ||
        !cxnet_System_Net_Sockets_Socket_Connected___const_get(socket)) {
        fprintf(stderr,
            "cancelling a pending Socket.WriteAsync failed completed=%d canceled=%d faulted=%d callbacks=%u connected=%d steps=%u\n",
            task->_isCompleted, task->_isCanceled, task->_isFaulted,
            (unsigned)source->_callbackCount,
            cxnet_System_Net_Sockets_Socket_Connected___const_get(socket),
            (unsigned)steps);
        goto cleanup;
    }
    if (!stream_peer_reads_expected(peer_handle, 0xa7u, filled_bytes)) {
        fputs("canceled Socket.WriteAsync changed the saturated peer payload\n",
            stderr);
        goto cleanup;
    }
    retry_task = cxnet_System_Net_Sockets_Socket_WriteAsync(socket, buffer,
        0u, 1u);
    retry_task_root = cx_gc_add_root(&retry_task, (cx_uint)sizeof(retry_task));
    if (retry_task == NULL || retry_task_root == NULL ||
        !retry_task->_isCompleted || retry_task->_isCanceled ||
        retry_task->_isFaulted ||
        !stream_peer_reads_expected(peer_handle, 0x5au, 1u)) {
        fputs("canceled Socket.WriteAsync did not release its send slot\n",
            stderr);
        goto cleanup;
    }
    cxnet_System_Net_Sockets_Socket_Dispose(socket);
    passed = stream_peer_observes_eof(peer_handle);
cleanup:
    if (socket_initialized) {
        cxnet_System_Net_Sockets_Socket_Dispose(socket);
    } else if (native_handle_open) {
#ifdef _WIN32
        closesocket((SOCKET)(uintptr_t)connected_handle);
#else
        close((int)connected_handle);
#endif
    }
    if (peer_handle != (uintptr_t)0) close_stream_read_peer(peer_handle);
    if (retry_task_root != NULL) cx_gc_remove_root(retry_task_root);
    if (task_root != NULL) cx_gc_remove_root(task_root);
    if (source_root != NULL) cx_gc_remove_root(source_root);
    if (buffer_root != NULL) cx_gc_remove_root(buffer_root);
    if (socket_root != NULL) cx_gc_remove_root(socket_root);
    return passed;
}

static int socket_write_task_completed(
    const struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *generic_task,
    const struct cxcore_System_Threading_Tasks_TaskHandle *task_handle);
static int socket_write_task_faulted(
    const struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *generic_task,
    const struct cxcore_System_Threading_Tasks_TaskHandle *task_handle);
static int socket_write_task_canceled(
    const struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *generic_task,
    const struct cxcore_System_Threading_Tasks_TaskHandle *task_handle);

static int test_managed_socket_pending_write_ready_cancel_race(
    int use_network_stream) {
    static const uint32_t cancel_delays[] = {0u, 1u, 10u, 50u};
    uint32_t canceled_wins = 0u;
    uint32_t readiness_wins = 0u;
    uint32_t iteration;
    for (iteration = 0u; iteration < 12u; ++iteration) {
        uint64_t connected_handle = 0u;
        uintptr_t peer_handle = (uintptr_t)0;
        size_t filled_bytes = 0u;
        struct cxnet_System_Net_Sockets_Socket *socket = NULL;
        struct cxnet_System_Net_Sockets_NetworkStream *network_stream = NULL;
        struct cxcore_System_Collections_Array *buffer = NULL;
        struct cxcore_System_Threading_Tasks_CancellationTokenSource *source = NULL;
        struct socket_write_cancel_race_context *context = NULL;
        struct socket_peer_cancel_race_drain drain = {0};
        struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *task = NULL;
        struct cxcore_System_Threading_Tasks_TaskHandle *stream_task = NULL;
        struct cx_gc_root_handle *socket_root = NULL;
        struct cx_gc_root_handle *network_stream_root = NULL;
        struct cx_gc_root_handle *buffer_root = NULL;
        struct cx_gc_root_handle *source_root = NULL;
        struct cx_gc_root_handle *context_root = NULL;
        struct cx_gc_root_handle *task_root = NULL;
        struct cx_gc_root_handle *stream_task_root = NULL;
        cx_action cancel_action = {0};
        cx_action completed_action = {0};
        uint32_t steps = 0u;
        int native_handle_open = 0;
        int socket_initialized = 0;
        int worker_started = 0;
        int cancel_action_posted = 0;
        int readiness_first = iteration % 4u == 3u;
        int passed = 0;
#ifdef _WIN32
        HANDLE worker = NULL;
#else
        pthread_t worker;
#endif

        if (!create_stream_read_pair(&connected_handle, &peer_handle)) {
            fputs("could not create a socket for the write readiness/cancel race\n",
                stderr);
            return 0;
        }
        native_handle_open = 1;
        if (!socket_prefill_send_buffer(connected_handle, peer_handle,
                &filled_bytes)) goto iteration_cleanup;
        socket = (struct cxnet_System_Net_Sockets_Socket *)
            cxcore_System_Memory_Alloc(
                cxnet_System_Net_Sockets_Socket___typeinfo.Size);
        buffer = cx_array_new(1u, (cx_uint)sizeof(cx_byte));
        source = (struct cxcore_System_Threading_Tasks_CancellationTokenSource *)
            cxcore_System_Memory_Alloc(
                cxcore_System_Threading_Tasks_CancellationTokenSource___typeinfo.Size);
        context = (struct socket_write_cancel_race_context *)
            cxcore_System_Memory_Alloc(
                (cx_uint)sizeof(*context));
        if (socket == NULL || buffer == NULL || source == NULL ||
            context == NULL) goto iteration_cleanup;
        socket_root = cx_gc_add_root(&socket, (cx_uint)sizeof(socket));
        buffer_root = cx_gc_add_root(&buffer, (cx_uint)sizeof(buffer));
        source_root = cx_gc_add_root(&source, (cx_uint)sizeof(source));
        context_root = cx_gc_add_root(&context, (cx_uint)sizeof(context));
        if (socket_root == NULL || buffer_root == NULL ||
            source_root == NULL || context_root == NULL)
            goto iteration_cleanup;
        memset(context, 0, sizeof(*context));
        cx_gc_store_reference((cx_ptr *)&context->source, (cx_ptr)source);
        cxcore_System_Threading_Tasks_CancellationTokenSource___constructor(source);
        cxnet_System_Net_Sockets_Socket___constructor(socket,
            (cx_ulong)connected_handle);
        socket_initialized = 1;
        native_handle_open = 0;
        if (use_network_stream) {
            network_stream =
                (struct cxnet_System_Net_Sockets_NetworkStream *)
                cxcore_System_Memory_Alloc(
                    cxnet_System_Net_Sockets_NetworkStream___typeinfo.Size);
            if (network_stream == NULL) goto iteration_cleanup;
            network_stream_root = cx_gc_add_root(&network_stream,
                (cx_uint)sizeof(network_stream));
            if (network_stream_root == NULL) goto iteration_cleanup;
            cxnet_System_Net_Sockets_NetworkStream___constructor__2(
                network_stream, socket, 0);
        }
        ((cx_byte *)buffer->_data)[0] = 0x5au;
        if (use_network_stream) {
            stream_task =
                cxnet_System_Net_Sockets_NetworkStream_WriteAsync(
                    network_stream, buffer, 0u, 1u,
                    cxcore_System_Threading_Tasks_CancellationTokenSource_Token___const_get(
                        source));
            stream_task_root = cx_gc_add_root(&stream_task,
                (cx_uint)sizeof(stream_task));
        } else {
            task = cxnet_System_Net_Sockets_Socket_WriteAsync__2(socket,
                buffer, 0u, 1u,
                cxcore_System_Threading_Tasks_CancellationTokenSource_Token___const_get(
                    source));
            task_root = cx_gc_add_root(&task, (cx_uint)sizeof(task));
        }
        if ((use_network_stream &&
                (stream_task == NULL || stream_task_root == NULL)) ||
            (!use_network_stream && (task == NULL || task_root == NULL)) ||
            socket_write_task_completed(task, stream_task)) {
            fprintf(stderr, "saturated %s.WriteAsync did not remain pending for the readiness/cancel race\n",
                use_network_stream ? "NetworkStream" : "Socket");
            goto iteration_cleanup;
        }
        socket_write_cancel_race_completion_count = 0u;
        completed_action.__function = socket_write_cancel_race_completed;
        if (use_network_stream) {
            cxcore_System_Threading_Tasks_TaskHandle_OnCompleted(stream_task,
                completed_action);
        } else {
            cxcore_System_Threading_Tasks_Task_OnCompleted(
                (struct cxcore_System_Threading_Tasks_Task *)task,
                completed_action);
        }
        cx_gc_collect();
        if (socket_write_task_completed(task, stream_task)) {
            fprintf(stderr, "forced GC completed a pending %s write in the readiness/cancel race\n",
                use_network_stream ? "NetworkStream" : "socket");
            goto iteration_cleanup;
        }
        drain.peer = peer_handle;
        drain.filled_bytes = filled_bytes;
#ifdef _WIN32
        worker = CreateThread(NULL, 0,
            socket_peer_cancel_race_drain_worker, &drain, 0, NULL);
        if (worker == NULL) goto iteration_cleanup;
#else
        if (pthread_create(&worker, NULL,
                socket_peer_cancel_race_drain_worker, &drain) != 0)
            goto iteration_cleanup;
#endif
        worker_started = 1;
        if (readiness_first) {
            socket_dispose_race_store(&drain.start, 1);
            while (!socket_dispose_race_load(&drain.completed) &&
                steps < 100000u) {
#ifdef _WIN32
                Sleep(1u);
#else
                usleep(1000u);
#endif
                steps++;
            }
            while (!socket_write_task_completed(task, stream_task) &&
                steps < 200000u) {
                if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                    Sleep(1u);
#else
                    usleep(1000u);
#endif
                }
                steps++;
            }
            cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(source);
            context->cancellation_fired = 1u;
        } else {
            cancel_action.__function = socket_write_cancel_race_cancel;
            cancel_action.__environment = context;
            cancel_action.__target = (cx_ptr)context;
            cxcore_System_Threading_Tasks_TaskScheduler_PostAfter(
                cancel_delays[iteration %
                    (sizeof(cancel_delays) / sizeof(cancel_delays[0]))],
                cancel_action);
            cancel_action_posted = 1;
            socket_dispose_race_store(&drain.start, 1);
            while ((!socket_write_task_completed(task, stream_task) ||
                    !context->cancellation_fired ||
                    !socket_dispose_race_load(&drain.completed)) &&
                steps < 100000u) {
                if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                    Sleep(1u);
#else
                    usleep(1000u);
#endif
                }
                steps++;
            }
        }
#ifdef _WIN32
        WaitForSingleObject(worker, INFINITE);
        CloseHandle(worker);
#else
        pthread_join(worker, NULL);
#endif
        worker_started = 0;
        for (steps = 0u; steps < 8u; ++steps)
            (void)cxcore_System_Threading_Tasks_TaskScheduler_RunOne();
        if (!socket_write_task_completed(task, stream_task) ||
            socket_write_task_faulted(task, stream_task) ||
            socket_write_cancel_race_completion_count != 1u ||
            source->_callbackCount != 0u ||
            !cxnet_System_Net_Sockets_Socket_Connected___const_get(socket) ||
            socket_dispose_race_load(&drain.failed) ||
            !socket_dispose_race_load(&drain.completed)) {
            fprintf(stderr,
                "%s write readiness/cancellation race failed iteration=%u completed=%d canceled=%d faulted=%d continuation=%u callbacks=%u steps=%u\n",
                use_network_stream ? "NetworkStream" : "socket",
                (unsigned)iteration,
                socket_write_task_completed(task, stream_task),
                socket_write_task_canceled(task, stream_task),
                socket_write_task_faulted(task, stream_task),
                (unsigned)socket_write_cancel_race_completion_count,
                (unsigned)source->_callbackCount, (unsigned)steps);
            goto iteration_cleanup;
        }
        if (socket_write_task_canceled(task, stream_task)) {
            canceled_wins += 1u;
        } else {
            uint32_t transferred = 1u;
            if (use_network_stream) {
                cxcore_System_Threading_Tasks_TaskHandle_GetResult(stream_task);
            } else {
                transferred =
                    cx_generic_32f22edd4c9ba40c64ed2658909ec8fb1d6e85d2c4276d46eebd01030eefa80f(task);
            }
            if (transferred != 1u ||
                !stream_peer_reads_expected(peer_handle, 0x5au, 1u)) {
                fprintf(stderr, "successful %s write did not preserve its byte in the readiness/cancel race\n",
                    use_network_stream ? "NetworkStream" : "socket");
                goto iteration_cleanup;
            }
            readiness_wins += 1u;
        }
        cxnet_System_Net_Sockets_Socket_Dispose(socket);
        passed = stream_peer_observes_eof(peer_handle);
        if (!passed)
            fputs("socket did not close cleanly after the readiness/cancel race\n",
                stderr);

iteration_cleanup:
        if (worker_started) {
            socket_dispose_race_store(&drain.start, 1);
#ifdef _WIN32
            WaitForSingleObject(worker, INFINITE);
            CloseHandle(worker);
#else
            pthread_join(worker, NULL);
#endif
        }
        if (socket_initialized) {
            cxnet_System_Net_Sockets_Socket_Dispose(socket);
        } else if (native_handle_open) {
#ifdef _WIN32
            closesocket((SOCKET)(uintptr_t)connected_handle);
#else
            close((int)connected_handle);
#endif
        }
        if (network_stream != NULL) {
            cxnet_System_Net_Sockets_NetworkStream_Dispose(network_stream);
        }
        if (task != NULL || stream_task != NULL) {
            steps = 0u;
            while (!socket_write_task_completed(task, stream_task) &&
                steps < 100000u) {
                if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                    Sleep(1u);
#else
                    usleep(1000u);
#endif
                }
                steps++;
            }
        }
        if (cancel_action_posted && context != NULL) {
            steps = 0u;
            while (context->cancellation_fired == 0u && steps < 100000u) {
                if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                    Sleep(1u);
#else
                    usleep(1000u);
#endif
                }
                steps++;
            }
        }
        if (peer_handle != (uintptr_t)0) close_stream_read_peer(peer_handle);
        if (task_root != NULL) cx_gc_remove_root(task_root);
        if (stream_task_root != NULL) cx_gc_remove_root(stream_task_root);
        if (network_stream_root != NULL)
            cx_gc_remove_root(network_stream_root);
        if (context_root != NULL) cx_gc_remove_root(context_root);
        if (source_root != NULL) cx_gc_remove_root(source_root);
        if (buffer_root != NULL) cx_gc_remove_root(buffer_root);
        if (socket_root != NULL) cx_gc_remove_root(socket_root);
        if (!passed) return 0;
    }
    fprintf(stderr,
        "%s write readiness/cancel race wins: cancellation=%u readiness=%u\n",
        use_network_stream ? "NetworkStream" : "socket",
        (unsigned)canceled_wins, (unsigned)readiness_wins);
    return canceled_wins != 0u && readiness_wins != 0u &&
        canceled_wins + readiness_wins == 12u;
}

static int socket_write_task_completed(
    const struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *generic_task,
    const struct cxcore_System_Threading_Tasks_TaskHandle *task_handle) {
    return task_handle != NULL ?
        cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(
            task_handle) : generic_task != NULL && generic_task->_isCompleted;
}

static int socket_write_task_faulted(
    const struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *generic_task,
    const struct cxcore_System_Threading_Tasks_TaskHandle *task_handle) {
    return task_handle != NULL ?
        cxcore_System_Threading_Tasks_TaskHandle_IsFaulted___const_get(
            task_handle) : generic_task != NULL && generic_task->_isFaulted;
}

static int socket_write_task_canceled(
    const struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *generic_task,
    const struct cxcore_System_Threading_Tasks_TaskHandle *task_handle) {
    return task_handle != NULL ?
        cxcore_System_Threading_Tasks_TaskHandle_IsCanceled___const_get(
            task_handle) : generic_task != NULL && generic_task->_isCanceled;
}

static struct cxcore_System_Exception *socket_write_task_exception(
    const struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *generic_task,
    const struct cxcore_System_Threading_Tasks_TaskHandle *task_handle) {
    return task_handle != NULL ? task_handle->_state->_exception :
        generic_task != NULL ? generic_task->_exception : NULL;
}

static int test_managed_socket_pending_write_shutdown(int use_network_stream) {
    uint32_t iteration;
    for (iteration = 0u; iteration < 24u; iteration++) {
        uint64_t connected_handle = 0u;
        uintptr_t peer_handle = (uintptr_t)0;
        size_t filled_bytes = 0u;
        struct cxnet_System_Net_Sockets_Socket *socket = NULL;
        struct cxnet_System_Net_Sockets_NetworkStream *stream = NULL;
        struct cxcore_System_Collections_Array *write_buffer = NULL;
        struct cxcore_System_Collections_Array *read_buffer = NULL;
        struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *write_task = NULL;
        struct cxcore_System_Threading_Tasks_TaskHandle *stream_write_task = NULL;
        struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *read_task = NULL;
        struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *retry_task = NULL;
        struct cxcore_System_Threading_Tasks_TaskHandle *stream_retry_task = NULL;
        struct cx_gc_root_handle *socket_root = NULL;
        struct cx_gc_root_handle *stream_root = NULL;
        struct cx_gc_root_handle *write_buffer_root = NULL;
        struct cx_gc_root_handle *read_buffer_root = NULL;
        struct cx_gc_root_handle *write_task_root = NULL;
        struct cx_gc_root_handle *stream_write_task_root = NULL;
        struct cx_gc_root_handle *read_task_root = NULL;
        struct cx_gc_root_handle *retry_task_root = NULL;
        struct cx_gc_root_handle *stream_retry_task_root = NULL;
        uint32_t steps = 0u;
        int native_handle_open = 0;
        int socket_initialized = 0;
        int passed = 0;

        if (!create_stream_read_pair(&connected_handle, &peer_handle)) {
            fputs("could not create a pending-write shutdown socket pair\n",
                stderr);
            return 0;
        }
        native_handle_open = 1;
        if (!socket_prefill_send_buffer(connected_handle, peer_handle,
                &filled_bytes)) {
            fputs("could not saturate the socket before write shutdown\n",
                stderr);
            goto cleanup;
        }
        socket = (struct cxnet_System_Net_Sockets_Socket *)
            cxcore_System_Memory_Alloc(
                cxnet_System_Net_Sockets_Socket___typeinfo.Size);
        if (use_network_stream) {
            stream = (struct cxnet_System_Net_Sockets_NetworkStream *)
                cxcore_System_Memory_Alloc((cx_uint)sizeof(*stream));
        }
        write_buffer = cx_array_new(1u, (cx_uint)sizeof(cx_byte));
        read_buffer = cx_array_new(1u, (cx_uint)sizeof(cx_byte));
        if (socket == NULL || (use_network_stream && stream == NULL) ||
            write_buffer == NULL || read_buffer == NULL)
            goto cleanup;
        socket_root = cx_gc_add_root(&socket, (cx_uint)sizeof(socket));
        if (stream != NULL) {
            stream_root = cx_gc_add_root(&stream, (cx_uint)sizeof(stream));
        }
        write_buffer_root = cx_gc_add_root(&write_buffer,
            (cx_uint)sizeof(write_buffer));
        read_buffer_root = cx_gc_add_root(&read_buffer,
            (cx_uint)sizeof(read_buffer));
        if (socket_root == NULL || (use_network_stream && stream_root == NULL) ||
            write_buffer_root == NULL ||
            read_buffer_root == NULL) goto cleanup;
        ((cx_byte *)write_buffer->_data)[0] = 0x5au;
        cxnet_System_Net_Sockets_Socket___constructor(socket,
            (cx_ulong)connected_handle);
        if (stream != NULL) {
            cxnet_System_Net_Sockets_NetworkStream___constructor__2(
                stream, socket, 0);
        }
        socket_initialized = 1;
        native_handle_open = 0;

        if (stream != NULL) {
            struct cxcore_System_Threading_Tasks_CancellationToken token = {0};
            stream_write_task =
                cxnet_System_Net_Sockets_NetworkStream_WriteAsync(
                    stream, write_buffer, 0u, 1u, token);
            stream_write_task_root = cx_gc_add_root(&stream_write_task,
                (cx_uint)sizeof(stream_write_task));
        } else {
            write_task = cxnet_System_Net_Sockets_Socket_WriteAsync(socket,
                write_buffer, 0u, 1u);
            write_task_root = cx_gc_add_root(&write_task,
                (cx_uint)sizeof(write_task));
        }
        if (socket_write_task_completed(write_task, stream_write_task) ||
            (stream != NULL && (stream_write_task == NULL ||
                stream_write_task_root == NULL)) ||
            (stream == NULL && (write_task == NULL || write_task_root == NULL))) {
            fprintf(stderr,
                "saturated managed write completed before shutdown (network_stream=%d faulted=%d canceled=%d)\n",
                use_network_stream,
                socket_write_task_faulted(write_task, stream_write_task),
                socket_write_task_canceled(write_task, stream_write_task));
            goto cleanup;
        }
        cx_gc_collect();
        if (socket_write_task_completed(write_task, stream_write_task)) {
            fputs("forced GC changed pending write before shutdown\n",
                stderr);
            goto cleanup;
        }
        cxnet_System_Net_Sockets_Socket_Shutdown(socket,
            cxnet_System_Net_Sockets_SocketShutdown_Send);
        if (!cxnet_System_Net_Sockets_Socket_Connected___const_get(socket) ||
            !stream_peer_reads_expected(peer_handle, 0xa7u, filled_bytes) ||
            !stream_peer_observes_eof(peer_handle)) {
            fputs("send shutdown lost queued bytes or did not half-close the peer\n",
                stderr);
            goto cleanup;
        }
        while (!socket_write_task_completed(write_task, stream_write_task) &&
            steps < 100000u) {
            if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                Sleep(1u);
#else
                usleep(1000u);
#endif
            }
            steps++;
        }
#ifdef _WIN32
        if (!socket_write_task_completed(write_task, stream_write_task) ||
            !socket_write_task_faulted(write_task, stream_write_task) ||
            socket_write_task_canceled(write_task, stream_write_task) ||
            !cxcore_System_Exception_Matches(
                socket_write_task_exception(write_task, stream_write_task),
                cxnet_System_Net_Sockets_SocketException___typeinfo) ||
            cxnet_System_Net_Sockets_SocketException_SocketErrorCode___const_get(
                (const struct cxnet_System_Net_Sockets_SocketException *)
                    socket_write_task_exception(write_task, stream_write_task)) !=
                cxnet_System_Net_Sockets_SocketError_Shutdown ||
            cxcore_System_IO_IOException_NativeErrorCode___const_get(
                (const struct cxcore_System_IO_IOException *)
                    socket_write_task_exception(write_task, stream_write_task)) !=
                10058) {
            fputs("pending Winsock write after Shutdown(Send) did not report Shutdown\n",
                stderr);
            goto cleanup;
        }
#else
        if (!socket_write_task_completed(write_task, stream_write_task) ||
            !socket_write_task_faulted(write_task, stream_write_task) ||
            socket_write_task_canceled(write_task, stream_write_task) ||
            !cxcore_System_Exception_Matches(
                socket_write_task_exception(write_task, stream_write_task),
                cxnet_System_Net_Sockets_SocketException___typeinfo) ||
            cxnet_System_Net_Sockets_SocketException_SocketErrorCode___const_get(
                (const struct cxnet_System_Net_Sockets_SocketException *)
                    socket_write_task_exception(write_task, stream_write_task)) !=
                cxnet_System_Net_Sockets_SocketError_Shutdown ||
            cxcore_System_IO_IOException_NativeErrorCode___const_get(
                (const struct cxcore_System_IO_IOException *)
                    socket_write_task_exception(write_task, stream_write_task)) !=
                32) {
            fputs("pending POSIX write after Shutdown(Send) did not report Shutdown\n",
                stderr);
            goto cleanup;
        }
#endif
        {
            const char inbound_byte = 'r';
#ifdef _WIN32
            int sent = send((SOCKET)peer_handle, &inbound_byte, 1, 0);
#else
            ssize_t sent = send((int)peer_handle, &inbound_byte, 1, 0);
#endif
            if (sent != 1) {
                fputs("peer could not send after local Shutdown(Send)\n",
                    stderr);
                goto cleanup;
            }
        }
        if (stream != NULL) {
            struct cxcore_System_Threading_Tasks_CancellationToken token = {0};
            read_task = cxnet_System_Net_Sockets_NetworkStream_ReadAsync(
                stream, read_buffer, 0u, 1u, token);
        } else {
            read_task = cxnet_System_Net_Sockets_Socket_ReadAsync(socket,
                read_buffer, 0u, 1u);
        }
        read_task_root = cx_gc_add_root(&read_task,
            (cx_uint)sizeof(read_task));
        steps = 0u;
        while (read_task != NULL && !read_task->_isCompleted &&
            steps < 100000u) {
            if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                Sleep(1u);
#else
                usleep(1000u);
#endif
            }
            steps++;
        }
        if (read_task == NULL || read_task_root == NULL ||
            !read_task->_isCompleted || read_task->_isFaulted ||
            read_task->_isCanceled ||
            cx_generic_32f22edd4c9ba40c64ed2658909ec8fb1d6e85d2c4276d46eebd01030eefa80f(
                read_task) != 1u ||
            ((cx_byte *)read_buffer->_data)[0] != (cx_byte)'r') {
            fputs("Shutdown(Send) changed the socket receive direction\n",
                stderr);
            goto cleanup;
        }

        if (stream != NULL) {
            struct cxcore_System_Threading_Tasks_CancellationToken token = {0};
            stream_retry_task =
                cxnet_System_Net_Sockets_NetworkStream_WriteAsync(
                    stream, write_buffer, 0u, 1u, token);
            stream_retry_task_root = cx_gc_add_root(&stream_retry_task,
                (cx_uint)sizeof(stream_retry_task));
        } else {
            retry_task = cxnet_System_Net_Sockets_Socket_WriteAsync(socket,
                write_buffer, 0u, 1u);
            retry_task_root = cx_gc_add_root(&retry_task,
                (cx_uint)sizeof(retry_task));
        }
        if (stream != NULL) {
            steps = 0u;
            while (!socket_write_task_completed(retry_task, stream_retry_task) &&
                steps < 100000u) {
                if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                    Sleep(1u);
#else
                    usleep(1000u);
#endif
                }
                steps++;
            }
        }
        if (!socket_write_task_completed(retry_task, stream_retry_task) ||
            (stream != NULL && (stream_retry_task == NULL ||
                stream_retry_task_root == NULL)) ||
            (stream == NULL && (retry_task == NULL || retry_task_root == NULL)) ||
            !socket_write_task_faulted(retry_task, stream_retry_task) ||
            socket_write_task_canceled(retry_task, stream_retry_task) ||
            !cxcore_System_Exception_Matches(
                socket_write_task_exception(retry_task, stream_retry_task),
                cxnet_System_Net_Sockets_SocketException___typeinfo) ||
            cxnet_System_Net_Sockets_SocketException_SocketErrorCode___const_get(
                (const struct cxnet_System_Net_Sockets_SocketException *)
                    socket_write_task_exception(retry_task, stream_retry_task)) !=
                cxnet_System_Net_Sockets_SocketError_Shutdown ||
            cxcore_System_IO_IOException_NativeErrorCode___const_get(
                (const struct cxcore_System_IO_IOException *)
                    socket_write_task_exception(retry_task, stream_retry_task)) !=
#ifdef _WIN32
                10058) {
#else
                32) {
#endif
            fputs("subsequent managed write after Shutdown(Send) did not report Shutdown\n",
                stderr);
            goto cleanup;
        }
        if (stream != NULL) {
            cxnet_System_Net_Sockets_NetworkStream_Dispose(stream);
        }
        cxnet_System_Net_Sockets_Socket_Dispose(socket);
        socket_initialized = 0;
        passed = stream_peer_observes_close_or_reset(peer_handle);
        if (!passed) {
            fputs("disposing half-closed socket did not finish peer cleanup\n",
                stderr);
        }

cleanup:
        if (socket_initialized) {
            cxnet_System_Net_Sockets_Socket_Dispose(socket);
        } else if (native_handle_open) {
#ifdef _WIN32
            closesocket((SOCKET)(uintptr_t)connected_handle);
#else
            close((int)connected_handle);
#endif
        }
        if (peer_handle != (uintptr_t)0) close_stream_read_peer(peer_handle);
        if (stream_root != NULL) cx_gc_remove_root(stream_root);
        if (stream_retry_task_root != NULL)
            cx_gc_remove_root(stream_retry_task_root);
        if (retry_task_root != NULL) cx_gc_remove_root(retry_task_root);
        if (read_task_root != NULL) cx_gc_remove_root(read_task_root);
        if (stream_write_task_root != NULL)
            cx_gc_remove_root(stream_write_task_root);
        if (write_task_root != NULL) cx_gc_remove_root(write_task_root);
        if (read_buffer_root != NULL) cx_gc_remove_root(read_buffer_root);
        if (write_buffer_root != NULL) cx_gc_remove_root(write_buffer_root);
        if (socket_root != NULL) cx_gc_remove_root(socket_root);
        if (!passed) return 0;
    }
    return 1;
}

static int test_managed_socket_pending_write_survives_receive_shutdown(
    int use_network_stream) {
    uint32_t iteration;
    for (iteration = 0u; iteration < 24u; iteration++) {
        uint64_t connected_handle = 0u;
        uintptr_t peer_handle = (uintptr_t)0;
        size_t filled_bytes = 0u;
        struct cxnet_System_Net_Sockets_Socket *socket = NULL;
        struct cxnet_System_Net_Sockets_NetworkStream *stream = NULL;
        struct cxcore_System_Collections_Array *buffer = NULL;
        struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *write_task = NULL;
        struct cxcore_System_Threading_Tasks_TaskHandle *stream_write_task = NULL;
        struct cx_gc_root_handle *socket_root = NULL;
        struct cx_gc_root_handle *stream_root = NULL;
        struct cx_gc_root_handle *buffer_root = NULL;
        struct cx_gc_root_handle *write_task_root = NULL;
        struct cx_gc_root_handle *stream_write_task_root = NULL;
        uint32_t steps = 0u;
        int native_handle_open = 0;
        int socket_initialized = 0;
        int passed = 0;

        if (!create_stream_read_pair(&connected_handle, &peer_handle)) {
            fputs("could not create a pending-write receive-shutdown pair\n",
                stderr);
            return 0;
        }
        native_handle_open = 1;
        if (!socket_prefill_send_buffer(connected_handle, peer_handle,
                &filled_bytes)) {
            fputs("could not saturate the send buffer before receive shutdown\n",
                stderr);
            goto cleanup;
        }
        socket = (struct cxnet_System_Net_Sockets_Socket *)
            cxcore_System_Memory_Alloc(
                cxnet_System_Net_Sockets_Socket___typeinfo.Size);
        if (use_network_stream) {
            stream = (struct cxnet_System_Net_Sockets_NetworkStream *)
                cxcore_System_Memory_Alloc((cx_uint)sizeof(*stream));
        }
        buffer = cx_array_new(1u, (cx_uint)sizeof(cx_byte));
        if (socket == NULL || (use_network_stream && stream == NULL) ||
            buffer == NULL) goto cleanup;
        socket_root = cx_gc_add_root(&socket, (cx_uint)sizeof(socket));
        if (stream != NULL) {
            stream_root = cx_gc_add_root(&stream, (cx_uint)sizeof(stream));
        }
        buffer_root = cx_gc_add_root(&buffer, (cx_uint)sizeof(buffer));
        if (socket_root == NULL || buffer_root == NULL ||
            (use_network_stream && stream_root == NULL)) goto cleanup;
        ((cx_byte *)buffer->_data)[0] = 0x5au;
        cxnet_System_Net_Sockets_Socket___constructor(socket,
            (cx_ulong)connected_handle);
        if (stream != NULL) {
            cxnet_System_Net_Sockets_NetworkStream___constructor__2(
                stream, socket, 0);
        }
        socket_initialized = 1;
        native_handle_open = 0;

        if (stream != NULL) {
            struct cxcore_System_Threading_Tasks_CancellationToken token = {0};
            stream_write_task =
                cxnet_System_Net_Sockets_NetworkStream_WriteAsync(
                    stream, buffer, 0u, 1u, token);
            stream_write_task_root = cx_gc_add_root(&stream_write_task,
                (cx_uint)sizeof(stream_write_task));
        } else {
            write_task = cxnet_System_Net_Sockets_Socket_WriteAsync(
                socket, buffer, 0u, 1u);
            write_task_root = cx_gc_add_root(&write_task,
                (cx_uint)sizeof(write_task));
        }
        if (socket_write_task_completed(write_task, stream_write_task) ||
            (stream != NULL && (stream_write_task == NULL ||
                stream_write_task_root == NULL)) ||
            (stream == NULL && (write_task == NULL || write_task_root == NULL))) {
            fputs("outbound write did not remain pending before Shutdown(Receive)\n",
                stderr);
            goto cleanup;
        }
        cx_gc_collect();
        if (socket_write_task_completed(write_task, stream_write_task)) {
            fputs("forced GC completed a write before Shutdown(Receive)\n",
                stderr);
            goto cleanup;
        }

        cxnet_System_Net_Sockets_Socket_Shutdown(socket,
            cxnet_System_Net_Sockets_SocketShutdown_Receive);
        if (!cxnet_System_Net_Sockets_Socket_Connected___const_get(socket) ||
            !stream_peer_reads_expected(peer_handle, 0xa7u, filled_bytes)) {
            fputs("receive shutdown affected state or queued outbound bytes\n",
                stderr);
            goto cleanup;
        }
        while (!socket_write_task_completed(write_task, stream_write_task) &&
            steps < 100000u) {
            if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                Sleep(1u);
#else
                usleep(1000u);
#endif
            }
            steps++;
        }
        if (!socket_write_task_completed(write_task, stream_write_task) ||
            socket_write_task_faulted(write_task, stream_write_task) ||
            socket_write_task_canceled(write_task, stream_write_task) ||
            (!use_network_stream && write_task->_result != 1u) ||
            !stream_peer_reads_expected(peer_handle, 0x5au, 1u) ||
            !cxnet_System_Net_Sockets_Socket_Connected___const_get(socket)) {
            fputs("Shutdown(Receive) interrupted the pending outbound write\n",
                stderr);
            goto cleanup;
        }

        if (stream != NULL) {
            cxnet_System_Net_Sockets_NetworkStream_Dispose(stream);
        }
        cxnet_System_Net_Sockets_Socket_Dispose(socket);
        socket_initialized = 0;
        passed = stream_peer_observes_close_or_reset(peer_handle);
        if (!passed) {
            fputs("disposing after receive shutdown did not close the peer\n",
                stderr);
        }

cleanup:
        if (socket_initialized) {
            if (stream != NULL) {
                cxnet_System_Net_Sockets_NetworkStream_Dispose(stream);
            }
            cxnet_System_Net_Sockets_Socket_Dispose(socket);
        } else if (native_handle_open) {
#ifdef _WIN32
            closesocket((SOCKET)(uintptr_t)connected_handle);
#else
            close((int)connected_handle);
#endif
        }
        if (peer_handle != (uintptr_t)0) close_stream_read_peer(peer_handle);
        if (stream_write_task_root != NULL)
            cx_gc_remove_root(stream_write_task_root);
        if (write_task_root != NULL) cx_gc_remove_root(write_task_root);
        if (buffer_root != NULL) cx_gc_remove_root(buffer_root);
        if (stream_root != NULL) cx_gc_remove_root(stream_root);
        if (socket_root != NULL) cx_gc_remove_root(socket_root);
        if (!passed) return 0;
    }
    return 1;
}

static int test_managed_socket_pending_read_cancel(int use_network_stream) {
    uint64_t connected_handle = 0u;
    uintptr_t peer_handle = (uintptr_t)0;
    const cx_byte expected = 0x36u;
    struct cxnet_System_Net_Sockets_Socket *socket = NULL;
    struct cxnet_System_Net_Sockets_NetworkStream *network_stream = NULL;
    struct cxcore_System_Collections_Array *buffer = NULL;
    struct cxcore_System_Threading_Tasks_CancellationTokenSource *source = NULL;
    struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *task = NULL;
    struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *retry_task = NULL;
    struct cx_gc_root_handle *socket_root = NULL;
    struct cx_gc_root_handle *network_stream_root = NULL;
    struct cx_gc_root_handle *buffer_root = NULL;
    struct cx_gc_root_handle *source_root = NULL;
    struct cx_gc_root_handle *task_root = NULL;
    struct cx_gc_root_handle *retry_task_root = NULL;
    struct cxcore_System_Threading_Tasks_CancellationToken no_cancellation = {0};
    uint32_t steps = 0u;
    int native_handle_open = 0;
    int socket_initialized = 0;
    int passed = 0;

    if (!create_stream_read_pair(&connected_handle, &peer_handle)) {
        fputs("could not create a managed read-cancellation socket pair\n",
            stderr);
        return 0;
    }
    native_handle_open = 1;
    socket = (struct cxnet_System_Net_Sockets_Socket *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Sockets_Socket___typeinfo.Size);
    buffer = cx_array_new(1u, (cx_uint)sizeof(cx_byte));
    source = (struct cxcore_System_Threading_Tasks_CancellationTokenSource *)
        cxcore_System_Memory_Alloc(
            cxcore_System_Threading_Tasks_CancellationTokenSource___typeinfo.Size);
    if (socket == NULL || buffer == NULL || source == NULL) goto cleanup;
    socket_root = cx_gc_add_root(&socket, (cx_uint)sizeof(socket));
    buffer_root = cx_gc_add_root(&buffer, (cx_uint)sizeof(buffer));
    source_root = cx_gc_add_root(&source, (cx_uint)sizeof(source));
    if (socket_root == NULL || buffer_root == NULL || source_root == NULL)
        goto cleanup;
    cxcore_System_Threading_Tasks_CancellationTokenSource___constructor(source);
    cxnet_System_Net_Sockets_Socket___constructor(socket,
        (cx_ulong)connected_handle);
    socket_initialized = 1;
    native_handle_open = 0;
    if (use_network_stream) {
        network_stream =
            (struct cxnet_System_Net_Sockets_NetworkStream *)
            cxcore_System_Memory_Alloc(
                cxnet_System_Net_Sockets_NetworkStream___typeinfo.Size);
        if (network_stream == NULL) goto cleanup;
        network_stream_root = cx_gc_add_root(&network_stream,
            (cx_uint)sizeof(network_stream));
        if (network_stream_root == NULL) goto cleanup;
        cxnet_System_Net_Sockets_NetworkStream___constructor__2(
            network_stream, socket, 0);
    }
    task = use_network_stream ?
        cxnet_System_Net_Sockets_NetworkStream_ReadAsync(network_stream,
            buffer, 0u, 1u,
            cxcore_System_Threading_Tasks_CancellationTokenSource_Token___const_get(
                source)) :
        cxnet_System_Net_Sockets_Socket_ReadAsync__2(socket, buffer, 0u,
            1u,
            cxcore_System_Threading_Tasks_CancellationTokenSource_Token___const_get(
                source));
    task_root = cx_gc_add_root(&task, (cx_uint)sizeof(task));
    if (task == NULL || task_root == NULL || task->_isCompleted) {
        fprintf(stderr, "%s.ReadAsync completed before caller cancellation\n",
            use_network_stream ? "NetworkStream" : "Socket");
        goto cleanup;
    }
    cx_gc_collect();
    if (task->_isCompleted) {
        fputs("forced GC changed a pending cancellable socket read\n", stderr);
        goto cleanup;
    }
    cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(source);
    while (!task->_isCompleted && steps < 100000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (!task->_isCompleted || !task->_isCanceled || task->_isFaulted ||
        source->_callbackCount != 0u ||
        !cxnet_System_Net_Sockets_Socket_Connected___const_get(socket)) {
        fprintf(stderr,
            "cancelling a pending %s.ReadAsync failed completed=%d canceled=%d faulted=%d callbacks=%u connected=%d steps=%u\n",
            use_network_stream ? "NetworkStream" : "Socket",
            task->_isCompleted, task->_isCanceled, task->_isFaulted,
            (unsigned)source->_callbackCount,
            cxnet_System_Net_Sockets_Socket_Connected___const_get(socket),
            (unsigned)steps);
        goto cleanup;
    }
#ifdef _WIN32
    if (send((SOCKET)peer_handle, (const char *)&expected, 1, 0) != 1)
#else
    if (send((int)peer_handle, (const char *)&expected, 1, 0) != 1)
#endif
        goto cleanup;
    retry_task = use_network_stream ?
        cxnet_System_Net_Sockets_NetworkStream_ReadAsync(network_stream,
            buffer, 0u, 1u, no_cancellation) :
        cxnet_System_Net_Sockets_Socket_ReadAsync(socket, buffer, 0u, 1u);
    retry_task_root = cx_gc_add_root(&retry_task, (cx_uint)sizeof(retry_task));
    steps = 0u;
    while (retry_task != NULL && !retry_task->_isCompleted && steps < 100000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (retry_task == NULL || retry_task_root == NULL ||
        !retry_task->_isCompleted || retry_task->_isCanceled ||
        retry_task->_isFaulted ||
        cx_generic_32f22edd4c9ba40c64ed2658909ec8fb1d6e85d2c4276d46eebd01030eefa80f(
            retry_task) != 1u || ((cx_byte *)buffer->_data)[0] != expected) {
        fprintf(stderr, "canceled %s.ReadAsync did not release its receive slot\n",
            use_network_stream ? "NetworkStream" : "Socket");
        goto cleanup;
    }
    cxnet_System_Net_Sockets_Socket_Dispose(socket);
    passed = stream_peer_observes_eof(peer_handle);
cleanup:
    if (network_stream != NULL) {
        cxnet_System_Net_Sockets_NetworkStream_Dispose(network_stream);
    }
    if (socket_initialized) {
        cxnet_System_Net_Sockets_Socket_Dispose(socket);
    } else if (native_handle_open) {
#ifdef _WIN32
        closesocket((SOCKET)(uintptr_t)connected_handle);
#else
        close((int)connected_handle);
#endif
    }
    if (peer_handle != (uintptr_t)0) close_stream_read_peer(peer_handle);
    if (retry_task_root != NULL) cx_gc_remove_root(retry_task_root);
    if (task_root != NULL) cx_gc_remove_root(task_root);
    if (source_root != NULL) cx_gc_remove_root(source_root);
    if (buffer_root != NULL) cx_gc_remove_root(buffer_root);
    if (network_stream_root != NULL) cx_gc_remove_root(network_stream_root);
    if (socket_root != NULL) cx_gc_remove_root(socket_root);
    return passed;
}

static int test_managed_socket_pending_read_ready_cancel_race(
    int use_network_stream) {
    static const uint32_t cancel_delays[] = {0u, 1u, 10u, 50u};
    uint32_t canceled_wins = 0u;
    uint32_t readiness_wins = 0u;
    uint32_t iteration;
    for (iteration = 0u; iteration < 12u; ++iteration) {
        uint64_t connected_handle = 0u;
        uintptr_t peer_handle = (uintptr_t)0;
        struct cxnet_System_Net_Sockets_Socket *socket = NULL;
        struct cxnet_System_Net_Sockets_NetworkStream *network_stream = NULL;
        struct cxcore_System_Collections_Array *buffer = NULL;
        struct cxcore_System_Threading_Tasks_CancellationTokenSource *source = NULL;
        struct socket_write_cancel_race_context *context = NULL;
        struct socket_peer_readiness_race_send send_state = {0};
        struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *task = NULL;
        struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *retry_task = NULL;
        struct cx_gc_root_handle *socket_root = NULL;
        struct cx_gc_root_handle *network_stream_root = NULL;
        struct cx_gc_root_handle *buffer_root = NULL;
        struct cx_gc_root_handle *source_root = NULL;
        struct cx_gc_root_handle *context_root = NULL;
        struct cx_gc_root_handle *task_root = NULL;
        struct cx_gc_root_handle *retry_task_root = NULL;
        struct cxcore_System_Threading_Tasks_CancellationToken no_cancellation = {0};
        cx_action cancel_action = {0};
        cx_action completed_action = {0};
        uint32_t steps = 0u;
        int native_handle_open = 0;
        int socket_initialized = 0;
        int worker_started = 0;
        int cancel_action_posted = 0;
        int readiness_first = iteration % 4u == 3u;
        int passed = 0;
#ifdef _WIN32
        HANDLE worker = NULL;
#else
        pthread_t worker;
#endif

        if (!create_stream_read_pair(&connected_handle, &peer_handle)) {
            fputs("could not create a socket for the read readiness/cancel race\n",
                stderr);
            return 0;
        }
        native_handle_open = 1;
        socket = (struct cxnet_System_Net_Sockets_Socket *)
            cxcore_System_Memory_Alloc(
                cxnet_System_Net_Sockets_Socket___typeinfo.Size);
        buffer = cx_array_new(1u, (cx_uint)sizeof(cx_byte));
        source = (struct cxcore_System_Threading_Tasks_CancellationTokenSource *)
            cxcore_System_Memory_Alloc(
                cxcore_System_Threading_Tasks_CancellationTokenSource___typeinfo.Size);
        context = (struct socket_write_cancel_race_context *)
            cxcore_System_Memory_Alloc((cx_uint)sizeof(*context));
        if (socket == NULL || buffer == NULL || source == NULL ||
            context == NULL) goto read_iteration_cleanup;
        socket_root = cx_gc_add_root(&socket, (cx_uint)sizeof(socket));
        buffer_root = cx_gc_add_root(&buffer, (cx_uint)sizeof(buffer));
        source_root = cx_gc_add_root(&source, (cx_uint)sizeof(source));
        context_root = cx_gc_add_root(&context, (cx_uint)sizeof(context));
        if (socket_root == NULL || buffer_root == NULL ||
            source_root == NULL || context_root == NULL)
            goto read_iteration_cleanup;
        memset(context, 0, sizeof(*context));
        cx_gc_store_reference((cx_ptr *)&context->source, (cx_ptr)source);
        cxcore_System_Threading_Tasks_CancellationTokenSource___constructor(source);
        cxnet_System_Net_Sockets_Socket___constructor(socket,
            (cx_ulong)connected_handle);
        socket_initialized = 1;
        native_handle_open = 0;
        if (use_network_stream) {
            network_stream =
                (struct cxnet_System_Net_Sockets_NetworkStream *)
                cxcore_System_Memory_Alloc(
                    cxnet_System_Net_Sockets_NetworkStream___typeinfo.Size);
            if (network_stream == NULL) goto read_iteration_cleanup;
            network_stream_root = cx_gc_add_root(&network_stream,
                (cx_uint)sizeof(network_stream));
            if (network_stream_root == NULL) goto read_iteration_cleanup;
            cxnet_System_Net_Sockets_NetworkStream___constructor__2(
                network_stream, socket, 0);
        }
        task = use_network_stream ?
            cxnet_System_Net_Sockets_NetworkStream_ReadAsync(network_stream,
                buffer, 0u, 1u,
                cxcore_System_Threading_Tasks_CancellationTokenSource_Token___const_get(
                    source)) :
            cxnet_System_Net_Sockets_Socket_ReadAsync__2(socket, buffer,
                0u, 1u,
                cxcore_System_Threading_Tasks_CancellationTokenSource_Token___const_get(
                    source));
        task_root = cx_gc_add_root(&task, (cx_uint)sizeof(task));
        if (task == NULL || task_root == NULL || task->_isCompleted) {
            fprintf(stderr, "%s.ReadAsync did not remain pending for the readiness/cancel race\n",
                use_network_stream ? "NetworkStream" : "Socket");
            goto read_iteration_cleanup;
        }
        socket_read_cancel_race_completion_count = 0u;
        completed_action.__function = socket_read_cancel_race_completed;
        cxcore_System_Threading_Tasks_Task_OnCompleted(
            (struct cxcore_System_Threading_Tasks_Task *)task,
            completed_action);
        cx_gc_collect();
        if (task->_isCompleted) {
            fputs("forced GC completed a pending socket read in the readiness/cancel race\n",
                stderr);
            goto read_iteration_cleanup;
        }
        send_state.peer = peer_handle;
#ifdef _WIN32
        worker = CreateThread(NULL, 0,
            socket_peer_readiness_race_send_worker, &send_state, 0, NULL);
        if (worker == NULL) goto read_iteration_cleanup;
#else
        if (pthread_create(&worker, NULL,
                socket_peer_readiness_race_send_worker, &send_state) != 0)
            goto read_iteration_cleanup;
#endif
        worker_started = 1;
        if (readiness_first) {
            socket_dispose_race_store(&send_state.start, 1);
            while (!socket_dispose_race_load(&send_state.completed) &&
                steps < 100000u) {
#ifdef _WIN32
                Sleep(1u);
#else
                usleep(1000u);
#endif
                steps++;
            }
            while (!task->_isCompleted && steps < 200000u) {
                if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                    Sleep(1u);
#else
                    usleep(1000u);
#endif
                }
                steps++;
            }
            cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(source);
            context->cancellation_fired = 1u;
        } else {
            cancel_action.__function = socket_write_cancel_race_cancel;
            cancel_action.__environment = context;
            cancel_action.__target = (cx_ptr)context;
            cxcore_System_Threading_Tasks_TaskScheduler_PostAfter(
                cancel_delays[iteration %
                    (sizeof(cancel_delays) / sizeof(cancel_delays[0]))],
                cancel_action);
            cancel_action_posted = 1;
            socket_dispose_race_store(&send_state.start, 1);
            while ((!task->_isCompleted || !context->cancellation_fired ||
                    !socket_dispose_race_load(&send_state.completed)) &&
                steps < 100000u) {
                if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                    Sleep(1u);
#else
                    usleep(1000u);
#endif
                }
                steps++;
            }
        }
#ifdef _WIN32
        WaitForSingleObject(worker, INFINITE);
        CloseHandle(worker);
#else
        pthread_join(worker, NULL);
#endif
        worker_started = 0;
        for (steps = 0u; steps < 8u; ++steps)
            (void)cxcore_System_Threading_Tasks_TaskScheduler_RunOne();
        if (!task->_isCompleted || task->_isFaulted ||
            socket_read_cancel_race_completion_count != 1u ||
            source->_callbackCount != 0u ||
            !cxnet_System_Net_Sockets_Socket_Connected___const_get(socket) ||
            socket_dispose_race_load(&send_state.failed) ||
            !socket_dispose_race_load(&send_state.completed)) {
            fprintf(stderr,
                "read readiness/cancellation race failed iteration=%u completed=%d canceled=%d faulted=%d continuation=%u callbacks=%u steps=%u\n",
                (unsigned)iteration, task->_isCompleted, task->_isCanceled,
                task->_isFaulted,
                (unsigned)socket_read_cancel_race_completion_count,
                (unsigned)source->_callbackCount, (unsigned)steps);
            goto read_iteration_cleanup;
        }
        if (task->_isCanceled) {
            canceled_wins += 1u;
            retry_task = use_network_stream ?
                cxnet_System_Net_Sockets_NetworkStream_ReadAsync(
                    network_stream, buffer, 0u, 1u, no_cancellation) :
                cxnet_System_Net_Sockets_Socket_ReadAsync(socket, buffer,
                    0u, 1u);
            retry_task_root = cx_gc_add_root(&retry_task,
                (cx_uint)sizeof(retry_task));
            steps = 0u;
            while (retry_task != NULL && !retry_task->_isCompleted &&
                steps < 100000u) {
                if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                    Sleep(1u);
#else
                    usleep(1000u);
#endif
                }
                steps++;
            }
            if (retry_task == NULL || retry_task_root == NULL ||
                !retry_task->_isCompleted || retry_task->_isFaulted ||
                retry_task->_isCanceled ||
                cx_generic_32f22edd4c9ba40c64ed2658909ec8fb1d6e85d2c4276d46eebd01030eefa80f(
                    retry_task) != 1u ||
                ((cx_byte *)buffer->_data)[0] != (cx_byte)0x36u) {
                fprintf(stderr, "canceled %s read lost peer data or retained its receive slot in the readiness race\n",
                    use_network_stream ? "NetworkStream" : "socket");
                goto read_iteration_cleanup;
            }
        } else {
            uint32_t transferred =
                cx_generic_32f22edd4c9ba40c64ed2658909ec8fb1d6e85d2c4276d46eebd01030eefa80f(task);
            if (transferred != 1u ||
                ((cx_byte *)buffer->_data)[0] != (cx_byte)0x36u) {
                fprintf(stderr, "readiness-completed %s read returned the wrong byte in the cancellation race\n",
                    use_network_stream ? "NetworkStream" : "socket");
                goto read_iteration_cleanup;
            }
            readiness_wins += 1u;
        }
        cxnet_System_Net_Sockets_Socket_Dispose(socket);
        passed = stream_peer_observes_eof(peer_handle);
        if (!passed)
            fputs("socket did not close cleanly after the read readiness/cancel race\n",
                stderr);

read_iteration_cleanup:
        if (worker_started) {
            socket_dispose_race_store(&send_state.start, 1);
#ifdef _WIN32
            WaitForSingleObject(worker, INFINITE);
            CloseHandle(worker);
#else
            pthread_join(worker, NULL);
#endif
        }
        if (network_stream != NULL)
            cxnet_System_Net_Sockets_NetworkStream_Dispose(network_stream);
        if (socket_initialized)
            cxnet_System_Net_Sockets_Socket_Dispose(socket);
        else if (native_handle_open) {
#ifdef _WIN32
            closesocket((SOCKET)(uintptr_t)connected_handle);
#else
            close((int)connected_handle);
#endif
        }
        if (task != NULL && !task->_isCompleted) {
            cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(source);
            steps = 0u;
            while (!task->_isCompleted && steps < 100000u) {
                if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                    Sleep(1u);
#else
                    usleep(1000u);
#endif
                }
                steps++;
            }
        }
        if (retry_task != NULL && !retry_task->_isCompleted) {
            steps = 0u;
            while (!retry_task->_isCompleted && steps < 100000u) {
                if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                    Sleep(1u);
#else
                    usleep(1000u);
#endif
                }
                steps++;
            }
        }
        if (cancel_action_posted && context != NULL) {
            steps = 0u;
            while (context->cancellation_fired == 0u && steps < 100000u) {
                if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                    Sleep(1u);
#else
                    usleep(1000u);
#endif
                }
                steps++;
            }
        }
        if (peer_handle != (uintptr_t)0) close_stream_read_peer(peer_handle);
        if (retry_task_root != NULL) cx_gc_remove_root(retry_task_root);
        if (task_root != NULL) cx_gc_remove_root(task_root);
        if (context_root != NULL) cx_gc_remove_root(context_root);
        if (source_root != NULL) cx_gc_remove_root(source_root);
        if (buffer_root != NULL) cx_gc_remove_root(buffer_root);
        if (network_stream_root != NULL)
            cx_gc_remove_root(network_stream_root);
        if (socket_root != NULL) cx_gc_remove_root(socket_root);
        if (!passed) return 0;
    }
    fprintf(stderr,
                "%s read readiness/cancel race wins: cancellation=%u readiness=%u\n",
        use_network_stream ? "NetworkStream" : "socket",
        (unsigned)canceled_wins, (unsigned)readiness_wins);
    return canceled_wins != 0u && readiness_wins != 0u &&
        canceled_wins + readiness_wins == 12u;
}

static int test_managed_socket_full_duplex_independent_cancel(
    int use_network_stream) {
    uint32_t iteration;
    for (iteration = 0u; iteration < 24u; iteration++) {
        uint64_t connected_handle = 0u;
        uintptr_t peer_handle = (uintptr_t)0;
        size_t filled_bytes = 0u;
        struct cxnet_System_Net_Sockets_Socket *socket = NULL;
        struct cxnet_System_Net_Sockets_NetworkStream *network_stream = NULL;
        struct cxcore_System_Collections_Array *read_buffer = NULL;
        struct cxcore_System_Collections_Array *write_buffer = NULL;
        struct cxcore_System_Threading_Tasks_CancellationTokenSource *source = NULL;
        struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *read_task = NULL;
        struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *write_task = NULL;
        struct cxcore_System_Threading_Tasks_TaskHandle *stream_write_task = NULL;
        struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *retry_read_task = NULL;
        struct cx_gc_root_handle *socket_root = NULL;
        struct cx_gc_root_handle *network_stream_root = NULL;
        struct cx_gc_root_handle *read_buffer_root = NULL;
        struct cx_gc_root_handle *write_buffer_root = NULL;
        struct cx_gc_root_handle *source_root = NULL;
        struct cx_gc_root_handle *read_task_root = NULL;
        struct cx_gc_root_handle *write_task_root = NULL;
        struct cx_gc_root_handle *stream_write_task_root = NULL;
        struct cx_gc_root_handle *retry_read_task_root = NULL;
        struct cxcore_System_Threading_Tasks_CancellationToken no_cancellation = {0};
        const cx_byte reply = 0x36u;
        uint32_t steps = 0u;
        int native_handle_open = 0;
        int socket_initialized = 0;
        int passed = 0;

        if (!create_stream_read_pair(&connected_handle, &peer_handle)) {
            fputs("could not create a full-duplex cancellation socket pair\n",
                stderr);
            goto cleanup;
        }
        native_handle_open = 1;
        if (!socket_prefill_send_buffer(connected_handle, peer_handle,
                &filled_bytes)) {
            fputs("could not saturate the full-duplex cancellation send buffer\n",
                stderr);
            goto cleanup;
        }
        socket = (struct cxnet_System_Net_Sockets_Socket *)
            cxcore_System_Memory_Alloc(
                cxnet_System_Net_Sockets_Socket___typeinfo.Size);
        if (use_network_stream) {
            network_stream =
                (struct cxnet_System_Net_Sockets_NetworkStream *)
                cxcore_System_Memory_Alloc(
                    cxnet_System_Net_Sockets_NetworkStream___typeinfo.Size);
        }
        read_buffer = cx_array_new(1u, (cx_uint)sizeof(cx_byte));
        write_buffer = cx_array_new(1u, (cx_uint)sizeof(cx_byte));
        source = (struct cxcore_System_Threading_Tasks_CancellationTokenSource *)
            cxcore_System_Memory_Alloc(
                cxcore_System_Threading_Tasks_CancellationTokenSource___typeinfo.Size);
        if (socket == NULL || (use_network_stream && network_stream == NULL) ||
            read_buffer == NULL || write_buffer == NULL || source == NULL)
            goto cleanup;
        socket_root = cx_gc_add_root(&socket, (cx_uint)sizeof(socket));
        if (network_stream != NULL)
            network_stream_root = cx_gc_add_root(&network_stream,
                (cx_uint)sizeof(network_stream));
        read_buffer_root = cx_gc_add_root(&read_buffer,
            (cx_uint)sizeof(read_buffer));
        write_buffer_root = cx_gc_add_root(&write_buffer,
            (cx_uint)sizeof(write_buffer));
        source_root = cx_gc_add_root(&source, (cx_uint)sizeof(source));
        if (socket_root == NULL ||
            (use_network_stream && network_stream_root == NULL) ||
            read_buffer_root == NULL ||
            write_buffer_root == NULL || source_root == NULL) goto cleanup;
        cxcore_System_Threading_Tasks_CancellationTokenSource___constructor(
            source);
        cxnet_System_Net_Sockets_Socket___constructor(socket,
            (cx_ulong)connected_handle);
        socket_initialized = 1;
        native_handle_open = 0;
        if (use_network_stream) {
            cxnet_System_Net_Sockets_NetworkStream___constructor__2(
                network_stream, socket, 0);
        }
        ((cx_byte *)write_buffer->_data)[0] = 0x5au;
        read_task = use_network_stream ?
            cxnet_System_Net_Sockets_NetworkStream_ReadAsync(network_stream,
                read_buffer, 0u, 1u,
                cxcore_System_Threading_Tasks_CancellationTokenSource_Token___const_get(
                    source)) :
            cxnet_System_Net_Sockets_Socket_ReadAsync__2(socket, read_buffer,
                0u, 1u,
                cxcore_System_Threading_Tasks_CancellationTokenSource_Token___const_get(
                    source));
        read_task_root = cx_gc_add_root(&read_task, (cx_uint)sizeof(read_task));
        if (use_network_stream) {
            stream_write_task =
                cxnet_System_Net_Sockets_NetworkStream_WriteAsync(
                    network_stream, write_buffer, 0u, 1u, no_cancellation);
            stream_write_task_root = cx_gc_add_root(&stream_write_task,
                (cx_uint)sizeof(stream_write_task));
        } else {
            write_task = cxnet_System_Net_Sockets_Socket_WriteAsync(socket,
                write_buffer, 0u, 1u);
            write_task_root = cx_gc_add_root(&write_task,
                (cx_uint)sizeof(write_task));
        }
        if (read_task == NULL || read_task_root == NULL ||
            read_task->_isCompleted ||
            socket_write_task_completed(write_task, stream_write_task) ||
            (use_network_stream && (stream_write_task == NULL ||
                stream_write_task_root == NULL)) ||
            (!use_network_stream && (write_task == NULL ||
                write_task_root == NULL))) {
            fputs("full-duplex read/write operations were not both pending\n",
                stderr);
            goto cleanup;
        }
        cx_gc_collect();
        if (read_task->_isCompleted ||
            socket_write_task_completed(write_task, stream_write_task)) {
            fputs("forced GC changed pending full-duplex operations\n", stderr);
            goto cleanup;
        }

        cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(source);
        while (!read_task->_isCompleted && steps < 100000u) {
            if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                Sleep(1u);
#else
                usleep(1000u);
#endif
            }
            steps++;
        }
        if (!read_task->_isCompleted || !read_task->_isCanceled ||
            read_task->_isFaulted ||
            socket_write_task_completed(write_task, stream_write_task) ||
            socket_write_task_canceled(write_task, stream_write_task) ||
            socket_write_task_faulted(write_task, stream_write_task) ||
            source->_callbackCount != 0u ||
            !cxnet_System_Net_Sockets_Socket_Connected___const_get(socket)) {
            fprintf(stderr,
                "read cancellation disturbed the independent pending write at iteration %u\n",
                (unsigned)iteration);
            goto cleanup;
        }

        if (!stream_peer_reads_expected(peer_handle, 0xa7u, filled_bytes)) {
            fputs("full-duplex cancellation changed the queued peer payload\n",
                stderr);
            goto cleanup;
        }
        steps = 0u;
        while (!socket_write_task_completed(write_task, stream_write_task) &&
            steps < 100000u) {
            if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                Sleep(1u);
#else
                usleep(1000u);
#endif
            }
            steps++;
        }
        if (!socket_write_task_completed(write_task, stream_write_task) ||
            socket_write_task_canceled(write_task, stream_write_task) ||
            socket_write_task_faulted(write_task, stream_write_task) ||
            !stream_peer_reads_expected(peer_handle, 0x5au, 1u)) {
            fputs("the pending write did not complete after read cancellation\n",
                stderr);
            goto cleanup;
        }

#ifdef _WIN32
        if (send((SOCKET)peer_handle, (const char *)&reply, 1, 0) != 1)
#else
        if (send((int)peer_handle, (const char *)&reply, 1, 0) != 1)
#endif
            goto cleanup;
        retry_read_task = use_network_stream ?
            cxnet_System_Net_Sockets_NetworkStream_ReadAsync(network_stream,
                read_buffer, 0u, 1u, no_cancellation) :
            cxnet_System_Net_Sockets_Socket_ReadAsync(socket, read_buffer,
                0u, 1u);
        retry_read_task_root = cx_gc_add_root(&retry_read_task,
            (cx_uint)sizeof(retry_read_task));
        steps = 0u;
        while (retry_read_task != NULL && !retry_read_task->_isCompleted &&
            steps < 100000u) {
            if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                Sleep(1u);
#else
                usleep(1000u);
#endif
            }
            steps++;
        }
        if (retry_read_task == NULL || retry_read_task_root == NULL ||
            !retry_read_task->_isCompleted || retry_read_task->_isCanceled ||
            retry_read_task->_isFaulted ||
            cx_generic_32f22edd4c9ba40c64ed2658909ec8fb1d6e85d2c4276d46eebd01030eefa80f(
                retry_read_task) != 1u ||
            ((cx_byte *)read_buffer->_data)[0] != reply) {
            fputs("read cancellation did not release the receive slot\n",
                stderr);
            goto cleanup;
        }
        cxnet_System_Net_Sockets_Socket_Dispose(socket);
        passed = stream_peer_observes_eof(peer_handle);
cleanup:
        if (network_stream != NULL)
            cxnet_System_Net_Sockets_NetworkStream_Dispose(network_stream);
        if (socket_initialized) {
            cxnet_System_Net_Sockets_Socket_Dispose(socket);
        } else if (native_handle_open) {
#ifdef _WIN32
            closesocket((SOCKET)(uintptr_t)connected_handle);
#else
            close((int)connected_handle);
#endif
        }
        if (peer_handle != (uintptr_t)0) close_stream_read_peer(peer_handle);
        if (retry_read_task_root != NULL)
            cx_gc_remove_root(retry_read_task_root);
        if (stream_write_task_root != NULL)
            cx_gc_remove_root(stream_write_task_root);
        if (write_task_root != NULL) cx_gc_remove_root(write_task_root);
        if (read_task_root != NULL) cx_gc_remove_root(read_task_root);
        if (source_root != NULL) cx_gc_remove_root(source_root);
        if (write_buffer_root != NULL) cx_gc_remove_root(write_buffer_root);
        if (read_buffer_root != NULL) cx_gc_remove_root(read_buffer_root);
        if (network_stream_root != NULL)
            cx_gc_remove_root(network_stream_root);
        if (socket_root != NULL) cx_gc_remove_root(socket_root);
        if (!passed) return 0;
    }
    return 1;
}

static int test_managed_socket_pending_write_dispose(void) {
    uint32_t iteration;
    for (iteration = 0u; iteration < 24u; iteration++) {
        uint64_t connected_handle = 0u;
        uintptr_t peer_handle = (uintptr_t)0;
        size_t filled_bytes = 0u;
        struct cxnet_System_Net_Sockets_Socket *socket = NULL;
        struct cxcore_System_Collections_Array *buffer = NULL;
        struct cxcore_System_Collections_Array *read_buffer = NULL;
        struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *task = NULL;
        struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *read_task = NULL;
        struct cx_gc_root_handle *socket_root = NULL;
        struct cx_gc_root_handle *buffer_root = NULL;
        struct cx_gc_root_handle *task_root = NULL;
        struct cx_gc_root_handle *read_buffer_root = NULL;
        struct cx_gc_root_handle *read_task_root = NULL;
        struct socket_cross_thread_dispose dispose_race = {0};
        uint32_t steps = 0u;
        int passed = 0;
        int second_write_rejected = 0;
        int second_read_rejected = 0;
        int native_handle_open = 0;
        int socket_initialized = 0;

        if (!create_stream_read_pair(&connected_handle, &peer_handle)) {
            fputs("could not create a managed socket write/dispose pair\n",
                stderr);
            goto cleanup;
        }
        native_handle_open = 1;
        if (!socket_prefill_send_buffer(connected_handle, peer_handle,
                &filled_bytes)) {
            fputs("could not fill the managed socket send buffer\n", stderr);
            goto cleanup;
        }
        socket = (struct cxnet_System_Net_Sockets_Socket *)
            cxcore_System_Memory_Alloc(
                cxnet_System_Net_Sockets_Socket___typeinfo.Size);
        buffer = cx_array_new(1u, (cx_uint)sizeof(cx_byte));
        read_buffer = cx_array_new(1u, (cx_uint)sizeof(cx_byte));
        if (socket == NULL || buffer == NULL || read_buffer == NULL)
            goto cleanup;
        socket_root = cx_gc_add_root(&socket, (cx_uint)sizeof(socket));
        buffer_root = cx_gc_add_root(&buffer, (cx_uint)sizeof(buffer));
        read_buffer_root = cx_gc_add_root(&read_buffer,
            (cx_uint)sizeof(read_buffer));
        if (socket_root == NULL || buffer_root == NULL ||
            read_buffer_root == NULL) goto cleanup;
        cxnet_System_Net_Sockets_Socket___constructor(socket,
            (cx_ulong)connected_handle);
        socket_initialized = 1;
        native_handle_open = 0;
        read_task = cxnet_System_Net_Sockets_Socket_ReadAsync(
            socket, read_buffer, 0u, 1u);
        read_task_root = cx_gc_add_root(&read_task, (cx_uint)sizeof(read_task));
        if (read_task == NULL || read_task_root == NULL ||
            read_task->_isCompleted) {
            fprintf(stderr,
                "managed socket receive did not remain pending beside a saturated send at iteration %u\n",
                (unsigned)iteration);
            goto cleanup;
        }
        ((cx_byte *)buffer->_data)[0] = 0x5au;
        task = cxnet_System_Net_Sockets_Socket_WriteAsync(
            socket, buffer, 0u, 1u);
        task_root = cx_gc_add_root(&task, (cx_uint)sizeof(task));
        if (task == NULL || task_root == NULL || task->_isCompleted) {
            fprintf(stderr,
                "managed socket write did not remain pending after send-buffer saturation at iteration %u (task=%p root=%p completed=%d canceled=%d faulted=%d filled=%u)\n",
                (unsigned)iteration, (void *)task, (void *)task_root,
                task == NULL ? -1 : (int)task->_isCompleted,
                task == NULL ? -1 : (int)task->_isCanceled,
                task == NULL ? -1 : (int)task->_isFaulted,
                (unsigned)filled_bytes);
            goto cleanup;
        }
        cx_gc_collect();
        if (task->_isCompleted || read_task->_isCompleted) {
            fputs("forced GC changed a pending full-duplex socket transfer\n",
                stderr);
            goto cleanup;
        }
        {
            struct cx_exception_frame frame;
            cx_exception_push(&frame);
            if (setjmp(frame.environment) == 0) {
                struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *second_task =
                    cxnet_System_Net_Sockets_Socket_ReadAsync(
                        socket, read_buffer, 0u, 1u);
                cx_exception_pop(&frame);
                (void)second_task;
            } else {
                second_read_rejected = cxcore_System_Exception_Matches(
                    (struct cxcore_System_Exception *)cx_exception_current(),
                    cxcore_System_InvalidOperationException___typeinfo);
                cx_exception_pop(&frame);
                cx_exception_clear();
            }
        }
        if (!second_read_rejected) {
            fputs("a second pending socket receive was not rejected beside a pending send\n",
                stderr);
            goto cleanup;
        }
        {
            struct cx_exception_frame frame;
            cx_exception_push(&frame);
            if (setjmp(frame.environment) == 0) {
                struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *second_task =
                    cxnet_System_Net_Sockets_Socket_WriteAsync(
                        socket, buffer, 0u, 1u);
                cx_exception_pop(&frame);
                (void)second_task;
            } else {
                second_write_rejected = cxcore_System_Exception_Matches(
                    (struct cxcore_System_Exception *)cx_exception_current(),
                    cxcore_System_InvalidOperationException___typeinfo);
                cx_exception_pop(&frame);
                cx_exception_clear();
            }
        }
        if (!second_write_rejected) {
            fputs("a second pending socket send was not rejected\n", stderr);
            goto cleanup;
        }
#ifdef _WIN32
        {
            HANDLE worker = CreateThread(NULL, 0,
                socket_cross_thread_dispose_worker, &dispose_race, 0, NULL);
            if (worker == NULL) {
                fputs("could not start cross-thread socket dispose worker\n",
                    stderr);
                goto cleanup;
            }
            dispose_race.socket = socket;
            while (!socket_dispose_race_load(&dispose_race.ready)) Sleep(0u);
            socket_dispose_race_store(&dispose_race.start, 1);
            WaitForSingleObject(worker, INFINITE);
            CloseHandle(worker);
        }
#else
        {
            pthread_t worker;
            dispose_race.socket = socket;
            if (pthread_create(&worker, NULL,
                    socket_cross_thread_dispose_worker, &dispose_race) != 0) {
                fputs("could not start cross-thread socket dispose worker\n",
                    stderr);
                goto cleanup;
            }
            while (!socket_dispose_race_load(&dispose_race.ready))
                sched_yield();
            socket_dispose_race_store(&dispose_race.start, 1);
            pthread_join(worker, NULL);
        }
#endif
        if (socket_dispose_race_load(&dispose_race.failed) ||
            !socket_dispose_race_load(&dispose_race.completed)) {
            fprintf(stderr,
                "cross-thread socket disposal failed at iteration %u\n",
                (unsigned)iteration);
            goto cleanup;
        }
        cx_gc_collect();
        while ((!task->_isCompleted || !read_task->_isCompleted) &&
            steps < 100000u) {
            if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                Sleep(1u);
#else
                usleep(1000u);
#endif
            }
            steps++;
        }
        if (!task->_isCompleted || !task->_isCanceled || task->_isFaulted ||
            !read_task->_isCompleted || !read_task->_isCanceled ||
            read_task->_isFaulted ||
            cxnet_System_Net_Sockets_Socket_Connected___const_get(socket) ||
            !stream_peer_observes_payload_then_eof(peer_handle, filled_bytes)) {
            fprintf(stderr,
                "disposing socket did not cancel both transfers and preserve peer data at iteration %u\n",
                (unsigned)iteration);
            goto cleanup;
        }
        passed = 1;
cleanup:
        if (socket_initialized) {
            cxnet_System_Net_Sockets_Socket_Dispose(socket);
        } else if (native_handle_open) {
#ifdef _WIN32
            closesocket((SOCKET)(uintptr_t)connected_handle);
#else
            close((int)connected_handle);
#endif
        }
        if (peer_handle != (uintptr_t)0) close_stream_read_peer(peer_handle);
        if (read_task_root != NULL) cx_gc_remove_root(read_task_root);
        if (read_buffer_root != NULL) cx_gc_remove_root(read_buffer_root);
        if (task_root != NULL) cx_gc_remove_root(task_root);
        if (buffer_root != NULL) cx_gc_remove_root(buffer_root);
        if (socket_root != NULL) cx_gc_remove_root(socket_root);
        if (!passed) return 0;
    }
    return 1;
}

struct socket_option_dispose_race {
    struct cxnet_System_Net_Sockets_Socket *socket;
    int option;
    volatile int ready;
    volatile int start;
    volatile int completed;
    volatile int set_succeeded;
    volatile int disposed_exception;
    volatile int failed;
};

static void set_socket_option_for_dispose_race(
    struct socket_option_dispose_race *race) {
    switch (race->option) {
    case 0:
        cxnet_System_Net_Sockets_Socket_NoDelay___set(race->socket, 1);
        break;
    case 1:
        cxnet_System_Net_Sockets_Socket_KeepAlive___set(race->socket, 1);
        break;
    case 2:
        cxnet_System_Net_Sockets_Socket_ReuseAddress___set(race->socket, 1);
        break;
    case 3:
        cxnet_System_Net_Sockets_Socket_ReceiveBufferSize___set(race->socket,
            8192);
        break;
    case 4:
        cxnet_System_Net_Sockets_Socket_SendBufferSize___set(race->socket,
            8192);
        break;
    default:
        break;
    }
}

#ifdef _WIN32
static DWORD WINAPI socket_option_dispose_worker(void *argument) {
#else
static void *socket_option_dispose_worker(void *argument) {
#endif
    struct socket_option_dispose_race *race =
        (struct socket_option_dispose_race *)argument;
    cx_bool attached = cx_gc_enter_thread();
    if (!attached) {
        socket_dispose_race_store(&race->failed, 1);
        socket_dispose_race_store(&race->ready, 1);
        socket_dispose_race_store(&race->completed, 1);
#ifdef _WIN32
        return 0;
#else
        return NULL;
#endif
    }
    socket_dispose_race_store(&race->ready, 1);
    while (!socket_dispose_race_load(&race->start)) {
#ifdef _WIN32
        Sleep(0u);
#else
        sched_yield();
#endif
    }
    {
        struct cx_exception_frame frame;
        cx_exception_push(&frame);
        if (setjmp(frame.environment) == 0) {
            set_socket_option_for_dispose_race(race);
            cx_exception_pop(&frame);
            socket_dispose_race_store(&race->set_succeeded, 1);
        } else {
            struct cxcore_System_Exception *exception =
                (struct cxcore_System_Exception *)cx_exception_current();
            cx_exception_pop(&frame);
            if (exception != NULL && cxcore_System_Exception_Matches(exception,
                    cxcore_System_ObjectDisposedException___typeinfo)) {
                socket_dispose_race_store(&race->disposed_exception, 1);
            } else {
                socket_dispose_race_store(&race->failed, 1);
            }
            cx_exception_clear();
        }
    }
    cx_gc_leave_thread(attached);
    socket_dispose_race_store(&race->completed, 1);
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

static int test_managed_socket_option_dispose_race(void) {
    int option;
    for (option = 0; option < 5; ++option) {
        uint32_t iteration;
        for (iteration = 0u; iteration < 24u; iteration++) {
            uint64_t connected_handle = 0u;
            uintptr_t peer_handle = (uintptr_t)0;
            struct socket_option_dispose_race race = {0};
            struct cx_gc_root_handle *socket_root = NULL;
            int passed = 0;
            int native_handle_open = 0;
            int socket_initialized = 0;
            #ifdef _WIN32
            HANDLE worker = NULL;
            #else
            pthread_t worker;
            int worker_started = 0;
            #endif
            if (!create_stream_read_pair(&connected_handle, &peer_handle)) {
                fputs("could not create socket option/dispose race pair\n", stderr);
                return 0;
            }
            native_handle_open = 1;
            race.socket = (struct cxnet_System_Net_Sockets_Socket *)
                cxcore_System_Memory_Alloc(
                    cxnet_System_Net_Sockets_Socket___typeinfo.Size);
            if (race.socket == NULL) goto cleanup;
            socket_root = cx_gc_add_root(&race.socket,
                (cx_uint)sizeof(race.socket));
            if (socket_root == NULL) goto cleanup;
            cxnet_System_Net_Sockets_Socket___constructor(race.socket,
                (cx_ulong)connected_handle);
            race.option = option;
            socket_initialized = 1;
            native_handle_open = 0;
            #ifdef _WIN32
            worker = CreateThread(NULL, 0, socket_option_dispose_worker,
                &race, 0, NULL);
            if (worker == NULL) {
            #else
            if (pthread_create(&worker, NULL, socket_option_dispose_worker,
                    &race) != 0) {
            #endif
                fputs("could not start socket option/dispose race worker\n", stderr);
                goto cleanup;
            }
            #ifndef _WIN32
            worker_started = 1;
            #endif
            while (!socket_dispose_race_load(&race.ready)) {
            #ifdef _WIN32
                Sleep(0u);
            #else
                sched_yield();
            #endif
            }
            socket_dispose_race_store(&race.start, 1);
            cxnet_System_Net_Sockets_Socket_Dispose(race.socket);
            #ifdef _WIN32
            WaitForSingleObject(worker, INFINITE);
            CloseHandle(worker);
            worker = NULL;
            #else
            pthread_join(worker, NULL);
            worker_started = 0;
            #endif
            if (!socket_dispose_race_load(&race.completed) ||
                socket_dispose_race_load(&race.failed) ||
                (socket_dispose_race_load(&race.set_succeeded) ==
                    socket_dispose_race_load(&race.disposed_exception)) ||
                cxnet_System_Net_Sockets_Socket_Connected___const_get(race.socket) ||
                !stream_peer_observes_eof(peer_handle)) {
                fprintf(stderr,
                    "socket option/dispose race had an invalid outcome for option %d at iteration %u (set=%d disposed=%d failed=%d completed=%d)\n",
                    option, (unsigned)iteration,
                    socket_dispose_race_load(&race.set_succeeded),
                    socket_dispose_race_load(&race.disposed_exception),
                    socket_dispose_race_load(&race.failed),
                    socket_dispose_race_load(&race.completed));
                goto cleanup;
            }
            passed = 1;
            cleanup:
            #ifdef _WIN32
            if (worker != NULL) {
                socket_dispose_race_store(&race.start, 1);
                WaitForSingleObject(worker, INFINITE);
                CloseHandle(worker);
            }
            #else
            if (worker_started) {
                socket_dispose_race_store(&race.start, 1);
                pthread_join(worker, NULL);
            }
            #endif
            if (socket_initialized)
                cxnet_System_Net_Sockets_Socket_Dispose(race.socket);
            #ifdef _WIN32
            else if (native_handle_open)
                closesocket((SOCKET)(uintptr_t)connected_handle);
            #else
            else if (native_handle_open)
                close((int)connected_handle);
            #endif
            close_stream_read_peer(peer_handle);
            if (socket_root != NULL) cx_gc_remove_root(socket_root);
            if (!passed) return 0;
        }
    }
    return 1;
}

struct socket_connect_reset_option_race {
    struct cxnet_System_Net_Sockets_Socket *socket;
    int option;
    volatile int ready;
    volatile int start;
    volatile int completed;
    volatile int failed;
    volatile int socket_error;
    volatile int object_disposed;
    volatile int other_exception;
    volatile int set_succeeded;
    volatile int expected_rejections;
    volatile int last_value;
};

static void set_connect_reset_option(
    struct socket_connect_reset_option_race *race, int value) {
    switch (race->option) {
    case 0:
        cxnet_System_Net_Sockets_Socket_NoDelay___set(race->socket, value);
        break;
    case 1:
        cxnet_System_Net_Sockets_Socket_KeepAlive___set(race->socket, value);
        break;
    case 2:
        cxnet_System_Net_Sockets_Socket_ReuseAddress___set(race->socket, value);
        break;
    case 3:
        cxnet_System_Net_Sockets_Socket_ReceiveBufferSize___set(
            race->socket, value);
        break;
    case 4:
        cxnet_System_Net_Sockets_Socket_SendBufferSize___set(
            race->socket, value);
        break;
    default:
        break;
    }
}

static int get_connect_reset_option(
    struct socket_connect_reset_option_race *race) {
    switch (race->option) {
    case 0:
        return cxnet_System_Net_Sockets_Socket_NoDelay___get(race->socket);
    case 1:
        return cxnet_System_Net_Sockets_Socket_KeepAlive___get(race->socket);
    case 2:
        return cxnet_System_Net_Sockets_Socket_ReuseAddress___get(race->socket);
    case 3:
        return cxnet_System_Net_Sockets_Socket_ReceiveBufferSize___get(
            race->socket);
    case 4:
        return cxnet_System_Net_Sockets_Socket_SendBufferSize___get(
            race->socket);
    default:
        return 0;
    }
}

#ifdef _WIN32
static DWORD WINAPI socket_connect_reset_option_worker(void *argument) {
#else
static void *socket_connect_reset_option_worker(void *argument) {
#endif
    struct socket_connect_reset_option_race *race =
        (struct socket_connect_reset_option_race *)argument;
    cx_bool attached = cx_gc_enter_thread();
    uint32_t iteration;
    if (!attached) {
        socket_dispose_race_store(&race->failed, 1);
        socket_dispose_race_store(&race->ready, 1);
        socket_dispose_race_store(&race->completed, 1);
#ifdef _WIN32
        return 0;
#else
        return NULL;
#endif
    }
    socket_dispose_race_store(&race->ready, 1);
    while (!socket_dispose_race_load(&race->start)) {
#ifdef _WIN32
        Sleep(0u);
#else
        sched_yield();
#endif
    }
    for (iteration = 0u; iteration < 16u; iteration++) {
        struct cx_exception_frame frame;
        int value = race->option < 3
            ? ((iteration & 1u) == 0u ? 0 : 1)
            : ((iteration & 1u) == 0u ? 4096 : 8192);
        cx_exception_push(&frame);
        if (setjmp(frame.environment) == 0) {
            set_connect_reset_option(race, value);
            cx_exception_pop(&frame);
            socket_dispose_race_store(&race->set_succeeded, 1);
            if (race->option >= 3) {
                socket_dispose_race_store(&race->last_value, value);
            }
        } else {
            struct cxcore_System_Exception *exception =
                (struct cxcore_System_Exception *)cx_exception_current();
            cx_exception_pop(&frame);
            if (exception != NULL && cxcore_System_Exception_Matches(exception,
                    cxnet_System_Net_Sockets_SocketException___typeinfo)) {
                int error_code =
                    cxnet_System_Net_Sockets_SocketException_SocketErrorCode___const_get(
                        (struct cxnet_System_Net_Sockets_SocketException *)
                            exception);
                socket_dispose_race_store(&race->socket_error, error_code);
                if (error_code ==
                        cxnet_System_Net_Sockets_SocketError_InvalidArgument) {
                    socket_dispose_race_store(&race->expected_rejections,
                        socket_dispose_race_load(&race->expected_rejections) + 1);
                    cx_exception_clear();
                    continue;
                }
            } else if (exception != NULL &&
                cxcore_System_Exception_Matches(exception,
                    cxcore_System_ObjectDisposedException___typeinfo)) {
                socket_dispose_race_store(&race->object_disposed, 1);
            } else {
                socket_dispose_race_store(&race->other_exception, 1);
            }
            socket_dispose_race_store(&race->failed, 1);
            cx_exception_clear();
            break;
        }
#ifdef _WIN32
        Sleep(1u);
#else
        {
            struct timespec delay = {0, 1000000L};
            nanosleep(&delay, NULL);
        }
#endif
    }
    cx_gc_leave_thread(attached);
    socket_dispose_race_store(&race->completed, 1);
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

static int test_endpoint_connect_reset_option_race(
    cx_ushort refused_port, int first_option, int option_limit) {
    int option;
    for (option = first_option; option < option_limit; ++option) {
        uint32_t iteration;
        uint32_t iteration_limit = option < 3 ? 24u : 4u;
        for (iteration = 0u; iteration < iteration_limit; iteration++) {
            struct cxnet_System_Net_Sockets_Socket *client =
                (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
                    cxnet_System_Net_Sockets_Socket___typeinfo.Size);
            struct cxnet_System_Net_IPEndPoint *endpoint =
                (struct cxnet_System_Net_IPEndPoint *)cxcore_System_Memory_Alloc(
                    cxnet_System_Net_IPEndPoint___typeinfo.Size);
            struct cxcore_System_Threading_Tasks_TaskHandle *task;
            struct socket_connect_reset_option_race race = {0};
            struct cx_gc_root_handle *client_root = cx_gc_add_root(&client,
                (cx_uint)sizeof(client));
            struct cx_gc_root_handle *endpoint_root = cx_gc_add_root(&endpoint,
                (cx_uint)sizeof(endpoint));
            struct cx_gc_root_handle *task_root = NULL;
            uint32_t steps = 0u;
            int passed = 0;
            #ifdef _WIN32
            HANDLE worker = NULL;
            #else
            pthread_t worker;
            int worker_started = 0;
            #endif
            if (client == NULL || endpoint == NULL || client_root == NULL ||
                endpoint_root == NULL) goto cleanup;
            cxnet_System_Net_Sockets_Socket___constructor__2(client,
                cxnet_System_Net_AddressFamily_InterNetwork,
                cxnet_System_Net_Sockets_SocketType_Stream,
                cxnet_System_Net_Sockets_ProtocolType_Tcp);
            race.option = option;
            race.socket = client;
            socket_dispose_race_store(&race.last_value,
                option < 3 ? 1 : 4096);
            set_connect_reset_option(&race,
                socket_dispose_race_load(&race.last_value));
            cxnet_System_Net_IPEndPoint___constructor__2(endpoint,
                cxnet_System_Net_IPAddress_Loopback___get(),
                (cx_int)refused_port);
            #ifdef _WIN32
            worker = CreateThread(NULL, 0, socket_connect_reset_option_worker,
                &race, 0, NULL);
            if (worker == NULL) goto cleanup;
            #else
            if (pthread_create(&worker, NULL, socket_connect_reset_option_worker,
                    &race) != 0) goto cleanup;
            worker_started = 1;
            #endif
            while (!socket_dispose_race_load(&race.ready)) {
            #ifdef _WIN32
                Sleep(0u);
            #else
                sched_yield();
            #endif
            }
            task = cxnet_System_Net_Sockets_Socket_ConnectAsync__5(client,
                (struct cxnet_System_Net_EndPoint *)endpoint);
            task_root = cx_gc_add_root(&task, (cx_uint)sizeof(task));
            if (task_root == NULL) goto cleanup;
            socket_dispose_race_store(&race.start, 1);
            cx_gc_collect();
            while ((!task->_state->_isCompleted ||
                    !socket_dispose_race_load(&race.completed)) &&
                    steps < 100000u) {
                if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
            #ifdef _WIN32
                    Sleep(1u);
            #else
                    usleep(1000u);
            #endif
                }
                steps++;
            }
            #ifdef _WIN32
            WaitForSingleObject(worker, INFINITE);
            CloseHandle(worker);
            worker = NULL;
            #else
            pthread_join(worker, NULL);
            worker_started = 0;
            #endif
            if (!socket_dispose_race_load(&race.failed) &&
                task->_state->_isCompleted && task->_state->_isFaulted &&
                cxcore_System_Exception_Matches(task->_state->_exception,
                    cxnet_System_Net_Sockets_SocketException___typeinfo) &&
                cxnet_System_Net_Sockets_SocketException_SocketErrorCode___const_get(
                    (struct cxnet_System_Net_Sockets_SocketException *)
                        task->_state->_exception) ==
                    cxnet_System_Net_Sockets_SocketError_ConnectionRefused) {
                struct cx_exception_frame frame;
                cx_exception_push(&frame);
                if (setjmp(frame.environment) == 0) {
                    if (option < 3) {
                        set_connect_reset_option(&race, 0);
                    } else {
                        set_connect_reset_option(&race, 8192);
                        if (get_connect_reset_option(&race) < 8192) {
                            socket_dispose_race_store(&race.failed, 1);
                        }
                    }
                    cx_exception_pop(&frame);
                } else {
                    cx_exception_pop(&frame);
                    socket_dispose_race_store(&race.failed, 1);
                    cx_exception_clear();
                }
            }
            passed = !socket_dispose_race_load(&race.failed) &&
                socket_dispose_race_load(&race.completed) &&
                task->_state->_isCompleted && task->_state->_isFaulted &&
                !task->_state->_isCanceled &&
                cxcore_System_Exception_Matches(task->_state->_exception,
                    cxnet_System_Net_Sockets_SocketException___typeinfo) &&
                cxnet_System_Net_Sockets_SocketException_SocketErrorCode___const_get(
                    (struct cxnet_System_Net_Sockets_SocketException *)
                        task->_state->_exception) ==
                    cxnet_System_Net_Sockets_SocketError_ConnectionRefused &&
                !client->_disposed && client->_state == 0u &&
                !cxnet_System_Net_Sockets_Socket_Connected___const_get(client) &&
                (option < 3
                    ? !get_connect_reset_option(&race)
                    : get_connect_reset_option(&race) >=
                        socket_dispose_race_load(&race.last_value)) &&
                (socket_dispose_race_load(&race.set_succeeded) ||
                    socket_dispose_race_load(&race.expected_rejections) > 0);
            if (!passed) {
                fprintf(stderr,
                    "connect-reset option race had an invalid outcome for option %d at iteration %u (completed=%d faulted=%d canceled=%d failed=%d socket_error=%d disposed=%d other=%d set=%d rejected=%d state=%u steps=%u)\n",
                    option, (unsigned)iteration, task->_state->_isCompleted,
                    task->_state->_isFaulted, task->_state->_isCanceled,
                    socket_dispose_race_load(&race.failed),
                    socket_dispose_race_load(&race.socket_error),
                    socket_dispose_race_load(&race.object_disposed),
                    socket_dispose_race_load(&race.other_exception),
                    socket_dispose_race_load(&race.set_succeeded),
                    socket_dispose_race_load(&race.expected_rejections),
                    (unsigned)client->_state, (unsigned)steps);
            }
            cleanup:
            #ifdef _WIN32
            if (worker != NULL) {
                socket_dispose_race_store(&race.start, 1);
                WaitForSingleObject(worker, INFINITE);
                CloseHandle(worker);
            }
            #else
            if (worker_started) {
                socket_dispose_race_store(&race.start, 1);
                pthread_join(worker, NULL);
            }
            #endif
            if (task_root != NULL) cx_gc_remove_root(task_root);
            if (client != NULL) cxnet_System_Net_Sockets_Socket_Dispose(client);
            if (endpoint_root != NULL) cx_gc_remove_root(endpoint_root);
            if (client_root != NULL) cx_gc_remove_root(client_root);
            if (!passed) return 0;
        }
    }
    return 1;
}

static int run_endpoint_connect_reset_option_race_only(int buffers_only) {
    struct cxnet_System_Net_Sockets_Socket *listener =
        (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Sockets_Socket___typeinfo.Size);
    struct cxnet_System_Net_IPEndPoint *bind_endpoint =
        (struct cxnet_System_Net_IPEndPoint *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_IPEndPoint___typeinfo.Size);
    struct cxnet_System_Net_IPEndPoint *local_endpoint = NULL;
    struct cx_gc_root_handle *listener_root = cx_gc_add_root(&listener,
        (cx_uint)sizeof(listener));
    struct cx_gc_root_handle *bind_endpoint_root = cx_gc_add_root(
        &bind_endpoint, (cx_uint)sizeof(bind_endpoint));
    struct cx_gc_root_handle *local_endpoint_root = NULL;
    cx_ushort refused_port;
    int passed;
    if (listener == NULL || bind_endpoint == NULL || listener_root == NULL ||
        bind_endpoint_root == NULL) return 0;
    cxnet_System_Net_Sockets_Socket___constructor__2(listener,
        cxnet_System_Net_AddressFamily_InterNetwork,
        cxnet_System_Net_Sockets_SocketType_Stream,
        cxnet_System_Net_Sockets_ProtocolType_Tcp);
    cxnet_System_Net_IPEndPoint___constructor__2(bind_endpoint,
        cxnet_System_Net_IPAddress_Loopback___get(), 0);
    cxnet_System_Net_Sockets_Socket_Bind(listener,
        (struct cxnet_System_Net_EndPoint *)bind_endpoint);
    cxnet_System_Net_Sockets_Socket_Listen__2(listener, 1);
    local_endpoint = (struct cxnet_System_Net_IPEndPoint *)
        cxnet_System_Net_Sockets_Socket_GetLocalEndPoint(listener);
    local_endpoint_root = cx_gc_add_root(&local_endpoint,
        (cx_uint)sizeof(local_endpoint));
    if (local_endpoint == NULL || local_endpoint_root == NULL) {
        cxnet_System_Net_Sockets_Socket_Dispose(listener);
        return 0;
    }
    refused_port = cxnet_System_Net_IPEndPoint_Port___const_get(local_endpoint);
    cxnet_System_Net_Sockets_Socket_Dispose(listener);
    passed = test_endpoint_connect_reset_option_race(refused_port,
        buffers_only ? 3 : 0, 5);
    cx_gc_remove_root(local_endpoint_root);
    cx_gc_remove_root(bind_endpoint_root);
    cx_gc_remove_root(listener_root);
    return passed;
}

static int send_stream_read_byte(uintptr_t peer_handle, cx_byte value) {
#ifdef _WIN32
    return send((SOCKET)peer_handle, (const char *)&value, 1, 0) == 1;
#else
    return send((int)peer_handle, (const char *)&value, 1, 0) == 1;
#endif
}

static int test_network_stream_dispose_during_pending_read(void) {
    uint64_t connected_handle = 0u;
    uintptr_t peer_handle = (uintptr_t)0;
    struct cxnet_System_Net_Sockets_Socket *socket =
        (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Sockets_Socket___typeinfo.Size);
    struct cxnet_System_Net_Sockets_NetworkStream *stream =
        (struct cxnet_System_Net_Sockets_NetworkStream *)cxcore_System_Memory_Alloc(
            (cx_uint)sizeof(struct cxnet_System_Net_Sockets_NetworkStream));
    struct cxcore_System_Collections_Array *buffer;
    struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *task;
    struct cx_gc_root_handle *socket_root = cx_gc_add_root(
        &socket, (cx_uint)sizeof(socket));
    struct cx_gc_root_handle *stream_root = cx_gc_add_root(
        &stream, (cx_uint)sizeof(stream));
    struct cx_gc_root_handle *buffer_root;
    struct cx_gc_root_handle *task_root;
    struct cxcore_System_Threading_Tasks_CancellationToken token = {0};
    cx_byte expected = (cx_byte)'N';
    uint32_t steps = 0u;
    int passed = 0;

    if (!create_stream_read_pair(&connected_handle, &peer_handle)) {
        fputs("could not create a NetworkStream pending-read fixture\n", stderr);
        cx_gc_remove_root(stream_root);
        cx_gc_remove_root(socket_root);
        return 0;
    }
    cxnet_System_Net_Sockets_Socket___constructor(socket,
        (cx_ulong)connected_handle);
    cxnet_System_Net_Sockets_NetworkStream___constructor__2(stream, socket, 0);
    buffer = cx_array_new(1u, (cx_uint)sizeof(cx_byte));
    buffer_root = cx_gc_add_root(&buffer, (cx_uint)sizeof(buffer));
    task = cxnet_System_Net_Sockets_NetworkStream_ReadAsync(
        stream, buffer, 0u, 1u, token);
    task_root = cx_gc_add_root(&task, (cx_uint)sizeof(task));
    cx_gc_collect();
    if (task->_isCompleted) {
        fputs("NetworkStream read completed before peer data was sent\n", stderr);
        goto cleanup;
    }
    cxnet_System_Net_Sockets_NetworkStream_Dispose(stream);
    cxnet_System_Net_Sockets_NetworkStream_Dispose(stream);
    if (cxnet_System_Net_Sockets_NetworkStream_CanRead___get(stream)) {
        fputs("disposed NetworkStream still reported readable\n", stderr);
        goto cleanup;
    }
    if (!send_stream_read_byte(peer_handle, expected)) {
        fputs("could not release the pending NetworkStream read\n", stderr);
        goto cleanup;
    }
    while (!task->_isCompleted && steps < 100000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    passed = task->_isCompleted && !task->_isFaulted && !task->_isCanceled &&
        task->_result == 1u && ((cx_byte *)buffer->_data)[0] == expected &&
        cxnet_System_Net_Sockets_Socket_Connected___const_get(socket);
    if (!passed) {
        fputs("disposing a non-owning NetworkStream corrupted its pending read\n",
            stderr);
    }
cleanup:
    cxnet_System_Net_Sockets_Socket_Dispose(socket);
    close_stream_read_peer(peer_handle);
    cx_gc_remove_root(task_root);
    cx_gc_remove_root(buffer_root);
    cx_gc_remove_root(stream_root);
    cx_gc_remove_root(socket_root);
    return passed;
}

static int test_owning_network_stream_dispose_during_pending_read(void) {
    uint64_t connected_handle = 0u;
    uintptr_t peer_handle = (uintptr_t)0;
    struct cxnet_System_Net_Sockets_Socket *socket =
        (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Sockets_Socket___typeinfo.Size);
    struct cxnet_System_Net_Sockets_NetworkStream *stream =
        (struct cxnet_System_Net_Sockets_NetworkStream *)cxcore_System_Memory_Alloc(
            (cx_uint)sizeof(struct cxnet_System_Net_Sockets_NetworkStream));
    struct cxcore_System_Collections_Array *buffer;
    struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *task;
    struct cx_gc_root_handle *socket_root = cx_gc_add_root(
        &socket, (cx_uint)sizeof(socket));
    struct cx_gc_root_handle *stream_root = cx_gc_add_root(
        &stream, (cx_uint)sizeof(stream));
    struct cx_gc_root_handle *buffer_root;
    struct cx_gc_root_handle *task_root;
    struct cxcore_System_Threading_Tasks_CancellationToken token = {0};
    uint32_t steps = 0u;
    int passed = 0;

    if (!create_stream_read_pair(&connected_handle, &peer_handle)) {
        fputs("could not create an owning NetworkStream fixture\n", stderr);
        cx_gc_remove_root(stream_root);
        cx_gc_remove_root(socket_root);
        return 0;
    }
    cxnet_System_Net_Sockets_Socket___constructor(socket,
        (cx_ulong)connected_handle);
    cxnet_System_Net_Sockets_NetworkStream___constructor__2(stream, socket, 1);
    buffer = cx_array_new(1u, (cx_uint)sizeof(cx_byte));
    buffer_root = cx_gc_add_root(&buffer, (cx_uint)sizeof(buffer));
    task = cxnet_System_Net_Sockets_NetworkStream_ReadAsync(
        stream, buffer, 0u, 1u, token);
    task_root = cx_gc_add_root(&task, (cx_uint)sizeof(task));
    cx_gc_collect();
    if (task->_isCompleted) {
        fputs("owning NetworkStream read completed before disposal\n", stderr);
        goto cleanup;
    }
    cxnet_System_Net_Sockets_NetworkStream_Dispose(stream);
    cxnet_System_Net_Sockets_NetworkStream_Dispose(stream);
    steps = 0u;
    while (!task->_isCompleted && steps < 100000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    passed = !cxnet_System_Net_Sockets_NetworkStream_CanRead___get(stream) &&
        socket->_disposed &&
        socket->_nativeHandle == cxcore_System_ULong_MaxValue &&
        task->_isCompleted && task->_isCanceled && !task->_isFaulted &&
        stream_peer_observes_eof(peer_handle);
    if (!passed) {
        fprintf(stderr,
            "disposing an owning NetworkStream failed disposed=%d completed=%d canceled=%d faulted=%d steps=%u\n",
            socket->_disposed, task->_isCompleted, task->_isCanceled,
            task->_isFaulted, (unsigned)steps);
    }
cleanup:
    cxnet_System_Net_Sockets_NetworkStream_Dispose(stream);
    cxnet_System_Net_Sockets_Socket_Dispose(socket);
    close_stream_read_peer(peer_handle);
    cx_gc_remove_root(task_root);
    cx_gc_remove_root(buffer_root);
    cx_gc_remove_root(stream_root);
    cx_gc_remove_root(socket_root);
    return passed;
}

static int test_owning_network_stream_dispose_during_pending_write(void) {
    uint64_t connected_handle = 0u;
    uintptr_t peer_handle = (uintptr_t)0;
    size_t filled_bytes = 0u;
    struct cxnet_System_Net_Sockets_Socket *socket =
        (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Sockets_Socket___typeinfo.Size);
    struct cxnet_System_Net_Sockets_NetworkStream *stream =
        (struct cxnet_System_Net_Sockets_NetworkStream *)cxcore_System_Memory_Alloc(
            (cx_uint)sizeof(struct cxnet_System_Net_Sockets_NetworkStream));
    struct cxcore_System_Collections_Array *buffer = NULL;
    struct cxcore_System_Threading_Tasks_TaskHandle *task = NULL;
    struct cxcore_System_Threading_Tasks_CancellationToken token = {0};
    struct cx_gc_root_handle *socket_root = cx_gc_add_root(&socket,
        (cx_uint)sizeof(socket));
    struct cx_gc_root_handle *stream_root = cx_gc_add_root(&stream,
        (cx_uint)sizeof(stream));
    struct cx_gc_root_handle *buffer_root = NULL;
    struct cx_gc_root_handle *task_root = NULL;
    uint32_t steps = 0u;
    int passed = 0;

    if (socket == NULL || stream == NULL || socket_root == NULL ||
        stream_root == NULL ||
        !create_stream_read_pair(&connected_handle, &peer_handle)) goto cleanup;
    cxnet_System_Net_Sockets_Socket___constructor(socket,
        (cx_ulong)connected_handle);
    cxnet_System_Net_Sockets_NetworkStream___constructor__2(stream, socket, 1);
    if (!socket_prefill_send_buffer(connected_handle, peer_handle,
            &filled_bytes)) goto cleanup;
    buffer = cx_array_new(1u, (cx_uint)sizeof(cx_byte));
    buffer_root = cx_gc_add_root(&buffer, (cx_uint)sizeof(buffer));
    if (buffer == NULL || buffer_root == NULL) goto cleanup;
    ((cx_byte *)buffer->_data)[0] = 0x5au;
    task = cxnet_System_Net_Sockets_NetworkStream_WriteAsync(
        stream, buffer, 0u, 1u, token);
    task_root = cx_gc_add_root(&task, (cx_uint)sizeof(task));
    if (task == NULL || task_root == NULL ||
        cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(task)) {
        fputs("NetworkStream write completed before send capacity was available\n",
            stderr);
        goto cleanup;
    }
    cx_gc_collect();
    if (cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(task)) {
        fputs("forced GC completed a pending NetworkStream write\n", stderr);
        goto cleanup;
    }
    cxnet_System_Net_Sockets_NetworkStream_Dispose(stream);
    cxnet_System_Net_Sockets_NetworkStream_Dispose(stream);
    while (!cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(task) &&
        steps < 100000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    passed = cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(task) &&
        cxcore_System_Threading_Tasks_TaskHandle_IsCanceled___const_get(task) &&
        !cxcore_System_Threading_Tasks_TaskHandle_IsFaulted___const_get(task) &&
        socket->_disposed &&
        socket->_nativeHandle == cxcore_System_ULong_MaxValue &&
        !cxnet_System_Net_Sockets_NetworkStream_CanWrite___get(stream) &&
        stream_peer_observes_payload_then_eof(peer_handle, filled_bytes);
    if (!passed) {
        fprintf(stderr,
            "disposing an owning NetworkStream during a pending write failed completed=%d canceled=%d faulted=%d disposed=%d steps=%u\n",
            cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(task),
            cxcore_System_Threading_Tasks_TaskHandle_IsCanceled___const_get(task),
            cxcore_System_Threading_Tasks_TaskHandle_IsFaulted___const_get(task),
            socket->_disposed, (unsigned)steps);
    }
cleanup:
    if (stream != NULL) cxnet_System_Net_Sockets_NetworkStream_Dispose(stream);
    if (socket != NULL) cxnet_System_Net_Sockets_Socket_Dispose(socket);
    if (peer_handle != (uintptr_t)0) close_stream_read_peer(peer_handle);
    if (task_root != NULL) cx_gc_remove_root(task_root);
    if (buffer_root != NULL) cx_gc_remove_root(buffer_root);
    if (stream_root != NULL) cx_gc_remove_root(stream_root);
    if (socket_root != NULL) cx_gc_remove_root(socket_root);
    return passed;
}

static int test_nonowning_network_stream_dispose_during_pending_write(void) {
    uint64_t connected_handle = 0u;
    uintptr_t peer_handle = (uintptr_t)0;
    size_t filled_bytes = 0u;
    struct cxnet_System_Net_Sockets_Socket *socket =
        (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Sockets_Socket___typeinfo.Size);
    struct cxnet_System_Net_Sockets_NetworkStream *stream =
        (struct cxnet_System_Net_Sockets_NetworkStream *)cxcore_System_Memory_Alloc(
            (cx_uint)sizeof(struct cxnet_System_Net_Sockets_NetworkStream));
    struct cxcore_System_Collections_Array *buffer = NULL;
    struct cxcore_System_Threading_Tasks_TaskHandle *task = NULL;
    struct cxcore_System_Threading_Tasks_CancellationToken token = {0};
    struct cx_gc_root_handle *socket_root = cx_gc_add_root(&socket,
        (cx_uint)sizeof(socket));
    struct cx_gc_root_handle *stream_root = cx_gc_add_root(&stream,
        (cx_uint)sizeof(stream));
    struct cx_gc_root_handle *buffer_root = NULL;
    struct cx_gc_root_handle *task_root = NULL;
    uint32_t steps = 0u;
    int passed = 0;

    if (socket == NULL || stream == NULL || socket_root == NULL ||
        stream_root == NULL ||
        !create_stream_read_pair(&connected_handle, &peer_handle)) goto cleanup;
    cxnet_System_Net_Sockets_Socket___constructor(socket,
        (cx_ulong)connected_handle);
    cxnet_System_Net_Sockets_NetworkStream___constructor__2(stream, socket, 0);
    if (!socket_prefill_send_buffer(connected_handle, peer_handle,
            &filled_bytes)) goto cleanup;
    buffer = cx_array_new(1u, (cx_uint)sizeof(cx_byte));
    buffer_root = cx_gc_add_root(&buffer, (cx_uint)sizeof(buffer));
    if (buffer == NULL || buffer_root == NULL) goto cleanup;
    ((cx_byte *)buffer->_data)[0] = 0x5au;
    task = cxnet_System_Net_Sockets_NetworkStream_WriteAsync(
        stream, buffer, 0u, 1u, token);
    task_root = cx_gc_add_root(&task, (cx_uint)sizeof(task));
    if (task == NULL || task_root == NULL ||
        cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(task)) {
        fputs("non-owning NetworkStream write completed before send capacity was available\n",
            stderr);
        goto cleanup;
    }
    cx_gc_collect();
    cxnet_System_Net_Sockets_NetworkStream_Dispose(stream);
    cxnet_System_Net_Sockets_NetworkStream_Dispose(stream);
    if (socket->_disposed ||
        !cxnet_System_Net_Sockets_Socket_Connected___const_get(socket) ||
        cxnet_System_Net_Sockets_NetworkStream_CanWrite___get(stream) ||
        cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(task)) {
        fputs("disposing a non-owning NetworkStream closed its socket or write\n",
            stderr);
        goto cleanup;
    }
    {
        struct socket_peer_payload_drain drain = {0};
#ifdef _WIN32
        HANDLE worker;
#else
        pthread_t worker;
#endif
        drain.peer = peer_handle;
        drain.filled_bytes = filled_bytes;
        drain.trailing_bytes = 1u;
#ifdef _WIN32
        worker = CreateThread(NULL, 0, socket_peer_payload_drain_worker,
            &drain, 0, NULL);
        if (worker == NULL) goto cleanup;
#else
        if (pthread_create(&worker, NULL, socket_peer_payload_drain_worker,
                &drain) != 0) goto cleanup;
#endif
        while (!cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(task) &&
            steps < 100000u) {
            if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                Sleep(1u);
#else
                usleep(1000u);
#endif
            }
            steps++;
        }
#ifdef _WIN32
        WaitForSingleObject(worker, INFINITE);
        CloseHandle(worker);
#else
        pthread_join(worker, NULL);
#endif
        if (!drain.completed || drain.failed) goto cleanup;
    }
    if (!cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(task) ||
        cxcore_System_Threading_Tasks_TaskHandle_IsCanceled___const_get(task) ||
        cxcore_System_Threading_Tasks_TaskHandle_IsFaulted___const_get(task) ||
        socket->_disposed ||
        !cxnet_System_Net_Sockets_Socket_Connected___const_get(socket)) {
        fputs("non-owning NetworkStream disposal did not preserve the pending write\n",
            stderr);
        goto cleanup;
    }
    {
        const unsigned char reply = 0x36u;
        if (cxnet_System_Net_Sockets_Socket_Receive(socket,
                buffer, 0u, 1u) != 1 || ((cx_byte *)buffer->_data)[0] != reply)
            goto cleanup;
    }
    cxnet_System_Net_Sockets_Socket_Dispose(socket);
    passed = stream_peer_observes_eof(peer_handle);
    if (!passed)
        fputs("non-owning NetworkStream socket did not close after explicit socket disposal\n",
            stderr);
cleanup:
    if (stream != NULL) cxnet_System_Net_Sockets_NetworkStream_Dispose(stream);
    if (socket != NULL) cxnet_System_Net_Sockets_Socket_Dispose(socket);
    if (peer_handle != (uintptr_t)0) close_stream_read_peer(peer_handle);
    if (task_root != NULL) cx_gc_remove_root(task_root);
    if (buffer_root != NULL) cx_gc_remove_root(buffer_root);
    if (stream_root != NULL) cx_gc_remove_root(stream_root);
    if (socket_root != NULL) cx_gc_remove_root(socket_root);
    return passed;
}

static int test_nonowning_network_stream_socket_dispose_during_pending_write(void) {
    uint64_t connected_handle = 0u;
    uintptr_t peer_handle = (uintptr_t)0;
    size_t filled_bytes = 0u;
    struct cxnet_System_Net_Sockets_Socket *socket =
        (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Sockets_Socket___typeinfo.Size);
    struct cxnet_System_Net_Sockets_NetworkStream *stream =
        (struct cxnet_System_Net_Sockets_NetworkStream *)cxcore_System_Memory_Alloc(
            (cx_uint)sizeof(struct cxnet_System_Net_Sockets_NetworkStream));
    struct cxcore_System_Collections_Array *buffer = NULL;
    struct cxcore_System_Threading_Tasks_TaskHandle *task = NULL;
    struct cxcore_System_Threading_Tasks_CancellationToken token = {0};
    struct cx_gc_root_handle *socket_root = cx_gc_add_root(&socket,
        (cx_uint)sizeof(socket));
    struct cx_gc_root_handle *stream_root = cx_gc_add_root(&stream,
        (cx_uint)sizeof(stream));
    struct cx_gc_root_handle *buffer_root = NULL;
    struct cx_gc_root_handle *task_root = NULL;
    uint32_t steps = 0u;
    int passed = 0;

    if (socket == NULL || stream == NULL || socket_root == NULL ||
        stream_root == NULL ||
        !create_stream_read_pair(&connected_handle, &peer_handle)) goto cleanup;
    cxnet_System_Net_Sockets_Socket___constructor(socket,
        (cx_ulong)connected_handle);
    cxnet_System_Net_Sockets_NetworkStream___constructor__2(stream, socket, 0);
    if (!socket_prefill_send_buffer(connected_handle, peer_handle,
            &filled_bytes)) goto cleanup;
    buffer = cx_array_new(1u, (cx_uint)sizeof(cx_byte));
    buffer_root = cx_gc_add_root(&buffer, (cx_uint)sizeof(buffer));
    if (buffer == NULL || buffer_root == NULL) goto cleanup;
    ((cx_byte *)buffer->_data)[0] = 0x5au;
    task = cxnet_System_Net_Sockets_NetworkStream_WriteAsync(
        stream, buffer, 0u, 1u, token);
    task_root = cx_gc_add_root(&task, (cx_uint)sizeof(task));
    if (task == NULL || task_root == NULL ||
        cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(task)) {
        fputs("non-owning NetworkStream write completed before socket disposal\n",
            stderr);
        goto cleanup;
    }
    cx_gc_collect();
    if (cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(task)) {
        fputs("forced GC completed the pending non-owning NetworkStream write\n",
            stderr);
        goto cleanup;
    }
    cxnet_System_Net_Sockets_Socket_Dispose(socket);
    cxnet_System_Net_Sockets_Socket_Dispose(socket);
    while (!cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(task) &&
        steps < 100000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    passed = socket->_disposed &&
        !cxnet_System_Net_Sockets_NetworkStream_CanWrite___get(stream) &&
        cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(task) &&
        cxcore_System_Threading_Tasks_TaskHandle_IsCanceled___const_get(task) &&
        !cxcore_System_Threading_Tasks_TaskHandle_IsFaulted___const_get(task) &&
        stream_peer_observes_payload_then_eof(peer_handle, filled_bytes);
    if (!passed) {
        fprintf(stderr,
            "disposing the socket during a non-owning NetworkStream write failed disposed=%d completed=%d canceled=%d faulted=%d steps=%u\n",
            socket->_disposed,
            cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(task),
            cxcore_System_Threading_Tasks_TaskHandle_IsCanceled___const_get(task),
            cxcore_System_Threading_Tasks_TaskHandle_IsFaulted___const_get(task),
            (unsigned)steps);
    }
cleanup:
    if (stream != NULL) cxnet_System_Net_Sockets_NetworkStream_Dispose(stream);
    if (socket != NULL) cxnet_System_Net_Sockets_Socket_Dispose(socket);
    if (peer_handle != (uintptr_t)0) close_stream_read_peer(peer_handle);
    if (task_root != NULL) cx_gc_remove_root(task_root);
    if (buffer_root != NULL) cx_gc_remove_root(buffer_root);
    if (stream_root != NULL) cx_gc_remove_root(stream_root);
    if (socket_root != NULL) cx_gc_remove_root(socket_root);
    return passed;
}

static int test_nonowning_network_stream_socket_dispose_during_pending_read(void) {
    uint64_t connected_handle = 0u;
    uintptr_t peer_handle = (uintptr_t)0;
    struct cxnet_System_Net_Sockets_Socket *socket =
        (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Sockets_Socket___typeinfo.Size);
    struct cxnet_System_Net_Sockets_NetworkStream *stream =
        (struct cxnet_System_Net_Sockets_NetworkStream *)cxcore_System_Memory_Alloc(
            (cx_uint)sizeof(struct cxnet_System_Net_Sockets_NetworkStream));
    struct cxcore_System_Collections_Array *buffer = NULL;
    struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *task = NULL;
    struct cxcore_System_Threading_Tasks_CancellationToken token = {0};
    struct cx_gc_root_handle *socket_root = cx_gc_add_root(&socket,
        (cx_uint)sizeof(socket));
    struct cx_gc_root_handle *stream_root = cx_gc_add_root(&stream,
        (cx_uint)sizeof(stream));
    struct cx_gc_root_handle *buffer_root = NULL;
    struct cx_gc_root_handle *task_root = NULL;
    uint32_t steps = 0u;
    int passed = 0;

    if (socket == NULL || stream == NULL || socket_root == NULL ||
        stream_root == NULL ||
        !create_stream_read_pair(&connected_handle, &peer_handle)) goto cleanup;
    cxnet_System_Net_Sockets_Socket___constructor(socket,
        (cx_ulong)connected_handle);
    cxnet_System_Net_Sockets_NetworkStream___constructor__2(stream, socket, 0);
    buffer = cx_array_new(1u, (cx_uint)sizeof(cx_byte));
    buffer_root = cx_gc_add_root(&buffer, (cx_uint)sizeof(buffer));
    if (buffer == NULL || buffer_root == NULL) goto cleanup;
    task = cxnet_System_Net_Sockets_NetworkStream_ReadAsync(
        stream, buffer, 0u, 1u, token);
    task_root = cx_gc_add_root(&task, (cx_uint)sizeof(task));
    if (task == NULL || task_root == NULL ||
        task->_isCompleted) {
        fputs("non-owning NetworkStream read completed before socket disposal\n",
            stderr);
        goto cleanup;
    }
    cx_gc_collect();
    if (task->_isCompleted) {
        fputs("forced GC completed the pending non-owning NetworkStream read\n",
            stderr);
        goto cleanup;
    }
    cxnet_System_Net_Sockets_Socket_Dispose(socket);
    cxnet_System_Net_Sockets_Socket_Dispose(socket);
    while (!task->_isCompleted && steps < 100000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    passed = socket->_disposed &&
        !cxnet_System_Net_Sockets_NetworkStream_CanRead___get(stream) &&
        task->_isCompleted && task->_isCanceled && !task->_isFaulted &&
        stream_peer_observes_eof(peer_handle);
    if (!passed) {
        fprintf(stderr,
            "disposing the socket during a non-owning NetworkStream read failed disposed=%d completed=%d canceled=%d faulted=%d steps=%u\n",
            socket->_disposed,
            task->_isCompleted, task->_isCanceled, task->_isFaulted,
            (unsigned)steps);
    }
cleanup:
    if (stream != NULL) cxnet_System_Net_Sockets_NetworkStream_Dispose(stream);
    if (socket != NULL) cxnet_System_Net_Sockets_Socket_Dispose(socket);
    if (peer_handle != (uintptr_t)0) close_stream_read_peer(peer_handle);
    if (task_root != NULL) cx_gc_remove_root(task_root);
    if (buffer_root != NULL) cx_gc_remove_root(buffer_root);
    if (stream_root != NULL) cx_gc_remove_root(stream_root);
    if (socket_root != NULL) cx_gc_remove_root(socket_root);
    return passed;
}

static int test_accepted_clients_survive_listener_disposal(void) {
    struct cxnet_System_Net_Sockets_Socket *listener = NULL;
    struct cxnet_System_Net_Sockets_Socket *client = NULL;
    struct cxnet_System_Net_Sockets_Socket *accepted = NULL;
    struct cxnet_System_Net_IPEndPoint *bind_endpoint = NULL;
    struct cxnet_System_Net_EndPoint *local_endpoint = NULL;
    struct cxcore_System_Collections_Array outbound = {0};
    struct cxcore_System_Collections_Array inbound = {0};
    unsigned char outbound_byte = 0x31u;
    unsigned char inbound_byte = 0u;
    struct cx_gc_root_handle *listener_root = cx_gc_add_root(&listener,
        (cx_uint)sizeof(listener));
    struct cx_gc_root_handle *client_root = cx_gc_add_root(&client,
        (cx_uint)sizeof(client));
    struct cx_gc_root_handle *accepted_root = cx_gc_add_root(&accepted,
        (cx_uint)sizeof(accepted));
    struct cx_gc_root_handle *bind_endpoint_root = cx_gc_add_root(
        &bind_endpoint, (cx_uint)sizeof(bind_endpoint));
    struct cx_gc_root_handle *local_endpoint_root = cx_gc_add_root(
        &local_endpoint, (cx_uint)sizeof(local_endpoint));
    int passed = 0;

    listener = (struct cxnet_System_Net_Sockets_Socket *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Sockets_Socket___typeinfo.Size);
    client = (struct cxnet_System_Net_Sockets_Socket *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Sockets_Socket___typeinfo.Size);
    bind_endpoint = (struct cxnet_System_Net_IPEndPoint *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_IPEndPoint___typeinfo.Size);
    if (listener == NULL || client == NULL || bind_endpoint == NULL ||
        listener_root == NULL || client_root == NULL || accepted_root == NULL ||
        bind_endpoint_root == NULL || local_endpoint_root == NULL) goto cleanup;

    outbound._length = 1u;
    outbound._data = (cx_byte *)&outbound_byte;
    inbound._length = 1u;
    inbound._data = (cx_byte *)&inbound_byte;
    cxnet_System_Net_Sockets_Socket___constructor__2(listener,
        cxnet_System_Net_AddressFamily_InterNetwork,
        cxnet_System_Net_Sockets_SocketType_Stream,
        cxnet_System_Net_Sockets_ProtocolType_Tcp);
    cxnet_System_Net_Sockets_Socket___constructor__2(client,
        cxnet_System_Net_AddressFamily_InterNetwork,
        cxnet_System_Net_Sockets_SocketType_Stream,
        cxnet_System_Net_Sockets_ProtocolType_Tcp);
    cxnet_System_Net_IPEndPoint___constructor__2(bind_endpoint,
        cxnet_System_Net_IPAddress_Loopback___get(), 0);
    cxnet_System_Net_Sockets_Socket_Bind(listener,
        (struct cxnet_System_Net_EndPoint *)bind_endpoint);
    cxnet_System_Net_Sockets_Socket_Listen__2(listener, 1);
    local_endpoint = (struct cxnet_System_Net_EndPoint *)
        cxnet_System_Net_Sockets_Socket_GetLocalEndPoint(listener);
    if (local_endpoint == NULL) goto cleanup;
    cxnet_System_Net_Sockets_Socket_Connect(client, local_endpoint);
    accepted = cxnet_System_Net_Sockets_Socket_Accept(listener);
    if (accepted == NULL) goto cleanup;

    cxnet_System_Net_Sockets_Socket_Dispose(listener);
    cxnet_System_Net_Sockets_Socket_Dispose(listener);
    cx_gc_collect();
    if (!listener->_disposed ||
        !cxnet_System_Net_Sockets_Socket_Connected___const_get(client) ||
        !cxnet_System_Net_Sockets_Socket_Connected___const_get(accepted) ||
        cxnet_System_Net_Sockets_Socket_Send(client, &outbound, 0u, 1u) != 1 ||
        cxnet_System_Net_Sockets_Socket_Receive(accepted, &inbound, 0u, 1u) != 1 ||
        inbound_byte != outbound_byte) goto cleanup;

    outbound_byte = 0x72u;
    inbound_byte = 0u;
    if (cxnet_System_Net_Sockets_Socket_Send(accepted,
            &outbound, 0u, 1u) != 1 ||
        cxnet_System_Net_Sockets_Socket_Receive(client,
            &inbound, 0u, 1u) != 1 || inbound_byte != outbound_byte) goto cleanup;
    passed = 1;
cleanup:
    if (accepted != NULL) cxnet_System_Net_Sockets_Socket_Dispose(accepted);
    if (client != NULL) cxnet_System_Net_Sockets_Socket_Dispose(client);
    if (listener != NULL) cxnet_System_Net_Sockets_Socket_Dispose(listener);
    if (local_endpoint_root != NULL) cx_gc_remove_root(local_endpoint_root);
    if (bind_endpoint_root != NULL) cx_gc_remove_root(bind_endpoint_root);
    if (accepted_root != NULL) cx_gc_remove_root(accepted_root);
    if (client_root != NULL) cx_gc_remove_root(client_root);
    if (listener_root != NULL) cx_gc_remove_root(listener_root);
    return passed;
}

static int test_socket_native_close_error_after_cleanup(void) {
    uint64_t connected_handle = 0u;
    uintptr_t peer_handle = (uintptr_t)0;
    struct cxnet_System_Net_Sockets_Socket *socket =
        (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Sockets_Socket___typeinfo.Size);
    struct cxcore_System_Collections_Array *buffer;
    struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *task;
    struct cx_gc_root_handle *socket_root = cx_gc_add_root(
        &socket, (cx_uint)sizeof(socket));
    struct cx_gc_root_handle *buffer_root;
    struct cx_gc_root_handle *task_root;
    struct cx_gc_root_handle *exception_root = NULL;
    struct cxcore_System_Exception *exception = NULL;
    uint32_t steps = 0u;
    int expected_native_error;
    const char *expected_domain;
    cx_uint expected_domain_length;
    int passed = 0;

#ifdef _WIN32
    expected_native_error = 10014;
    expected_domain = "Winsock";
#else
    expected_native_error = 5;
    expected_domain = "POSIX errno";
#endif
    expected_domain_length = (cx_uint)strlen(expected_domain);
    if (!create_stream_read_pair(&connected_handle, &peer_handle)) {
        fputs("could not create native close-error fixture\n", stderr);
        cx_gc_remove_root(socket_root);
        return 0;
    }
    cxnet_System_Net_Sockets_Socket___constructor(socket,
        (cx_ulong)connected_handle);
    buffer = cx_array_new(1u, (cx_uint)sizeof(cx_byte));
    buffer_root = cx_gc_add_root(&buffer, (cx_uint)sizeof(buffer));
    task = cxnet_System_Net_Sockets_Socket_ReadAsync(socket, buffer, 0u, 1u);
    task_root = cx_gc_add_root(&task, (cx_uint)sizeof(task));
    cx_gc_collect();
    if (task->_isCompleted) {
        fputs("native close-error read completed before close\n", stderr);
        goto cleanup;
    }

    cxnet_System_Net_Sockets_SocketRuntime_TestFailNextClose();
    {
        struct cx_exception_frame frame;
        cx_exception_push(&frame);
        if (setjmp(frame.environment) == 0) {
            cxnet_System_Net_Sockets_Socket_Dispose(socket);
            cx_exception_pop(&frame);
            fputs("injected native close error was not reported\n", stderr);
            goto cleanup;
        }
        exception = (struct cxcore_System_Exception *)cx_exception_current();
        exception_root = cx_gc_add_root(&exception, (cx_uint)sizeof(exception));
        cx_exception_pop(&frame);
    }
    cx_exception_clear();

    while (!task->_isCompleted && steps < 100000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (exception != NULL && cxcore_System_Exception_Matches(exception,
            cxnet_System_Net_Sockets_SocketException___typeinfo)) {
        struct cxcore_System_String *domain =
            cxcore_System_IO_IOException_NativeErrorDomain___const_get(
                (struct cxcore_System_IO_IOException *)exception);
        passed = cxnet_System_Net_Sockets_SocketException_SocketErrorCode___const_get(
                (struct cxnet_System_Net_Sockets_SocketException *)exception) ==
                cxnet_System_Net_Sockets_SocketError_Unknown &&
            cxcore_System_IO_IOException_NativeErrorCode___const_get(
                (struct cxcore_System_IO_IOException *)exception) ==
                expected_native_error && domain != NULL &&
            domain->_length == expected_domain_length &&
            memcmp(domain->_data, expected_domain, expected_domain_length) == 0 &&
            socket->_disposed &&
            socket->_nativeHandle == cxcore_System_ULong_MaxValue &&
            task->_isCompleted && task->_isCanceled && !task->_isFaulted &&
            stream_peer_observes_eof(peer_handle);
    }
    if (!passed) {
        fputs("native close error was reported before socket cleanup completed\n",
            stderr);
    }
cleanup:
    cx_exception_clear();
    cxnet_System_Net_Sockets_Socket_Dispose(socket);
    close_stream_read_peer(peer_handle);
    if (exception_root != NULL) cx_gc_remove_root(exception_root);
    cx_gc_remove_root(task_root);
    cx_gc_remove_root(buffer_root);
    cx_gc_remove_root(socket_root);
    return passed;
}

int main(int argument_count, char **arguments) {
    if (argument_count == 2 && strcmp(arguments[1],
            "--endpoint-connect-failure-reuse") == 0) {
        cx_gc_check_thread();
        return run_endpoint_connect_failure_reuse_only() ? 0 : 1;
    }
    if (argument_count == 2 && strcmp(arguments[1],
            "--endpoint-connect-duplicate-pending") == 0) {
        cx_gc_check_thread();
        return run_endpoint_connect_duplicate_pending_only() ? 0 : 1;
    }
    if (argument_count == 2 && strcmp(arguments[1],
            "--network-stream-peer-reset") == 0) {
        cx_gc_check_thread();
        return test_managed_read_reports_peer_reset(1) ? 0 : 1;
    }
    if (argument_count == 2 && strcmp(arguments[1],
            "--network-stream-pending-write-peer-reset") == 0) {
        cx_gc_check_thread();
        return test_managed_pending_write_peer_reset(1) ? 0 : 1;
    }
    if (argument_count == 2 && strcmp(arguments[1],
            "--socket-pending-write-peer-reset") == 0) {
        cx_gc_check_thread();
        return test_managed_pending_write_peer_reset(0) ? 0 : 1;
    }
    if (argument_count == 2 && strcmp(arguments[1],
            "--host-connect-cross-thread-dispose") == 0) {
        cxnet_test_resolver_set_callback(test_resolver_callback, NULL);
        cx_gc_check_thread();
        return test_hostname_connect_dispose_during_resolution() ? 0 : 1;
    }
    if (argument_count == 2 && strcmp(arguments[1],
            "--pending-write-cross-thread-dispose") == 0) {
        cx_gc_check_thread();
        return test_managed_socket_pending_write_dispose() ? 0 : 1;
    }
    if (argument_count == 2 && strcmp(arguments[1],
            "--saturated-socket-write") == 0) {
        cx_gc_check_thread();
        return test_managed_socket_saturated_write() ? 0 : 1;
    }
    if (argument_count == 2 && strcmp(arguments[1],
            "--socket-concurrent-write-rejection") == 0) {
        cx_gc_check_thread();
        return test_managed_socket_rejects_concurrent_writes() ? 0 : 1;
    }
    if (argument_count == 2 && strcmp(arguments[1],
            "--pending-socket-write-cancel") == 0) {
        cx_gc_check_thread();
        return test_managed_socket_pending_write_cancel() ? 0 : 1;
    }
    if (argument_count == 2 && strcmp(arguments[1],
            "--pending-socket-write-ready-cancel-race") == 0) {
        cx_gc_check_thread();
        return test_managed_socket_pending_write_ready_cancel_race(0) ? 0 : 1;
    }
    if (argument_count == 2 && strcmp(arguments[1],
            "--network-stream-pending-write-ready-cancel-race") == 0) {
        cx_gc_check_thread();
        return test_managed_socket_pending_write_ready_cancel_race(1) ? 0 : 1;
    }
    if (argument_count == 2 && strcmp(arguments[1],
            "--pending-write-shutdown") == 0) {
        cx_gc_check_thread();
        return test_managed_socket_pending_write_shutdown(0) ? 0 : 1;
    }
    if (argument_count == 2 && strcmp(arguments[1],
            "--network-stream-pending-write-shutdown") == 0) {
        cx_gc_check_thread();
        return test_managed_socket_pending_write_shutdown(1) ? 0 : 1;
    }
    if (argument_count == 2 && strcmp(arguments[1],
            "--write-receive-shutdown") == 0) {
        cx_gc_check_thread();
        return test_managed_socket_pending_write_survives_receive_shutdown(0) ?
            0 : 1;
    }
    if (argument_count == 2 && strcmp(arguments[1],
            "--network-stream-write-receive-shutdown") == 0) {
        cx_gc_check_thread();
        return test_managed_socket_pending_write_survives_receive_shutdown(1) ?
            0 : 1;
    }
    if (argument_count == 2 && strcmp(arguments[1],
            "--pending-socket-read-cancel") == 0) {
        cx_gc_check_thread();
        return test_managed_socket_pending_read_cancel(0) ? 0 : 1;
    }
    if (argument_count == 2 && strcmp(arguments[1],
            "--network-stream-pending-read-cancel") == 0) {
        cx_gc_check_thread();
        return test_managed_socket_pending_read_cancel(1) ? 0 : 1;
    }
    if (argument_count == 2 && strcmp(arguments[1],
            "--pending-socket-read-ready-cancel-race") == 0) {
        cx_gc_check_thread();
        return test_managed_socket_pending_read_ready_cancel_race(0) ? 0 : 1;
    }
    if (argument_count == 2 && strcmp(arguments[1],
            "--network-stream-pending-read-ready-cancel-race") == 0) {
        cx_gc_check_thread();
        return test_managed_socket_pending_read_ready_cancel_race(1) ? 0 : 1;
    }
    if (argument_count == 2 && strcmp(arguments[1],
            "--full-duplex-independent-cancel") == 0) {
        cx_gc_check_thread();
        return test_managed_socket_full_duplex_independent_cancel(0) ? 0 : 1;
    }
    if (argument_count == 2 && strcmp(arguments[1],
            "--network-stream-full-duplex-independent-cancel") == 0) {
        cx_gc_check_thread();
        return test_managed_socket_full_duplex_independent_cancel(1) ? 0 : 1;
    }
    if (argument_count == 2 && strcmp(arguments[1],
            "--network-stream-read-dispose-start-race") == 0) {
        cx_gc_check_thread();
        return test_managed_socket_read_dispose_race(1) ? 0 : 1;
    }
    if (argument_count == 2 && strcmp(arguments[1],
            "--network-stream-nonowning-read-dispose-start-race") == 0) {
        cx_gc_check_thread();
        return test_managed_socket_read_dispose_race(2) ? 0 : 1;
    }
    if (argument_count == 2 && strcmp(arguments[1],
            "--write-dispose-start-race") == 0) {
        cx_gc_check_thread();
        return test_managed_socket_write_dispose_start_race(0) ? 0 : 1;
    }
    if (argument_count == 2 && strcmp(arguments[1],
            "--network-stream-write-dispose-start-race") == 0) {
        cx_gc_check_thread();
        return test_managed_socket_write_dispose_start_race(1) ? 0 : 1;
    }
    if (argument_count == 2 && strcmp(arguments[1],
            "--network-stream-nonowning-write-dispose-start-race") == 0) {
        cx_gc_check_thread();
        return test_managed_socket_write_dispose_start_race(2) ? 0 : 1;
    }
    if (argument_count == 2 && strcmp(arguments[1],
            "--write-dispose-handle-reuse") == 0) {
        cx_gc_check_thread();
        return test_managed_socket_write_dispose_start_race(0) &&
            test_managed_socket_pending_receive_shutdown(0) ? 0 : 1;
    }
    if (argument_count == 2 && strcmp(arguments[1],
            "--receive-shutdown") == 0) {
        cx_gc_check_thread();
        return test_managed_socket_receive_shutdown() ? 0 : 1;
    }
    if (argument_count == 2 && strcmp(arguments[1],
            "--pending-receive-shutdown") == 0) {
        cx_gc_check_thread();
        return test_managed_socket_pending_receive_shutdown(0) ? 0 : 1;
    }
    if (argument_count == 2 && strcmp(arguments[1],
            "--network-stream-pending-receive-shutdown") == 0) {
        cx_gc_check_thread();
        return test_managed_socket_pending_receive_shutdown(1) ? 0 : 1;
    }
    if (argument_count == 2 && strcmp(arguments[1],
            "--multiple-accept-dispose") == 0) {
        cx_gc_check_thread();
        return test_managed_socket_multiple_accept_dispose() ? 0 : 1;
    }
    if (argument_count == 2 && strcmp(arguments[1],
            "--concurrent-accepts-succeed") == 0) {
        cx_gc_check_thread();
        return test_concurrent_accepts_succeed() ? 0 : 1;
    }
    if (argument_count == 2 && strcmp(arguments[1],
            "--accept-cancel-after-native-accept") == 0) {
        cx_gc_check_thread();
        return test_managed_socket_cancel_after_native_accept() ? 0 : 1;
    }
    if (argument_count == 2 && strcmp(arguments[1],
            "--network-stream-pending-write-dispose") == 0) {
        cx_gc_check_thread();
        return test_owning_network_stream_dispose_during_pending_write() ? 0 : 1;
    }
    if (argument_count == 2 && strcmp(arguments[1],
            "--network-stream-nonowning-pending-write-dispose") == 0) {
        cx_gc_check_thread();
        return test_nonowning_network_stream_dispose_during_pending_write() ? 0 : 1;
    }
    if (argument_count == 2 && strcmp(arguments[1],
            "--network-stream-nonowning-pending-write-socket-dispose") == 0) {
        cx_gc_check_thread();
        return test_nonowning_network_stream_socket_dispose_during_pending_write()
            ? 0 : 1;
    }
    if (argument_count == 2 && strcmp(arguments[1],
            "--network-stream-nonowning-pending-read-socket-dispose") == 0) {
        cx_gc_check_thread();
        return test_nonowning_network_stream_socket_dispose_during_pending_read()
            ? 0 : 1;
    }
    if (argument_count == 2 && strcmp(arguments[1],
            "--network-stream-concurrent-read-rejection") == 0) {
        cx_gc_check_thread();
        return test_network_stream_rejects_concurrent_reads() ? 0 : 1;
    }
    if (argument_count == 2 && strcmp(arguments[1],
            "--network-stream-concurrent-write-rejection") == 0) {
        cx_gc_check_thread();
        return test_network_stream_rejects_concurrent_writes() ? 0 : 1;
    }
    if (argument_count == 2 && strcmp(arguments[1],
            "--accepted-clients-survive-listener-dispose") == 0) {
        cx_gc_check_thread();
        return test_accepted_clients_survive_listener_disposal() ? 0 : 1;
    }
    if (argument_count == 2 && strcmp(arguments[1],
            "--connect-reset-option-race") == 0) {
        cxnet_test_resolver_set_callback(test_resolver_callback, NULL);
        cx_gc_check_thread();
        return run_endpoint_connect_reset_option_race_only(0) ? 0 : 1;
    }
    if (argument_count == 2 && strcmp(arguments[1],
            "--connect-reset-buffer-option-race") == 0) {
        cxnet_test_resolver_set_callback(test_resolver_callback, NULL);
        cx_gc_check_thread();
        return run_endpoint_connect_reset_option_race_only(1) ? 0 : 1;
    }
    if (argument_count == 2 && strcmp(arguments[1],
            "--endpoint-connect-cross-thread-dispose") == 0) {
        cx_gc_check_thread();
        return test_endpoint_connect_cross_thread_dispose() ? 0 : 1;
    }
    if (argument_count != 1) return 2;
    struct cxcore_System_Threading_Tasks_Task *task;
    struct cxcore_System_Collections_Array *addresses;
    struct cxnet_System_Net_Sockets_Socket *listener;
    struct cxnet_System_Net_Sockets_Socket *cancel_listener;
    struct cxnet_System_Net_Sockets_Socket *client;
    struct cxnet_System_Net_IPEndPoint *bind_endpoint;
    struct cxnet_System_Net_IPEndPoint *local_endpoint;
    struct cxcore_System_Threading_Tasks_TaskHandle *connect_task;
    struct cxcore_System_Threading_Tasks_Task *accept_task;
    struct cxnet_System_Net_Sockets_Socket *accepted;
    struct cx_gc_root_handle *task_root;
    struct cx_gc_root_handle *accept_root;
    struct cx_gc_root_handle *connect_root;
    struct cx_gc_root_handle *client_root;
    struct cx_gc_root_handle *listener_root;
    struct cx_gc_root_handle *cancel_listener_root;
    struct cx_gc_root_handle *cancellation_source_root;
    struct cxcore_System_Threading_Tasks_CancellationTokenSource *cancellation_source;
    struct cxcore_System_Threading_Tasks_CancellationToken cancellation_token;
    cx_action cancel_action = {0};
    cx_ushort port;
    uint32_t steps = 0u;
    cxnet_test_resolver_set_callback(test_resolver_callback, NULL);
    cx_gc_check_thread();
    task = cxnet_System_Net_Dns_GetHostAddressesAsync(make_string("localhost"));
    task_root = cx_gc_add_root(&task, (cx_uint)sizeof(task));
    cx_gc_collect();
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        steps < 100000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task)) {
        fputs("async DNS did not complete\n", stderr);
        return 1;
    }
    addresses = cx_generic_47e308080f2e4db04484c1e6ac349bad319be87946d6b240deb100ea368f522e(task);
    if (addresses == NULL || addresses->_length == 0u) {
        fputs("async DNS returned no localhost addresses\n", stderr);
        return 2;
    }

    {
        struct cxcore_System_Threading_Tasks_Task *load_tasks[16] = {0};
        struct cx_gc_root_handle *load_tasks_root = cx_gc_add_root(
            load_tasks, (cx_uint)sizeof(load_tasks));
        uint32_t load_index;
        for (load_index = 0u;
             load_index < (uint32_t)(sizeof(load_tasks) / sizeof(load_tasks[0]));
             load_index += 1u) {
            load_tasks[load_index] = cxnet_System_Net_Dns_GetHostAddressesAsync(
                make_string("cxnet-test-resolution-delay.invalid"));
        }
        cx_gc_collect();
        steps = 0u;
        for (;;) {
            int all_completed = 1;
            for (load_index = 0u;
                 load_index < (uint32_t)(sizeof(load_tasks) / sizeof(load_tasks[0]));
                 load_index += 1u) {
                if (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(
                        load_tasks[load_index])) {
                    all_completed = 0;
                    break;
                }
            }
            if (all_completed || steps >= 100000u) break;
            if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                Sleep(1u);
#else
                usleep(1000u);
#endif
            }
            steps++;
        }
        if (steps >= 100000u) {
            fputs("concurrent async DNS requests did not complete\n", stderr);
            return 22;
        }
        for (load_index = 0u;
             load_index < (uint32_t)(sizeof(load_tasks) / sizeof(load_tasks[0]));
             load_index += 1u) {
            struct cxcore_System_Collections_Array *load_addresses;
            load_addresses = cx_generic_47e308080f2e4db04484c1e6ac349bad319be87946d6b240deb100ea368f522e(
                load_tasks[load_index]);
            if (load_addresses == NULL || load_addresses->_length != 1u) {
                fputs("concurrent async DNS request returned an invalid result\n",
                    stderr);
                return 24;
            }
        }
        cx_gc_remove_root(load_tasks_root);
    }

    {
        struct cx_exception_frame frame;
        struct cxcore_System_Exception *exception;
        cxnet_System_Net_Sockets_SocketError expected_socket_error;
        int expected_native_error;
#ifdef _WIN32
        expected_socket_error = cxnet_System_Net_Sockets_SocketError_HostNotFound;
        expected_native_error = 11001;
#else
        expected_socket_error = cxnet_System_Net_Sockets_SocketError_TryAgain;
        expected_native_error = -3;
#endif
        cx_exception_push(&frame);
        if (setjmp(frame.environment) == 0) {
            (void)cxnet_System_Net_Dns_GetHostAddresses(
                make_string("cxnet-test-resolution-failure.invalid"));
            cx_exception_pop(&frame);
            fputs("sync DNS failure fixture unexpectedly resolved\n", stderr);
            return 11;
        }
        exception = (struct cxcore_System_Exception *)cx_exception_current();
        if (!cxcore_System_Exception_Matches(exception,
                cxnet_System_Net_Sockets_SocketException___typeinfo) ||
            cxnet_System_Net_Sockets_SocketException_SocketErrorCode___const_get(
                (struct cxnet_System_Net_Sockets_SocketException *)exception) !=
                expected_socket_error ||
            cxcore_System_IO_IOException_NativeErrorCode___const_get(
                (struct cxcore_System_IO_IOException *)exception) !=
                expected_native_error ||
            cxcore_System_IO_IOException_NativeErrorDomain___const_get(
                (struct cxcore_System_IO_IOException *)exception) == NULL ||
            cxcore_System_IO_IOException_NativeErrorDomain___const_get(
                (struct cxcore_System_IO_IOException *)exception)->_length != 3u ||
            memcmp(cxcore_System_IO_IOException_NativeErrorDomain___const_get(
                    (struct cxcore_System_IO_IOException *)exception)->_data,
                "dns", 3u) != 0) {
            cx_exception_pop(&frame);
            cx_exception_clear();
            fputs("sync DNS failure lost resolver error identity\n", stderr);
            return 12;
        }
        cx_exception_pop(&frame);
        cx_exception_clear();
    }

    listener = (struct cxnet_System_Net_Sockets_Socket *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Sockets_Socket___typeinfo.Size);
    listener_root = cx_gc_add_root(&listener, (cx_uint)sizeof(listener));
    client = (struct cxnet_System_Net_Sockets_Socket *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Sockets_Socket___typeinfo.Size);
    client_root = cx_gc_add_root(&client, (cx_uint)sizeof(client));
    bind_endpoint = (struct cxnet_System_Net_IPEndPoint *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_IPEndPoint___typeinfo.Size);
    cxnet_System_Net_Sockets_Socket___constructor__2(listener,
        cxnet_System_Net_AddressFamily_InterNetwork,
        cxnet_System_Net_Sockets_SocketType_Stream,
        cxnet_System_Net_Sockets_ProtocolType_Tcp);
    cxnet_System_Net_IPEndPoint___constructor__2(bind_endpoint,
        cxnet_System_Net_IPAddress_Loopback___get(), 0);
    cxnet_System_Net_Sockets_Socket_Bind(listener,
        (struct cxnet_System_Net_EndPoint *)bind_endpoint);
    cxnet_System_Net_Sockets_Socket_Listen__2(listener, 4);
    local_endpoint = cxnet_System_Net_Sockets_Socket_GetLocalEndPoint(listener);
    port = cxnet_System_Net_IPEndPoint_Port___const_get(local_endpoint);
    if (port == 0u) {
        fputs("hostname connect fixture did not get an ephemeral port\n", stderr);
        return 3;
    }
    {
        struct cxnet_System_Net_Sockets_Socket *timeout_client =
            (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
                cxnet_System_Net_Sockets_Socket___typeinfo.Size);
        struct cxcore_System_Threading_Tasks_TaskHandle *timeout_task;
        struct cxcore_System_Threading_Tasks_Task *timeout_accept_task;
        struct cxnet_System_Net_Sockets_Socket *timeout_accepted;
        struct cx_gc_root_handle *timeout_client_root = cx_gc_add_root(
            &timeout_client, (cx_uint)sizeof(timeout_client));
        struct cx_gc_root_handle *timeout_task_root;
        struct cx_gc_root_handle *timeout_accept_root;
        struct cx_gc_root_handle *timeout_accepted_root;
        struct cx_exception_frame timeout_frame;
        struct cxcore_System_Exception *timeout_exception;
        cxnet_System_Net_Sockets_Socket___constructor__2(timeout_client,
            cxnet_System_Net_AddressFamily_InterNetwork,
            cxnet_System_Net_Sockets_SocketType_Stream,
            cxnet_System_Net_Sockets_ProtocolType_Tcp);
        if (cxnet_System_Net_Sockets_Socket_ConnectTimeoutMilliseconds___get(
                timeout_client) != 15000u) {
            fputs("hostname connect timeout default changed\n", stderr);
            return 18;
        }
        cxnet_System_Net_Sockets_Socket_ConnectTimeoutMilliseconds___set(
            timeout_client, 25u);
        timeout_task = cxnet_System_Net_Sockets_Socket_ConnectAsync__3(
            timeout_client, make_string("cxnet-test-resolution-delay.invalid"),
            port);
        timeout_task_root = cx_gc_add_root(&timeout_task,
            (cx_uint)sizeof(timeout_task));
        cx_gc_collect();
        steps = 0u;
        while (!cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(
                timeout_task) && steps < 1000u) {
            if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                Sleep(1u);
#else
                usleep(1000u);
#endif
            }
            steps++;
        }
        if (!cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(
                timeout_task) || timeout_task->_state->_isCanceled ||
            !timeout_task->_state->_isFaulted) {
            fputs("hostname connect deadline did not fault its task\n", stderr);
            return 18;
        }
        cx_exception_push(&timeout_frame);
        if (setjmp(timeout_frame.environment) == 0) {
            cxcore_System_Threading_Tasks_TaskHandle_GetResult(timeout_task);
            cx_exception_pop(&timeout_frame);
            fputs("timed-out hostname connect unexpectedly succeeded\n", stderr);
            return 18;
        }
        timeout_exception = (struct cxcore_System_Exception *)cx_exception_current();
        if (!cxcore_System_Exception_Matches(timeout_exception,
                cxcore_System_TimeoutException___typeinfo)) {
            cx_exception_pop(&timeout_frame);
            cx_exception_clear();
            fputs("hostname connect deadline did not report TimeoutException\n",
                stderr);
            return 18;
        }
        cx_exception_pop(&timeout_frame);
        cx_exception_clear();

        /* Let the delayed worker finish; its late result must not reconnect. */
        steps = 0u;
        while (steps < 220u) {
            if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                Sleep(1u);
#else
                usleep(1000u);
#endif
            }
            steps++;
        }
        if (cxnet_System_Net_Sockets_Socket_Connected___const_get(timeout_client)) {
            fputs("late DNS result changed the timed-out socket\n", stderr);
            return 18;
        }

        timeout_accept_task = cxnet_System_Net_Sockets_Socket_AcceptAsync(listener);
        timeout_accept_root = cx_gc_add_root(&timeout_accept_task,
            (cx_uint)sizeof(timeout_accept_task));
        timeout_task = cxnet_System_Net_Sockets_Socket_ConnectAsync__5(
            timeout_client, (struct cxnet_System_Net_EndPoint *)local_endpoint);
        steps = 0u;
        while ((!cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(
                    timeout_task) ||
                !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(
                    timeout_accept_task)) && steps < 100000u) {
            if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                Sleep(1u);
#else
                usleep(1000u);
#endif
            }
            steps++;
        }
        if (!cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(
                timeout_task) ||
            !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(
                timeout_accept_task)) {
            fputs("socket did not recover after hostname timeout\n", stderr);
            return 18;
        }
        cxcore_System_Threading_Tasks_TaskHandle_GetResult(timeout_task);
        timeout_accepted = cx_generic_7c4851112331385bf3ec935e4daabb246ea0a9a28971c93643ab4b4268d85138(
            (const struct cxcore_System_Threading_Tasks_Task *)timeout_accept_task);
        timeout_accepted_root = cx_gc_add_root(&timeout_accepted,
            (cx_uint)sizeof(timeout_accepted));
        if (timeout_accepted == NULL ||
            !cxnet_System_Net_Sockets_Socket_Connected___const_get(timeout_client) ||
            !cxnet_System_Net_Sockets_Socket_Connected___const_get(timeout_accepted)) {
            fputs("socket reuse after hostname timeout did not connect\n", stderr);
            return 18;
        }
        cxnet_System_Net_Sockets_Socket_Dispose(timeout_accepted);
        cxnet_System_Net_Sockets_Socket_Dispose(timeout_client);
        cx_gc_remove_root(timeout_accepted_root);
        cx_gc_remove_root(timeout_task_root);
        cx_gc_remove_root(timeout_accept_root);
        cx_gc_remove_root(timeout_client_root);
    }
    {
        struct cxnet_System_Net_Sockets_Socket *unbounded_client =
            (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
                cxnet_System_Net_Sockets_Socket___typeinfo.Size);
        struct cxcore_System_Threading_Tasks_TaskHandle *unbounded_task;
        struct cxcore_System_Threading_Tasks_Task *unbounded_accept_task;
        struct cxnet_System_Net_Sockets_Socket *unbounded_accepted;
        struct cx_gc_root_handle *unbounded_client_root = cx_gc_add_root(
            &unbounded_client, (cx_uint)sizeof(unbounded_client));
        struct cx_gc_root_handle *unbounded_task_root;
        struct cx_gc_root_handle *unbounded_accept_root;
        struct cx_gc_root_handle *unbounded_accepted_root;
        cxnet_System_Net_Sockets_Socket___constructor__2(unbounded_client,
            cxnet_System_Net_AddressFamily_InterNetwork,
            cxnet_System_Net_Sockets_SocketType_Stream,
            cxnet_System_Net_Sockets_ProtocolType_Tcp);
        cxnet_System_Net_Sockets_Socket_ConnectTimeoutMilliseconds___set(
            unbounded_client, 0u);
        unbounded_accept_task = cxnet_System_Net_Sockets_Socket_AcceptAsync(listener);
        unbounded_accept_root = cx_gc_add_root(&unbounded_accept_task,
            (cx_uint)sizeof(unbounded_accept_task));
        unbounded_task = cxnet_System_Net_Sockets_Socket_ConnectAsync__3(
            unbounded_client,
            make_string("cxnet-test-resolution-delay.invalid"), port);
        unbounded_task_root = cx_gc_add_root(&unbounded_task,
            (cx_uint)sizeof(unbounded_task));
        cx_gc_collect();
        steps = 0u;
        while ((!cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(
                    unbounded_task) ||
                !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(
                    unbounded_accept_task)) && steps < 100000u) {
            if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                Sleep(1u);
#else
                usleep(1000u);
#endif
            }
            steps++;
        }
        if (!cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(
                unbounded_task) ||
            !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(
                unbounded_accept_task)) {
            fputs("disabled hostname connect deadline left tasks pending\n", stderr);
            return 20;
        }
        cxcore_System_Threading_Tasks_TaskHandle_GetResult(unbounded_task);
        unbounded_accepted = cx_generic_7c4851112331385bf3ec935e4daabb246ea0a9a28971c93643ab4b4268d85138(
            (const struct cxcore_System_Threading_Tasks_Task *)unbounded_accept_task);
        unbounded_accepted_root = cx_gc_add_root(&unbounded_accepted,
            (cx_uint)sizeof(unbounded_accepted));
        if (unbounded_accepted == NULL ||
            !cxnet_System_Net_Sockets_Socket_Connected___const_get(unbounded_client) ||
            !cxnet_System_Net_Sockets_Socket_Connected___const_get(unbounded_accepted)) {
            fputs("disabled hostname connect deadline did not allow the connection\n",
                stderr);
            return 20;
        }
        cxnet_System_Net_Sockets_Socket_Dispose(unbounded_accepted);
        cxnet_System_Net_Sockets_Socket_Dispose(unbounded_client);
        cx_gc_remove_root(unbounded_accepted_root);
        cx_gc_remove_root(unbounded_task_root);
        cx_gc_remove_root(unbounded_accept_root);
        cx_gc_remove_root(unbounded_client_root);
    }
    {
        struct cxnet_System_Net_Sockets_Socket *pre_canceled_client =
            (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
                cxnet_System_Net_Sockets_Socket___typeinfo.Size);
        struct cxcore_System_Threading_Tasks_CancellationTokenSource *pre_canceled_source =
            (struct cxcore_System_Threading_Tasks_CancellationTokenSource *)
                cxcore_System_Memory_Alloc(
                    cxcore_System_Threading_Tasks_CancellationTokenSource___typeinfo.Size);
        struct cxcore_System_Threading_Tasks_TaskHandle *pre_canceled_task;
        struct cxcore_System_Threading_Tasks_Task *pre_canceled_accept;
        struct cxnet_System_Net_Sockets_Socket *pre_canceled_accepted;
        struct cx_gc_root_handle *pre_canceled_client_root = cx_gc_add_root(
            &pre_canceled_client, (cx_uint)sizeof(pre_canceled_client));
        struct cx_gc_root_handle *pre_canceled_source_root = cx_gc_add_root(
            &pre_canceled_source, (cx_uint)sizeof(pre_canceled_source));
        struct cx_gc_root_handle *pre_canceled_task_root;
        struct cx_gc_root_handle *pre_canceled_accept_root;
        struct cx_gc_root_handle *pre_canceled_accepted_root;
        cxnet_System_Net_Sockets_Socket___constructor__2(pre_canceled_client,
            cxnet_System_Net_AddressFamily_InterNetwork,
            cxnet_System_Net_Sockets_SocketType_Stream,
            cxnet_System_Net_Sockets_ProtocolType_Tcp);
        cxcore_System_Threading_Tasks_CancellationTokenSource___constructor(
            pre_canceled_source);
        cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(
            pre_canceled_source);
        cancellation_token =
            cxcore_System_Threading_Tasks_CancellationTokenSource_Token___const_get(
                pre_canceled_source);
        pre_canceled_task = cxnet_System_Net_Sockets_Socket_ConnectAsync__4(
            pre_canceled_client,
            make_string("cxnet-test-resolution-delay.invalid"), port,
            cancellation_token);
        pre_canceled_task_root = cx_gc_add_root(&pre_canceled_task,
            (cx_uint)sizeof(pre_canceled_task));
        if (!cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(
                pre_canceled_task) ||
            !cxcore_System_Threading_Tasks_TaskHandle_IsCanceled___const_get(
                pre_canceled_task) ||
            cxnet_System_Net_Sockets_Socket_Connected___const_get(
                pre_canceled_client)) {
            fputs("pre-canceled hostname connect did not return a canceled task\n",
                stderr);
            return 21;
        }
        pre_canceled_accept = cxnet_System_Net_Sockets_Socket_AcceptAsync(listener);
        pre_canceled_accept_root = cx_gc_add_root(&pre_canceled_accept,
            (cx_uint)sizeof(pre_canceled_accept));
        pre_canceled_task = cxnet_System_Net_Sockets_Socket_ConnectAsync__5(
            pre_canceled_client, (struct cxnet_System_Net_EndPoint *)local_endpoint);
        steps = 0u;
        while ((!cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(
                    pre_canceled_task) ||
                !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(
                    pre_canceled_accept)) && steps < 100000u) {
            if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                Sleep(1u);
#else
                usleep(1000u);
#endif
            }
            steps++;
        }
        if (!cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(
                pre_canceled_task) ||
            !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(
                pre_canceled_accept)) {
            fputs("socket did not recover after pre-canceled hostname connect\n",
                stderr);
            return 21;
        }
        cxcore_System_Threading_Tasks_TaskHandle_GetResult(pre_canceled_task);
        pre_canceled_accepted = cx_generic_7c4851112331385bf3ec935e4daabb246ea0a9a28971c93643ab4b4268d85138(
            (const struct cxcore_System_Threading_Tasks_Task *)pre_canceled_accept);
        pre_canceled_accepted_root = cx_gc_add_root(&pre_canceled_accepted,
            (cx_uint)sizeof(pre_canceled_accepted));
        if (pre_canceled_accepted == NULL ||
            !cxnet_System_Net_Sockets_Socket_Connected___const_get(
                pre_canceled_client) ||
            !cxnet_System_Net_Sockets_Socket_Connected___const_get(
                pre_canceled_accepted)) {
            fputs("socket reuse after pre-canceled hostname connect failed\n",
                stderr);
            return 21;
        }
        cxnet_System_Net_Sockets_Socket_Dispose(pre_canceled_accepted);
        cxnet_System_Net_Sockets_Socket_Dispose(pre_canceled_client);
        cx_gc_remove_root(pre_canceled_accepted_root);
        cx_gc_remove_root(pre_canceled_task_root);
        cx_gc_remove_root(pre_canceled_accept_root);
        cx_gc_remove_root(pre_canceled_source_root);
        cx_gc_remove_root(pre_canceled_client_root);
    }
    {
        struct cxnet_System_Net_Sockets_Socket *cancel_dns_client =
            (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
                cxnet_System_Net_Sockets_Socket___typeinfo.Size);
        struct cxcore_System_Threading_Tasks_CancellationTokenSource *cancel_dns_source =
            (struct cxcore_System_Threading_Tasks_CancellationTokenSource *)
                cxcore_System_Memory_Alloc(
                    cxcore_System_Threading_Tasks_CancellationTokenSource___typeinfo.Size);
        struct cxcore_System_Threading_Tasks_TaskHandle *cancel_dns_task;
        struct cx_gc_root_handle *cancel_dns_client_root = cx_gc_add_root(
            &cancel_dns_client, (cx_uint)sizeof(cancel_dns_client));
        struct cx_gc_root_handle *cancel_dns_source_root = cx_gc_add_root(
            &cancel_dns_source, (cx_uint)sizeof(cancel_dns_source));
        struct cx_gc_root_handle *cancel_dns_task_root;
        cxnet_System_Net_Sockets_Socket___constructor__2(cancel_dns_client,
            cxnet_System_Net_AddressFamily_InterNetwork,
            cxnet_System_Net_Sockets_SocketType_Stream,
            cxnet_System_Net_Sockets_ProtocolType_Tcp);
        cxnet_System_Net_Sockets_Socket_ConnectTimeoutMilliseconds___set(
            cancel_dns_client, 500u);
        cxcore_System_Threading_Tasks_CancellationTokenSource___constructor(
            cancel_dns_source);
        cancellation_token =
            cxcore_System_Threading_Tasks_CancellationTokenSource_Token___const_get(
                cancel_dns_source);
        cancel_action.__function = cancel_token_on_scheduler;
        cancel_action.__environment = cancel_dns_source;
        cancel_action.__target = (cx_ptr)cancel_dns_source;
        cxcore_System_Threading_Tasks_TaskScheduler_PostAfter(10u, cancel_action);
        cancel_dns_task = cxnet_System_Net_Sockets_Socket_ConnectAsync__4(
            cancel_dns_client,
            make_string("cxnet-test-resolution-delay.invalid"), port,
            cancellation_token);
        cancel_dns_task_root = cx_gc_add_root(&cancel_dns_task,
            (cx_uint)sizeof(cancel_dns_task));
        cx_gc_collect();
        steps = 0u;
        while (!cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(
                cancel_dns_task) && steps < 1000u) {
            if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                Sleep(1u);
#else
                usleep(1000u);
#endif
            }
            steps++;
        }
        if (!cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(
                cancel_dns_task) ||
            !cxcore_System_Threading_Tasks_TaskHandle_IsCanceled___const_get(
                cancel_dns_task) ||
            cxnet_System_Net_Sockets_Socket_Connected___const_get(cancel_dns_client)) {
            fputs("caller cancellation during DNS was not preserved\n", stderr);
            return 19;
        }
        cxnet_System_Net_Sockets_Socket_Dispose(cancel_dns_client);
        cx_gc_remove_root(cancel_dns_task_root);
        cx_gc_remove_root(cancel_dns_source_root);
        cx_gc_remove_root(cancel_dns_client_root);
    }
    {
        struct cxnet_System_Net_Sockets_Socket *collision_socket =
            (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
                cxnet_System_Net_Sockets_Socket___typeinfo.Size);
        struct cxnet_System_Net_IPEndPoint *collision_endpoint =
            cxnet_System_Net_Sockets_Socket_GetLocalEndPoint(listener);
        struct cx_gc_root_handle *collision_socket_root = cx_gc_add_root(
            &collision_socket, (cx_uint)sizeof(collision_socket));
        struct cx_gc_root_handle *collision_endpoint_root = cx_gc_add_root(
            &collision_endpoint, (cx_uint)sizeof(collision_endpoint));
        struct cx_exception_frame frame;
        struct cxcore_System_Exception *exception;
        struct cxcore_System_String *expected_native_domain;
        struct cx_gc_root_handle *expected_native_domain_root;
        int expected_native_error;
#ifdef _WIN32
        expected_native_error = 10048;
        expected_native_domain = make_string("Winsock");
#else
        expected_native_error = 98;
        expected_native_domain = make_string("POSIX errno");
#endif
        expected_native_domain_root = cx_gc_add_root(&expected_native_domain,
            (cx_uint)sizeof(expected_native_domain));
        cxnet_System_Net_Sockets_Socket___constructor__2(collision_socket,
            cxnet_System_Net_AddressFamily_InterNetwork,
            cxnet_System_Net_Sockets_SocketType_Stream,
            cxnet_System_Net_Sockets_ProtocolType_Tcp);
        cx_exception_push(&frame);
        if (setjmp(frame.environment) == 0) {
            cxnet_System_Net_Sockets_Socket_Bind(collision_socket,
                (struct cxnet_System_Net_EndPoint *)collision_endpoint);
            cx_exception_pop(&frame);
            fputs("second loopback socket unexpectedly bound the occupied port\n",
                stderr);
            return 16;
        }
        exception = (struct cxcore_System_Exception *)cx_exception_current();
        if (!cxcore_System_Exception_Matches(exception,
                cxnet_System_Net_Sockets_SocketException___typeinfo) ||
            cxnet_System_Net_Sockets_SocketException_SocketErrorCode___const_get(
                (struct cxnet_System_Net_Sockets_SocketException *)exception) !=
                cxnet_System_Net_Sockets_SocketError_AddressAlreadyInUse ||
            cxcore_System_IO_IOException_NativeErrorCode___const_get(
                (struct cxcore_System_IO_IOException *)exception) !=
                expected_native_error ||
            cxcore_System_IO_IOException_NativeErrorDomain___const_get(
                (struct cxcore_System_IO_IOException *)exception) == NULL ||
            cxcore_System_IO_IOException_NativeErrorDomain___const_get(
                (struct cxcore_System_IO_IOException *)exception)->_length !=
                expected_native_domain->_length ||
            memcmp(cxcore_System_IO_IOException_NativeErrorDomain___const_get(
                    (struct cxcore_System_IO_IOException *)exception)->_data,
                expected_native_domain->_data,
                expected_native_domain->_length) != 0) {
            fprintf(stderr, "port collision identity: socket=%d native=%d domain=%.*s expected=%d/%.*s\n",
                cxnet_System_Net_Sockets_SocketException_SocketErrorCode___const_get(
                    (struct cxnet_System_Net_Sockets_SocketException *)exception),
                cxcore_System_IO_IOException_NativeErrorCode___const_get(
                    (struct cxcore_System_IO_IOException *)exception),
                cxcore_System_IO_IOException_NativeErrorDomain___const_get(
                    (struct cxcore_System_IO_IOException *)exception) == NULL ? 0 :
                    (int)cxcore_System_IO_IOException_NativeErrorDomain___const_get(
                        (struct cxcore_System_IO_IOException *)exception)->_length,
                cxcore_System_IO_IOException_NativeErrorDomain___const_get(
                    (struct cxcore_System_IO_IOException *)exception) == NULL ? "" :
                    (char *)cxcore_System_IO_IOException_NativeErrorDomain___const_get(
                        (struct cxcore_System_IO_IOException *)exception)->_data,
                expected_native_error, (int)expected_native_domain->_length,
                (char *)expected_native_domain->_data);
            cx_exception_pop(&frame);
            cx_exception_clear();
            fputs("loopback port collision lost its portable/native error identity\n",
                stderr);
            return 16;
        }
        cx_exception_pop(&frame);
        cx_exception_clear();
        cxnet_System_Net_Sockets_Socket_Dispose(collision_socket);
        cx_gc_remove_root(expected_native_domain_root);
        cx_gc_remove_root(collision_endpoint_root);
        cx_gc_remove_root(collision_socket_root);
    }
    cxnet_System_Net_Sockets_Socket___constructor__2(client,
        cxnet_System_Net_AddressFamily_InterNetwork,
        cxnet_System_Net_Sockets_SocketType_Stream,
        cxnet_System_Net_Sockets_ProtocolType_Tcp);
    accept_task = cxnet_System_Net_Sockets_Socket_AcceptAsync(listener);
    accept_root = cx_gc_add_root(&accept_task, (cx_uint)sizeof(accept_task));
    cx_gc_collect();
    connect_task = cxnet_System_Net_Sockets_Socket_ConnectAsync__5(
        client, (struct cxnet_System_Net_EndPoint *)local_endpoint);
    connect_root = cx_gc_add_root(&connect_task, (cx_uint)sizeof(connect_task));
    cx_gc_collect();
    steps = 0u;
    while ((!cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(
                connect_task) ||
            !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(accept_task)) &&
        steps < 100000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(connect_task)) {
        cxcore_System_Threading_Tasks_TaskHandle_GetResult(connect_task);
    }
    if (!cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(connect_task) ||
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(accept_task)) {
        fputs("endpoint ConnectAsync/AcceptAsync did not complete\n", stderr);
        return 4;
    }
    accepted = cx_generic_7c4851112331385bf3ec935e4daabb246ea0a9a28971c93643ab4b4268d85138(
        (const struct cxcore_System_Threading_Tasks_Task *)accept_task);
    if (accepted == NULL ||
        !cxnet_System_Net_Sockets_Socket_Connected___const_get(client) ||
        !cxnet_System_Net_Sockets_Socket_Connected___const_get(accepted)) {
        fputs("endpoint ConnectAsync/AcceptAsync returned disconnected sockets\n", stderr);
        return 5;
    }
    cxnet_System_Net_Sockets_Socket_Dispose(accepted);
    cxnet_System_Net_Sockets_Socket_Dispose(client);
    cx_gc_remove_root(accept_root);

    cancel_listener = (struct cxnet_System_Net_Sockets_Socket *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Sockets_Socket___typeinfo.Size);
    cancel_listener_root = cx_gc_add_root(&cancel_listener,
        (cx_uint)sizeof(cancel_listener));
    cxnet_System_Net_Sockets_Socket___constructor__2(cancel_listener,
        cxnet_System_Net_AddressFamily_InterNetwork,
        cxnet_System_Net_Sockets_SocketType_Stream,
        cxnet_System_Net_Sockets_ProtocolType_Tcp);
    {
        struct cxnet_System_Net_IPEndPoint *cancel_bind_endpoint =
            (struct cxnet_System_Net_IPEndPoint *)cxcore_System_Memory_Alloc(
                cxnet_System_Net_IPEndPoint___typeinfo.Size);
        cxnet_System_Net_IPEndPoint___constructor__2(cancel_bind_endpoint,
            cxnet_System_Net_IPAddress_Loopback___get(), 0);
        cxnet_System_Net_Sockets_Socket_Bind(cancel_listener,
            (struct cxnet_System_Net_EndPoint *)cancel_bind_endpoint);
        cxnet_System_Net_Sockets_Socket_Listen__2(cancel_listener, 4);
    }
    cancellation_source = (struct cxcore_System_Threading_Tasks_CancellationTokenSource *)
        cxcore_System_Memory_Alloc(
            cxcore_System_Threading_Tasks_CancellationTokenSource___typeinfo.Size);
    cxcore_System_Threading_Tasks_CancellationTokenSource___constructor(
        cancellation_source);
    cancellation_source_root = cx_gc_add_root(&cancellation_source,
        (cx_uint)sizeof(cancellation_source));
    cancellation_token =
        cxcore_System_Threading_Tasks_CancellationTokenSource_Token___const_get(
            cancellation_source);
    cancel_action.__function = cancel_token_on_scheduler;
    cancel_action.__environment = cancellation_source;
    cancel_action.__target = (cx_ptr)cancellation_source;
    accept_task = cxnet_System_Net_Sockets_Socket_AcceptAsync__2(
        cancel_listener, cancellation_token);
    accept_root = cx_gc_add_root(&accept_task, (cx_uint)sizeof(accept_task));
    if (cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(accept_task)) {
        fputs("fresh listener AcceptAsync did not remain pending\n", stderr);
        return 6;
    }
    cx_gc_collect();
    cxcore_System_Threading_Tasks_TaskScheduler_PostAfter(20u, cancel_action);
    steps = 0u;
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(accept_task) &&
        steps < 100000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(accept_task) ||
        !cxcore_System_Threading_Tasks_Task_IsCanceled___const_get(accept_task)) {
        fprintf(stderr, "pending accept cancellation completed=%d canceled=%d steps=%u\n",
            cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(accept_task),
            cxcore_System_Threading_Tasks_Task_IsCanceled___const_get(accept_task), steps);
        if (accept_task->_isFaulted && accept_task->_exception != NULL &&
            accept_task->_exception->_message != NULL) {
            fprintf(stderr, "pending accept fault: %.*s\n",
                (int)accept_task->_exception->_message->_length,
                (char *)accept_task->_exception->_message->_data);
        }
        fputs("pending AcceptAsync did not finish as canceled\n", stderr);
        return 6;
    }
    cx_gc_remove_root(accept_root);
    cx_gc_remove_root(cancellation_source_root);
    cxnet_System_Net_Sockets_Socket_Dispose(cancel_listener);
    cx_gc_remove_root(cancel_listener_root);

    {
        struct cxnet_System_Net_Sockets_Socket *race_client =
            (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
                cxnet_System_Net_Sockets_Socket___typeinfo.Size);
        struct cxnet_System_Net_IPEndPoint *race_endpoint =
            cxnet_System_Net_Sockets_Socket_GetLocalEndPoint(listener);
        struct cxcore_System_Threading_Tasks_CancellationTokenSource *race_source =
            (struct cxcore_System_Threading_Tasks_CancellationTokenSource *)
                cxcore_System_Memory_Alloc(
                    cxcore_System_Threading_Tasks_CancellationTokenSource___typeinfo.Size);
        struct cx_gc_root_handle *race_client_root = cx_gc_add_root(
            &race_client, (cx_uint)sizeof(race_client));
        struct cx_gc_root_handle *race_endpoint_root = cx_gc_add_root(
            &race_endpoint, (cx_uint)sizeof(race_endpoint));
        struct cx_gc_root_handle *race_source_root = cx_gc_add_root(
            &race_source, (cx_uint)sizeof(race_source));
        struct cx_gc_root_handle *race_accepted_root;
        cxnet_System_Net_Sockets_Socket___constructor__2(race_client,
            cxnet_System_Net_AddressFamily_InterNetwork,
            cxnet_System_Net_Sockets_SocketType_Stream,
            cxnet_System_Net_Sockets_ProtocolType_Tcp);
        cxcore_System_Threading_Tasks_CancellationTokenSource___constructor(
            race_source);
        cancellation_token =
            cxcore_System_Threading_Tasks_CancellationTokenSource_Token___const_get(
                race_source);
        accept_task = cxnet_System_Net_Sockets_Socket_AcceptAsync__2(
            listener, cancellation_token);
        accept_root = cx_gc_add_root(&accept_task, (cx_uint)sizeof(accept_task));
        if (cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(accept_task)) {
            fputs("race AcceptAsync completed before a client connected\n", stderr);
            return 13;
        }
        cx_gc_collect();
        cxnet_System_Net_Sockets_Socket_Connect(race_client,
            (struct cxnet_System_Net_EndPoint *)race_endpoint);
        cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(race_source);
        steps = 0u;
        while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(accept_task) &&
            steps < 100000u) {
            if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                Sleep(1u);
#else
                usleep(1000u);
#endif
            }
            steps++;
        }
        if (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(accept_task) ||
            !cxcore_System_Threading_Tasks_Task_IsCanceled___const_get(accept_task)) {
            fputs("ready AcceptAsync did not honor cancellation\n", stderr);
            return 13;
        }
        cx_gc_remove_root(accept_root);
        accept_task = cxnet_System_Net_Sockets_Socket_AcceptAsync(listener);
        accept_root = cx_gc_add_root(&accept_task, (cx_uint)sizeof(accept_task));
        if (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(accept_task) ||
            cxcore_System_Threading_Tasks_Task_IsCanceled___const_get(accept_task) ||
            accept_task->_isFaulted) {
            fputs("fresh AcceptAsync did not recover the queued connection\n", stderr);
            return 13;
        }
        accepted = cx_generic_7c4851112331385bf3ec935e4daabb246ea0a9a28971c93643ab4b4268d85138(
            (const struct cxcore_System_Threading_Tasks_Task *)accept_task);
        race_accepted_root = cx_gc_add_root(&accepted, (cx_uint)sizeof(accepted));
        if (accepted == NULL ||
            !cxnet_System_Net_Sockets_Socket_Connected___const_get(accepted) ||
            !cxnet_System_Net_Sockets_Socket_Connected___const_get(race_client)) {
            fputs("fresh AcceptAsync returned no connected queued client\n", stderr);
            return 13;
        }
        cxnet_System_Net_Sockets_Socket_Dispose(accepted);
        cxnet_System_Net_Sockets_Socket_Dispose(race_client);
        cx_gc_remove_root(race_accepted_root);
        cx_gc_remove_root(accept_root);
        cx_gc_remove_root(race_source_root);
        cx_gc_remove_root(race_endpoint_root);
        cx_gc_remove_root(race_client_root);
    }

    {
        struct cxnet_System_Net_Sockets_Socket *retry_client =
            (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
                cxnet_System_Net_Sockets_Socket___typeinfo.Size);
        struct cxnet_System_Net_IPEndPoint *retry_endpoint =
            cxnet_System_Net_Sockets_Socket_GetLocalEndPoint(listener);
        struct cxcore_System_Threading_Tasks_CancellationTokenSource *retry_source =
            (struct cxcore_System_Threading_Tasks_CancellationTokenSource *)
                cxcore_System_Memory_Alloc(
                    cxcore_System_Threading_Tasks_CancellationTokenSource___typeinfo.Size);
        struct cxcore_System_Threading_Tasks_TaskHandle *retry_connect_task;
        struct cxcore_System_Threading_Tasks_Task *retry_accept_task;
        struct cxnet_System_Net_Sockets_Socket *retry_accepted;
        struct cx_gc_root_handle *retry_client_root = cx_gc_add_root(
            &retry_client, (cx_uint)sizeof(retry_client));
        struct cx_gc_root_handle *retry_endpoint_root = cx_gc_add_root(
            &retry_endpoint, (cx_uint)sizeof(retry_endpoint));
        struct cx_gc_root_handle *retry_source_root = cx_gc_add_root(
            &retry_source, (cx_uint)sizeof(retry_source));
        struct cx_gc_root_handle *retry_connect_root;
        struct cx_gc_root_handle *retry_accept_root;
        struct cx_gc_root_handle *retry_accepted_root;
        struct cxcore_System_Threading_Tasks_CancellationToken retry_token;
        cxnet_System_Net_Sockets_Socket___constructor__2(retry_client,
            cxnet_System_Net_AddressFamily_InterNetwork,
            cxnet_System_Net_Sockets_SocketType_Stream,
            cxnet_System_Net_Sockets_ProtocolType_Tcp);
        cxcore_System_Threading_Tasks_CancellationTokenSource___constructor(
            retry_source);
        cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(retry_source);
        retry_token =
            cxcore_System_Threading_Tasks_CancellationTokenSource_Token___const_get(
                retry_source);
        retry_connect_task = cxnet_System_Net_Sockets_Socket_ConnectAsync__6(
            retry_client, (struct cxnet_System_Net_EndPoint *)retry_endpoint,
            retry_token);
        retry_connect_root = cx_gc_add_root(&retry_connect_task,
            (cx_uint)sizeof(retry_connect_task));
        if (!cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(
                retry_connect_task) ||
            !cxcore_System_Threading_Tasks_TaskHandle_IsCanceled___const_get(
                retry_connect_task) ||
            cxnet_System_Net_Sockets_Socket_Connected___const_get(retry_client)) {
            fputs("pre-canceled ConnectAsync did not leave the socket reusable\n",
                stderr);
            return 14;
        }
        cx_gc_remove_root(retry_connect_root);
        retry_accept_task = cxnet_System_Net_Sockets_Socket_AcceptAsync(listener);
        retry_accept_root = cx_gc_add_root(&retry_accept_task,
            (cx_uint)sizeof(retry_accept_task));
        retry_connect_task = cxnet_System_Net_Sockets_Socket_ConnectAsync__5(
            retry_client, (struct cxnet_System_Net_EndPoint *)retry_endpoint);
        retry_connect_root = cx_gc_add_root(&retry_connect_task,
            (cx_uint)sizeof(retry_connect_task));
        cx_gc_collect();
        steps = 0u;
        while ((!cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(
                    retry_connect_task) ||
                !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(
                    retry_accept_task)) && steps < 100000u) {
            if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                Sleep(1u);
#else
                usleep(1000u);
#endif
            }
            steps++;
        }
        if (cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(
                retry_connect_task)) {
            cxcore_System_Threading_Tasks_TaskHandle_GetResult(retry_connect_task);
        }
        if (!cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(
                retry_connect_task) ||
            !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(
                retry_accept_task)) {
            fputs("ConnectAsync did not recover after pre-cancellation\n", stderr);
            return 14;
        }
        retry_accepted = cx_generic_7c4851112331385bf3ec935e4daabb246ea0a9a28971c93643ab4b4268d85138(
            (const struct cxcore_System_Threading_Tasks_Task *)retry_accept_task);
        retry_accepted_root = cx_gc_add_root(&retry_accepted,
            (cx_uint)sizeof(retry_accepted));
        if (retry_accepted == NULL ||
            !cxnet_System_Net_Sockets_Socket_Connected___const_get(retry_client) ||
            !cxnet_System_Net_Sockets_Socket_Connected___const_get(retry_accepted)) {
            fputs("recovered ConnectAsync did not establish its connection\n", stderr);
            return 14;
        }
        cxnet_System_Net_Sockets_Socket_Dispose(retry_accepted);
        cxnet_System_Net_Sockets_Socket_Dispose(retry_client);
        cx_gc_remove_root(retry_accepted_root);
        cx_gc_remove_root(retry_connect_root);
        cx_gc_remove_root(retry_accept_root);
        cx_gc_remove_root(retry_source_root);
        cx_gc_remove_root(retry_endpoint_root);
        cx_gc_remove_root(retry_client_root);
    }

    {
        struct cxnet_System_Net_Sockets_Socket *race_listener =
            (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
                cxnet_System_Net_Sockets_Socket___typeinfo.Size);
        struct cxnet_System_Net_Sockets_Socket *race_client =
            (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
                cxnet_System_Net_Sockets_Socket___typeinfo.Size);
        struct cxnet_System_Net_IPEndPoint *race_endpoint =
            (struct cxnet_System_Net_IPEndPoint *)cxcore_System_Memory_Alloc(
                cxnet_System_Net_IPEndPoint___typeinfo.Size);
        struct cxcore_System_Threading_Tasks_CancellationTokenSource *race_source =
            (struct cxcore_System_Threading_Tasks_CancellationTokenSource *)
                cxcore_System_Memory_Alloc(
                    cxcore_System_Threading_Tasks_CancellationTokenSource___typeinfo.Size);
        struct cxcore_System_Threading_Tasks_CancellationToken race_token;
        struct cxcore_System_Threading_Tasks_TaskHandle *race_connect_task;
        struct cxcore_System_Threading_Tasks_Task *race_accept_task;
        struct cxnet_System_Net_Sockets_Socket *race_accepted = NULL;
        struct cx_gc_root_handle *race_listener_root = cx_gc_add_root(
            &race_listener, (cx_uint)sizeof(race_listener));
        struct cx_gc_root_handle *race_client_root = cx_gc_add_root(
            &race_client, (cx_uint)sizeof(race_client));
        struct cx_gc_root_handle *race_endpoint_root = cx_gc_add_root(
            &race_endpoint, (cx_uint)sizeof(race_endpoint));
        struct cx_gc_root_handle *race_source_root = cx_gc_add_root(
            &race_source, (cx_uint)sizeof(race_source));
        struct cx_gc_root_handle *race_connect_root;
        struct cx_gc_root_handle *race_accept_root;
        struct cx_gc_root_handle *race_accepted_root = NULL;
        cxnet_System_Net_Sockets_Socket___constructor__2(race_listener,
            cxnet_System_Net_AddressFamily_InterNetwork,
            cxnet_System_Net_Sockets_SocketType_Stream,
            cxnet_System_Net_Sockets_ProtocolType_Tcp);
        cxnet_System_Net_IPEndPoint___constructor__2(race_endpoint,
            cxnet_System_Net_IPAddress_Loopback___get(), 0);
        cxnet_System_Net_Sockets_Socket_Bind(race_listener,
            (struct cxnet_System_Net_EndPoint *)race_endpoint);
        cxnet_System_Net_Sockets_Socket_Listen__2(race_listener, 4);
        race_endpoint = cxnet_System_Net_Sockets_Socket_GetLocalEndPoint(
            race_listener);
        cxnet_System_Net_Sockets_Socket___constructor__2(race_client,
            cxnet_System_Net_AddressFamily_InterNetwork,
            cxnet_System_Net_Sockets_SocketType_Stream,
            cxnet_System_Net_Sockets_ProtocolType_Tcp);
        cxcore_System_Threading_Tasks_CancellationTokenSource___constructor(
            race_source);
        race_token =
            cxcore_System_Threading_Tasks_CancellationTokenSource_Token___const_get(
                race_source);
        race_accept_task = cxnet_System_Net_Sockets_Socket_AcceptAsync(
            race_listener);
        race_accept_root = cx_gc_add_root(&race_accept_task,
            (cx_uint)sizeof(race_accept_task));
        race_connect_task = cxnet_System_Net_Sockets_Socket_ConnectAsync__6(
            race_client, (struct cxnet_System_Net_EndPoint *)race_endpoint,
            race_token);
        race_connect_root = cx_gc_add_root(&race_connect_task,
            (cx_uint)sizeof(race_connect_task));
        if (cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(
                race_connect_task)) {
            fputs("loopback ConnectAsync did not remain pending for cancellation\n",
                stderr);
            return 15;
        }
        cx_gc_collect();
        cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(race_source);
        steps = 0u;
        while (!cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(
                race_connect_task) && steps < 100000u) {
            if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                Sleep(1u);
#else
                usleep(1000u);
#endif
            }
            steps++;
        }
        if (!cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(
                race_connect_task) ||
            !cxcore_System_Threading_Tasks_TaskHandle_IsCanceled___const_get(
                race_connect_task) ||
            cxnet_System_Net_Sockets_Socket_Connected___const_get(race_client)) {
            struct cxcore_System_String *fault_message =
                race_connect_task->_state->_exception == NULL ? NULL :
                cxcore_System_Exception_Message___const_get(
                    race_connect_task->_state->_exception);
            struct cxcore_System_Reflection_TypeInfo *fault_type =
                race_connect_task->_state->_exception == NULL ? NULL :
                &CX_GET_TYPEINFO(race_connect_task->_state->_exception);
            const struct cxcore_System_String *fault_type_name =
                fault_type == NULL ? NULL : fault_type->Name;
            fprintf(stderr,
                "pending ConnectAsync did not honor pre-dispatch cancellation completed=%d canceled=%d faulted=%d connected=%d callbacks=%u steps=%u exception=%.*s type=%.*s\n",
                cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(
                    race_connect_task),
                cxcore_System_Threading_Tasks_TaskHandle_IsCanceled___const_get(
                    race_connect_task),
                race_connect_task->_state->_isFaulted,
                cxnet_System_Net_Sockets_Socket_Connected___const_get(race_client),
                (unsigned)connect_race_completion_count,
                (unsigned)steps,
                fault_message == NULL ? 0 : (int)fault_message->_length,
                fault_message == NULL ? "" : (const char *)fault_message->_data,
                fault_type_name == NULL ? 0 : (int)fault_type_name->_length,
                fault_type_name == NULL ? "" :
                    (const char *)fault_type_name->_data);
            return 15;
        }
        cxnet_System_Net_Sockets_Socket_Dispose(race_listener);
        steps = 0u;
        while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(
                race_accept_task) && steps < 100000u) {
            if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                Sleep(1u);
#else
                usleep(1000u);
#endif
            }
            steps++;
        }
        if (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(
                race_accept_task)) {
            fputs("race listener disposal left AcceptAsync pending\n", stderr);
            return 15;
        }
        if (race_accepted == NULL && !race_accept_task->_isFaulted &&
            !race_accept_task->_isCanceled) {
            race_accepted = cx_generic_7c4851112331385bf3ec935e4daabb246ea0a9a28971c93643ab4b4268d85138(
                (const struct cxcore_System_Threading_Tasks_Task *)race_accept_task);
            race_accepted_root = cx_gc_add_root(&race_accepted,
                (cx_uint)sizeof(race_accepted));
            if (race_accepted != NULL) {
                cxnet_System_Net_Sockets_Socket_Dispose(race_accepted);
            }
            cx_gc_remove_root(race_accepted_root);
            race_accepted_root = NULL;
        }
        {
            struct cxnet_System_Net_IPEndPoint *retry_endpoint =
                cxnet_System_Net_Sockets_Socket_GetLocalEndPoint(listener);
            struct cx_gc_root_handle *retry_endpoint_root = cx_gc_add_root(
                &retry_endpoint, (cx_uint)sizeof(retry_endpoint));
            race_accept_task = cxnet_System_Net_Sockets_Socket_AcceptAsync(
                listener);
            race_connect_task = cxnet_System_Net_Sockets_Socket_ConnectAsync__5(
                race_client, (struct cxnet_System_Net_EndPoint *)retry_endpoint);
            cx_gc_collect();
            steps = 0u;
            while ((!cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(
                        race_connect_task) ||
                    !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(
                        race_accept_task)) && steps < 100000u) {
                if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                    Sleep(1u);
#else
                    usleep(1000u);
#endif
                }
                steps++;
            }
            if (cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(
                    race_connect_task)) {
                cxcore_System_Threading_Tasks_TaskHandle_GetResult(race_connect_task);
            }
            if (!cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(
                    race_connect_task) ||
                !cxnet_System_Net_Sockets_Socket_Connected___const_get(race_client) ||
                !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(
                    race_accept_task)) {
                fputs("socket was not reusable after pending connect cancellation\n",
                    stderr);
                return 15;
            }
            race_accepted = cx_generic_7c4851112331385bf3ec935e4daabb246ea0a9a28971c93643ab4b4268d85138(
                (const struct cxcore_System_Threading_Tasks_Task *)race_accept_task);
            race_accepted_root = cx_gc_add_root(&race_accepted,
                (cx_uint)sizeof(race_accepted));
            if (race_accepted == NULL ||
                !cxnet_System_Net_Sockets_Socket_Connected___const_get(
                    race_accepted)) {
                fputs("reused ConnectAsync returned no accepted socket\n", stderr);
                return 15;
            }
            cxnet_System_Net_Sockets_Socket_Dispose(race_accepted);
            cx_gc_remove_root(race_accepted_root);
            cx_gc_remove_root(retry_endpoint_root);
        }
        cxnet_System_Net_Sockets_Socket_Dispose(race_client);
        cx_gc_remove_root(race_connect_root);
        cx_gc_remove_root(race_accept_root);
        cx_gc_remove_root(race_source_root);
        cx_gc_remove_root(race_endpoint_root);
        cx_gc_remove_root(race_client_root);
        cx_gc_remove_root(race_listener_root);
    }

    for (uint32_t race_index = 0u;
         race_index < sizeof(connect_cancel_delays_ms) /
             sizeof(connect_cancel_delays_ms[0]);
         race_index += 1u) {
        if (!test_connect_cancellation_race(
                connect_cancel_delays_ms[race_index])) {
            return 16;
        }
    }

    for (uint32_t race_index = 0u;
         race_index < sizeof(connect_cancel_delays_ms) /
             sizeof(connect_cancel_delays_ms[0]);
         race_index += 1u) {
        if (!test_accept_cancellation_race(
                connect_cancel_delays_ms[race_index])) {
            return 18;
        }
    }

    {
        struct cxnet_System_Net_Sockets_Socket *disposed_listener =
            (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
                cxnet_System_Net_Sockets_Socket___typeinfo.Size);
        struct cxnet_System_Net_IPEndPoint *disposed_bind_endpoint =
            (struct cxnet_System_Net_IPEndPoint *)cxcore_System_Memory_Alloc(
                cxnet_System_Net_IPEndPoint___typeinfo.Size);
        struct cxcore_System_Threading_Tasks_Task *disposed_accept_task;
        struct cx_gc_root_handle *disposed_listener_root =
            cx_gc_add_root(&disposed_listener, (cx_uint)sizeof(disposed_listener));
        struct cx_gc_root_handle *disposed_accept_root;
        cxnet_System_Net_Sockets_Socket___constructor__2(disposed_listener,
            cxnet_System_Net_AddressFamily_InterNetwork,
            cxnet_System_Net_Sockets_SocketType_Stream,
            cxnet_System_Net_Sockets_ProtocolType_Tcp);
        cxnet_System_Net_IPEndPoint___constructor__2(disposed_bind_endpoint,
            cxnet_System_Net_IPAddress_Loopback___get(), 0);
        cxnet_System_Net_Sockets_Socket_Bind(disposed_listener,
            (struct cxnet_System_Net_EndPoint *)disposed_bind_endpoint);
        cxnet_System_Net_Sockets_Socket_Listen__2(disposed_listener, 4);
        disposed_accept_task = cxnet_System_Net_Sockets_Socket_AcceptAsync(
            disposed_listener);
        disposed_accept_root = cx_gc_add_root(&disposed_accept_task,
            (cx_uint)sizeof(disposed_accept_task));
        if (cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(
                disposed_accept_task)) {
            fputs("fresh listener AcceptAsync completed before disposal\n", stderr);
            return 9;
        }
        cx_gc_collect();
        cxnet_System_Net_Sockets_Socket_Dispose(disposed_listener);
        cxnet_System_Net_Sockets_Socket_Dispose(disposed_listener);
        steps = 0u;
        while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(
                disposed_accept_task) && steps < 100000u) {
            if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                Sleep(1u);
#else
                usleep(1000u);
#endif
            }
            steps++;
        }
        if (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(
                disposed_accept_task) || !disposed_accept_task->_isFaulted ||
            disposed_accept_task->_isCanceled) {
            fputs("disposing a listener did not fault its pending AcceptAsync\n",
                stderr);
            return 9;
        }
        cx_gc_remove_root(disposed_accept_root);
        cx_gc_remove_root(disposed_listener_root);
    }

    {
        struct cxnet_System_Net_Sockets_Socket *closing_client =
            (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
                cxnet_System_Net_Sockets_Socket___typeinfo.Size);
        struct cxcore_System_Threading_Tasks_TaskHandle *closing_task;
        struct cx_gc_root_handle *closing_client_root =
            cx_gc_add_root(&closing_client, (cx_uint)sizeof(closing_client));
        struct cx_gc_root_handle *closing_task_root;
        cx_bool was_pending;
        cxnet_System_Net_Sockets_Socket___constructor__2(closing_client,
            cxnet_System_Net_AddressFamily_InterNetwork,
            cxnet_System_Net_Sockets_SocketType_Stream,
            cxnet_System_Net_Sockets_ProtocolType_Tcp);
        closing_task = cxnet_System_Net_Sockets_Socket_ConnectAsync__5(
            closing_client, (struct cxnet_System_Net_EndPoint *)local_endpoint);
        closing_task_root = cx_gc_add_root(&closing_task,
            (cx_uint)sizeof(closing_task));
        was_pending = !cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(
            closing_task);
        cxnet_System_Net_Sockets_Socket_Dispose(closing_client);
        steps = 0u;
        while (!cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(
                closing_task) && steps < 100000u) {
            if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                Sleep(1u);
#else
                usleep(1000u);
#endif
            }
            steps++;
        }
        if (!cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(
                closing_task) ||
            cxnet_System_Net_Sockets_Socket_Connected___const_get(closing_client) ||
            (was_pending && (!closing_task->_state->_isFaulted ||
                !cxcore_System_Exception_Matches(
                    closing_task->_state->_exception,
                    cxcore_System_ObjectDisposedException___typeinfo)))) {
            fputs("disposing during ConnectAsync did not finish cleanly\n", stderr);
            return 7;
        }
        cxnet_System_Net_Sockets_Socket_Dispose(closing_client);
        cx_gc_remove_root(closing_task_root);
        cx_gc_remove_root(closing_client_root);
    }

    client = (struct cxnet_System_Net_Sockets_Socket *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Sockets_Socket___typeinfo.Size);
    cxnet_System_Net_Sockets_Socket___constructor__2(client,
        cxnet_System_Net_AddressFamily_InterNetwork,
        cxnet_System_Net_Sockets_SocketType_Stream,
        cxnet_System_Net_Sockets_ProtocolType_Tcp);
    connect_task = cxnet_System_Net_Sockets_Socket_ConnectAsync__3(
        client, make_string("localhost"), (cx_int)port);
    steps = 0u;
    while (!cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(
            connect_task) && steps < 100000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(
            connect_task)) {
        cxcore_System_Threading_Tasks_TaskHandle_GetResult(connect_task);
    }
    if (!cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(
            connect_task) ||
        !cxnet_System_Net_Sockets_Socket_Connected___const_get(client)) {
        fputs("hostname Socket.ConnectAsync did not connect to localhost\n", stderr);
        return 4;
    }
    {
        struct cxnet_System_Net_Sockets_Socket *accepted =
            cxnet_System_Net_Sockets_Socket_Accept(listener);
        if (accepted == NULL ||
            !cxnet_System_Net_Sockets_Socket_Connected___const_get(accepted)) {
            fputs("localhost ConnectAsync was not accepted\n", stderr);
            return 4;
        }
        cxnet_System_Net_Sockets_Socket_Dispose(accepted);
    }
    cxnet_System_Net_Sockets_Socket_Dispose(client);
    if (!test_hostname_connect_throwing_completion(listener, port)) {
        fputs("hostname connect completion callback failure skipped cleanup\n",
            stderr);
        return 12;
    }
    if (!test_hostname_connect_dispose_during_resolution()) {
        fputs("disposing hostname connect during DNS skipped cleanup or accepted a late result\n",
            stderr);
        return 13;
    }
    if (!test_socket_dispose_reentrant_throwing_completion()) {
        fputs("socket disposal failed to contain a reentrant throwing completion\n",
            stderr);
        return 20;
    }

    {
        struct cxcore_System_Threading_Tasks_TaskHandle *fallback_task;
        struct cxnet_System_Net_Sockets_Socket *fallback_accepted;
        struct cx_gc_root_handle *fallback_task_root;
        struct cx_gc_root_handle *fallback_accepted_root;
        client = (struct cxnet_System_Net_Sockets_Socket *)
            cxcore_System_Memory_Alloc(
                cxnet_System_Net_Sockets_Socket___typeinfo.Size);
        cxnet_System_Net_Sockets_Socket___constructor__2(client,
            cxnet_System_Net_AddressFamily_InterNetwork,
            cxnet_System_Net_Sockets_SocketType_Stream,
            cxnet_System_Net_Sockets_ProtocolType_Tcp);
        fallback_task = cxnet_System_Net_Sockets_Socket_ConnectAsync__3(
            client, make_string("cxnet-test-resolution-fallback.invalid"),
            (cx_int)port);
        fallback_task_root = cx_gc_add_root(&fallback_task,
            (cx_uint)sizeof(fallback_task));
        cx_gc_collect();
        steps = 0u;
        while (!cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(
                fallback_task) && steps < 100000u) {
            if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                Sleep(1u);
#else
                usleep(1000u);
#endif
            }
            steps++;
        }
        if (!cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(
                fallback_task) || fallback_task->_state->_isFaulted ||
            cxcore_System_Threading_Tasks_TaskHandle_IsCanceled___const_get(
                fallback_task) ||
            !cxnet_System_Net_Sockets_Socket_Connected___const_get(client)) {
            fputs("hostname ConnectAsync did not fall back to its second address\n",
                stderr);
            return 4;
        }
        cxcore_System_Threading_Tasks_TaskHandle_GetResult(fallback_task);
        fallback_accepted = cxnet_System_Net_Sockets_Socket_Accept(listener);
        fallback_accepted_root = cx_gc_add_root(&fallback_accepted,
            (cx_uint)sizeof(fallback_accepted));
        if (fallback_accepted == NULL ||
            !cxnet_System_Net_Sockets_Socket_Connected___const_get(
                fallback_accepted)) {
            fputs("fallback hostname connection was not accepted\n", stderr);
            return 4;
        }
        cxnet_System_Net_Sockets_Socket_Dispose(fallback_accepted);
        cxnet_System_Net_Sockets_Socket_Dispose(client);
        cx_gc_remove_root(fallback_accepted_root);
        cx_gc_remove_root(fallback_task_root);
    }
    cxnet_System_Net_Sockets_Socket_Dispose(listener);
    if (!test_endpoint_connect_failure_reuse(port)) return 16;
    if (!run_endpoint_connect_duplicate_pending_only()) return 54;
    if (!test_endpoint_connect_reset_option_race(port, 0, 5)) return 17;

    {
        uint32_t attempt;
        cxnet_System_Net_Sockets_SocketError expected_socket_error;
        int expected_native_error;
#ifdef _WIN32
        expected_socket_error = cxnet_System_Net_Sockets_SocketError_HostNotFound;
        expected_native_error = 11001;
#else
        expected_socket_error = cxnet_System_Net_Sockets_SocketError_TryAgain;
        expected_native_error = -3;
#endif
        for (attempt = 0u; attempt < 5u; attempt++) {
            struct cxnet_System_Net_Sockets_Socket *resolver_client =
                (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
                    cxnet_System_Net_Sockets_Socket___typeinfo.Size);
            struct cxcore_System_Threading_Tasks_TaskHandle *resolver_task;
            struct cxcore_System_Exception *resolver_exception;
            struct cx_gc_root_handle *resolver_client_root = cx_gc_add_root(
                &resolver_client, (cx_uint)sizeof(resolver_client));
            struct cx_gc_root_handle *resolver_task_root;
            cx_action resolver_completion_action = {0};
            cxnet_System_Net_Sockets_Socket___constructor__2(resolver_client,
                cxnet_System_Net_AddressFamily_InterNetwork,
                cxnet_System_Net_Sockets_SocketType_Stream,
                cxnet_System_Net_Sockets_ProtocolType_Tcp);
            resolver_task = cxnet_System_Net_Sockets_Socket_ConnectAsync__3(
                resolver_client,
                make_string("cxnet-test-resolution-failure.invalid"), 80);
            resolver_task_root = cx_gc_add_root(&resolver_task,
                (cx_uint)sizeof(resolver_task));
            connect_race_completion_count = 0u;
            resolver_completion_action.__function = on_connect_race_completed;
            resolver_completion_action.__environment = NULL;
            resolver_completion_action.__target = NULL;
            cxcore_System_Threading_Tasks_TaskHandle_OnCompleted(resolver_task,
                resolver_completion_action);
            cx_gc_collect();
            steps = 0u;
            while ((!cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(
                        resolver_task) || connect_race_completion_count == 0u) &&
                    steps < 100000u) {
                if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                    Sleep(1u);
#else
                    usleep(1000u);
#endif
                }
                steps++;
            }
            resolver_exception = cxcore_System_Threading_Tasks_TaskState_Exception___const_get(
                resolver_task->_state);
            if (!cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(
                    resolver_task) || connect_race_completion_count != 1u ||
                !cxcore_System_Threading_Tasks_TaskState_IsFaulted___const_get(
                    resolver_task->_state) ||
                cxcore_System_Threading_Tasks_TaskState_IsCanceled___const_get(
                    resolver_task->_state) ||
                !cxcore_System_Exception_Matches(resolver_exception,
                    cxnet_System_Net_Sockets_SocketException___typeinfo) ||
                cxnet_System_Net_Sockets_SocketException_SocketErrorCode___const_get(
                    (struct cxnet_System_Net_Sockets_SocketException *)resolver_exception) !=
                    expected_socket_error ||
                cxcore_System_IO_IOException_NativeErrorCode___const_get(
                    (struct cxcore_System_IO_IOException *)resolver_exception) !=
                    expected_native_error ||
                cxcore_System_IO_IOException_NativeErrorDomain___const_get(
                    (struct cxcore_System_IO_IOException *)resolver_exception) == NULL ||
                cxcore_System_IO_IOException_NativeErrorDomain___const_get(
                    (struct cxcore_System_IO_IOException *)resolver_exception)->_length != 3u ||
                memcmp(cxcore_System_IO_IOException_NativeErrorDomain___const_get(
                        (struct cxcore_System_IO_IOException *)resolver_exception)->_data,
                    "dns", 3u) != 0) {
                fputs("async hostname connect lost resolver error identity or completed more than once\n",
                    stderr);
                cxnet_System_Net_Sockets_Socket_Dispose(resolver_client);
                cx_gc_remove_root(resolver_task_root);
                cx_gc_remove_root(resolver_client_root);
                return 10;
            }
            cxnet_System_Net_Sockets_Socket_Dispose(resolver_client);
            cx_gc_remove_root(resolver_task_root);
            cx_gc_remove_root(resolver_client_root);
        }
    }

    {
        uint32_t attempt;
        for (attempt = 0u; attempt < 5u; attempt++) {
            struct cxnet_System_Net_Sockets_Socket *refused_client =
                (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
                    cxnet_System_Net_Sockets_Socket___typeinfo.Size);
            struct cxnet_System_Net_IPEndPoint *refused_endpoint =
                (struct cxnet_System_Net_IPEndPoint *)cxcore_System_Memory_Alloc(
                    cxnet_System_Net_IPEndPoint___typeinfo.Size);
            struct cxcore_System_Threading_Tasks_TaskHandle *refused_task;
            struct cx_gc_root_handle *refused_client_root = cx_gc_add_root(
                &refused_client, (cx_uint)sizeof(refused_client));
            struct cx_gc_root_handle *refused_endpoint_root = cx_gc_add_root(
                &refused_endpoint, (cx_uint)sizeof(refused_endpoint));
            struct cx_gc_root_handle *refused_task_root;
            cx_action refused_completion_action = {0};
            cxnet_System_Net_Sockets_Socket___constructor__2(refused_client,
                cxnet_System_Net_AddressFamily_InterNetwork,
                cxnet_System_Net_Sockets_SocketType_Stream,
                cxnet_System_Net_Sockets_ProtocolType_Tcp);
            cxnet_System_Net_IPEndPoint___constructor__2(refused_endpoint,
                cxnet_System_Net_IPAddress_Loopback___get(), (cx_int)port);
            refused_task = cxnet_System_Net_Sockets_Socket_ConnectAsync__5(
                refused_client, (struct cxnet_System_Net_EndPoint *)refused_endpoint);
            refused_task_root = cx_gc_add_root(&refused_task,
                (cx_uint)sizeof(refused_task));
            connect_race_completion_count = 0u;
            refused_completion_action.__function = on_connect_race_completed;
            refused_completion_action.__environment = NULL;
            refused_completion_action.__target = NULL;
            cxcore_System_Threading_Tasks_TaskHandle_OnCompleted(refused_task,
                refused_completion_action);
            cx_gc_collect();
            steps = 0u;
            while ((!cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(
                        refused_task) || connect_race_completion_count == 0u) &&
                    steps < 100000u) {
                if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                    Sleep(1u);
#else
                    usleep(1000u);
#endif
                }
                steps++;
            }
            if (!cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(
                    refused_task) || connect_race_completion_count != 1u ||
                !refused_task->_state->_isFaulted || refused_task->_state->_isCanceled ||
                cxnet_System_Net_Sockets_Socket_Connected___const_get(refused_client) ||
                !cxcore_System_Exception_Matches(refused_task->_state->_exception,
                    cxnet_System_Net_Sockets_SocketException___typeinfo) ||
                cxnet_System_Net_Sockets_SocketException_SocketErrorCode___const_get(
                    (const struct cxnet_System_Net_Sockets_SocketException *)
                        refused_task->_state->_exception) !=
                    cxnet_System_Net_Sockets_SocketError_ConnectionRefused) {
                fprintf(stderr,
                    "ConnectAsync refused-port failure attempt=%u completed=%d faulted=%d canceled=%d completionCount=%u connected=%d exceptionType=%d socketError=%d steps=%u\n",
                    attempt,
                    cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(
                        refused_task),
                    refused_task->_state->_isFaulted,
                    refused_task->_state->_isCanceled,
                    connect_race_completion_count,
                    cxnet_System_Net_Sockets_Socket_Connected___const_get(
                        refused_client),
                    cxcore_System_Exception_Matches(refused_task->_state->_exception,
                        cxnet_System_Net_Sockets_SocketException___typeinfo),
                    cxcore_System_Exception_Matches(refused_task->_state->_exception,
                        cxnet_System_Net_Sockets_SocketException___typeinfo)
                        ? (int)cxnet_System_Net_Sockets_SocketException_SocketErrorCode___const_get(
                            (const struct cxnet_System_Net_Sockets_SocketException *)
                                refused_task->_state->_exception)
                        : -1,
                    steps);
                cxnet_System_Net_Sockets_Socket_Dispose(refused_client);
                cx_gc_remove_root(refused_task_root);
                cx_gc_remove_root(refused_endpoint_root);
                cx_gc_remove_root(refused_client_root);
                return 8;
            }
            cxnet_System_Net_Sockets_Socket_Dispose(refused_client);
            cx_gc_remove_root(refused_task_root);
            cx_gc_remove_root(refused_endpoint_root);
            cx_gc_remove_root(refused_client_root);
        }
    }

    {
        struct cxnet_System_Net_Sockets_Socket *multi_listener;
        struct cxnet_System_Net_IPEndPoint *multi_bind_endpoint;
        struct cxnet_System_Net_IPEndPoint *multi_endpoint;
        struct cxnet_System_Net_Sockets_Socket *client_a;
        struct cxnet_System_Net_Sockets_Socket *client_b;
        struct cxnet_System_Net_Sockets_Socket *accepted_a;
        struct cxnet_System_Net_Sockets_Socket *accepted_b;
        struct cxcore_System_Collections_Array send_a = {0};
        struct cxcore_System_Collections_Array send_b = {0};
        struct cxcore_System_Collections_Array receive_a = {0};
        struct cxcore_System_Collections_Array receive_b = {0};
        struct cxcore_System_Threading_Tasks_Task *accept_a;
        struct cxcore_System_Threading_Tasks_Task *accept_b;
        struct cxcore_System_Threading_Tasks_TaskHandle *connect_a;
        struct cxcore_System_Threading_Tasks_TaskHandle *connect_b;
        struct cx_gc_root_handle *client_a_root;
        struct cx_gc_root_handle *client_b_root;
        struct cx_gc_root_handle *multi_listener_root;
        struct cx_gc_root_handle *multi_bind_endpoint_root;
        struct cx_gc_root_handle *multi_endpoint_root;
        struct cx_gc_root_handle *accepted_a_root;
        struct cx_gc_root_handle *accepted_b_root;
        struct cx_gc_root_handle *accept_a_root;
        struct cx_gc_root_handle *accept_b_root;
        struct cx_gc_root_handle *connect_a_root;
        struct cx_gc_root_handle *connect_b_root;
        cx_byte send_a_data = (cx_byte)'A';
        cx_byte send_b_data = (cx_byte)'B';
        cx_byte receive_a_data = 0u;
        cx_byte receive_b_data = 0u;

        multi_listener = (struct cxnet_System_Net_Sockets_Socket *)
            cxcore_System_Memory_Alloc(cxnet_System_Net_Sockets_Socket___typeinfo.Size);
        multi_listener_root = cx_gc_add_root(&multi_listener,
            (cx_uint)sizeof(multi_listener));
        multi_bind_endpoint = (struct cxnet_System_Net_IPEndPoint *)
            cxcore_System_Memory_Alloc(cxnet_System_Net_IPEndPoint___typeinfo.Size);
        multi_bind_endpoint_root = cx_gc_add_root(&multi_bind_endpoint,
            (cx_uint)sizeof(multi_bind_endpoint));
        client_a = (struct cxnet_System_Net_Sockets_Socket *)
            cxcore_System_Memory_Alloc(cxnet_System_Net_Sockets_Socket___typeinfo.Size);
        client_a_root = cx_gc_add_root(&client_a, (cx_uint)sizeof(client_a));
        client_b = (struct cxnet_System_Net_Sockets_Socket *)
            cxcore_System_Memory_Alloc(cxnet_System_Net_Sockets_Socket___typeinfo.Size);
        client_b_root = cx_gc_add_root(&client_b, (cx_uint)sizeof(client_b));

        send_a._length = 1u;
        send_a._data = &send_a_data;
        send_b._length = 1u;
        send_b._data = &send_b_data;
        receive_a._length = 1u;
        receive_a._data = &receive_a_data;
        receive_b._length = 1u;
        receive_b._data = &receive_b_data;
        cxnet_System_Net_Sockets_Socket___constructor__2(multi_listener,
            cxnet_System_Net_AddressFamily_InterNetwork,
            cxnet_System_Net_Sockets_SocketType_Stream,
            cxnet_System_Net_Sockets_ProtocolType_Tcp);
        cxnet_System_Net_IPEndPoint___constructor__2(multi_bind_endpoint,
            cxnet_System_Net_IPAddress_Loopback___get(), 0);
        cxnet_System_Net_Sockets_Socket_Bind(multi_listener,
            (struct cxnet_System_Net_EndPoint *)multi_bind_endpoint);
        cxnet_System_Net_Sockets_Socket_Listen__2(multi_listener, 4);
        multi_endpoint = cxnet_System_Net_Sockets_Socket_GetLocalEndPoint(
            multi_listener);
        multi_endpoint_root = cx_gc_add_root(&multi_endpoint,
            (cx_uint)sizeof(multi_endpoint));
        cxnet_System_Net_Sockets_Socket___constructor__2(client_a,
            cxnet_System_Net_AddressFamily_InterNetwork,
            cxnet_System_Net_Sockets_SocketType_Stream,
            cxnet_System_Net_Sockets_ProtocolType_Tcp);
        cxnet_System_Net_Sockets_Socket___constructor__2(client_b,
            cxnet_System_Net_AddressFamily_InterNetwork,
            cxnet_System_Net_Sockets_SocketType_Stream,
            cxnet_System_Net_Sockets_ProtocolType_Tcp);
        accept_a = cxnet_System_Net_Sockets_Socket_AcceptAsync(multi_listener);
        accept_a_root = cx_gc_add_root(&accept_a, (cx_uint)sizeof(accept_a));
        cx_gc_collect();
        connect_a = cxnet_System_Net_Sockets_Socket_ConnectAsync__5(
            client_a, (struct cxnet_System_Net_EndPoint *)multi_endpoint);
        connect_a_root = cx_gc_add_root(&connect_a, (cx_uint)sizeof(connect_a));
        cx_gc_collect();
        steps = 0u;
        while ((!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(accept_a) ||
                !cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(connect_a)) &&
            steps < 100000u) {
            if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                Sleep(1u);
#else
                usleep(1000u);
#endif
            }
            steps++;
        }
        if (!cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(connect_a) ||
            !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(accept_a)) {
            fputs("first managed client did not connect and accept\n", stderr);
            return 13;
        }
        cxcore_System_Threading_Tasks_TaskHandle_GetResult(connect_a);
        accepted_a = cx_generic_7c4851112331385bf3ec935e4daabb246ea0a9a28971c93643ab4b4268d85138(
            (const struct cxcore_System_Threading_Tasks_Task *)accept_a);
        accepted_a_root = cx_gc_add_root(&accepted_a, (cx_uint)sizeof(accepted_a));
        if (accepted_a == NULL ||
            cxnet_System_Net_Sockets_Socket_Send(client_a, &send_a, 0u, 1u) != 1) {
            fputs("first managed client was not accepted or could not send\n", stderr);
            return 14;
        }
        cxnet_System_Net_Sockets_Socket_Shutdown(client_a,
            cxnet_System_Net_Sockets_SocketShutdown_Send);
        if (cxnet_System_Net_Sockets_Socket_Receive(accepted_a,
                &receive_a, 0u, 1u) != 1 || receive_a_data != (cx_byte)'A' ||
            cxnet_System_Net_Sockets_Socket_Receive(accepted_a,
                &receive_a, 0u, 1u) != 0) {
            fputs("managed send shutdown did not preserve data then produce EOF\n", stderr);
            return 14;
        }

        accept_b = cxnet_System_Net_Sockets_Socket_AcceptAsync(multi_listener);
        accept_b_root = cx_gc_add_root(&accept_b, (cx_uint)sizeof(accept_b));
        cx_gc_collect();
        connect_b = cxnet_System_Net_Sockets_Socket_ConnectAsync__5(
            client_b, (struct cxnet_System_Net_EndPoint *)multi_endpoint);
        connect_b_root = cx_gc_add_root(&connect_b, (cx_uint)sizeof(connect_b));
        cx_gc_collect();
        steps = 0u;
        while ((!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(accept_b) ||
                !cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(connect_b)) &&
            steps < 100000u) {
            if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                Sleep(1u);
#else
                usleep(1000u);
#endif
            }
            steps++;
        }
        if (!cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(connect_b) ||
            !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(accept_b)) {
            fputs("second managed client did not connect and accept\n", stderr);
            return 15;
        }
        cxcore_System_Threading_Tasks_TaskHandle_GetResult(connect_b);
        accepted_b = cx_generic_7c4851112331385bf3ec935e4daabb246ea0a9a28971c93643ab4b4268d85138(
            (const struct cxcore_System_Threading_Tasks_Task *)accept_b);
        accepted_b_root = cx_gc_add_root(&accepted_b, (cx_uint)sizeof(accepted_b));
        if (accepted_b == NULL ||
            cxnet_System_Net_Sockets_Socket_Send(client_b, &send_b, 0u, 1u) != 1 ||
            cxnet_System_Net_Sockets_Socket_Receive(accepted_b,
                &receive_b, 0u, 1u) != 1 || receive_b_data != (cx_byte)'B') {
            fputs("second managed client stopped working after the first half-close\n",
                stderr);
            return 15;
        }
        cxnet_System_Net_Sockets_Socket_Dispose(client_a);
        cxnet_System_Net_Sockets_Socket_Dispose(client_b);
        cxnet_System_Net_Sockets_Socket_Dispose(accepted_a);
        cxnet_System_Net_Sockets_Socket_Dispose(accepted_b);
        cxnet_System_Net_Sockets_Socket_Dispose(multi_listener);
        cx_gc_remove_root(multi_endpoint_root);
        cx_gc_remove_root(multi_bind_endpoint_root);
        cx_gc_remove_root(multi_listener_root);
        cx_gc_remove_root(accepted_a_root);
        cx_gc_remove_root(accepted_b_root);
        cx_gc_remove_root(connect_a_root);
        cx_gc_remove_root(connect_b_root);
        cx_gc_remove_root(accept_a_root);
        cx_gc_remove_root(accept_b_root);
        cx_gc_remove_root(client_a_root);
        cx_gc_remove_root(client_b_root);
    }

    cx_gc_remove_root(connect_root);
    cx_gc_remove_root(client_root);
    cx_gc_remove_root(listener_root);
    cx_gc_remove_root(task_root);
    if (test_concurrent_pending_accepts() != 0) return 17;
    if (!test_managed_read_reports_peer_reset(0)) return 18;
    if (!test_managed_read_reports_peer_reset(1)) return 39;
    if (!test_managed_pending_write_peer_reset(0)) return 40;
    if (!test_managed_pending_write_peer_reset(1)) return 41;
    if (!test_network_stream_dispose_during_pending_read()) return 19;
    if (!test_owning_network_stream_dispose_during_pending_read()) return 21;
    if (!test_owning_network_stream_dispose_during_pending_write()) return 27;
    if (!test_nonowning_network_stream_dispose_during_pending_write()) return 28;
    if (!test_nonowning_network_stream_socket_dispose_during_pending_write())
        return 62;
    if (!test_nonowning_network_stream_socket_dispose_during_pending_read())
        return 63;
    if (!test_network_stream_rejects_concurrent_reads()) return 64;
    if (!test_network_stream_rejects_concurrent_writes()) return 65;
    if (!test_socket_native_close_error_after_cleanup()) return 22;
    if (!test_managed_socket_read_dispose_race(0)) return 23;
    if (!test_managed_socket_read_dispose_race(1)) return 59;
    if (!test_managed_socket_write_dispose_start_race(0)) return 41;
    if (!test_managed_socket_write_dispose_start_race(1)) return 58;
    if (!test_managed_socket_write_dispose_start_race(2)) return 61;
    if (!test_managed_socket_receive_shutdown()) return 29;
    if (!test_managed_socket_pending_receive_shutdown(0)) return 34;
    if (!test_managed_socket_multiple_accept_dispose()) return 30;
    if (!test_managed_socket_cancel_after_native_accept()) return 67;
    if (!test_managed_socket_rejects_concurrent_reads()) return 24;
    if (!test_managed_socket_rejects_concurrent_writes()) return 66;
    if (!test_managed_socket_pending_write_dispose()) return 25;
    if (!test_managed_socket_saturated_write()) return 26;
    if (!test_managed_socket_pending_write_cancel()) return 35;
    if (!test_managed_socket_pending_write_ready_cancel_race(0)) return 42;
    if (!test_managed_socket_pending_write_ready_cancel_race(1)) return 56;
    if (!test_managed_socket_pending_write_shutdown(0) ||
        !test_managed_socket_pending_write_shutdown(1)) return 37;
    if (!test_managed_socket_pending_write_survives_receive_shutdown(0) ||
        !test_managed_socket_pending_write_survives_receive_shutdown(1))
        return 38;
    if (!test_managed_socket_pending_read_cancel(0)) return 36;
    if (!test_managed_socket_pending_read_cancel(1)) return 53;
    if (!test_managed_socket_pending_read_ready_cancel_race(0)) return 43;
    if (!test_managed_socket_pending_read_ready_cancel_race(1)) return 54;
    if (!test_managed_socket_full_duplex_independent_cancel(0)) return 40;
    if (!test_managed_socket_full_duplex_independent_cancel(1)) return 55;
    if (!test_managed_socket_option_dispose_race()) return 26;
    return 0;
}

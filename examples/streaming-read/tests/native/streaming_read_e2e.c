#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "streamingRead.h"
#include "cxnet_socket_runtime.h"
#include "cxruntime.h"

extern CX_CXNET_DATA_API struct cxcore_System_Reflection_TypeInfo
    cxnet_System_Net_Sockets___CxIterator_ReadBytesAsync_CBF15E29248D___typeinfo;

extern size_t cx_memory_test_allocation_count_get(void);
extern size_t cx_memory_test_allocation_bytes_get(void);
extern size_t cx_memory_test_heap_bytes_get(void);
extern void cx_memory_test_allocation_sizes_reset(void);
extern size_t cx_memory_test_allocation_size_entry_count_get(void);
extern size_t cx_memory_test_allocation_size_entry_get(size_t index, size_t *size);
extern void cx_memory_test_allocation_tracking_set(cx_bool enabled);
extern void cx_task_scheduler_test_post_allocation_count_reset(void);
extern size_t cx_task_scheduler_test_post_allocation_count_get(void);

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
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
static int send_byte(cx_byte value) {
    return send(sockets[1], (const char *)&value, 1, 0) == 1;
}
static int set_primary_send_buffer(int size) {
    return setsockopt(sockets[0], SOL_SOCKET, SO_SNDBUF,
        (const char *)&size, (int)sizeof(size)) == 0;
}
static int set_peer_receive_buffer(int size) {
    return setsockopt(sockets[1], SOL_SOCKET, SO_RCVBUF,
        (const char *)&size, (int)sizeof(size)) == 0;
}
static int drain_peer_bytes(uint32_t count) {
    char bytes[8192];
    uint32_t drained = 0;
    while (drained < count) {
        int remaining = (int)(count - drained);
        int requested = remaining < (int)sizeof(bytes) ? remaining : (int)sizeof(bytes);
        int received = recv(sockets[1], bytes, requested, 0);
        if (received <= 0) return 0;
        drained += (uint32_t)received;
    }
    return 1;
}
static void close_peer(void) {
    if (sockets[1] != INVALID_SOCKET) {
        closesocket(sockets[1]);
        sockets[1] = INVALID_SOCKET;
    }
}
static void close_sockets(void) {
    if (sockets[0] != INVALID_SOCKET) closesocket(sockets[0]);
    if (sockets[1] != INVALID_SOCKET) closesocket(sockets[1]);
    WSACleanup();
}
#else
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
static int sockets[2] = { -1, -1 };
static int create_socket_pair(void) {
    return socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0;
}
static int send_byte(cx_byte value) {
    return send(sockets[1], &value, 1, 0) == 1;
}
static int set_primary_send_buffer(int size) {
    return setsockopt(sockets[0], SOL_SOCKET, SO_SNDBUF,
        &size, (socklen_t)sizeof(size)) == 0;
}
static int set_peer_receive_buffer(int size) {
    return setsockopt(sockets[1], SOL_SOCKET, SO_RCVBUF,
        &size, (socklen_t)sizeof(size)) == 0;
}
static int drain_peer_bytes(uint32_t count) {
    char bytes[8192];
    uint32_t drained = 0;
    while (drained < count) {
        size_t remaining = (size_t)(count - drained);
        size_t requested = remaining < sizeof(bytes) ? remaining : sizeof(bytes);
        ssize_t received = recv(sockets[1], bytes, requested, 0);
        if (received <= 0) return 0;
        drained += (uint32_t)received;
    }
    return 1;
}
static void close_peer(void) {
    if (sockets[1] >= 0) {
        close(sockets[1]);
        sockets[1] = -1;
    }
}
static void close_sockets(void) {
    if (sockets[0] >= 0) close(sockets[0]);
    if (sockets[1] >= 0) close(sockets[1]);
}
#endif

static int is_completed(
    struct streamingRead_Examples_StreamingRead_StreamingReadSession *session) {
    return streamingRead_Examples_StreamingRead_StreamingReadSession_MoveIsCompleted(session);
}

static struct cxnet_System_Net_Sockets_Socket *create_socket(cx_ulong handle) {
    struct cxnet_System_Net_Sockets_Socket *socket =
        (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
            (cx_uint)sizeof(*socket));
    cxnet_System_Net_Sockets_Socket___constructor(socket, handle);
    return socket;
}

static int validate_async_iterator_reflection_and_public_header(void) {
    typedef struct cx_iface_ref (*read_bytes_with_token_fn)(
        struct cxnet_System_Net_Sockets_Socket *,
        struct cxcore_System_Collections_Array *,
        cx_uint, cx_uint,
        struct cxcore_System_Threading_Tasks_CancellationToken);
    typedef struct cx_iface_ref (*read_bytes_default_fn)(
        struct cxnet_System_Net_Sockets_Socket *,
        struct cxcore_System_Collections_Array *,
        cx_uint, cx_uint);
    typedef struct cx_iface_ref (*read_bytes_with_source_fn)(
        struct cxnet_System_Net_Sockets_Socket *,
        struct cxcore_System_Collections_Array *,
        cx_uint, cx_uint,
        struct cxcore_System_Threading_Tasks_CancellationToken,
        struct cx_generic_f5adc7f5037e4af96d3c3dc098894dff2919964c44d32ff935cfc2fb6dec5a6a *);
    const struct cxcore_System_Reflection_TypeInfo *iterator_type =
        &cxnet_System_Net_Sockets___CxIterator_ReadBytesAsync_CBF15E29248D___typeinfo;
    const struct cxcore_System_Reflection_TypeInfo *socket_type =
        &cxnet_System_Net_Sockets_Socket___typeinfo;
    const struct cxcore_System_Reflection_TypeInfo *read_parameters[4] = {
        &cxcore_System_Collections_Array___typeinfo,
        &cxcore_System_UInt___typeinfo,
        &cxcore_System_UInt___typeinfo,
        &cxcore_System_Threading_Tasks_CancellationToken___typeinfo,
    };
    const struct cxcore_System_Reflection_TypeInfo *default_read_parameters[3] = {
        &cxcore_System_Collections_Array___typeinfo,
        &cxcore_System_UInt___typeinfo,
        &cxcore_System_UInt___typeinfo,
    };
    const struct cxcore_System_Reflection_TypeInfo *move_next_type =
        &CX_TYPEINFO_NAME(cx_generic_8b078e0bf7a5ebc0704c42aa0c63f3733eedfa9c025ed3a1ecba383e9b02eead);
    const struct cxcore_System_Reflection_TypeInfo *byte_type =
        &cxcore_System_Byte___typeinfo;
    const struct cxcore_System_Reflection_TypeInfo *cancellation_token_type =
        &cxcore_System_Threading_Tasks_CancellationToken___typeinfo;
    const struct cxcore_System_Reflection_TypeInfo *value_task_type =
        &cxcore_System_Threading_Tasks_ValueTaskHandle___typeinfo;
    const struct cx_interface_impl *interfaces;
    const struct cx_reflection_function *function;
    cx_uint interface_count = 0;
    cx_bool has_async_disposable = CX_FALSE;
    cx_uint other_interface_count = 0u;
    cx_uint index;
    read_bytes_with_token_fn read_with_token =
        cxnet_System_Net_Sockets_Socket_ReadBytesAsync;
    read_bytes_default_fn read_default =
        cxnet_System_Net_Sockets_Socket_ReadBytesAsync__2;
    read_bytes_with_source_fn read_with_source =
        cxnet_System_Net_Sockets_Socket_ReadBytesAsyncWithSource;

    if (read_with_token == NULL || read_default == NULL || read_with_source == NULL) return 0;
    interfaces = cx_reflection_interfaces(iterator_type, &interface_count);
    if (interfaces == NULL || interface_count != 3u) return 0;
    for (index = 0; index < interface_count; ++index) {
        const struct cxcore_System_Reflection_TypeInfo *type =
            (const struct cxcore_System_Reflection_TypeInfo *)interfaces[index].typeInfo;
        if (type == NULL) return 0;
        if (type->Hash == cxcore_System_IAsyncDisposable___typeinfo.Hash) {
            has_async_disposable = CX_TRUE;
        } else ++other_interface_count;
    }
    if (!has_async_disposable || other_interface_count != 2u) return 0;

    function = cx_reflection_find_function(
        socket_type, "ReadBytesAsync", 4u, read_parameters);
    if (function == NULL ||
        (function->flags & CX_REFLECTION_FLAG_VISIBILITY_PUBLIC) == 0u) return 0;
    function = cx_reflection_find_function(
        socket_type, "ReadBytesAsync", 3u, default_read_parameters);
    if (function == NULL ||
        (function->flags & CX_REFLECTION_FLAG_VISIBILITY_PUBLIC) == 0u) return 0;
    {
        const struct cx_reflection_function *functions;
        cx_uint function_count = 0u;
        cx_bool found_source_overload = CX_FALSE;
        functions = cx_reflection_functions(socket_type, &function_count);
        for (index = 0u; functions != NULL && index < function_count; ++index) {
            if (strcmp(functions[index].name, "ReadBytesAsyncWithSource") == 0 &&
                functions[index].parameterCount == 5u &&
                (functions[index].flags & CX_REFLECTION_FLAG_VISIBILITY_PUBLIC) != 0u) {
                found_source_overload = CX_TRUE;
                break;
            }
        }
        if (!found_source_overload) return 0;
    }

    function = cx_reflection_find_function(
        iterator_type, "GetAsyncEnumerator", 1u,
        (const struct cxcore_System_Reflection_TypeInfo *const *)&cancellation_token_type);
    if (function == NULL || function->returnTypeInfo == NULL ||
        (function->flags & CX_REFLECTION_FLAG_VISIBILITY_PUBLIC) == 0u) return 0;
    function = cx_reflection_find_function(iterator_type, "MoveNextAsync", 0u, NULL);
    if (function == NULL || function->returnTypeInfo == NULL ||
        ((const struct cxcore_System_Reflection_TypeInfo *)function->returnTypeInfo)->Hash !=
            move_next_type->Hash ||
        (function->flags & CX_REFLECTION_FLAG_VISIBILITY_PUBLIC) == 0u) return 0;
    function = cx_reflection_find_function(iterator_type, "DisposeAsync", 0u, NULL);
    if (function == NULL || function->returnTypeInfo == NULL ||
        ((const struct cxcore_System_Reflection_TypeInfo *)function->returnTypeInfo)->Hash !=
            value_task_type->Hash ||
        (function->flags & CX_REFLECTION_FLAG_VISIBILITY_PUBLIC) == 0u) return 0;
    function = cx_reflection_find_function(iterator_type, "Current", 0u, NULL);
    if (function == NULL || function->returnTypeInfo == NULL ||
        ((const struct cxcore_System_Reflection_TypeInfo *)function->returnTypeInfo)->Hash !=
            byte_type->Hash ||
        (function->flags & CX_REFLECTION_FLAG_VISIBILITY_PUBLIC) == 0u ||
        (function->flags & CX_REFLECTION_FLAG_FUNCTION_PROPERTY_GET) == 0u) return 0;

    puts("PASS async iterator reflection, interface map, and public socket/source header signatures");
    return 1;
}

static int wait_for_task(
    struct streamingRead_Examples_StreamingRead_StreamingReadSession *session) {
    unsigned int turns = 0;
    while (!is_completed(session) && turns++ < 32u &&
        cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) { }
    return is_completed(session);
}

static void print_allocation_size_histogram(const char *phase) {
    size_t entries = cx_memory_test_allocation_size_entry_count_get();
    size_t index;
    printf("INFO %s allocation sizes:", phase);
    for (index = 0; index < entries; ++index) {
        size_t size = 0;
        size_t count = cx_memory_test_allocation_size_entry_get(index, &size);
        printf(" %zu-byte x %zu;", size, count);
    }
    putchar('\n');
}

static int wait_for_socket_task(
    struct streamingRead_Examples_StreamingRead_SocketTaskReadSession *session) {
    unsigned int turns = 0;
    while (!streamingRead_Examples_StreamingRead_SocketTaskReadSession_IsCompleted(session) && turns++ < 32u &&
        cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) { }
    return streamingRead_Examples_StreamingRead_SocketTaskReadSession_IsCompleted(session);
}

static struct cxcore_System_Collections_Array *create_buffer(void) {
    return (struct cxcore_System_Collections_Array *)
        cx_array_new(8u, (cx_uint)sizeof(cx_byte));
}

static struct cxcore_System_Collections_Array *create_sized_buffer(cx_uint length) {
    return (struct cxcore_System_Collections_Array *)cx_array_new(
        length, (cx_uint)sizeof(cx_byte));
}

static uint64_t benchmark_nanoseconds(void) {
#ifdef _WIN32
    LARGE_INTEGER counter;
    LARGE_INTEGER frequency;
    QueryPerformanceCounter(&counter);
    QueryPerformanceFrequency(&frequency);
    return (uint64_t)((double)counter.QuadPart * 1000000000.0 /
        (double)frequency.QuadPart);
#else
    struct timespec timestamp;
    if (clock_gettime(CLOCK_MONOTONIC, &timestamp) != 0) return 0u;
    return (uint64_t)timestamp.tv_sec * UINT64_C(1000000000) +
        (uint64_t)timestamp.tv_nsec;
#endif
}

static int queued_cancel_callback_count;
static struct streamingRead_Examples_StreamingRead_StreamingReadSession
    *active_retained_stream_session;
static size_t retained_probe_live_heap_bytes;
static const struct cx_gc_root_region retained_stream_session_roots[] = {
    {&active_retained_stream_session, (cx_uint)sizeof(active_retained_stream_session)},
};
static struct streamingRead_Examples_StreamingRead_SocketTaskReadSession
    *active_task_session;
static cx_bool task_session_root_registered;
static const struct cx_gc_root_region task_session_roots[] = {
    {&active_task_session, (cx_uint)sizeof(active_task_session)},
};

static void cancel_session_from_queued_readiness(void *environment) {
    struct streamingRead_Examples_StreamingRead_StreamingReadSession *session =
        (struct streamingRead_Examples_StreamingRead_StreamingReadSession *)environment;
    ++queued_cancel_callback_count;
    streamingRead_Examples_StreamingRead_StreamingReadSession_Cancel(session);
}

static int direct_read_is_rejected_by_streaming_sequence(
    struct cxnet_System_Net_Sockets_Socket *socket) {
    struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *volatile overlapping_task = NULL;
    volatile int rejected = 0;
    struct cx_exception_frame frame;
    cx_exception_push(&frame);
    if (setjmp(frame.environment) == 0) {
        overlapping_task = cxnet_System_Net_Sockets_Socket_ReadAsync(
            socket, create_buffer(), 0u, 1u);
        cx_exception_pop(&frame);
    } else {
        rejected = cxcore_System_Exception_Matches(
            (struct cxcore_System_Exception *)cx_exception_current(),
            cxcore_System_InvalidOperationException___typeinfo);
        cx_exception_pop(&frame);
        cx_exception_clear();
    }
    return rejected && overlapping_task == NULL;
}

static int run_eof_case(void) {
    const cx_byte expected = 0x5a;
    struct cxnet_System_Net_Sockets_Socket *socket;
    struct streamingRead_Examples_StreamingRead_StreamingReadSession *session;
    size_t allocation_start;
    size_t operation_start_allocations;
    size_t completion_allocations;
    size_t pending_read_allocations;
    size_t repeated_read_allocations;
    size_t setup_scheduler_posts;
    size_t completion_scheduler_posts;
    if (!create_socket_pair()) return 1;
    socket = create_socket((cx_ulong)sockets[0]);
    session = streamingRead_Examples_StreamingRead_StreamingReadSession_Create(
            socket, create_buffer(), 0u, 8u,
            cxcore_System_Threading_Tasks_CancellationToken_None___const_get());
    allocation_start = cx_memory_test_allocation_count_get();
    cx_memory_test_allocation_sizes_reset();
    cx_task_scheduler_test_post_allocation_count_reset();
    cx_memory_test_allocation_tracking_set(CX_TRUE);
    (void)streamingRead_Examples_StreamingRead_StreamingReadSession_MoveNextAsync(session);
    if (is_completed(session)) {
        cx_memory_test_allocation_tracking_set(CX_FALSE);
        fputs("streaming move completed before peer sent a byte\n", stderr);
        goto failed;
    }
    operation_start_allocations =
        cx_memory_test_allocation_count_get() - allocation_start;
    setup_scheduler_posts = cx_task_scheduler_test_post_allocation_count_get();
    cx_memory_test_allocation_tracking_set(CX_FALSE);
    print_allocation_size_histogram("first pending read setup");
    if (!direct_read_is_rejected_by_streaming_sequence(socket)) {
        fputs("direct socket read was admitted while ReadBytesAsync held the receive sequence\n",
            stderr);
        goto failed;
    }
    puts("PASS ReadBytesAsync excludes overlapping direct socket reads");
    cx_gc_collect();
    allocation_start = cx_memory_test_allocation_count_get();
    cx_memory_test_allocation_sizes_reset();
    cx_memory_test_allocation_tracking_set(CX_TRUE);
    if (!send_byte(expected)) {
        cx_memory_test_allocation_tracking_set(CX_FALSE);
        fputs("streaming peer failed to send the byte\n", stderr);
        goto failed;
    }
    if (!wait_for_task(session)) {
        cx_memory_test_allocation_tracking_set(CX_FALSE);
        fputs("streaming move remained pending after readiness dispatch\n", stderr);
        goto failed;
    }
    completion_allocations = cx_memory_test_allocation_count_get() - allocation_start;
    completion_scheduler_posts =
        cx_task_scheduler_test_post_allocation_count_get() - setup_scheduler_posts;
    pending_read_allocations = operation_start_allocations + completion_allocations;
    cx_memory_test_allocation_tracking_set(CX_FALSE);
    print_allocation_size_histogram("first read completion");
    if (!streamingRead_Examples_StreamingRead_StreamingReadSession_MoveResult(session)) {
        fputs("streaming move completed without yielding an element\n", stderr);
        goto failed;
    }
    if (streamingRead_Examples_StreamingRead_StreamingReadSession_Current___const_get(session) != expected) {
        fputs("streaming move yielded the wrong byte\n", stderr);
        goto failed;
    }
    puts("PASS first byte was yielded before EOF");
    printf("INFO first pending read+move allocated %zu CX objects at start, %zu "
        "during completion, %zu total\n", operation_start_allocations,
        completion_allocations, pending_read_allocations);
    printf("INFO first read allocated %zu scheduler queue items at start and %zu during completion\n",
        setup_scheduler_posts, completion_scheduler_posts);
    if (operation_start_allocations != 14u || completion_allocations != 2u ||
        pending_read_allocations != 16u || setup_scheduler_posts != 0u ||
        completion_scheduler_posts != 2u) {
        fputs("first pending read allocation profile exceeded its recorded baseline\n",
            stderr);
        goto failed;
    }

    allocation_start = cx_memory_test_allocation_count_get();
    cx_memory_test_allocation_sizes_reset();
    cx_task_scheduler_test_post_allocation_count_reset();
    cx_memory_test_allocation_tracking_set(CX_TRUE);
    (void)streamingRead_Examples_StreamingRead_StreamingReadSession_MoveNextAsync(session);
    if (is_completed(session)) {
        cx_memory_test_allocation_tracking_set(CX_FALSE);
        fputs("repeated streaming read completed before peer sent its byte\n", stderr);
        goto failed;
    }
    operation_start_allocations =
        cx_memory_test_allocation_count_get() - allocation_start;
    setup_scheduler_posts = cx_task_scheduler_test_post_allocation_count_get();
    cx_memory_test_allocation_tracking_set(CX_FALSE);
    print_allocation_size_histogram("repeated pending read setup");
    cx_gc_collect();
    allocation_start = cx_memory_test_allocation_count_get();
    cx_memory_test_allocation_sizes_reset();
    cx_memory_test_allocation_tracking_set(CX_TRUE);
    if (!send_byte(0x5b) || !wait_for_task(session)) {
        cx_memory_test_allocation_tracking_set(CX_FALSE);
        fputs("repeated streaming read did not complete after readiness dispatch\n",
            stderr);
        goto failed;
    }
    completion_allocations = cx_memory_test_allocation_count_get() - allocation_start;
    completion_scheduler_posts =
        cx_task_scheduler_test_post_allocation_count_get() - setup_scheduler_posts;
    repeated_read_allocations = operation_start_allocations + completion_allocations;
    cx_memory_test_allocation_tracking_set(CX_FALSE);
    print_allocation_size_histogram("repeated read completion");
    if (!streamingRead_Examples_StreamingRead_StreamingReadSession_MoveResult(session) ||
        streamingRead_Examples_StreamingRead_StreamingReadSession_Current___const_get(session) != 0x5b) {
        fputs("repeated streaming read yielded the wrong byte\n", stderr);
        goto failed;
    }
    puts("PASS reusable socket read source reset after consumed result and callback drain");
    printf("INFO repeated pending read+move allocated %zu CX objects at start, "
        "%zu during completion, %zu total\n", operation_start_allocations,
        completion_allocations, repeated_read_allocations);
    printf("INFO repeated read allocated %zu scheduler queue items at start and %zu during completion\n",
        setup_scheduler_posts, completion_scheduler_posts);
    if (operation_start_allocations != 6u || completion_allocations != 0u ||
        repeated_read_allocations != 6u || setup_scheduler_posts != 0u ||
        completion_scheduler_posts != 0u) {
        fputs("repeated pending read allocation profile exceeded its recorded budget\n",
            stderr);
        goto failed;
    }

    close_peer();
    (void)streamingRead_Examples_StreamingRead_StreamingReadSession_MoveNextAsync(session);
    int eof_completed = is_completed(session) || wait_for_task(session);
    int eof_has_value = eof_completed
        ? streamingRead_Examples_StreamingRead_StreamingReadSession_MoveResult(session)
        : -1;
    if (!eof_completed || eof_has_value) {
        fprintf(stderr,
            "streaming adapter did not end after peer EOF (completed=%d, has_value=%d)\n",
            eof_completed, eof_has_value);
        goto failed;
    }
    puts("PASS iterator ended after peer EOF");
    streamingRead_Examples_StreamingRead_StreamingReadSession_Dispose(session);
    cxnet_System_Net_Sockets_Socket_Dispose(socket);
    close_sockets();
    return 0;
failed:
    streamingRead_Examples_StreamingRead_StreamingReadSession_Dispose(session);
    cxnet_System_Net_Sockets_Socket_Dispose(socket);
    close_sockets();
    return 3;
}

static int run_pending_stream_cycle(cx_byte expected) {
    struct cxnet_System_Net_Sockets_Socket *socket;
    struct streamingRead_Examples_StreamingRead_StreamingReadSession *session;
    if (!create_socket_pair()) return 1;
    socket = create_socket((cx_ulong)sockets[0]);
    session = streamingRead_Examples_StreamingRead_StreamingReadSession_Create(socket, create_buffer(), 0u, 8u,
            cxcore_System_Threading_Tasks_CancellationToken_None___const_get());
    active_retained_stream_session = session;
    (void)streamingRead_Examples_StreamingRead_StreamingReadSession_MoveNextAsync(session);
    if (is_completed(session) || !send_byte(expected)) {
        fputs("retained-heap stream did not remain pending for peer data\n", stderr);
        goto failed;
    }
    cx_gc_collect();
    if (!wait_for_task(session) ||
        !streamingRead_Examples_StreamingRead_StreamingReadSession_MoveResult(session) ||
        streamingRead_Examples_StreamingRead_StreamingReadSession_Current___const_get(session) != expected) {
        fputs("retained-heap stream did not yield its pending byte\n", stderr);
        goto failed;
    }
    if (cx_memory_test_heap_bytes_get() > retained_probe_live_heap_bytes) {
        retained_probe_live_heap_bytes = cx_memory_test_heap_bytes_get();
    }
    streamingRead_Examples_StreamingRead_StreamingReadSession_Dispose(session);
    active_retained_stream_session = NULL;
    cxnet_System_Net_Sockets_Socket_Dispose(socket);
    close_sockets();
    return 0;
failed:
    streamingRead_Examples_StreamingRead_StreamingReadSession_Dispose(session);
    active_retained_stream_session = NULL;
    cxnet_System_Net_Sockets_Socket_Dispose(socket);
    close_sockets();
    return 3;
}

static int run_retained_heap_case(void) {
    const unsigned int cycles = 16u;
    size_t baseline;
    size_t one_cycle;
    size_t after_cycles;
    unsigned int index;
    cx_gc_register_static_roots(retained_stream_session_roots, 1u);
    if (run_pending_stream_cycle(0x3cu) != 0 ||
        run_pending_stream_cycle(0x3du) != 0) return 1;
    cx_gc_collect();
    baseline = cx_memory_test_heap_bytes_get();
    if (run_pending_stream_cycle(0x3eu) != 0) return 2;
    cx_gc_collect();
    one_cycle = cx_memory_test_heap_bytes_get();
    for (index = 0u; index < cycles; index += 1u) {
        if (run_pending_stream_cycle((cx_byte)(0x40u + index)) != 0) return 3;
        cx_gc_collect();
    }
    after_cycles = cx_memory_test_heap_bytes_get();
    printf("INFO %u pending stream create/read/dispose/collect cycles retained "
        "%zu CX heap bytes (live sample=%zu, warm baseline=%zu, one cycle=%zu, "
        "after=%zu)\n",
        cycles, after_cycles > baseline ? after_cycles - baseline : 0u,
        retained_probe_live_heap_bytes, baseline, one_cycle, after_cycles);
    if (retained_probe_live_heap_bytes == 0u) {
        fputs("retained-heap probe did not observe a live managed heap\n", stderr);
        return 5;
    }
    if (after_cycles > one_cycle) {
        fputs("completed stream cycles retained more heap than one cycle\n", stderr);
        return 4;
    }
    puts("PASS repeated pending streams did not increase retained CX heap bytes");
    return 0;
}

static int run_early_dispose_case(void) {
    struct cxnet_System_Net_Sockets_Socket *socket;
    struct streamingRead_Examples_StreamingRead_StreamingReadSession *session;
    if (!create_socket_pair()) return 1;
    socket = create_socket((cx_ulong)sockets[0]);
    streamingRead_Examples_StreamingRead_StreamingReadSession_ResetFinalizers();
    session = streamingRead_Examples_StreamingRead_StreamingReadSession_Create(
            socket, create_buffer(), 0u, 8u,
            cxcore_System_Threading_Tasks_CancellationToken_None___const_get());
    (void)streamingRead_Examples_StreamingRead_StreamingReadSession_MoveNextAsync(session);
    if (is_completed(session) || !send_byte(0x31) || !wait_for_task(session) ||
        !streamingRead_Examples_StreamingRead_StreamingReadSession_MoveResult(session)) goto failed;
    streamingRead_Examples_StreamingRead_StreamingReadSession_Dispose(session);
    if (streamingRead_Examples_StreamingRead_StreamingReadSession_Finalizers != 1) {
        fputs("early disposal did not run the iterator finally exactly once\n", stderr);
        goto failed;
    }
    puts("PASS early disposal while peer remained open");
    cxnet_System_Net_Sockets_Socket_Dispose(socket);
    close_sockets();
    return 0;
failed:
    fputs("early disposal did not preserve the yielded item and cleanup\n", stderr);
    streamingRead_Examples_StreamingRead_StreamingReadSession_Dispose(session);
    cxnet_System_Net_Sockets_Socket_Dispose(socket);
    close_sockets();
    return 3;
}

static int run_cancel_case(void) {
    struct cxnet_System_Net_Sockets_Socket *socket;
    struct streamingRead_Examples_StreamingRead_StreamingReadSession *session;
    if (!create_socket_pair()) return 1;
    socket = create_socket((cx_ulong)sockets[0]);
    streamingRead_Examples_StreamingRead_StreamingReadSession_ResetFinalizers();
    session = streamingRead_Examples_StreamingRead_StreamingReadSession_CreateCancelable(
            socket, create_buffer(), 0u, 8u);
    (void)streamingRead_Examples_StreamingRead_StreamingReadSession_MoveNextAsync(session);
    if (is_completed(session)) {
        fputs("cancellable read completed before cancellation\n", stderr);
        goto failed;
    }
    streamingRead_Examples_StreamingRead_StreamingReadSession_Cancel(session);
    cx_gc_collect();
    if (!send_byte(0x7a)) {
        fputs("peer could not send the late byte after cancellation\n", stderr);
        goto failed;
    }
    if (!wait_for_task(session)) {
        fputs("pending streaming read was not canceled\n", stderr);
        goto failed;
    }
    (void)streamingRead_Examples_StreamingRead_StreamingReadSession_MoveResult(session);
    if (!streamingRead_Examples_StreamingRead_StreamingReadSession_MoveIsCanceled(session)) {
        fputs("pending streaming read was not canceled\n", stderr);
        goto failed;
    }
    if (!streamingRead_Examples_StreamingRead_StreamingReadSession_MoveNextAsync(session) ||
        streamingRead_Examples_StreamingRead_StreamingReadSession_MoveResult(session)) {
        fputs("late readiness revived a canceled streaming generation\n", stderr);
        goto failed;
    }
    streamingRead_Examples_StreamingRead_StreamingReadSession_Dispose(session);
    if (streamingRead_Examples_StreamingRead_StreamingReadSession_Finalizers != 1) {
        fputs("canceled stream did not run the iterator finally exactly once\n", stderr);
        goto failed;
    }
    puts("PASS cancellation rejected late readiness and completed cleanup");
    cxnet_System_Net_Sockets_Socket_Dispose(socket);
    close_sockets();
    return 0;
failed:
    streamingRead_Examples_StreamingRead_StreamingReadSession_Dispose(session);
    cxnet_System_Net_Sockets_Socket_Dispose(socket);
    close_sockets();
    return 3;
}

/*
 * Add the control wait after the pending socket operation. Socket waits are
 * prepended to the scheduler poll list, so both become ready in one poll and
 * the control callback runs first. At that point the operation's readiness
 * callback is already queued; cancellation must remove it before dispatch.
 */
static int run_queued_readiness_cancel_case(void) {
    struct cxnet_System_Net_Sockets_Socket *socket;
    struct streamingRead_Examples_StreamingRead_StreamingReadSession *session;
    cx_action cancel_action = {0};
    uint64_t cancel_registration;
    if (!create_socket_pair()) return 1;
    socket = create_socket((cx_ulong)sockets[0]);
    streamingRead_Examples_StreamingRead_StreamingReadSession_ResetFinalizers();
    session = streamingRead_Examples_StreamingRead_StreamingReadSession_CreateCancelable(
            socket, create_buffer(), 0u, 8u);
    (void)streamingRead_Examples_StreamingRead_StreamingReadSession_MoveNextAsync(session);
    if (is_completed(session)) {
        fputs("queued-readiness cancellation read completed before peer sent a byte\n", stderr);
        goto failed;
    }

    cancel_action.__function = cancel_session_from_queued_readiness;
    cancel_action.__environment = session;
    cancel_action.__target = (cx_ptr)session;
    cancel_registration = cxnet_socket_post_on_readiness(
        (uintptr_t)sockets[0], 0, cancel_action, (cx_action){0});
    if (cancel_registration == 0 || !send_byte(0x6d)) {
        fputs("could not arrange simultaneous read/cancel readiness\n", stderr);
        goto failed;
    }
    cx_gc_collect();
    {
        int dispatched = cxcore_System_Threading_Tasks_TaskScheduler_RunOne();
        int completed = wait_for_task(session);
        if (!dispatched || queued_cancel_callback_count != 1 || !completed) {
            fprintf(stderr,
                "queued-readiness cancellation failed (dispatched=%d, callbacks=%d, completed=%d)\n",
                dispatched, queued_cancel_callback_count, completed);
            goto failed;
        }
    }
    (void)streamingRead_Examples_StreamingRead_StreamingReadSession_MoveResult(session);
    if (!streamingRead_Examples_StreamingRead_StreamingReadSession_MoveIsCanceled(session) ||
        queued_cancel_callback_count != 1) {
        fputs("queued read readiness was not rejected by exactly-once cancellation\n", stderr);
        goto failed;
    }
    if (!streamingRead_Examples_StreamingRead_StreamingReadSession_MoveNextAsync(session) ||
        streamingRead_Examples_StreamingRead_StreamingReadSession_MoveResult(session) ||
        queued_cancel_callback_count != 1) {
        fputs("queued readiness revived or reentered a canceled generation\n", stderr);
        goto failed;
    }
    streamingRead_Examples_StreamingRead_StreamingReadSession_Dispose(session);
    if (streamingRead_Examples_StreamingRead_StreamingReadSession_Finalizers != 1) {
        fputs("queued-readiness cancellation did not run cleanup exactly once\n", stderr);
        goto failed;
    }
    cxnet_System_Net_Sockets_Socket_Dispose(socket);
    close_sockets();
    puts("PASS cancellation removed already-queued read readiness after forced GC");
    return 0;
failed:
    streamingRead_Examples_StreamingRead_StreamingReadSession_Dispose(session);
    cxnet_System_Net_Sockets_Socket_Dispose(socket);
    close_sockets();
    return 3;
}

static int run_public_task_socket_case(void) {
    struct cxnet_System_Net_Sockets_Socket *socket;
    struct streamingRead_Examples_StreamingRead_SocketTaskReadSession *session;
    struct cxcore_System_Collections_Array *buffer;
    if (!task_session_root_registered) {
        cx_gc_register_static_roots(task_session_roots, 1u);
        task_session_root_registered = CX_TRUE;
    }
    if (!create_socket_pair()) return 1;
    if (streamingRead_Examples_StreamingRead_SocketTaskReadSession_NoneIsCanceled()) {
        fputs("CancellationToken.None reported canceled after prior socket cancellation\n", stderr);
        close_sockets();
        return 3;
    }
    socket = create_socket((cx_ulong)sockets[0]);
    buffer = create_buffer();
    session = streamingRead_Examples_StreamingRead_SocketTaskReadSession_Create(socket, buffer);
    active_task_session = session;
    if (streamingRead_Examples_StreamingRead_SocketTaskReadSession_IsCompleted(session) ||
        !send_byte(0x42)) {
        fputs("public Task<uint> socket read did not pend before peer data\n", stderr);
        goto failed;
    }
    cx_gc_collect();
    if (!wait_for_socket_task(session)) {
        fputs("public Task<uint> socket read remained pending after readiness\n", stderr);
        goto failed;
    }
    if (streamingRead_Examples_StreamingRead_SocketTaskReadSession_IsCanceled(session)) {
        fputs("public Task<uint> socket read completed canceled without cancellation\n", stderr);
        goto failed;
    }
    {
        cx_uint read_count = streamingRead_Examples_StreamingRead_SocketTaskReadSession_GetResult(session);
        cx_byte read_byte = streamingRead_Examples_StreamingRead_SocketTaskReadSession_GetByte(session);
        if (read_count != 1u || read_byte != 0x42) {
            fprintf(stderr,
                "public Task<uint> socket read returned count=%u byte=%u\n",
                (unsigned)read_count, (unsigned)read_byte);
            goto failed;
        }
    }
    cxnet_System_Net_Sockets_Socket_Dispose(socket);
    active_task_session = NULL;
    close_sockets();
    puts("PASS public Socket.ReadAsync Task<uint> ABI through pending read and GC");
    return 0;
failed:
    cxnet_System_Net_Sockets_Socket_Dispose(socket);
    active_task_session = NULL;
    close_sockets();
    return 3;
}

static int run_reset_order_reuse_case(void) {
    struct cxnet_System_Net_Sockets_Socket *socket;
    struct streamingRead_Examples_StreamingRead_StreamingReadSession *session;
    cx_action cancel_action = {0};
    uint64_t cancel_registration;
    if (!create_socket_pair()) return 1;
    socket = create_socket((cx_ulong)sockets[0]);
    streamingRead_Examples_StreamingRead_StreamingReadSession_ResetFinalizers();
    session = streamingRead_Examples_StreamingRead_StreamingReadSession_CreateCancelableWithOwnedReadSource(
            socket, create_buffer(), 0u, 8u);
    (void)streamingRead_Examples_StreamingRead_StreamingReadSession_MoveNextAsync(session);
    if (is_completed(session)) {
        fputs("source-reset ordering read completed before registration\n", stderr);
        goto failed;
    }
    if (streamingRead_Examples_StreamingRead_StreamingReadSession_TryResetReadCompletion(session)) {
        fputs("reusable read source reset while native readiness was pending\n", stderr);
        goto failed;
    }

    cancel_action.__function = cancel_session_from_queued_readiness;
    cancel_action.__environment = session;
    cancel_action.__target = (cx_ptr)session;
    cancel_registration = cxnet_socket_post_on_readiness(
        (uintptr_t)sockets[0], 0, cancel_action, (cx_action){0});
    if (cancel_registration == 0 || !send_byte(0x6e)) {
        fputs("could not arrange readiness/cancellation source-reset race\n", stderr);
        goto failed;
    }
    cx_gc_collect();
    {
        int dispatched = cxcore_System_Threading_Tasks_TaskScheduler_RunOne();
        int completed = wait_for_task(session);
        if (!dispatched || queued_cancel_callback_count != 1 || !completed) {
            fprintf(stderr,
                "queued cancellation did not finish reusable-source read "
                "(dispatched=%d, callbacks=%d, completed=%d)\n",
                dispatched, queued_cancel_callback_count, completed);
            goto failed;
        }
    }
    (void)streamingRead_Examples_StreamingRead_StreamingReadSession_MoveResult(session);
    if (!streamingRead_Examples_StreamingRead_StreamingReadSession_MoveIsCanceled(session)) {
        fputs("queued readiness/cancellation did not cancel the active generation\n", stderr);
        goto failed;
    }
    while (cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) { }
    if (!streamingRead_Examples_StreamingRead_StreamingReadSession_TryResetReadCompletion(session) &&
        (!streamingRead_Examples_StreamingRead_StreamingReadSession_ConsumeReadCompletion(session) ||
         !streamingRead_Examples_StreamingRead_StreamingReadSession_TryResetReadCompletion(session))) {
        fputs("read source did not reset after native readiness detachment and consumption\n",
            stderr);
        goto failed;
    }

    streamingRead_Examples_StreamingRead_StreamingReadSession_RestartWithReadSource(session);
    (void)streamingRead_Examples_StreamingRead_StreamingReadSession_MoveNextAsync(session);
    if (!wait_for_task(session) ||
        !streamingRead_Examples_StreamingRead_StreamingReadSession_MoveResult(session) ||
        streamingRead_Examples_StreamingRead_StreamingReadSession_Current___const_get(session) != 0x6e) {
        fputs("reused read source did not consume readiness after cancellation cleanup\n",
            stderr);
        goto failed;
    }
    streamingRead_Examples_StreamingRead_StreamingReadSession_Dispose(session);
    if (queued_cancel_callback_count != 1 ||
        streamingRead_Examples_StreamingRead_StreamingReadSession_Finalizers != 2) {
        fputs("read-source cancellation/reuse cleanup was not exactly once per iterator\n",
            stderr);
        goto failed;
    }
    puts("PASS reusable source rejects reset during readiness and reuses after detachment");
    cxnet_System_Net_Sockets_Socket_Dispose(socket);
    close_sockets();
    return 0;
failed:
    streamingRead_Examples_StreamingRead_StreamingReadSession_Dispose(session);
    cxnet_System_Net_Sockets_Socket_Dispose(socket);
    close_sockets();
    return 3;
}

static int run_partial_write_benchmark(void) {
    const cx_uint requested = 4u * 1024u * 1024u;
    const unsigned int operation_count = 32u;
    const unsigned int sample_count = 3u;
    struct cxnet_System_Net_Sockets_Socket *socket;
    struct streamingRead_Examples_StreamingRead_SocketTaskWriteSession *session;
    struct cxcore_System_Collections_Array *buffer;
    size_t expected_allocations = 0;
    size_t expected_bytes = 0;
    unsigned int sample;
    unsigned int warmup;

    if (!create_socket_pair() || !set_primary_send_buffer(4096) ||
        !set_peer_receive_buffer(4096)) {
        fputs("could not create or constrain the partial-write socket pair\n", stderr);
        close_sockets();
        return 1;
    }
    socket = create_socket((cx_ulong)sockets[0]);
#ifdef _WIN32
    sockets[0] = INVALID_SOCKET;
#else
    sockets[0] = -1;
#endif
    buffer = create_sized_buffer(requested);
    session = streamingRead_Examples_StreamingRead_SocketTaskWriteSession_Create(socket);

    for (warmup = 0; warmup < 4u; ++warmup) {
        cx_uint transferred;
        streamingRead_Examples_StreamingRead_SocketTaskWriteSession_StartWrite(session, buffer, requested);
        if (!streamingRead_Examples_StreamingRead_SocketTaskWriteSession_IsCompleted(session)) {
            fputs("warmup Socket.WriteAsync unexpectedly pended\n", stderr);
            goto failed;
        }
        transferred = streamingRead_Examples_StreamingRead_SocketTaskWriteSession_GetResult(session);
        if (transferred == 0u || transferred > requested ||
            !drain_peer_bytes(transferred)) {
            fprintf(stderr, "warmup write returned an invalid count or peer drain failed (%u bytes)\n",
                (unsigned)transferred);
            goto failed;
        }
    }

    for (sample = 1u; sample <= sample_count; ++sample) {
        size_t allocation_start = cx_memory_test_allocation_count_get();
        size_t bytes_start = cx_memory_test_allocation_bytes_get();
        size_t allocations;
        size_t allocated_bytes;
        uint64_t elapsed = 0u;
        uint64_t transferred_total = 0u;
        unsigned int partial_operations = 0u;
        unsigned int operation;
        cx_memory_test_allocation_tracking_set(CX_TRUE);
        for (operation = 0; operation < operation_count; ++operation) {
            uint64_t start = benchmark_nanoseconds();
            uint64_t end;
            cx_uint transferred;
            streamingRead_Examples_StreamingRead_SocketTaskWriteSession_StartWrite(session, buffer, requested);
            if (!streamingRead_Examples_StreamingRead_SocketTaskWriteSession_IsCompleted(session)) {
                cx_memory_test_allocation_tracking_set(CX_FALSE);
                fputs("measured partial Socket.WriteAsync unexpectedly pended\n", stderr);
                goto failed;
            }
            transferred = streamingRead_Examples_StreamingRead_SocketTaskWriteSession_GetResult(session);
            end = benchmark_nanoseconds();
            if (transferred == 0u || transferred > requested) {
                cx_memory_test_allocation_tracking_set(CX_FALSE);
                fprintf(stderr, "measured socket write returned an invalid count (%u)\n",
                    (unsigned)transferred);
                goto failed;
            }
            if (transferred < requested) ++partial_operations;
            elapsed += end - start;
            transferred_total += transferred;
            if (!drain_peer_bytes(transferred)) {
                cx_memory_test_allocation_tracking_set(CX_FALSE);
                fputs("could not drain measured partial socket write\n", stderr);
                goto failed;
            }
        }
        allocations = cx_memory_test_allocation_count_get() - allocation_start;
        allocated_bytes = cx_memory_test_allocation_bytes_get() - bytes_start;
        cx_memory_test_allocation_tracking_set(CX_FALSE);
        if (sample == 1u) {
            expected_allocations = allocations;
            expected_bytes = allocated_bytes;
        } else if (allocations != expected_allocations || allocated_bytes != expected_bytes) {
            fprintf(stderr,
                "partial write allocation totals varied by sample (%zu/%zu vs %zu/%zu)\n",
                allocations, allocated_bytes, expected_allocations, expected_bytes);
            goto failed;
        }
#ifndef _WIN32
        if (partial_operations == 0u) {
            fputs("Linux socket write workload did not produce a partial transfer\n", stderr);
            goto failed;
        }
#endif
        printf("INFO socket_write_task_uint sample=%u operations=%u requested=%u "
            "partial_operations=%u "
            "average_transferred=%.2f allocations_per_operation=%.2f "
            "bytes_per_operation=%.2f ns_per_operation=%.2f\n",
            sample, operation_count, (unsigned)requested, partial_operations,
            (double)transferred_total / operation_count,
            (double)allocations / operation_count,
            (double)allocated_bytes / operation_count,
            (double)elapsed / operation_count);
    }

    if (expected_allocations != operation_count * 7u ||
        expected_bytes != operation_count * (sizeof(void *) == 8u ? 400u : 228u)) {
        fprintf(stderr, "Socket.WriteAsync allocation budget changed (%zu/%zu for %u operations)\n",
            expected_allocations, expected_bytes, operation_count);
        goto failed;
    }

    cxnet_System_Net_Sockets_Socket_Dispose(socket);
    close_sockets();
    puts("PASS generated Socket.WriteAsync returned valid counts with stable allocation totals");
    return 0;

failed:
    cxnet_System_Net_Sockets_Socket_Dispose(socket);
    close_sockets();
    return 2;
}

static int run_inline_allocation_case(void) {
    struct streamingRead_Examples_StreamingRead_StreamingReadSession *session =
        streamingRead_Examples_StreamingRead_StreamingReadSession_CreateImmediate(64);
    size_t start;
    size_t move_allocations;
    size_t dispose_allocations;
    int index;

    /* Enumerator and reusable source creation are intentionally outside the probe. */
    if (!streamingRead_Examples_StreamingRead_StreamingReadSession_MoveNextAsync(session) ||
        !streamingRead_Examples_StreamingRead_StreamingReadSession_MoveResult(session)) {
        fputs("immediate iterator did not produce its first item inline\n", stderr);
        goto failed;
    }

    start = cx_memory_test_allocation_count_get();
    cx_memory_test_allocation_tracking_set(CX_TRUE);
    for (index = 1; index < 64; index++) {
        if (!streamingRead_Examples_StreamingRead_StreamingReadSession_MoveNextAsync(session) ||
            !streamingRead_Examples_StreamingRead_StreamingReadSession_MoveResult(session)) {
            cx_memory_test_allocation_tracking_set(CX_FALSE);
            fputs("immediate iterator move was not completed inline\n", stderr);
            goto failed;
        }
    }
    if (!streamingRead_Examples_StreamingRead_StreamingReadSession_MoveNextAsync(session) ||
        streamingRead_Examples_StreamingRead_StreamingReadSession_MoveResult(session)) {
        cx_memory_test_allocation_tracking_set(CX_FALSE);
        fputs("immediate iterator did not terminate inline\n", stderr);
        goto failed;
    }
    move_allocations = cx_memory_test_allocation_count_get() - start;
    cx_memory_test_allocation_tracking_set(CX_FALSE);

    start = cx_memory_test_allocation_count_get();
    cx_memory_test_allocation_tracking_set(CX_TRUE);
    streamingRead_Examples_StreamingRead_StreamingReadSession_Dispose(session);
    dispose_allocations = cx_memory_test_allocation_count_get() - start;
    cx_memory_test_allocation_tracking_set(CX_FALSE);
    if (move_allocations != 0 || dispose_allocations != 0) {
        fprintf(stderr, "completed iterator allocated: moves=%zu dispose=%zu\n",
            move_allocations, dispose_allocations);
        return 4;
    }
    puts("PASS 64 completed moves and completed disposal allocate zero CX objects");
    return 0;

failed:
    streamingRead_Examples_StreamingRead_StreamingReadSession_Dispose(session);
    return 4;
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    __cx_module_init_CXNET();
    if (!validate_async_iterator_reflection_and_public_header()) return 9;
    int result = run_eof_case();
    if (result != 0) return 10 + result;
    result = run_retained_heap_case();
    if (result != 0) return 15 + result;
    result = run_early_dispose_case();
    if (result != 0) return 20 + result;
    result = run_public_task_socket_case();
    if (result != 0) return 28 + result;
    result = run_cancel_case();
    if (result != 0) return 30 + result;
    result = run_public_task_socket_case();
    if (result != 0) return 38 + result;
    queued_cancel_callback_count = 0;
    result = run_queued_readiness_cancel_case();
    if (result != 0) return 35 + result;
    queued_cancel_callback_count = 0;
    result = run_reset_order_reuse_case();
    if (result != 0) return 45 + result;
    result = run_public_task_socket_case();
    if (result != 0) return 38 + result;
    result = run_partial_write_benchmark();
    if (result != 0) return 41 + result;
    result = run_inline_allocation_case();
    if (result != 0) return 40 + result;
    return 0;
}

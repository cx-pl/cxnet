#include <stdio.h>
#include <string.h>

#include "cxnet_socket_runtime.h"

extern size_t cx_task_scheduler_test_item_size(void);

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
static SOCKET sockets[2] = { INVALID_SOCKET, INVALID_SOCKET };
static int create_socket_pair(void) {
    WSADATA data;
    SOCKET listener = INVALID_SOCKET;
    struct sockaddr_in address;
    int address_length = (int)sizeof(address);
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return 0;
    listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET) return 0;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
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
static void close_socket_pair(void) {
    if (sockets[0] != INVALID_SOCKET) closesocket(sockets[0]);
    if (sockets[1] != INVALID_SOCKET) closesocket(sockets[1]);
    WSACleanup();
}
static int send_byte(void) {
    const char value = 'x';
    return send(sockets[1], &value, 1, 0) == 1;
}
static int consume_byte(void) {
    char value;
    return recv(sockets[0], &value, 1, 0) == 1;
}
#else
#include <sys/socket.h>
#include <unistd.h>
static int sockets[2] = {-1, -1};
static int create_socket_pair(void) {
    return socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0;
}
static void close_socket_pair(void) {
    if (sockets[0] >= 0) close(sockets[0]);
    if (sockets[1] >= 0) close(sockets[1]);
}
static int send_byte(void) {
    const char value = 'x';
    return send(sockets[1], &value, 1, 0) == 1;
}
static int consume_byte(void) {
    char value;
    return recv(sockets[0], &value, 1, 0) == 1;
}
#endif

static cx_uint callback_count;
static cx_uint timer_count;
static cx_uint shutdown_count;
static cx_uint shutdown_post_count;
static cx_uint interleave_order[2];
static cx_uint interleave_order_count;
static cx_uint canceled_interleave_count;
static cx_uint captured_value;
struct socket_capture_context { cx_uint *target; };
static void socket_ready(void *environment) {
    (void)environment;
    ++callback_count;
}
static void timer_ready(void *environment) {
    (void)environment;
    ++timer_count;
}
static void shutdown_post_ready(void *environment);
static void shutdown_ready(void *environment) {
    (void)environment;
    ++shutdown_count;
    {
        cx_action continuation = {0};
        continuation.__function = shutdown_post_ready;
        cxcore_System_Threading_Tasks_TaskScheduler_Post(continuation);
    }
}
static void shutdown_post_ready(void *environment) {
    (void)environment;
    ++shutdown_post_count;
}
static void socket_ready_capture(void *environment) {
    struct socket_capture_context *context =
        (struct socket_capture_context *)environment;
    captured_value = context->target == NULL ? 0 : *context->target;
}
static void interleave_timer(void *environment) {
    (void)environment;
    if (interleave_order_count < 2u) {
        interleave_order[interleave_order_count] = 1u;
    }
    ++interleave_order_count;
    if (!send_byte()) ++canceled_interleave_count;
}
static void interleave_socket_ready(void *environment) {
    (void)environment;
    if (interleave_order_count < 2u) {
        interleave_order[interleave_order_count] = 2u;
    }
    ++interleave_order_count;
}
static void interleave_canceled(void *environment) {
    (void)environment;
    ++canceled_interleave_count;
}

#ifdef _MSC_VER
__declspec(noinline)
#else
__attribute__((noinline))
#endif
static cx_action create_socket_capture_action(void) {
    struct socket_capture_context *context =
        (struct socket_capture_context *)cxcore_System_Memory_Alloc((cx_uint)sizeof(*context));
    cx_action action = {0};
    context->target = (cx_uint *)cxcore_System_Memory_Alloc((cx_uint)sizeof(*context->target));
    *context->target = 0x18c0ffeeu;
    action.__function = socket_ready_capture;
    action.__environment = context;
    action.__target = (cx_ptr)context;
    return action;
}

int main(void) {
    // The x64 item includes the logical-arena pointer carried by retained
    // callbacks; the corresponding x86 size remains unchanged by alignment.
    size_t expected_item_size = sizeof(void *) == 8u ? 128u : 72u;
    if (cx_task_scheduler_test_item_size() != expected_item_size) {
        fprintf(stderr, "unexpected scheduler item size: %u bytes (expected %u)\n",
            (unsigned)cx_task_scheduler_test_item_size(),
            (unsigned)expected_item_size);
        return 1;
    }
    cx_action continuation = {0};
    cx_gc_check_thread();
    if (!create_socket_pair()) {
        fputs("could not create local socket pair\n", stderr);
        return 1;
    }
    continuation.__function = socket_ready;
    continuation.__environment = NULL;
    continuation.__target = NULL;
    continuation.__method = NULL;
    continuation.__invocation_list = NULL;
    cxnet_socket_post_on_readiness((uintptr_t)sockets[0], 0, continuation, (cx_action){0});
    if (!send_byte() || callback_count != 0 ||
        !cxcore_System_Threading_Tasks_TaskScheduler_RunOne() ||
        callback_count != 1 ||
        cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
        fputs("scheduler did not resume exactly once on socket readability\n", stderr);
        close_socket_pair();
        return 1;
    }
    {
        uint64_t registration_id = cxnet_socket_post_on_readiness(
            (uintptr_t)sockets[0], 0, continuation, (cx_action){0});
        if (registration_id == 0 ||
            !cxnet_socket_cancel_wait(registration_id) ||
            cxnet_socket_cancel_wait(registration_id) ||
            !send_byte() ||
            cxcore_System_Threading_Tasks_TaskScheduler_RunOne() ||
            callback_count != 1 || !consume_byte() || !consume_byte()) {
            fputs("scheduler did not remove a canceled socket wait\n", stderr);
            close_socket_pair();
            return 1;
        }
    }
    {
        cx_action timer = {0};
        cx_action read_continuation = continuation;
        uint64_t read_id;
        timer.__function = timer_ready;
        read_id = cxnet_socket_post_on_readiness(
            (uintptr_t)sockets[0], 0, read_continuation, (cx_action){0});
        cxcore_System_Threading_Tasks_TaskScheduler_PostAfter(
            1u, timer);
        if (read_id == 0 ||
            !cxcore_System_Threading_Tasks_TaskScheduler_RunOne() ||
            timer_count != 1 || callback_count != 1 ||
            !send_byte() ||
            !cxcore_System_Threading_Tasks_TaskScheduler_RunOne() ||
            timer_count != 1 || callback_count != 2) {
            fprintf(stderr,
                "scheduler timer/socket interleave failed (id=%llu, timers=%u, callbacks=%u)\n",
                (unsigned long long)read_id, (unsigned)timer_count,
                (unsigned)callback_count);
            close_socket_pair();
            return 1;
        }
        (void)read_id;
    }
    {
        cx_action read_continuation = continuation;
        cx_action write_continuation = continuation;
        cxnet_socket_post_on_readiness(
            (uintptr_t)sockets[0], 0, read_continuation, (cx_action){0});
        cxnet_socket_post_on_readiness(
            (uintptr_t)sockets[0], 1, write_continuation, (cx_action){0});
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne() ||
            !cxcore_System_Threading_Tasks_TaskScheduler_RunOne() ||
            callback_count != 4) {
            fputs("scheduler did not dispatch simultaneous socket interests\n", stderr);
            close_socket_pair();
            return 1;
        }
    }
    {
        cx_action combined_continuation = continuation;
        uint32_t combined_ready_mask = 0u;
        uint64_t combined_id = cxnet_socket_post_on_readiness_mask(
            (uintptr_t)sockets[0],
            CXNET_SOCKET_READABLE | CXNET_SOCKET_WRITABLE,
            &combined_ready_mask,
            combined_continuation, (cx_action){0});
        if (combined_id == 0 ||
            cxnet_socket_post_on_readiness_mask((uintptr_t)sockets[0], 0u,
                NULL, combined_continuation, (cx_action){0}) != 0 ||
            cxnet_socket_post_on_readiness_mask((uintptr_t)sockets[0], 4u,
                NULL, combined_continuation, (cx_action){0}) != 0 ||
            !cxcore_System_Threading_Tasks_TaskScheduler_RunOne() ||
            callback_count != 5 ||
            (combined_ready_mask &
                (CXNET_SOCKET_READABLE | CXNET_SOCKET_WRITABLE)) !=
                (CXNET_SOCKET_READABLE | CXNET_SOCKET_WRITABLE)) {
            fputs("scheduler did not dispatch combined read/write interest once\n", stderr);
            close_socket_pair();
            return 1;
        }
    }
    {
        cx_action captured_action = create_socket_capture_action();
        captured_value = 0;
        cxnet_socket_post_on_readiness(
            (uintptr_t)sockets[0], 0, captured_action, (cx_action){0});
        captured_action.__environment = NULL;
        captured_action.__target = NULL;
        cx_gc_collect();
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne() ||
            captured_value != 0x18c0ffeeu) {
            fputs("socket readiness lost its captured GC object\n", stderr);
            close_socket_pair();
            return 1;
        }
    }
    {
        cx_action timer = {0};
        cx_action read = {0};
        cx_action canceled = {0};
        uint64_t canceled_id;
        interleave_order_count = 0;
        canceled_interleave_count = 0;
        if (!consume_byte()) {
            fputs("scheduler interleave fixture did not consume its prior byte\n", stderr);
            close_socket_pair();
            return 1;
        }
        timer.__function = interleave_timer;
        read.__function = interleave_socket_ready;
        canceled.__function = interleave_canceled;
        cxnet_socket_post_on_readiness(
            (uintptr_t)sockets[0], 0, read, (cx_action){0});
        canceled_id = cxnet_socket_post_on_readiness(
            (uintptr_t)sockets[0], 0, canceled, (cx_action){0});
        if (canceled_id == 0 ||
            !cxnet_socket_cancel_wait(canceled_id)) {
            fputs("scheduler failed to cancel the interleaved socket wait\n", stderr);
            close_socket_pair();
            return 1;
        }
        cxcore_System_Threading_Tasks_TaskScheduler_PostAfter(1u, timer);
        cx_gc_collect();
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne() ||
            !cxcore_System_Threading_Tasks_TaskScheduler_RunOne() ||
            interleave_order_count != 2u || interleave_order[0] != 1u ||
            interleave_order[1] != 2u || canceled_interleave_count != 0u ||
            !consume_byte()) {
            fprintf(stderr,
                "timer/readiness/cancellation order changed under collection "
                "(count=%u, order=%u,%u, canceled=%u)\n",
                (unsigned)interleave_order_count, (unsigned)interleave_order[0],
                (unsigned)interleave_order[1],
                (unsigned)canceled_interleave_count);
            close_socket_pair();
            return 1;
        }
    }
    {
        cx_action shutdown_continuation = {0};
        cx_action timer = {0};
        shutdown_continuation.__function = shutdown_ready;
        timer.__function = timer_ready;
        shutdown_count = 0;
        shutdown_post_count = 0;
        cxcore_System_Threading_Tasks_TaskScheduler_PostAfter(
            3600000u, timer);
        if (cxnet_socket_post_on_readiness(
                (uintptr_t)sockets[0], 0, continuation,
                shutdown_continuation) == 0) {
            fputs("scheduler failed to register pending shutdown I/O\n", stderr);
            close_socket_pair();
            return 1;
        }
        cxcore_System_Threading_Tasks_TaskScheduler_Shutdown();
        if (shutdown_count != 1 || shutdown_post_count != 1 ||
            timer_count != 1 || callback_count != 5) {
            fprintf(stderr,
                "scheduler shutdown did not drain posted/canceled work "
                "(shutdown=%u, posted=%u, timers=%u, callbacks=%u)\n",
                (unsigned)shutdown_count, (unsigned)shutdown_post_count,
                (unsigned)timer_count,
                (unsigned)callback_count);
            close_socket_pair();
            return 1;
        }
    }
    {
        struct cx_exception_frame frame;
        cx_exception_push(&frame);
        if (setjmp(frame.environment) == 0) {
            cxcore_System_Threading_Tasks_TaskScheduler_Post(continuation);
            cx_exception_pop(&frame);
            fputs("scheduler accepted work after shutdown completed\n", stderr);
            close_socket_pair();
            return 1;
        }
        cx_exception_pop(&frame);
        cx_exception_clear();
    }
    close_socket_pair();
    cx_gc_collect();
    return 0;
}

#include "cxnet.internal.h"
#include "SocketTestResolver.internal.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <time.h>
#include <unistd.h>
#endif

static volatile long resolver_started;
static volatile long resolver_finished;
static volatile long task_completed;
static volatile long resolver_release;

extern void cxcore_System_String___constructor__2(
    struct cxcore_System_String *value, cx_uint length, cx_ptr data);

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

static long read_counter(volatile long *value) {
#ifdef _WIN32
    return InterlockedCompareExchange((volatile LONG *)value, 0, 0);
#else
    return __sync_val_compare_and_swap(value, 0, 0);
#endif
}

static void increment_counter(volatile long *value) {
#ifdef _WIN32
    InterlockedIncrement((volatile LONG *)value);
#else
    __sync_add_and_fetch(value, 1);
#endif
}

static int shutdown_resolver(const void *host,
    struct cxnet_resolved_address *addresses, uint32_t capacity,
    uint32_t *count, int *error_code, void *context) {
    (void)context;
#ifdef _WIN32
    if (wcscmp((const wchar_t *)host,
            L"cxnet-test-resolution-shutdown.invalid") != 0) return 0;
#else
    if (strcmp((const char *)host,
            "cxnet-test-resolution-shutdown.invalid") != 0) return 0;
#endif
    increment_counter(&resolver_started);
    while (read_counter(&resolver_release) == 0) {
#ifdef _WIN32
        Sleep(1u);
#else
        usleep(1000u);
#endif
    }
    if (capacity < 1u) return 0;
    memset(addresses, 0, sizeof(*addresses));
    addresses[0].address_family = 2u;
    addresses[0].word0 = 0x7f000001u;
    *count = 1u;
    *error_code = 0;
    increment_counter(&resolver_finished);
    return 1;
}

static void on_task_completed(void *environment) {
    (void)environment;
    increment_counter(&task_completed);
}

int main(void) {
    struct cxcore_System_Threading_Tasks_Task *task;
    struct cx_gc_root_handle *task_root;
    cx_action completion = {0};
    uint32_t wait_steps = 0u;

    cx_gc_check_thread();
    cxnet_test_resolver_set_callback(shutdown_resolver, NULL);
    task = cxnet_System_Net_Dns_GetHostAddressesAsync(
        make_string("cxnet-test-resolution-shutdown.invalid"));
    task_root = cx_gc_add_root(&task, (cx_uint)sizeof(task));
    completion.__function = on_task_completed;
    completion.__environment = NULL;
    completion.__target = NULL;
    cxcore_System_Threading_Tasks_Task_OnCompleted(task, completion);
    cx_gc_collect();

    while (read_counter(&resolver_started) == 0 && wait_steps < 2000u) {
#ifdef _WIN32
        Sleep(1u);
#else
        usleep(1000u);
#endif
        wait_steps++;
    }
    if (read_counter(&resolver_started) == 0) {
        fputs("async DNS resolver did not start before scheduler shutdown\n", stderr);
        cx_gc_remove_root(task_root);
        return 1;
    }

    cxcore_System_Threading_Tasks_TaskScheduler_Shutdown();
    cx_gc_collect();
    increment_counter(&resolver_release);
    wait_steps = 0u;
    while (read_counter(&resolver_finished) == 0 && wait_steps < 2000u) {
#ifdef _WIN32
        Sleep(1u);
#else
        usleep(1000u);
#endif
        wait_steps++;
    }
#ifdef _WIN32
    Sleep(20u);
#else
    usleep(20000u);
#endif
    cx_gc_collect();

    if (read_counter(&resolver_finished) == 0 || task->_isCompleted ||
        read_counter(&task_completed) != 0) {
        fprintf(stderr,
            "async DNS shutdown handoff failed: started=%ld finished=%ld task=%u callbacks=%ld\n",
            read_counter(&resolver_started), read_counter(&resolver_finished),
            task->_isCompleted, read_counter(&task_completed));
        cx_gc_remove_root(task_root);
        return 2;
    }
    cx_gc_remove_root(task_root);
    cx_gc_collect();
    return 0;
}

#include "cxnet.internal.h"
#include "cxruntime.h"
#include "cxgc.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
static uint64_t benchmark_now_ns(void) {
    LARGE_INTEGER counter;
    LARGE_INTEGER frequency;
    if (!QueryPerformanceCounter(&counter) ||
        !QueryPerformanceFrequency(&frequency) ||
        counter.QuadPart < 0 || frequency.QuadPart <= 0) {
        fputs("could not read the monotonic benchmark clock\n", stderr);
        exit(EXIT_FAILURE);
    }
    return ((uint64_t)counter.QuadPart / (uint64_t)frequency.QuadPart) *
            UINT64_C(1000000000) +
        (((uint64_t)counter.QuadPart % (uint64_t)frequency.QuadPart) *
            UINT64_C(1000000000)) / (uint64_t)frequency.QuadPart;
}
static void benchmark_yield(void) { (void)SwitchToThread(); }
#else
#include <sched.h>
#include <time.h>
static uint64_t benchmark_now_ns(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0 || now.tv_sec < 0) {
        fputs("could not read the monotonic benchmark clock\n", stderr);
        exit(EXIT_FAILURE);
    }
    return (uint64_t)now.tv_sec * UINT64_C(1000000000) +
        (uint64_t)now.tv_nsec;
}
static void benchmark_yield(void) { (void)sched_yield(); }
#endif

#define CX_BENCH_BUFFER_BYTES 8192u
#define CX_BENCH_RESPONSE_TASK_LIMIT 10000000u

extern void cxcore_System_String___constructor__2(
    struct cxcore_System_String *value, cx_uint length, cx_ptr data);

static struct cxcore_System_String *make_string(const char *text) {
    size_t length = strlen(text);
    cx_ptr data = cxcore_System_Memory_Alloc((cx_uint)length);
    struct cxcore_System_String *value =
        (struct cxcore_System_String *)cxcore_System_Memory_Alloc(
            cxcore_System_String___typeinfo.Size);
    if (length != 0u) memcpy(data, text, length);
    cxcore_System_String___constructor__2(value, (cx_uint)length, data);
    return value;
}

static struct cxcore_System_Uri *make_uri(const char *text) {
    struct cxcore_System_Uri *value =
        (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
            cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(value, make_string(text));
    return value;
}

static int pump_task(struct cxcore_System_Threading_Tasks_Task *task) {
    uint32_t attempts = 0u;
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        attempts < CX_BENCH_RESPONSE_TASK_LIMIT) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
            benchmark_yield();
        }
        attempts++;
    }
    return cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        !task->_isFaulted && !task->_isCanceled;
}

static int pump_read(
    struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *task) {
    uint32_t attempts = 0u;
    while (!task->_isCompleted && attempts < CX_BENCH_RESPONSE_TASK_LIMIT) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
            benchmark_yield();
        }
        attempts++;
    }
    return task->_isCompleted && !task->_isFaulted && !task->_isCanceled;
}

struct request_measurement {
    uint64_t elapsed_ns;
    uint64_t headers_ns;
    uint64_t first_byte_ns;
    uint64_t body_ns;
    uint64_t bytes;
};

static int read_stream(
    struct cxcore_System_IO_Stream *stream,
    struct cxcore_System_Collections_Array *buffer,
    uint8_t *buffer_data,
    uint32_t expected_body_bytes,
    struct request_measurement *measurement) {
    uint32_t offset = 0u;
    uint64_t started = benchmark_now_ns();
    uint64_t first_byte_time = 0u;
    while (1) {
        struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *read_task =
            cxcore_System_IO_Stream_ReadAsync__2(
                stream, buffer, 0u, CX_BENCH_BUFFER_BYTES);
        uint32_t count;
        uint32_t index;
        if (!pump_read(read_task)) {
            fputs("stream body read failed during benchmark\n", stderr);
            return 0;
        }
        count = cx_generic_32f22edd4c9ba40c64ed2658909ec8fb1d6e85d2c4276d46eebd01030eefa80f(
            read_task);
        if (count == 0u) break;
        if (offset == 0u) first_byte_time = benchmark_now_ns();
        if (count > expected_body_bytes - offset) {
            fputs("HTTP benchmark response exceeded its declared body\n", stderr);
            return 0;
        }
        for (index = 0u; index < count; ++index) {
            uint8_t expected = (uint8_t)(((offset + index) * 31u + 17u) & 0xffu);
            if (buffer_data[index] != expected) {
                fputs("HTTP benchmark response payload did not match\n", stderr);
                return 0;
            }
        }
        offset += count;
    }
    measurement->bytes = offset;
    measurement->body_ns = benchmark_now_ns() - started;
    measurement->first_byte_ns = first_byte_time == 0u
        ? 0u : first_byte_time - started;
    if (offset != expected_body_bytes) {
        fprintf(stderr, "HTTP benchmark body length=%u expected=%u\n",
            offset, expected_body_bytes);
        return 0;
    }
    return 1;
}

static int measure_request(
    struct cxnet_System_Net_Http_HttpClient *client,
    struct cxcore_System_Uri *uri,
    struct cxcore_System_Collections_Array *buffer,
    uint8_t *buffer_data,
    uint32_t expected_body_bytes,
    struct request_measurement *measurement) {
    struct cxcore_System_Threading_Tasks_Task *task;
    struct cxcore_System_IO_Stream *stream;
    struct cx_gc_root_handle *task_root;
    struct cx_gc_root_handle *stream_root;
    uint64_t started = benchmark_now_ns();
    memset(measurement, 0, sizeof(*measurement));
    task = cxnet_System_Net_Http_HttpClient_GetStreamAsync__2(client, uri);
    task_root = cx_gc_add_root(&task, (cx_uint)sizeof(task));
    if (!pump_task(task)) {
        fputs("GetStreamAsync failed during HTTP benchmark\n", stderr);
        cx_gc_remove_root(task_root);
        return 0;
    }
    measurement->headers_ns = benchmark_now_ns() - started;
    stream = cx_generic_b7a000e5a1e82c2cc28bb7d06efab618c555997e8e29f1d0e05edd0237738424(
        task);
    stream_root = cx_gc_add_root(&stream, (cx_uint)sizeof(stream));
    if (stream == NULL || !read_stream(stream, buffer, buffer_data,
            expected_body_bytes, measurement)) {
        cx_gc_remove_root(stream_root);
        cx_gc_remove_root(task_root);
        return 0;
    }
    measurement->elapsed_ns = benchmark_now_ns() - started;
    cxnet_System_Net_Http_ResponseOwnedStream_Dispose(
        (struct cxnet_System_Net_Http_ResponseOwnedStream *)stream);
    stream = NULL;
    task = NULL;
    cx_gc_remove_root(stream_root);
    cx_gc_remove_root(task_root);
    return 1;
}

static int measure_cancellation(
    uint64_t *cancellation_latency_ns,
    uint32_t *cancellation_prefix_bytes) {
    const char *base = getenv("CXNET_HTTP_BENCH_BASE_URL");
    char url[1024];
    struct cxnet_System_Net_Http_HttpClient *client;
    struct cxcore_System_Threading_Tasks_CancellationTokenSource *source;
    struct cxcore_System_Threading_Tasks_CancellationToken token;
    struct cxcore_System_Uri *uri;
    struct cxcore_System_Collections_Array *buffer;
    struct cxcore_System_IO_Stream *stream;
    struct cxcore_System_Threading_Tasks_Task *response_task;
    struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *read_task;
    struct cx_gc_root_handle *client_root;
    struct cx_gc_root_handle *source_root;
    struct cx_gc_root_handle *uri_root;
    struct cx_gc_root_handle *buffer_root;
    struct cx_gc_root_handle *response_root;
    struct cx_gc_root_handle *stream_root;
    int written;
    uint32_t attempts = 0u;
    uint64_t cancel_started;
    if (base == NULL) return 0;
    written = snprintf(url, sizeof(url), "%s/cancel", base);
    if (written < 0 || (size_t)written >= sizeof(url)) return 0;
    client = (struct cxnet_System_Net_Http_HttpClient *)cxcore_System_Memory_Alloc(
        cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    source = (struct cxcore_System_Threading_Tasks_CancellationTokenSource *)
        cxcore_System_Memory_Alloc(
            cxcore_System_Threading_Tasks_CancellationTokenSource___typeinfo.Size);
    uri = make_uri(url);
    buffer = cx_array_with_type(cx_array_new(CX_BENCH_BUFFER_BYTES,
        (cx_uint)sizeof(cx_byte)),
        "cx_generic_9c4b0dcf5365274a639812f507806a7475845f1a6e6b8c085532c41d4210ef4b");
    client_root = cx_gc_add_root(&client, (cx_uint)sizeof(client));
    source_root = cx_gc_add_root(&source, (cx_uint)sizeof(source));
    uri_root = cx_gc_add_root(&uri, (cx_uint)sizeof(uri));
    buffer_root = cx_gc_add_root(&buffer, (cx_uint)sizeof(buffer));
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    cxcore_System_Threading_Tasks_CancellationTokenSource___constructor(source);
    token = cxcore_System_Threading_Tasks_CancellationTokenSource_Token___const_get(
        source);
    response_task = cxnet_System_Net_Http_HttpClient_GetStreamAsync(
        client, uri, token);
    response_root = cx_gc_add_root(&response_task, (cx_uint)sizeof(response_task));
    if (!pump_task(response_task)) {
        fputs("cancellation benchmark did not receive response headers\n", stderr);
        return 0;
    }
    stream = cx_generic_b7a000e5a1e82c2cc28bb7d06efab618c555997e8e29f1d0e05edd0237738424(
        response_task);
    stream_root = cx_gc_add_root(&stream, (cx_uint)sizeof(stream));
    read_task = cxcore_System_IO_Stream_ReadAsync__2(stream, buffer, 0u, 4u);
    if (!pump_read(read_task) ||
        cx_generic_32f22edd4c9ba40c64ed2658909ec8fb1d6e85d2c4276d46eebd01030eefa80f(
            read_task) != 4u ||
        memcmp(cx_array_at(buffer, 0u, (cx_uint)sizeof(cx_byte)), "pong", 4u) != 0) {
        fputs("cancellation benchmark did not receive its body prefix\n", stderr);
        return 0;
    }
    *cancellation_prefix_bytes = 4u;
    read_task = cxcore_System_IO_Stream_ReadAsync__2(stream, buffer, 0u, 4u);
    cancel_started = benchmark_now_ns();
    cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(source);
    while (!read_task->_isCompleted &&
        attempts < CX_BENCH_RESPONSE_TASK_LIMIT) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
            benchmark_yield();
        }
        attempts++;
    }
    if (!read_task->_isCompleted) {
        fputs("cancellation benchmark did not complete within its scheduler bound\n",
            stderr);
        return 0;
    }
    *cancellation_latency_ns = benchmark_now_ns() - cancel_started;
    if (!read_task->_isCanceled) {
        fputs("cancellation benchmark pending read did not cancel\n", stderr);
        return 0;
    }
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    cxnet_System_Net_Http_ResponseOwnedStream_Dispose(
        (struct cxnet_System_Net_Http_ResponseOwnedStream *)stream);
    client = NULL;
    source = NULL;
    uri = NULL;
    buffer = NULL;
    response_task = NULL;
    stream = NULL;
    cx_gc_remove_root(stream_root);
    cx_gc_remove_root(response_root);
    cx_gc_remove_root(buffer_root);
    cx_gc_remove_root(uri_root);
    cx_gc_remove_root(source_root);
    cx_gc_remove_root(client_root);
    return 1;
}

static int parse_positive(const char *text, uint32_t *value) {
    char *end = NULL;
    unsigned long parsed;
    if (text == NULL || *text == '\0') return 0;
    parsed = strtoul(text, &end, 10);
    if (end == text || *end != '\0' || parsed == 0u || parsed > UINT32_MAX) {
        return 0;
    }
    *value = (uint32_t)parsed;
    return 1;
}

int main(int argc, char **argv) {
    const char *base = getenv("CXNET_HTTP_BENCH_BASE_URL");
    uint32_t requests = 20u;
    uint32_t samples = 5u;
    uint32_t body_bytes = 1024u * 1024u;
    char body_url[1024];
    struct cxnet_System_Net_Http_HttpClient *client;
    struct cxcore_System_Uri *uri;
    struct cxcore_System_Collections_Array *buffer;
    struct cx_gc_root_handle *client_root;
    struct cx_gc_root_handle *uri_root;
    struct cx_gc_root_handle *buffer_root;
    uint8_t *buffer_data;
    uint32_t sample;
    uint32_t request;
    int written;
    if (argc > 1 && !parse_positive(argv[1], &requests)) return 2;
    if (argc > 2 && !parse_positive(argv[2], &samples)) return 2;
    if (argc > 3 && !parse_positive(argv[3], &body_bytes)) return 2;
    if (base == NULL) {
        fputs("CXNET_HTTP_BENCH_BASE_URL is required\n", stderr);
        return 2;
    }
    if (body_bytes > 64u * 1024u * 1024u) {
        fputs("benchmark body size exceeds 64 MiB\n", stderr);
        return 2;
    }
    written = snprintf(body_url, sizeof(body_url), "%s/body", base);
    if (written < 0 || (size_t)written >= sizeof(body_url)) return 2;
    cx_gc_check_thread();
    client = (struct cxnet_System_Net_Http_HttpClient *)cxcore_System_Memory_Alloc(
        cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    uri = make_uri(body_url);
    buffer = cx_array_with_type(cx_array_new(CX_BENCH_BUFFER_BYTES,
        (cx_uint)sizeof(cx_byte)),
        "cx_generic_9c4b0dcf5365274a639812f507806a7475845f1a6e6b8c085532c41d4210ef4b");
    client_root = cx_gc_add_root(&client, (cx_uint)sizeof(client));
    uri_root = cx_gc_add_root(&uri, (cx_uint)sizeof(uri));
    buffer_root = cx_gc_add_root(&buffer, (cx_uint)sizeof(buffer));
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    buffer_data = (uint8_t *)cx_array_at(buffer, 0u, (cx_uint)sizeof(cx_byte));
    for (request = 0u; request < 2u; ++request) {
        struct request_measurement warmup;
        if (!measure_request(client, uri, buffer, buffer_data,
                body_bytes, &warmup)) return 3;
    }
    cx_gc_collect();
    puts("sample,requests,body_bytes,elapsed_ns,headers_mean_ns,first_byte_mean_ns,body_mib_per_second,managed_allocations,managed_bytes,gc_collections,gc_collected_bytes,gc_pause_ns,heap_before_bytes,heap_after_gc_bytes");
    for (sample = 0u; sample < samples; ++sample) {
        size_t allocations_before;
        size_t allocation_bytes_before;
        size_t collections_before;
        size_t collected_before;
        uint64_t pause_before;
        size_t heap_before;
        size_t allocations_after;
        size_t allocation_bytes_after;
        size_t collections_after;
        size_t collected_after;
        uint64_t pause_after;
        size_t heap_after;
        uint64_t started = benchmark_now_ns();
        uint64_t headers_total = 0u;
        uint64_t first_byte_total = 0u;
        uint64_t bytes_total = 0u;
        uint64_t elapsed;
        cx_gc_collect();
        heap_before = cx_memory_benchmark_heap_bytes_get();
        allocations_before = cx_memory_benchmark_allocation_count_get();
        allocation_bytes_before = cx_memory_benchmark_allocation_bytes_get();
        collections_before = cx_memory_benchmark_collection_count_get();
        collected_before = cx_memory_benchmark_collected_bytes_get();
        pause_before = cx_memory_benchmark_collection_pause_ns_get();
        started = benchmark_now_ns();
        for (request = 0u; request < requests; ++request) {
            struct request_measurement current;
            if (!measure_request(client, uri, buffer, buffer_data,
                    body_bytes, &current)) return 3;
            headers_total += current.headers_ns;
            first_byte_total += current.first_byte_ns;
            bytes_total += current.bytes;
        }
        elapsed = benchmark_now_ns() - started;
        allocations_after = cx_memory_benchmark_allocation_count_get();
        allocation_bytes_after = cx_memory_benchmark_allocation_bytes_get();
        collections_after = cx_memory_benchmark_collection_count_get();
        collected_after = cx_memory_benchmark_collected_bytes_get();
        pause_after = cx_memory_benchmark_collection_pause_ns_get();
        cx_gc_collect();
        heap_after = cx_memory_benchmark_heap_bytes_get();
        printf("%u,%u,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%.3f,%zu,%zu,%zu,%zu,%" PRIu64 ",%zu,%zu\n",
            sample + 1u, requests, bytes_total, elapsed,
            headers_total / requests, first_byte_total / requests,
            (double)bytes_total * 1000000000.0 /
                (double)(elapsed == 0u ? 1u : elapsed) /
                (1024.0 * 1024.0),
            allocations_after - allocations_before,
            allocation_bytes_after - allocation_bytes_before,
            collections_after - collections_before,
            collected_after - collected_before,
            pause_after - pause_before, heap_before, heap_after);
    }
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    client = NULL;
    uri = NULL;
    buffer = NULL;
    cx_gc_remove_root(buffer_root);
    cx_gc_remove_root(uri_root);
    cx_gc_remove_root(client_root);
    cx_gc_collect();
    {
        uint64_t cancel_latency;
        uint32_t cancel_prefix;
        if (!measure_cancellation(&cancel_latency, &cancel_prefix)) return 4;
        fprintf(stderr, "cancel_latency_ns=%" PRIu64 ",prefix_bytes=%u\n",
            cancel_latency, cancel_prefix);
    }
    fprintf(stderr, "post_dispose_heap_bytes=%zu\n",
        cx_memory_benchmark_heap_bytes_get());
    return 0;
}

#include "cxnet.internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

extern void cxcore_System_String___constructor__2(
    struct cxcore_System_String *value, cx_uint length, cx_ptr data);

struct cancel_dispose_context {
    struct cxcore_System_Threading_Tasks_CancellationTokenSource *source;
    struct cxnet_System_Net_Http_HttpClient *client;
    int fired;
};

static void wait_for_scheduler_event(void) {
#ifdef _WIN32
    Sleep(1u);
#else
    struct timespec delay = {0, 1000000L};
    (void)nanosleep(&delay, NULL);
#endif
}

static struct cxcore_System_String *make_string(const char *text) {
    size_t length = strlen(text);
    cx_ptr data = length == 0u ? NULL :
        cxcore_System_Memory_Alloc((cx_uint)length);
    struct cxcore_System_String *value = (struct cxcore_System_String *)
        cxcore_System_Memory_Alloc(cxcore_System_String___typeinfo.Size);
    if (length != 0u) memcpy(data, text, length);
    cxcore_System_String___constructor__2(value, (cx_uint)length, data);
    return value;
}

static struct cxcore_System_Uri *make_uri(const char *text) {
    struct cxcore_System_Uri *uri = (struct cxcore_System_Uri *)
        cxcore_System_Memory_Alloc(cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri, make_string(text));
    return uri;
}

static int string_equals(const struct cxcore_System_String *value,
    const char *expected) {
    size_t length = strlen(expected);
    return value != NULL && value->_length == length && value->_data != NULL &&
        memcmp(value->_data, expected, length) == 0;
}

typedef struct cxcore_System_Threading_Tasks_Task *(*shared_content_send)(
    struct cxnet_System_Net_Http_HttpClient *client,
    struct cxcore_System_Uri *uri,
    struct cxnet_System_Net_Http_HttpContent *content);

static int run_shared_content_send_case(const char *base_url,
    const char *path, const char *method, const char *request_body,
    const char *expected_body, shared_content_send send) {
    char url[512];
    struct cxnet_System_Net_Http_HttpClient *client =
        (struct cxnet_System_Net_Http_HttpClient *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    struct cxnet_System_Net_Http_StringContent *request_content =
        (struct cxnet_System_Net_Http_StringContent *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_StringContent___typeinfo.Size);
    struct cxcore_System_Threading_Tasks_Task *task = NULL;
    struct cxnet_System_Net_Http_HttpResponseMessage *response = NULL;
    struct cx_gc_root_handle *client_root = cx_gc_add_root(
        &client, (cx_uint)sizeof(client));
    struct cx_gc_root_handle *request_content_root = cx_gc_add_root(
        &request_content, (cx_uint)sizeof(request_content));
    struct cx_gc_root_handle *task_root = cx_gc_add_root(
        &task, (cx_uint)sizeof(task));
    struct cx_gc_root_handle *response_root = cx_gc_add_root(
        &response, (cx_uint)sizeof(response));
    uint32_t steps = 0u;
    int length = snprintf(url, sizeof(url), "%s/%s", base_url, path);
    int passed = 0;
    if (length < 0 || (size_t)length >= sizeof(url) ||
        client_root == NULL || request_content_root == NULL ||
        task_root == NULL || response_root == NULL) {
        fprintf(stderr, "could not initialize shared HttpClient %s test state\n",
            method);
        goto cleanup;
    }
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    cxnet_System_Net_Http_StringContent___constructor(request_content,
        make_string(request_body));
    task = send(client, make_uri(url),
        (struct cxnet_System_Net_Http_HttpContent *)request_content);
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
            wait_for_scheduler_event();
        }
        steps++;
    }
    if (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) ||
        task->_isFaulted || task->_isCanceled) {
        fprintf(stderr, "shared HttpClient %s did not complete\n", method);
        goto cleanup;
    }
    response = cx_generic_640110dc1a526c1c94f059cac4ed8d246fe089fbe535a78bc1997c7adde4688b(
        task);
    if (response == NULL ||
        cxnet_System_Net_Http_HttpResponseMessage_StatusCode___get(response) !=
            cxnet_System_Net_Http_HttpStatusCode_OK ||
        !string_equals(cxnet_System_Net_Http_HttpContent_ReadAsString(
            cxnet_System_Net_Http_HttpResponseMessage_Content___get(response)),
            expected_body)) {
        fprintf(stderr, "shared HttpClient %s returned an incorrect response\n",
            method);
        goto cleanup;
    }
    passed = 1;

cleanup:
    if (response != NULL)
        cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
    if (request_content != NULL)
        cxnet_System_Net_Http_HttpContent_Dispose(
            (struct cxnet_System_Net_Http_HttpContent *)request_content);
    if (client != NULL) cxnet_System_Net_Http_HttpClient_Dispose(client);
    if (response_root != NULL) cx_gc_remove_root(response_root);
    if (task_root != NULL) cx_gc_remove_root(task_root);
    if (request_content_root != NULL) cx_gc_remove_root(request_content_root);
    if (client_root != NULL) cx_gc_remove_root(client_root);
    return passed;
}

static int run_shared_delete_case(const char *base_url) {
    char url[512];
    struct cxnet_System_Net_Http_HttpClient *client =
        (struct cxnet_System_Net_Http_HttpClient *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    struct cxcore_System_Threading_Tasks_Task *task = NULL;
    struct cxnet_System_Net_Http_HttpResponseMessage *response = NULL;
    struct cx_gc_root_handle *client_root = cx_gc_add_root(
        &client, (cx_uint)sizeof(client));
    struct cx_gc_root_handle *task_root = cx_gc_add_root(
        &task, (cx_uint)sizeof(task));
    struct cx_gc_root_handle *response_root = cx_gc_add_root(
        &response, (cx_uint)sizeof(response));
    uint32_t steps = 0u;
    int length = snprintf(url, sizeof(url), "%s/delete-echo", base_url);
    int passed = 0;
    if (length < 0 || (size_t)length >= sizeof(url) ||
        client_root == NULL || task_root == NULL || response_root == NULL) {
        fputs("could not initialize shared HttpClient DELETE test state\n",
            stderr);
        goto cleanup;
    }
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    task = cxnet_System_Net_Http_HttpClient_DeleteAsync__2(client,
        make_uri(url));
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne())
            wait_for_scheduler_event();
        steps++;
    }
    if (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) ||
        task->_isFaulted || task->_isCanceled) {
        fputs("shared HttpClient DELETE did not complete\n", stderr);
        goto cleanup;
    }
    response = cx_generic_640110dc1a526c1c94f059cac4ed8d246fe089fbe535a78bc1997c7adde4688b(
        task);
    if (response == NULL ||
        cxnet_System_Net_Http_HttpResponseMessage_StatusCode___get(response) !=
            cxnet_System_Net_Http_HttpStatusCode_OK ||
        !string_equals(cxnet_System_Net_Http_HttpContent_ReadAsString(
            cxnet_System_Net_Http_HttpResponseMessage_Content___get(response)),
            "DELETE:empty")) {
        fputs("shared HttpClient DELETE returned an incorrect response\n",
            stderr);
        goto cleanup;
    }
    passed = 1;

cleanup:
    if (response != NULL)
        cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
    if (client != NULL) cxnet_System_Net_Http_HttpClient_Dispose(client);
    if (response_root != NULL) cx_gc_remove_root(response_root);
    if (task_root != NULL) cx_gc_remove_root(task_root);
    if (client_root != NULL) cx_gc_remove_root(client_root);
    return passed;
}

static int run_shared_head_case(const char *base_url) {
    char url[512];
    struct cxnet_System_Net_Http_HttpClient *client =
        (struct cxnet_System_Net_Http_HttpClient *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    struct cxcore_System_Threading_Tasks_Task *task = NULL;
    struct cxnet_System_Net_Http_HttpResponseMessage *response = NULL;
    struct cx_gc_root_handle *client_root = cx_gc_add_root(
        &client, (cx_uint)sizeof(client));
    struct cx_gc_root_handle *task_root = cx_gc_add_root(
        &task, (cx_uint)sizeof(task));
    struct cx_gc_root_handle *response_root = cx_gc_add_root(
        &response, (cx_uint)sizeof(response));
    uint32_t steps = 0u;
    int length = snprintf(url, sizeof(url), "%s/head-echo", base_url);
    int passed = 0;
    if (length < 0 || (size_t)length >= sizeof(url) ||
        client_root == NULL || task_root == NULL || response_root == NULL) {
        fputs("could not initialize shared HttpClient HEAD test state\n",
            stderr);
        goto cleanup;
    }
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    task = cxnet_System_Net_Http_HttpClient_HeadAsync__2(client,
        make_uri(url));
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne())
            wait_for_scheduler_event();
        steps++;
    }
    if (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) ||
        task->_isFaulted || task->_isCanceled) {
        fputs("shared HttpClient HEAD did not complete\n", stderr);
        goto cleanup;
    }
    response = cx_generic_640110dc1a526c1c94f059cac4ed8d246fe089fbe535a78bc1997c7adde4688b(
        task);
    if (response == NULL ||
        cxnet_System_Net_Http_HttpResponseMessage_StatusCode___get(response) !=
            cxnet_System_Net_Http_HttpStatusCode_OK ||
        !string_equals(cxnet_System_Net_Http_HttpContent_ReadAsString(
            cxnet_System_Net_Http_HttpResponseMessage_Content___get(response)),
            "")) {
        fputs("shared HttpClient HEAD returned an incorrect response\n", stderr);
        goto cleanup;
    }
    passed = 1;

cleanup:
    if (response != NULL)
        cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
    if (client != NULL) cxnet_System_Net_Http_HttpClient_Dispose(client);
    if (response_root != NULL) cx_gc_remove_root(response_root);
    if (task_root != NULL) cx_gc_remove_root(task_root);
    if (client_root != NULL) cx_gc_remove_root(client_root);
    return passed;
}

static int run_request_framing_rejection_case(const char *header_name) {
    struct cxnet_System_Net_Http_HttpClient *client =
        (struct cxnet_System_Net_Http_HttpClient *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    struct cxnet_System_Net_Http_HttpRequestMessage *request =
        (struct cxnet_System_Net_Http_HttpRequestMessage *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpRequestMessage___typeinfo.Size);
    struct cxcore_System_Threading_Tasks_Task *task;
    struct cxcore_System_Exception *exception;
    uint32_t steps = 0u;

    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    cxnet_System_Net_Http_HttpRequestMessage___constructor(request,
        cxnet_System_Net_Http_HttpMethod_Get___const_get(),
        make_uri("http://127.0.0.1:1/no-network-request-expected"));
    if (!cxnet_System_Net_Http_HttpHeaders_TryAddWithoutValidation(
            cxnet_System_Net_Http_HttpRequestMessage_Headers___const_get(request),
            make_string(header_name), make_string("4"))) {
        fputs("could not construct shared framing regression case\n", stderr);
        cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        return 0;
    }
    task = cxnet_System_Net_Http_HttpClient_SendAsync(client, request);
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) break;
        steps++;
    }
    exception = task->_exception;
    if (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) ||
        !task->_isFaulted || task->_isCanceled || exception == NULL ||
        !string_equals(cxcore_System_Exception_Message___const_get(exception),
            "Request Content-Length and Transfer-Encoding headers are managed by the HTTP transport.")) {
        fprintf(stderr, "shared %s framing rejection was incorrect\n", header_name);
        cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        return 0;
    }
    cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    return 1;
}

static int run_https_case(const char *url, const char *ca_file,
    int expect_success, const char *expected_body) {
    struct cxnet_System_Net_Http_CurlHttpMessageHandler *handler =
        (struct cxnet_System_Net_Http_CurlHttpMessageHandler *)
            cxcore_System_Memory_Alloc(
                cxnet_System_Net_Http_CurlHttpMessageHandler___typeinfo.Size);
    struct cxnet_System_Net_Http_HttpClient *client =
        (struct cxnet_System_Net_Http_HttpClient *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    struct cxcore_System_Threading_Tasks_Task *task;
    struct cxnet_System_Net_Http_HttpResponseMessage *response;
    uint32_t steps = 0u;

    cxnet_System_Net_Http_CurlHttpMessageHandler___constructor(handler);
    cxnet_System_Net_Http_CurlHttpMessageHandler_CaCertificateFile___set(
        handler, make_string(ca_file));
    cxnet_System_Net_Http_CurlHttpMessageHandler_AllowUnknownCertificateRevocation___set(
        handler, CX_TRUE);
    cxnet_System_Net_Http_HttpClient___constructor__3(client,
        (struct cxnet_System_Net_Http_HttpMessageHandler *)handler, CX_TRUE);
    task = cxnet_System_Net_Http_HttpClient_GetAsync__2(client, make_uri(url));
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) break;
        steps++;
    }
    if (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task)) {
        fputs("shared HttpClient HTTPS request did not complete\n", stderr);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        return 0;
    }
    if (!expect_success) {
        int rejected = task->_isFaulted && !task->_isCanceled;
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        if (!rejected) fputs("shared HttpClient accepted a wrong HTTPS hostname\n", stderr);
        return rejected;
    }
    if (task->_isFaulted || task->_isCanceled) {
        fputs("shared HttpClient rejected the trusted HTTPS fixture\n", stderr);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        return 0;
    }
    response = cx_generic_640110dc1a526c1c94f059cac4ed8d246fe089fbe535a78bc1997c7adde4688b(
        task);
    if (response == NULL ||
        cxnet_System_Net_Http_HttpResponseMessage_StatusCode___get(response) !=
            cxnet_System_Net_Http_HttpStatusCode_OK ||
        !string_equals(cxnet_System_Net_Http_HttpContent_ReadAsString(
            cxnet_System_Net_Http_HttpResponseMessage_Content___get(response)),
            expected_body)) {
        fputs("shared HttpClient HTTPS response was incorrect\n", stderr);
        if (response != NULL)
            cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        return 0;
    }
    cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    return 1;
}

static int run_https_proxy_case(const char *url, const char *proxy,
    const char *ca_file, int trust_proxy, int expect_success) {
    struct cxnet_System_Net_Http_CurlHttpMessageHandler *handler =
        (struct cxnet_System_Net_Http_CurlHttpMessageHandler *)
            cxcore_System_Memory_Alloc(
                cxnet_System_Net_Http_CurlHttpMessageHandler___typeinfo.Size);
    struct cxnet_System_Net_Http_HttpClient *client =
        (struct cxnet_System_Net_Http_HttpClient *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    struct cxcore_System_Threading_Tasks_Task *task;
    struct cxnet_System_Net_Http_HttpResponseMessage *response;
    uint32_t steps = 0u;

    cxnet_System_Net_Http_CurlHttpMessageHandler___constructor(handler);
    cxnet_System_Net_Http_CurlHttpMessageHandler_CaCertificateFile___set(
        handler, make_string(ca_file));
    cxnet_System_Net_Http_CurlHttpMessageHandler_AllowUnknownCertificateRevocation___set(
        handler, CX_TRUE);
    cxnet_System_Net_Http_CurlHttpMessageHandler_ProxyCaCertificateFile___set(
        handler, make_string(trust_proxy ? ca_file : ""));
    cxnet_System_Net_Http_HttpClient___constructor__3(client,
        (struct cxnet_System_Net_Http_HttpMessageHandler *)handler, CX_TRUE);
    cxnet_System_Net_Http_HttpClient_Proxy___set(client, make_string(proxy));
    cxnet_System_Net_Http_HttpClient_ProxyBypassList___set(client, make_string(""));
    task = cxnet_System_Net_Http_HttpClient_GetAsync__2(client, make_uri(url));
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) break;
        steps++;
    }
    if (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task)) {
        fputs("shared HttpClient HTTPS proxy request did not complete\n", stderr);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        return 0;
    }
    if (!expect_success) {
        int rejected = task->_isFaulted && !task->_isCanceled;
        if (!rejected)
            fputs("shared HttpClient accepted an invalid HTTPS proxy certificate\n",
                stderr);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        return rejected;
    }
    if (task->_isFaulted || task->_isCanceled) {
        fputs("shared HttpClient rejected the trusted HTTPS proxy\n", stderr);
        if (task->_exception != NULL) {
            struct cxcore_System_String *message =
                cxcore_System_Exception_Message___const_get(task->_exception);
            if (message != NULL && message->_data != NULL)
                fprintf(stderr, "%.*s\n", (int)message->_length,
                    (const char *)message->_data);
        }
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        return 0;
    }
    response = cx_generic_640110dc1a526c1c94f059cac4ed8d246fe089fbe535a78bc1997c7adde4688b(
        task);
    if (response == NULL ||
        cxnet_System_Net_Http_HttpResponseMessage_StatusCode___get(response) !=
            cxnet_System_Net_Http_HttpStatusCode_OK ||
        !string_equals(cxnet_System_Net_Http_HttpContent_ReadAsString(
            cxnet_System_Net_Http_HttpResponseMessage_Content___get(response)),
            "pong")) {
        fputs("shared HttpClient HTTPS proxy response was incorrect\n", stderr);
        if (response != NULL)
            cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        return 0;
    }
    cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    return 1;
}

static void cancel_and_dispose_on_scheduler(void *environment) {
    struct cancel_dispose_context *context =
        (struct cancel_dispose_context *)environment;
    cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(context->source);
    cxnet_System_Net_Http_HttpClient_Dispose(context->client);
    cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(context->source);
    context->fired = 1;
}

static int run_cancellation_case(const char *base_url) {
    struct cxnet_System_Net_Http_HttpClient *client =
        (struct cxnet_System_Net_Http_HttpClient *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    struct cxcore_System_Threading_Tasks_CancellationTokenSource *source =
        (struct cxcore_System_Threading_Tasks_CancellationTokenSource *)
            cxcore_System_Memory_Alloc(
                cxcore_System_Threading_Tasks_CancellationTokenSource___typeinfo.Size);
    struct cxcore_System_Threading_Tasks_Task *task;
    struct cxcore_System_Threading_Tasks_CancellationToken token;
    cx_action cancel_action = {0};
    struct cancel_dispose_context *cancel_dispose;
    char url[512];
    int length = snprintf(url, sizeof(url), "%s/slow-cancel", base_url);
    uint32_t steps = 0u;

    if (length < 0 || (size_t)length >= sizeof(url)) return 0;
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    cxcore_System_Threading_Tasks_CancellationTokenSource___constructor(source);
    token = cxcore_System_Threading_Tasks_CancellationTokenSource_Token___const_get(
        source);
    cancel_dispose = (struct cancel_dispose_context *)cxcore_System_Memory_Alloc(
        (cx_uint)sizeof(*cancel_dispose));
    cancel_dispose->source = source;
    cancel_dispose->client = client;
    cancel_dispose->fired = 0;
    cancel_action.__function = cancel_and_dispose_on_scheduler;
    cancel_action.__environment = cancel_dispose;
    cancel_action.__target = (cx_ptr)source;
    task = cxnet_System_Net_Http_HttpClient_GetAsync(client, make_uri(url), token);
    cxcore_System_Threading_Tasks_TaskScheduler_PostAfter(50u, cancel_action);
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) break;
        steps++;
    }
    if (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) ||
        !cxcore_System_Threading_Tasks_Task_IsCanceled___const_get(task) ||
        !cancel_dispose->fired) {
        fputs("shared HttpClient caller cancellation did not cancel the request\n",
            stderr);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        return 0;
    }
    return 1;
}

static int run_headers_first_cancellation_case(const char *base_url) {
    struct cxnet_System_Net_Http_HttpClient *client =
        (struct cxnet_System_Net_Http_HttpClient *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    struct cxcore_System_Threading_Tasks_CancellationTokenSource *source =
        (struct cxcore_System_Threading_Tasks_CancellationTokenSource *)
            cxcore_System_Memory_Alloc(
                cxcore_System_Threading_Tasks_CancellationTokenSource___typeinfo.Size);
    struct cxcore_System_Threading_Tasks_CancellationToken token;
    struct cxcore_System_IO_Stream *stream;
    struct cxcore_System_Collections_Array *bytes;
    struct cxcore_System_Threading_Tasks_Task *response_task;
    struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *read_task;
    char url[512];
    int length = snprintf(url, sizeof(url), "%s/stream-cancel", base_url);
    uint32_t steps = 0u;

    if (length < 0 || (size_t)length >= sizeof(url)) return 0;
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    cxcore_System_Threading_Tasks_CancellationTokenSource___constructor(source);
    token = cxcore_System_Threading_Tasks_CancellationTokenSource_Token___const_get(source);
    response_task = cxnet_System_Net_Http_HttpClient_GetStreamAsync(
        client, make_uri(url), token);
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(response_task) &&
        steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
            wait_for_scheduler_event();
        }
        steps++;
    }
    if (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(response_task) ||
        response_task->_isFaulted || response_task->_isCanceled) {
        fputs("shared headers-first request did not return its response\n", stderr);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        return 0;
    }
    stream = cx_generic_b7a000e5a1e82c2cc28bb7d06efab618c555997e8e29f1d0e05edd0237738424(
        response_task);
    bytes = cx_array_with_type(cx_array_new(8u, (cx_uint)sizeof(cx_byte)),
        "cx_generic_9c4b0dcf5365274a639812f507806a7475845f1a6e6b8c085532c41d4210ef4b");
    read_task = cxcore_System_IO_Stream_ReadAsync__2(stream, bytes, 0u, 8u);
    steps = 0u;
    while (!read_task->_isCompleted && steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
            wait_for_scheduler_event();
        }
        steps++;
    }
    if (!read_task->_isCompleted || read_task->_isFaulted || read_task->_isCanceled ||
        cx_generic_32f22edd4c9ba40c64ed2658909ec8fb1d6e85d2c4276d46eebd01030eefa80f(
            read_task) != 4u ||
        memcmp(cx_array_at(bytes, 0u, (cx_uint)sizeof(cx_byte)), "pong", 4u) != 0) {
        fputs("shared headers-first request did not yield its body prefix\n", stderr);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        return 0;
    }
    read_task = cxcore_System_IO_Stream_ReadAsync__2(stream, bytes, 0u, 4u);
    cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(source);
    steps = 0u;
    while (!read_task->_isCompleted && steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
            wait_for_scheduler_event();
        }
        steps++;
    }
    if (!read_task->_isCompleted || !read_task->_isCanceled) {
        fputs("shared request cancellation did not cancel the pending body read\n",
            stderr);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        return 0;
    }
    read_task = cxcore_System_IO_Stream_ReadAsync__2(stream, bytes, 0u, 4u);
    if (!read_task->_isCompleted || !read_task->_isCanceled) {
        fputs("shared body read after request cancellation reported EOF\n", stderr);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        return 0;
    }
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    return 1;
}

static int run_concurrent_cancellation_case(const char *base_url,
    int dispose_overlap) {
    struct cxnet_System_Net_Http_HttpClient *client =
        (struct cxnet_System_Net_Http_HttpClient *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    struct cxcore_System_Threading_Tasks_CancellationTokenSource *source_one =
        (struct cxcore_System_Threading_Tasks_CancellationTokenSource *)
            cxcore_System_Memory_Alloc(
                cxcore_System_Threading_Tasks_CancellationTokenSource___typeinfo.Size);
    struct cxcore_System_Threading_Tasks_CancellationTokenSource *source_two =
        (struct cxcore_System_Threading_Tasks_CancellationTokenSource *)
            cxcore_System_Memory_Alloc(
                cxcore_System_Threading_Tasks_CancellationTokenSource___typeinfo.Size);
    struct cxcore_System_Threading_Tasks_CancellationToken token_one;
    struct cxcore_System_Threading_Tasks_CancellationToken token_two;
    struct cxcore_System_Threading_Tasks_Task *response_one;
    struct cxcore_System_Threading_Tasks_Task *response_two;
    struct cxcore_System_IO_Stream *stream_one;
    struct cxcore_System_IO_Stream *stream_two;
    struct cxcore_System_Collections_Array *bytes_one;
    struct cxcore_System_Collections_Array *bytes_two;
    struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *prefix_one;
    struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *prefix_two;
    struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *pending_one;
    struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *pending_two;
    struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *later_one;
    struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *later_two;
    char url_one[512];
    char url_two[512];
    int length_one = snprintf(url_one, sizeof(url_one),
        "%s/concurrent-cancel%s/one", base_url,
        dispose_overlap ? "-dispose" : "");
    int length_two = snprintf(url_two, sizeof(url_two),
        "%s/concurrent-cancel%s/two", base_url,
        dispose_overlap ? "-dispose" : "");
    uint32_t steps = 0u;

    if (length_one < 0 || (size_t)length_one >= sizeof(url_one) ||
        length_two < 0 || (size_t)length_two >= sizeof(url_two)) return 0;
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    cxcore_System_Threading_Tasks_CancellationTokenSource___constructor(source_one);
    cxcore_System_Threading_Tasks_CancellationTokenSource___constructor(source_two);
    token_one = cxcore_System_Threading_Tasks_CancellationTokenSource_Token___const_get(
        source_one);
    token_two = cxcore_System_Threading_Tasks_CancellationTokenSource_Token___const_get(
        source_two);
    response_one = cxnet_System_Net_Http_HttpClient_GetStreamAsync(
        client, make_uri(url_one), token_one);
    response_two = cxnet_System_Net_Http_HttpClient_GetStreamAsync(
        client, make_uri(url_two), token_two);
    while ((!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(response_one) ||
            !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(response_two)) &&
        steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne())
            wait_for_scheduler_event();
        steps++;
    }
    if (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(response_one) ||
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(response_two) ||
        response_one->_isFaulted || response_one->_isCanceled ||
        response_two->_isFaulted || response_two->_isCanceled) {
        fputs("concurrent headers-first requests did not both complete\n", stderr);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        return 0;
    }
    stream_one = cx_generic_b7a000e5a1e82c2cc28bb7d06efab618c555997e8e29f1d0e05edd0237738424(
        response_one);
    stream_two = cx_generic_b7a000e5a1e82c2cc28bb7d06efab618c555997e8e29f1d0e05edd0237738424(
        response_two);
    bytes_one = cx_array_with_type(cx_array_new(4u, (cx_uint)sizeof(cx_byte)),
        "cx_generic_9c4b0dcf5365274a639812f507806a7475845f1a6e6b8c085532c41d4210ef4b");
    bytes_two = cx_array_with_type(cx_array_new(4u, (cx_uint)sizeof(cx_byte)),
        "cx_generic_9c4b0dcf5365274a639812f507806a7475845f1a6e6b8c085532c41d4210ef4b");
    prefix_one = cxcore_System_IO_Stream_ReadAsync__2(stream_one, bytes_one, 0u, 4u);
    prefix_two = cxcore_System_IO_Stream_ReadAsync__2(stream_two, bytes_two, 0u, 4u);
    steps = 0u;
    while ((!prefix_one->_isCompleted || !prefix_two->_isCompleted) &&
        steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne())
            wait_for_scheduler_event();
        steps++;
    }
    if (!prefix_one->_isCompleted || prefix_one->_isFaulted || prefix_one->_isCanceled ||
        cx_generic_32f22edd4c9ba40c64ed2658909ec8fb1d6e85d2c4276d46eebd01030eefa80f(
            prefix_one) != 4u ||
        memcmp(cx_array_at(bytes_one, 0u, (cx_uint)sizeof(cx_byte)), "pong", 4u) != 0 ||
        !prefix_two->_isCompleted || prefix_two->_isFaulted || prefix_two->_isCanceled ||
        cx_generic_32f22edd4c9ba40c64ed2658909ec8fb1d6e85d2c4276d46eebd01030eefa80f(
            prefix_two) != 4u ||
        memcmp(cx_array_at(bytes_two, 0u, (cx_uint)sizeof(cx_byte)), "pong", 4u) != 0) {
        fputs("concurrent streams did not both yield their body prefix\n", stderr);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        return 0;
    }
    pending_one = cxcore_System_IO_Stream_ReadAsync__2(stream_one, bytes_one, 0u, 4u);
    pending_two = cxcore_System_IO_Stream_ReadAsync__2(stream_two, bytes_two, 0u, 4u);
    if (pending_one->_isCompleted || pending_two->_isCompleted) {
        fputs("concurrent stream body reads were not both pending\n", stderr);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        return 0;
    }
    cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(source_one);
    if (dispose_overlap) {
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(source_two);
        steps = 0u;
        while ((!pending_one->_isCompleted || !pending_two->_isCompleted) &&
            steps < 200000u) {
            if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne())
                wait_for_scheduler_event();
            steps++;
        }
        if (!pending_one->_isCompleted || !pending_one->_isCanceled ||
            !pending_two->_isCompleted || !pending_two->_isCanceled) {
            fputs("caller cancellation and client disposal did not cancel both concurrent reads\n",
                stderr);
            return 0;
        }
    } else {
        steps = 0u;
        while (!pending_one->_isCompleted && steps < 200000u) {
            if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne())
                wait_for_scheduler_event();
            steps++;
        }
        if (!pending_one->_isCompleted || !pending_one->_isCanceled ||
            pending_two->_isCompleted) {
            fputs("canceling one request affected the other pending read\n", stderr);
            cxnet_System_Net_Http_HttpClient_Dispose(client);
            return 0;
        }
        cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(source_two);
        steps = 0u;
        while (!pending_two->_isCompleted && steps < 200000u) {
            if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne())
                wait_for_scheduler_event();
            steps++;
        }
        if (!pending_two->_isCompleted || !pending_two->_isCanceled) {
            fputs("canceling the second concurrent request did not cancel its read\n",
                stderr);
            cxnet_System_Net_Http_HttpClient_Dispose(client);
            return 0;
        }
    }
    later_one = cxcore_System_IO_Stream_ReadAsync__2(stream_one, bytes_one, 0u, 4u);
    later_two = cxcore_System_IO_Stream_ReadAsync__2(stream_two, bytes_two, 0u, 4u);
    if (!later_one->_isCompleted || !later_one->_isCanceled ||
        !later_two->_isCompleted || !later_two->_isCanceled) {
        fputs("reads after concurrent cancellation did not remain canceled\n", stderr);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        return 0;
    }
    cxnet_System_Net_Http_ResponseOwnedStream_Dispose(
        (struct cxnet_System_Net_Http_ResponseOwnedStream *)stream_one);
    cxnet_System_Net_Http_ResponseOwnedStream_Dispose(
        (struct cxnet_System_Net_Http_ResponseOwnedStream *)stream_two);
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    return 1;
}

static int run_precanceled_upload_case(const char *base_url) {
    struct cxnet_System_Net_Http_HttpClient *client =
        (struct cxnet_System_Net_Http_HttpClient *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    struct cxcore_System_Threading_Tasks_CancellationTokenSource *source =
        (struct cxcore_System_Threading_Tasks_CancellationTokenSource *)
            cxcore_System_Memory_Alloc(
                cxcore_System_Threading_Tasks_CancellationTokenSource___typeinfo.Size);
    struct cxcore_System_Collections_Array *bytes;
    struct cxcore_System_IO_MemoryStream *stream;
    struct cxnet_System_Net_Http_StreamContent *content;
    struct cxcore_System_Threading_Tasks_CancellationToken token;
    struct cxcore_System_Threading_Tasks_Task *task;
    char url[512];
    int length = snprintf(url, sizeof(url), "%s/precanceled-upload", base_url);

    if (length < 0 || (size_t)length >= sizeof(url)) return 0;
    bytes = cx_array_with_type(cx_array_new(1024u * 1024u,
        (cx_uint)sizeof(cx_byte)),
        "cx_generic_9c4b0dcf5365274a639812f507806a7475845f1a6e6b8c085532c41d4210ef4b");
    memset(bytes->_data, 'u', 1024u * 1024u);
    stream = (struct cxcore_System_IO_MemoryStream *)cxcore_System_Memory_Alloc(
        cxcore_System_IO_MemoryStream___typeinfo.Size);
    cxcore_System_IO_MemoryStream___constructor__3(stream, bytes);
    content = (struct cxnet_System_Net_Http_StreamContent *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Http_StreamContent___typeinfo.Size);
    cxnet_System_Net_Http_StreamContent___constructor__2(content,
        (struct cxcore_System_IO_Stream *)stream, 1024u, 1);
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    cxcore_System_Threading_Tasks_CancellationTokenSource___constructor(source);
    token = cxcore_System_Threading_Tasks_CancellationTokenSource_Token___const_get(
        source);
    cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(source);
    task = cxnet_System_Net_Http_HttpClient_PostAsync(client, make_uri(url),
        (struct cxnet_System_Net_Http_HttpContent *)content, token);
    if (task == NULL || !task->_isCompleted || !task->_isCanceled ||
        task->_isFaulted) {
        fputs("pre-canceled HttpClient.PostAsync did not cancel\n", stderr);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        return 0;
    }
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    return 1;
}

static int run_scheduler_shutdown_case(void) {
    const char *base_url = getenv("CXNET_TEST_REDIRECT_BASE_URL");
    struct cxnet_System_Net_Http_HttpClient *client;
    struct cxcore_System_Threading_Tasks_Task *task;
    struct cx_gc_root_handle *task_root;
    char url[512];
    int length;

    if (base_url == NULL) {
        fputs("HTTP fixture base URL is required for scheduler shutdown\n",
            stderr);
        return 2;
    }
    length = snprintf(url, sizeof(url), "%s/scheduler-shutdown", base_url);
    if (length < 0 || (size_t)length >= sizeof(url)) {
        fputs("HTTP fixture URL is too long for scheduler shutdown\n", stderr);
        return 2;
    }
    cx_gc_check_thread();
    client = (struct cxnet_System_Net_Http_HttpClient *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    task = cxnet_System_Net_Http_HttpClient_GetAsync__2(client, make_uri(url));
    if (task == NULL) {
        fputs("pending HTTP request returned no task\n", stderr);
        return 1;
    }
    task_root = cx_gc_add_root(&task, (cx_uint)sizeof(task));
    if (cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task)) {
        fputs("HTTP request completed before scheduler shutdown setup\n",
            stderr);
        cx_gc_remove_root(task_root);
        return 1;
    }
    if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
        fputs("HTTP request did not start on the scheduler\n", stderr);
        cx_gc_remove_root(task_root);
        return 1;
    }
    if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
        fputs("HTTP request did not reach socket readiness\n", stderr);
        cx_gc_remove_root(task_root);
        return 1;
    }
    cxcore_System_Threading_Tasks_TaskScheduler_Shutdown();
    cx_gc_collect();
    if (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) ||
        !cxcore_System_Threading_Tasks_Task_IsCanceled___const_get(task)) {
        fputs("scheduler shutdown did not cancel the pending HTTP request\n",
            stderr);
        cx_gc_remove_root(task_root);
        return 1;
    }
    cx_gc_remove_root(task_root);
    return 0;
}

int main(int argc, char **argv) {
    const char *proxy = getenv("CXNET_TEST_HTTP_PROXY_URL");
    const char *redirect_base = getenv("CXNET_TEST_REDIRECT_BASE_URL");
    const char *https_url = getenv("CXNET_TEST_HTTPS_URL");
    const char *wrong_host_url = getenv("CXNET_TEST_HTTPS_WRONG_HOST_URL");
    const char *ca_file = getenv("CXNET_TEST_HTTPS_CA_FILE");
    const char *https_proxy = getenv("CXNET_TEST_HTTPS_PROXY_URL");
    const char *https_proxy_wrong_host =
        getenv("CXNET_TEST_HTTPS_PROXY_WRONG_HOST_URL");
    struct cxnet_System_Net_Http_HttpClient *client;
    struct cxnet_System_Net_Http_ProxyCredentials *credentials;
    struct cxcore_System_Threading_Tasks_Task *task;
    struct cxnet_System_Net_Http_HttpResponseMessage *response;
    uint32_t steps = 0u;
    cx_gc_check_thread();
    if (argc == 2 && strcmp(argv[1], "--scheduler-shutdown") == 0)
        return run_scheduler_shutdown_case();
    if (!run_request_framing_rejection_case("Content-Length") ||
        !run_request_framing_rejection_case("Transfer-Encoding")) return 1;
    if (proxy == NULL) {
        fputs("authenticated proxy fixture URL is required\n", stderr);
        return 2;
    }
    client = (struct cxnet_System_Net_Http_HttpClient *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    credentials = (struct cxnet_System_Net_Http_ProxyCredentials *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_ProxyCredentials___typeinfo.Size);
    cxnet_System_Net_Http_ProxyCredentials___constructor(credentials,
        make_string("proxy-user"), make_string("proxy-secret"));
    cxnet_System_Net_Http_HttpClient_Proxy___set(client, make_string(proxy));
    cxnet_System_Net_Http_HttpClient_ProxyCredentials___set(client, credentials);
    task = cxnet_System_Net_Http_HttpClient_GetAsync__2(client,
        make_uri("http://cxnet-proxy-auth-origin.invalid/resource"));
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) break;
        steps++;
    }
    if (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) ||
        task->_isFaulted || task->_isCanceled) {
        fputs("shared HttpClient authenticated proxy request did not complete\n",
            stderr);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        return 1;
    }
    response = cx_generic_640110dc1a526c1c94f059cac4ed8d246fe089fbe535a78bc1997c7adde4688b(
        task);
    if (response == NULL ||
        cxnet_System_Net_Http_HttpResponseMessage_StatusCode___get(response) !=
            cxnet_System_Net_Http_HttpStatusCode_OK) {
        fputs("shared HttpClient proxy request did not receive HTTP 200\n", stderr);
        if (response != NULL)
            cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        return 1;
    }
    cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
    cxnet_System_Net_Http_HttpClient_Dispose(client);

    if (redirect_base == NULL) {
        fputs("redirect fixture base URL is required\n", stderr);
        return 3;
    }
    if (!run_shared_content_send_case(redirect_base, "echo-post", "POST",
            "shared-public-post", "POST:shared-public-post",
            cxnet_System_Net_Http_HttpClient_PostAsync__2) ||
        !run_shared_content_send_case(redirect_base, "echo-put", "PUT",
            "shared-public-put", "PUT:shared-public-put",
            cxnet_System_Net_Http_HttpClient_PutAsync__2) ||
        !run_shared_content_send_case(redirect_base, "echo-patch", "PATCH",
            "shared-public-patch", "PATCH:shared-public-patch",
            cxnet_System_Net_Http_HttpClient_PatchAsync__2) ||
        !run_shared_delete_case(redirect_base) ||
        !run_shared_head_case(redirect_base)) return 4;
    client = (struct cxnet_System_Net_Http_HttpClient *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    {
        char url[512];
        struct cxcore_System_Threading_Tasks_Task *redirect_task;
        struct cxnet_System_Net_Http_HttpResponseMessage *redirect_response;
        struct cxnet_System_Net_Http_HttpContent *content;
        int length = snprintf(url, sizeof(url), "%s/relative", redirect_base);
        if (length < 0 || (size_t)length >= sizeof(url)) {
            fputs("redirect fixture URL is too long\n", stderr);
            cxnet_System_Net_Http_HttpClient_Dispose(client);
            return 3;
        }
        redirect_task = cxnet_System_Net_Http_HttpClient_GetAsync__2(
            client, make_uri(url));
        steps = 0u;
        while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(
                redirect_task) && steps < 200000u) {
            if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) break;
            steps++;
        }
        if (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(
                redirect_task) || redirect_task->_isFaulted ||
            redirect_task->_isCanceled) {
            fputs("shared HttpClient redirect request did not complete\n", stderr);
            cxnet_System_Net_Http_HttpClient_Dispose(client);
            return 4;
        }
        redirect_response = cx_generic_640110dc1a526c1c94f059cac4ed8d246fe089fbe535a78bc1997c7adde4688b(
            redirect_task);
        if (redirect_response == NULL ||
            cxnet_System_Net_Http_HttpResponseMessage_StatusCode___get(
                redirect_response) != cxnet_System_Net_Http_HttpStatusCode_OK) {
            fputs("shared HttpClient redirect did not return HTTP 200\n", stderr);
            if (redirect_response != NULL)
                cxnet_System_Net_Http_HttpResponseMessage_Dispose(redirect_response);
            cxnet_System_Net_Http_HttpClient_Dispose(client);
            return 4;
        }
        content = cxnet_System_Net_Http_HttpResponseMessage_Content___get(
            redirect_response);
        if (content == NULL || !string_equals(
                cxnet_System_Net_Http_HttpContent_ReadAsString(content),
                "redirect-ok")) {
            fputs("shared HttpClient redirect returned the wrong body\n", stderr);
            cxnet_System_Net_Http_HttpResponseMessage_Dispose(redirect_response);
            cxnet_System_Net_Http_HttpClient_Dispose(client);
            return 4;
        }
        cxnet_System_Net_Http_HttpResponseMessage_Dispose(redirect_response);
    }
    cxnet_System_Net_Http_HttpClient_Dispose(client);

    if (https_url == NULL || wrong_host_url == NULL || ca_file == NULL) {
        fputs("HTTPS fixture environment is required\n", stderr);
        return 5;
    }
    if (https_proxy == NULL || https_proxy_wrong_host == NULL) {
        fputs("HTTPS proxy fixture URLs are required\n", stderr);
        return 4;
    }
    if (!run_https_case(https_url, ca_file, 1, "pong") ||
        !run_https_case(wrong_host_url, ca_file, 0, NULL)) return 5;
    if (!run_https_proxy_case(https_url, https_proxy, ca_file, 1, 1) ||
        !run_https_proxy_case(https_url, https_proxy, ca_file, 0, 0) ||
        !run_https_proxy_case(https_url, https_proxy_wrong_host, ca_file, 1, 0))
        return 6;
    if (!run_cancellation_case(redirect_base) ||
        !run_headers_first_cancellation_case(redirect_base) ||
        !run_concurrent_cancellation_case(redirect_base, 0) ||
        !run_concurrent_cancellation_case(redirect_base, 1) ||
        !run_precanceled_upload_case(redirect_base)) return 6;
    return 0;
}

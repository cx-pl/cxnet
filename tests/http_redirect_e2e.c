#include "cxnet.internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
typedef HANDLE redirect_race_thread;
#else
#include <pthread.h>
#include <unistd.h>
typedef pthread_t redirect_race_thread;
#endif

extern void cxcore_System_String___constructor__2(
    struct cxcore_System_String *value, cx_uint length, cx_ptr data);

static struct cxcore_System_String *make_string(const char *text) {
    size_t length = strlen(text);
    cx_ptr data = length == 0u ? NULL : cxcore_System_Memory_Alloc((cx_uint)length);
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
    return value != NULL && value->_length == length &&
        memcmp(value->_data, expected, length) == 0;
}

static int wait_task(struct cxcore_System_Threading_Tasks_Task *task) {
    uint32_t steps = 0u;
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) break;
        steps++;
    }
    return cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task);
}

static void cancel_on_scheduler(void *environment) {
    cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(
        (struct cxcore_System_Threading_Tasks_CancellationTokenSource *)environment);
}

static cx_uint redirect_policy_maximum;
static int redirect_policy_disables_automatic_redirects;

static void update_redirect_policy_on_scheduler(void *environment) {
    struct cxnet_System_Net_Http_HttpClient *client =
        (struct cxnet_System_Net_Http_HttpClient *)environment;
    if (redirect_policy_disables_automatic_redirects) {
        cxnet_System_Net_Http_HttpClient_AllowAutoRedirect___set(
            client, 0);
    } else {
        cxnet_System_Net_Http_HttpClient_MaxAutomaticRedirections___set(
            client, redirect_policy_maximum);
    }
}

static int verify_response(struct cxcore_System_Threading_Tasks_Task *task,
    cxnet_System_Net_Http_HttpStatusCode status, const char *body,
    const char *case_name) {
    struct cxnet_System_Net_Http_HttpResponseMessage *response =
        cx_generic_640110dc1a526c1c94f059cac4ed8d246fe089fbe535a78bc1997c7adde4688b(task);
    struct cxnet_System_Net_Http_HttpContent *content =
        cxnet_System_Net_Http_HttpResponseMessage_Content___get(response);
    struct cxcore_System_String *actual = content == NULL ? NULL :
        cxnet_System_Net_Http_HttpContent_ReadAsString(content);
    int passed = response != NULL && content != NULL &&
        cxnet_System_Net_Http_HttpResponseMessage_StatusCode___get(response) == status &&
        string_equals(actual, body);
    if (!passed) fprintf(stderr, "HTTP redirect case failed: %s (body=%.*s)\n",
        case_name, actual == NULL ? 0 : (int)actual->_length,
        actual == NULL ? "" : (const char *)actual->_data);
    if (response != NULL) cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
    return passed;
}

struct redirect_setting_race {
    struct cxnet_System_Net_Http_HttpClient *client;
    cx_uint newMaximumRedirections;
    int setting;
    volatile int ready;
    volatile int start;
    volatile int completed;
    volatile int failed;
};

static int redirect_race_load(volatile int *value) {
#ifdef _WIN32
    return (int)InterlockedCompareExchange((volatile LONG *)value, 0, 0);
#else
    return __atomic_load_n(value, __ATOMIC_ACQUIRE);
#endif
}

static void redirect_race_store(volatile int *value, int next) {
#ifdef _WIN32
    InterlockedExchange((volatile LONG *)value, (LONG)next);
#else
    __atomic_store_n(value, next, __ATOMIC_RELEASE);
#endif
}

#ifdef _WIN32
static DWORD WINAPI redirect_setting_worker(void *environment) {
#else
static void *redirect_setting_worker(void *environment) {
#endif
    struct redirect_setting_race *race =
        (struct redirect_setting_race *)environment;
    struct cxnet_System_Net_Http_HttpClient *client = race->client;
    struct cx_gc_root_handle *race_root = NULL;
    cx_bool attached = cx_gc_enter_thread();
    if (!attached) {
        redirect_race_store(&race->failed, 1);
        redirect_race_store(&race->completed, 1);
#ifdef _WIN32
        return 0;
#else
        return NULL;
#endif
    }
    race_root = cx_gc_add_root(&race, (cx_uint)sizeof(race));
    if (race_root == NULL) {
        redirect_race_store(&race->failed, 1);
        goto cleanup;
    }
    redirect_race_store(&race->ready, 1);
    while (!redirect_race_load(&race->start)) {
#ifdef _WIN32
        Sleep(0u);
#else
        usleep(100u);
#endif
    }
    if (race->setting == 1) {
        cxnet_System_Net_Http_HttpClient_AllowAutoRedirect___set(client, 1);
    } else {
        cxnet_System_Net_Http_HttpClient_MaxAutomaticRedirections___set(
            client, race->newMaximumRedirections);
    }
cleanup:
    if (race_root != NULL) cx_gc_remove_root(race_root);
    cx_gc_leave_thread(attached);
    redirect_race_store(&race->completed, 1);
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

static int start_redirect_setting_worker(redirect_race_thread *thread,
    struct redirect_setting_race *race) {
#ifdef _WIN32
    *thread = CreateThread(NULL, 0, redirect_setting_worker, race, 0, NULL);
    return *thread != NULL;
#else
    return pthread_create(thread, NULL, redirect_setting_worker, race) == 0;
#endif
}

static void join_redirect_setting_worker(redirect_race_thread thread) {
#ifdef _WIN32
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
#else
    pthread_join(thread, NULL);
#endif
}

static int verify_redirect_race_result(
    struct cxcore_System_Threading_Tasks_Task *task, int setting) {
    if (setting == 2 && task->_isFaulted &&
        cxnet_System_Net_Http_CurlHttpTransport_FailedResponseTransfer(task)) {
        return 1;
    }
    struct cxnet_System_Net_Http_HttpResponseMessage *response =
        cx_generic_640110dc1a526c1c94f059cac4ed8d246fe089fbe535a78bc1997c7adde4688b(task);
    struct cxnet_System_Net_Http_HttpContent *content = response == NULL ? NULL :
        cxnet_System_Net_Http_HttpResponseMessage_Content___get(response);
    struct cxcore_System_String *body = content == NULL ? NULL :
        cxnet_System_Net_Http_HttpContent_ReadAsString(content);
    cxnet_System_Net_Http_HttpStatusCode status = response == NULL
        ? cxnet_System_Net_Http_HttpStatusCode_InternalServerError
        : cxnet_System_Net_Http_HttpResponseMessage_StatusCode___get(response);
    int passed = (status == cxnet_System_Net_Http_HttpStatusCode_Found &&
            string_equals(body, "")) ||
        (status == cxnet_System_Net_Http_HttpStatusCode_OK &&
            string_equals(body, "redirect-ok"));
    if (!passed) fprintf(stderr,
        "redirect setting race returned unexpected status=%d body=%.*s\n",
        (int)status, body == NULL ? 0 : (int)body->_length,
        body == NULL ? "" : (const char *)body->_data);
    if (response != NULL) cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
    return passed;
}

static int run_redirect_setting_race(
    struct cxnet_System_Net_Http_HttpClient *client, const char *base,
    int setting) {
    struct redirect_setting_race race = {0};
    struct cx_gc_root_handle *race_root;
    redirect_race_thread thread;
    struct cxcore_System_Threading_Tasks_Task *task;
    char url[512];
    uint32_t steps = 0u;
    race_root = cx_gc_add_root(&race, (cx_uint)sizeof(race));
    if (race_root == NULL) return 0;
    race.client = client;
    race.setting = setting;
    race.newMaximumRedirections = 2u;
    if (setting == 1) {
        cxnet_System_Net_Http_HttpClient_AllowAutoRedirect___set(client, 0);
        cxnet_System_Net_Http_HttpClient_MaxAutomaticRedirections___set(client, 10u);
        (void)snprintf(url, sizeof(url), "%s/policy-race-allow", base);
    } else {
        cxnet_System_Net_Http_HttpClient_AllowAutoRedirect___set(client, 1);
        cxnet_System_Net_Http_HttpClient_MaxAutomaticRedirections___set(client, 1u);
        (void)snprintf(url, sizeof(url), "%s/policy-race-max-one", base);
    }
    if (!start_redirect_setting_worker(&thread, &race)) {
        cx_gc_remove_root(race_root);
        return 0;
    }
    while (!redirect_race_load(&race.ready) &&
        !redirect_race_load(&race.completed) && steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (!redirect_race_load(&race.ready)) {
        redirect_race_store(&race.start, 1);
        join_redirect_setting_worker(thread);
        cx_gc_remove_root(race_root);
        return 0;
    }
    redirect_race_store(&race.start, 1);
    task = cxnet_System_Net_Http_HttpClient_GetAsync__2(client, make_uri(url));
    join_redirect_setting_worker(thread);
    if (redirect_race_load(&race.failed) ||
        !redirect_race_load(&race.completed) ||
        !wait_task(task) || !verify_redirect_race_result(task, setting)) {
        cx_gc_remove_root(race_root);
        return 0;
    }
    cx_gc_remove_root(race_root);
    return 1;
}

static struct cxnet_System_Net_Http_HttpRequestMessage *make_request(
    struct cxnet_System_Net_Http_HttpMethod *method, const char *url) {
    struct cxnet_System_Net_Http_HttpRequestMessage *request =
        (struct cxnet_System_Net_Http_HttpRequestMessage *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpRequestMessage___typeinfo.Size);
    cxnet_System_Net_Http_HttpRequestMessage___constructor(request, method, make_uri(url));
    return request;
}

int main(void) {
    const char *base = getenv("CXNET_TEST_REDIRECT_BASE_URL");
    const char *ipv6_redirect_url = getenv("CXNET_TEST_IPV6_REDIRECT_URL");
    const char *proxy_url = getenv("CXNET_TEST_HTTP_PROXY_URL");
    struct cxnet_System_Net_Http_HttpClient *client;
    struct cxnet_System_Net_Http_ProxyCredentials *proxy_credentials;
    struct cxnet_System_Net_Http_HttpRequestMessage *request;
    struct cxnet_System_Net_Http_StringContent *content;
    struct cxnet_System_Net_Http_StreamContent *stream_content;
    struct cxcore_System_IO_MemoryStream *source_stream;
    struct cxcore_System_Collections_Array *source_bytes;
    struct cxcore_System_Threading_Tasks_Task *task;
    struct cxcore_System_Threading_Tasks_CancellationTokenSource *cancellation_source;
    struct cxcore_System_Threading_Tasks_CancellationToken cancellation_token;
    cx_action cancel_action = {0};
    cx_action redirect_policy_action = {0};
    char url[512];
    if (base == NULL || ipv6_redirect_url == NULL || proxy_url == NULL) {
        fputs("redirect and proxy fixture URLs are required\n", stderr);
        return 2;
    }
    cx_gc_check_thread();
    client = (struct cxnet_System_Net_Http_HttpClient *)cxcore_System_Memory_Alloc(
        cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    cxnet_System_Net_Http_HttpClient___constructor__2(client);

    cxnet_System_Net_Http_HttpClient_Proxy___set(client, make_string(proxy_url));
    (void)snprintf(url, sizeof(url), "http://cxnet-proxy-origin.invalid/resource");
    task = cxnet_System_Net_Http_HttpClient_GetAsync__2(client, make_uri(url));
    if (!wait_task(task) || !verify_response(task,
            cxnet_System_Net_Http_HttpStatusCode_OK, "proxy-ok", "explicit proxy")) return 1;

    proxy_credentials = (struct cxnet_System_Net_Http_ProxyCredentials *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_ProxyCredentials___typeinfo.Size);
    cxnet_System_Net_Http_ProxyCredentials___constructor(proxy_credentials,
        make_string("proxy-user"), make_string("proxy-secret"));
    cxnet_System_Net_Http_HttpClient_ProxyCredentials___set(client,
        proxy_credentials);
    (void)snprintf(url, sizeof(url), "http://cxnet-proxy-auth-origin.invalid/resource");
    task = cxnet_System_Net_Http_HttpClient_GetAsync__2(client, make_uri(url));
    if (!wait_task(task) || !verify_response(task,
            cxnet_System_Net_Http_HttpStatusCode_OK, "proxy-auth-ok",
            "proxy challenge credentials")) return 1;
    (void)snprintf(url, sizeof(url),
        "http://cxnet-proxy-auth-origin.invalid/proxy-auth-redirect");
    task = cxnet_System_Net_Http_HttpClient_GetAsync__2(client, make_uri(url));
    if (!wait_task(task) || !verify_response(task,
            cxnet_System_Net_Http_HttpStatusCode_OK, "proxy-auth-ok",
            "proxy credentials retained across redirect")) return 1;
    cxnet_System_Net_Http_HttpClient_ProxyCredentials___set(client, NULL);

    request = make_request(cxnet_System_Net_Http_HttpMethod_Get___const_get(),
        ipv6_redirect_url);
    cxnet_System_Net_Http_HttpHeaders_Add(
        cxnet_System_Net_Http_HttpRequestMessage_Headers___const_get(request),
        make_string("Authorization"), make_string("Bearer ipv6-alias-secret"));
    task = cxnet_System_Net_Http_HttpClient_SendAsync(client, request);
    if (!wait_task(task) || !verify_response(task,
            cxnet_System_Net_Http_HttpStatusCode_OK, "ipv6-same-origin",
            "equivalent IPv6 redirect origin")) return 1;

    cxnet_System_Net_Http_HttpClient_ProxyBypassList___set(client, make_string("127.0.0.1"));
    (void)snprintf(url, sizeof(url), "%s/final", base);
    task = cxnet_System_Net_Http_HttpClient_GetAsync__2(client, make_uri(url));
    if (!wait_task(task) || !verify_response(task,
            cxnet_System_Net_Http_HttpStatusCode_OK, "redirect-ok", "proxy bypass list")) return 1;

    cxnet_System_Net_Http_HttpClient_ProxyBypassList___set(client, NULL);
    cxnet_System_Net_Http_HttpClient_Proxy___set(client, make_string(""));
    task = cxnet_System_Net_Http_HttpClient_GetAsync__2(client, make_uri(url));
    if (!wait_task(task) || !verify_response(task,
            cxnet_System_Net_Http_HttpStatusCode_OK, "redirect-ok", "empty proxy disables proxy")) return 1;
    cxnet_System_Net_Http_HttpClient_Proxy___set(client, NULL);

    (void)snprintf(url, sizeof(url), "%s/relative", base);
    task = cxnet_System_Net_Http_HttpClient_GetAsync__2(client, make_uri(url));
    if (!wait_task(task) || !verify_response(task,
            cxnet_System_Net_Http_HttpStatusCode_OK, "redirect-ok", "relative location")) return 1;

    // Redirect settings are captured before the first network hop. Mutating
    // them while the origin delays its redirect must not change this send.
    cxnet_System_Net_Http_HttpClient_AllowAutoRedirect___set(client, 1);
    redirect_policy_disables_automatic_redirects = 1;
    redirect_policy_action.__function = update_redirect_policy_on_scheduler;
    redirect_policy_action.__environment = client;
    redirect_policy_action.__target = (cx_ptr)client;
    cxcore_System_Threading_Tasks_TaskScheduler_PostAfter(
        20u, redirect_policy_action);
    (void)snprintf(url, sizeof(url), "%s/policy-snapshot-allow", base);
    task = cxnet_System_Net_Http_HttpClient_GetAsync__2(client, make_uri(url));
    if (!wait_task(task) || !verify_response(task,
            cxnet_System_Net_Http_HttpStatusCode_OK, "redirect-ok",
            "redirect enablement captured for send")) return 1;

    cxnet_System_Net_Http_HttpClient_AllowAutoRedirect___set(client, 1);
    cxnet_System_Net_Http_HttpClient_MaxAutomaticRedirections___set(client, 2u);
    redirect_policy_disables_automatic_redirects = 0;
    redirect_policy_maximum = 1u;
    redirect_policy_action.__environment = client;
    redirect_policy_action.__target = (cx_ptr)client;
    cxcore_System_Threading_Tasks_TaskScheduler_PostAfter(
        20u, redirect_policy_action);
    (void)snprintf(url, sizeof(url), "%s/policy-snapshot-max-one", base);
    task = cxnet_System_Net_Http_HttpClient_GetAsync__2(client, make_uri(url));
    if (!wait_task(task) || !verify_response(task,
            cxnet_System_Net_Http_HttpStatusCode_OK, "redirect-ok",
            "redirect limit captured for send")) return 1;
    cxnet_System_Net_Http_HttpClient_MaxAutomaticRedirections___set(client, 10u);
    if (!run_redirect_setting_race(client, base, 1) ||
        !run_redirect_setting_race(client, base, 2)) {
        fputs("concurrent redirect policy/send-start race failed\n", stderr);
        return 1;
    }

    (void)snprintf(url, sizeof(url), "%s/post", base);
    request = make_request(cxnet_System_Net_Http_HttpMethod_Post___const_get(), url);
    cxnet_System_Net_Http_HttpHeaders_Add(
        cxnet_System_Net_Http_HttpRequestMessage_Headers___const_get(request),
        make_string("Content-Type"), make_string("application/json"));
    content = (struct cxnet_System_Net_Http_StringContent *)cxcore_System_Memory_Alloc(
        cxnet_System_Net_Http_StringContent___typeinfo.Size);
    cxnet_System_Net_Http_StringContent___constructor(content, make_string("payload"));
    cxnet_System_Net_Http_HttpRequestMessage_Content___set(request,
        (struct cxnet_System_Net_Http_HttpContent *)content);
    task = cxnet_System_Net_Http_HttpClient_SendAsync(client, request);
    if (!wait_task(task) || !verify_response(task,
            cxnet_System_Net_Http_HttpStatusCode_OK, "GET:", "302 POST becomes GET")) return 1;

    (void)snprintf(url, sizeof(url), "%s/post-303", base);
    request = make_request(cxnet_System_Net_Http_HttpMethod_Post___const_get(), url);
    content = (struct cxnet_System_Net_Http_StringContent *)cxcore_System_Memory_Alloc(
        cxnet_System_Net_Http_StringContent___typeinfo.Size);
    cxnet_System_Net_Http_StringContent___constructor(content, make_string("payload"));
    cxnet_System_Net_Http_HttpRequestMessage_Content___set(request,
        (struct cxnet_System_Net_Http_HttpContent *)content);
    task = cxnet_System_Net_Http_HttpClient_SendAsync(client, request);
    if (!wait_task(task) || !verify_response(task,
            cxnet_System_Net_Http_HttpStatusCode_OK, "GET:", "303 POST becomes GET")) return 1;

    (void)snprintf(url, sizeof(url), "%s/post-307", base);
    request = make_request(cxnet_System_Net_Http_HttpMethod_Post___const_get(), url);
    content = (struct cxnet_System_Net_Http_StringContent *)cxcore_System_Memory_Alloc(
        cxnet_System_Net_Http_StringContent___typeinfo.Size);
    cxnet_System_Net_Http_StringContent___constructor(content, make_string("payload"));
    cxnet_System_Net_Http_HttpRequestMessage_Content___set(request,
        (struct cxnet_System_Net_Http_HttpContent *)content);
    task = cxnet_System_Net_Http_HttpClient_SendAsync(client, request);
    if (!wait_task(task) || !verify_response(task,
            cxnet_System_Net_Http_HttpStatusCode_OK, "POST:payload", "307 preserves method and body")) return 1;

    (void)snprintf(url, sizeof(url), "%s/post-307", base);
    request = make_request(cxnet_System_Net_Http_HttpMethod_Post___const_get(), url);
    source_bytes = cx_array_with_type(cx_array_new(7u, (cx_uint)sizeof(cx_byte)),
        "cx_generic_9c4b0dcf5365274a639812f507806a7475845f1a6e6b8c085532c41d4210ef4b");
    memcpy(source_bytes->_data, "payload", 7u);
    source_stream = (struct cxcore_System_IO_MemoryStream *)cxcore_System_Memory_Alloc(
        cxcore_System_IO_MemoryStream___typeinfo.Size);
    cxcore_System_IO_MemoryStream___constructor__3(source_stream, source_bytes);
    stream_content = (struct cxnet_System_Net_Http_StreamContent *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Http_StreamContent___typeinfo.Size);
    cxnet_System_Net_Http_StreamContent___constructor__2(stream_content,
        (struct cxcore_System_IO_Stream *)source_stream, 4u, 1);
    cxnet_System_Net_Http_HttpRequestMessage_Content___set(request,
        (struct cxnet_System_Net_Http_HttpContent *)stream_content);
    task = cxnet_System_Net_Http_HttpClient_SendAsync(client, request);
    if (!wait_task(task) || !verify_response(task,
            cxnet_System_Net_Http_HttpStatusCode_OK, "POST:payload",
            "307 replays seekable streaming content")) return 1;

    cxnet_System_Net_Http_HttpClient_MaxAutomaticRedirections___set(client, 2u);
    (void)snprintf(url, sizeof(url), "%s/loop", base);
    task = cxnet_System_Net_Http_HttpClient_GetAsync__2(client, make_uri(url));
    if (!wait_task(task) || !cxnet_System_Net_Http_CurlHttpTransport_FailedResponseTransfer(task)) {
        fputs("redirect limit did not fault the redirect loop\n", stderr);
        return 1;
    }
    cxnet_System_Net_Http_HttpClient_MaxAutomaticRedirections___set(client, 10u);

    cancellation_source = (struct cxcore_System_Threading_Tasks_CancellationTokenSource *)
        cxcore_System_Memory_Alloc(
            cxcore_System_Threading_Tasks_CancellationTokenSource___typeinfo.Size);
    cxcore_System_Threading_Tasks_CancellationTokenSource___constructor(cancellation_source);
    cancellation_token =
        cxcore_System_Threading_Tasks_CancellationTokenSource_Token___const_get(
            cancellation_source);
    cancel_action.__function = cancel_on_scheduler;
    cancel_action.__environment = cancellation_source;
    cancel_action.__target = (cx_ptr)cancellation_source;
    cxcore_System_Threading_Tasks_TaskScheduler_PostAfter(100u, cancel_action);
    (void)snprintf(url, sizeof(url), "%s/slow-redirect", base);
    task = cxnet_System_Net_Http_HttpClient_GetAsync(client, make_uri(url), cancellation_token);
    if (!wait_task(task) || !cxnet_System_Net_Http_CurlHttpTransport_WasCanceled(task)) {
        fputs("cancellation during a redirected request did not cancel the send\n", stderr);
        return 1;
    }

    cxnet_System_Net_Http_HttpClient_TimeoutMilliseconds___set(client, 300u);
    (void)snprintf(url, sizeof(url), "%s/timeout-1", base);
    task = cxnet_System_Net_Http_HttpClient_GetAsync__2(client, make_uri(url));
    if (!wait_task(task) || !cxnet_System_Net_Http_CurlHttpTransport_WasTimedOut(task)) {
        fputs("redirect chain exceeded its total request timeout without timing out\n", stderr);
        return 1;
    }
    cxnet_System_Net_Http_HttpClient_TimeoutMilliseconds___set(client, 100000u);

    (void)snprintf(url, sizeof(url), "%s/cross-origin", base);
    request = make_request(cxnet_System_Net_Http_HttpMethod_Get___const_get(), url);
    cxnet_System_Net_Http_HttpHeaders_Add(
        cxnet_System_Net_Http_HttpRequestMessage_Headers___const_get(request),
        make_string("Authorization"), make_string("Bearer redirect-secret"));
    cxnet_System_Net_Http_HttpHeaders_Add(
        cxnet_System_Net_Http_HttpRequestMessage_Headers___const_get(request),
        make_string("Cookie"), make_string("session=redirect-secret"));
    task = cxnet_System_Net_Http_HttpClient_SendAsync(client, request);
    if (!wait_task(task) || !verify_response(task,
            cxnet_System_Net_Http_HttpStatusCode_OK, "clean", "cross-origin credentials stripped")) return 1;

    (void)snprintf(url, sizeof(url), "%s/cookie-redirect", base);
    task = cxnet_System_Net_Http_HttpClient_GetAsync__2(client, make_uri(url));
    if (!wait_task(task) || !verify_response(task,
            cxnet_System_Net_Http_HttpStatusCode_OK, "cookie-ok",
            "response cookies are applied to the redirect hop")) return 1;

    struct cxnet_System_Net_Http_HttpHeaders *default_headers =
        cxnet_System_Net_Http_HttpClient_DefaultRequestHeaders___get(client);
    cxnet_System_Net_Http_HttpHeaders_Add(default_headers,
        make_string("Authorization"), make_string("Bearer default-secret"));
    cxnet_System_Net_Http_HttpHeaders_Add(default_headers,
        make_string("Cookie"), make_string("session=default-secret"));
    (void)snprintf(url, sizeof(url), "%s/cross-origin", base);
    request = make_request(cxnet_System_Net_Http_HttpMethod_Get___const_get(), url);
    task = cxnet_System_Net_Http_HttpClient_SendAsync(client, request);
    if (!wait_task(task) || !verify_response(task,
            cxnet_System_Net_Http_HttpStatusCode_OK, "clean",
            "cross-origin default credentials stripped")) return 1;

    cxnet_System_Net_Http_HttpClient_AllowAutoRedirect___set(client, 0);
    (void)snprintf(url, sizeof(url), "%s/relative", base);
    task = cxnet_System_Net_Http_HttpClient_GetAsync__2(client, make_uri(url));
    if (!wait_task(task) || !verify_response(task,
            cxnet_System_Net_Http_HttpStatusCode_Found, "", "redirect disabled")) return 1;

    cxnet_System_Net_Http_HttpClient_AllowAutoRedirect___set(client, 1);
    (void)snprintf(url, sizeof(url), "%s/slow-final", base);
    task = cxnet_System_Net_Http_HttpClient_GetAsync__2(client, make_uri(url));
    if (cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task)) {
        fputs("slow request completed before client-disposal cancellation\n", stderr);
        return 1;
    }
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    if (!wait_task(task) || !cxnet_System_Net_Http_CurlHttpTransport_WasCanceled(task)) {
        fputs("disposing HttpClient did not cancel its active request\n", stderr);
        return 1;
    }

    cxnet_System_Net_Http_HttpClient_Dispose(client);
    puts("PASS managed HttpClient redirect and proxy policy cases");
    return 0;
}

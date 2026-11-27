#include "cxnet.internal.h"
#include "cxruntime.h"
#if defined(CX_CXNET_BUILD)
#define CXNET_RUNTIME_API CX_EXPORT
#elif defined(CX_STATIC_LINK)
#define CXNET_RUNTIME_API
#else
#define CXNET_RUNTIME_API CX_IMPORT
#endif
#ifdef CXNET_TESTING
#include "../../../../../tests/http_chunk_fuzz.h"
#endif
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#endif

CXNET_RUNTIME_API void RestGeneratorFixture_RestGeneratorFixture_NativeTestHooks_FailContinuationRegistrationAfter(
    cx_uint successful_registrations) {
#ifdef CXNET_TESTING
    extern void cxcore_System_Threading_Tasks_TaskRuntime_TestFailContinuationRegistrationAfter(
        cx_uint successful_registrations);
    cxcore_System_Threading_Tasks_TaskRuntime_TestFailContinuationRegistrationAfter(
        successful_registrations);
#else
    (void)successful_registrations;
#endif
}

#ifdef CXNET_TESTING
static cx_bool cxnet_http_fail_next_buffering_started;
#endif

cx_bool cxnet_System_Net_Http_HttpContent_TestShouldFailBufferingStarted(void) {
#ifdef CXNET_TESTING
    if (cxnet_http_fail_next_buffering_started) {
        cxnet_http_fail_next_buffering_started = CX_FALSE;
        return CX_TRUE;
    }
#endif
    return CX_FALSE;
}

CXNET_RUNTIME_API void HttpContentSetup_HttpContentSetup_NativeTestHooks_FailNextBufferingStarted(
    void) {
#ifdef CXNET_TESTING
    cxnet_http_fail_next_buffering_started = CX_TRUE;
#endif
}

CXNET_RUNTIME_API void HttpContentSetup_HttpContentSetup_NativeTestHooks_FailContinuationRegistrationAfter(
    cx_uint successful_registrations) {
#ifdef CXNET_TESTING
    extern void cxcore_System_Threading_Tasks_TaskRuntime_TestFailContinuationRegistrationAfter(
        cx_uint successful_registrations);
    cxcore_System_Threading_Tasks_TaskRuntime_TestFailContinuationRegistrationAfter(
        successful_registrations);
#else
    (void)successful_registrations;
#endif
}

CXNET_RUNTIME_API cx_bool HttpContentSetup_HttpContentSetup_NativeTestHooks_IsAvailable(void) {
#ifdef CXNET_TESTING
    return CX_TRUE;
#else
    return CX_FALSE;
#endif
}

CXNET_RUNTIME_API void RestCancellationFixture_RestCancellationFixture_NativeTestHooks_FailNextSchedulerPost(
    void) {
#ifdef CXNET_TESTING
    extern void cx_task_scheduler_test_fail_next_post(void);
    cx_task_scheduler_test_fail_next_post();
#endif
}

CXNET_RUNTIME_API cx_bool RestCancellationFixture_RestCancellationFixture_NativeTestHooks_SchedulerPostFailureWasConsumed(
    void) {
#ifdef CXNET_TESTING
    extern cx_bool cx_task_scheduler_test_post_failure_was_consumed(void);
    return cx_task_scheduler_test_post_failure_was_consumed();
#else
    return CX_FALSE;
#endif
}

CXNET_RUNTIME_API cx_bool RestCancellationFixture_RestCancellationFixture_NativeTestHooks_ResponseWasDisposed(
    struct cxnet_System_Net_Http_HttpResponseMessage *response) {
    return response != NULL && response->_disposed;
}

CXNET_RUNTIME_API cx_bool RestCancellationFixture_RestCancellationFixture_NativeTestHooks_ContentWasDisposed(
    struct cxnet_System_Net_Http_HttpContent *content) {
    return content != NULL &&
        cxcore_System_Threading_AtomicInt_Value___const_get(
            &content->_disposed) != 0;
}

#ifdef CXNET_TESTING
static cx_bool cxnet_http_fail_next_cancel_send;
static cx_bool cxnet_http_fail_next_redirect_end_send;
static cx_bool cxnet_http_fail_next_redirect_content_dispose;
static cx_bool cxnet_http_fail_next_byte_array_content_dispose;
static cx_bool cxnet_http_byte_array_content_dispose_failure_consumed;
static cx_bool cxnet_http_fail_next_handler_continuation_registration;
static cx_bool cxnet_http_handler_continuation_failure_consumed;
static cx_bool cxnet_http_fail_next_upload_continuation_registration;
static cx_bool cxnet_http_upload_continuation_failure_consumed;
static cx_bool cxnet_http_fail_next_begin_send_setup;
static cx_bool cxnet_http_begin_send_setup_failure_consumed;
static cx_uint cxnet_http_redirect_cleanup_failures_consumed;

void cxnet_System_Net_Http_CurlHttpTransport_TestFailNextCancelSend(void) {
    cxnet_http_fail_next_cancel_send = 1;
}

CX_CXNET_API void cxnet_System_Net_Http_HttpRequestMessage_TestInjectRedirectCleanupFailures(
    void) {
    cxnet_http_fail_next_redirect_end_send = 1;
    cxnet_http_fail_next_redirect_content_dispose = 1;
    cxnet_http_redirect_cleanup_failures_consumed = 0u;
}

CX_CXNET_API void cxnet_System_Net_Http_ByteArrayContent_TestInjectDisposeFailure(
    void) {
    cxnet_http_fail_next_byte_array_content_dispose = CX_TRUE;
    cxnet_http_byte_array_content_dispose_failure_consumed = CX_FALSE;
}

CX_CXNET_API cx_bool cxnet_System_Net_Http_ByteArrayContent_TestDisposeFailureConsumed(
    void) {
    return cxnet_http_byte_array_content_dispose_failure_consumed;
}

CX_CXNET_API void cxnet_System_Net_Http_HttpClient_TestFailNextHandlerContinuationRegistration(
    void) {
    cxnet_http_fail_next_handler_continuation_registration = 1;
    cxnet_http_handler_continuation_failure_consumed = 0;
}

CX_CXNET_API cx_bool cxnet_System_Net_Http_HttpClient_TestHandlerContinuationRegistrationFailureConsumed(
    void) {
    return cxnet_http_handler_continuation_failure_consumed;
}

CX_CXNET_API void cxnet_System_Net_Http_CurlUploadStream_TestFailNextContinuationRegistration(
    void) {
    cxnet_http_fail_next_upload_continuation_registration = 1;
    cxnet_http_upload_continuation_failure_consumed = 0;
}

CX_CXNET_API cx_bool cxnet_System_Net_Http_CurlUploadStream_TestContinuationRegistrationFailureConsumed(
    void) {
    return cxnet_http_upload_continuation_failure_consumed;
}

CX_CXNET_API void cxnet_System_Net_Http_CurlHttpTransport_TestFailNextBeginSendSetup(
    void) {
    cxnet_http_fail_next_begin_send_setup = 1;
    cxnet_http_begin_send_setup_failure_consumed = 0;
}

CX_CXNET_API cx_bool cxnet_System_Net_Http_CurlHttpTransport_TestBeginSendSetupFailureConsumed(
    void) {
    return cxnet_http_begin_send_setup_failure_consumed;
}

CX_CXNET_API cx_uint cxnet_System_Net_Http_HttpRequestMessage_TestRedirectCleanupFailuresConsumed(
    void) {
    return cxnet_http_redirect_cleanup_failures_consumed;
}

CX_CXNET_API void cxnet_System_Net_Http_HttpClient_TestCleanupRedirectRequest(
    struct cxnet_System_Net_Http_HttpRequestMessage *request) {
    cxnet_System_Net_Http_HttpClient_CleanupRedirectRequest(request);
}

CX_CXNET_API void cxnet_System_Net_Http_Rest_RestClient_TestCompleteStringResponse(
    struct cxnet_System_Net_Http_Rest_RestClient *client,
    struct cxcore_System_Threading_Tasks_Task *pending,
    struct cxcore_System_Threading_Tasks_Task *completion) {
    cxnet_System_Net_Http_Rest_RestClient_CompleteStringResponse(
        client, pending, completion);
}

CX_CXNET_API void cxnet_System_Net_Http_Rest_RestClient_TestCompleteByteArrayResponse(
    struct cxnet_System_Net_Http_Rest_RestClient *client,
    struct cxcore_System_Threading_Tasks_Task *pending,
    struct cxcore_System_Threading_Tasks_Task *completion) {
    cxnet_System_Net_Http_Rest_RestClient_CompleteByteArrayResponse(
        client, pending, completion);
}
#endif

cx_bool cxnet_System_Net_Http_HttpClient_ShouldInjectHandlerContinuationRegistrationFailure(
    void) {
#ifdef CXNET_TESTING
    if (cxnet_http_fail_next_handler_continuation_registration) {
        cxnet_http_fail_next_handler_continuation_registration = 0;
        cxnet_http_handler_continuation_failure_consumed = 1;
        return 1;
    }
#endif
    return 0;
}

cx_bool cxnet_System_Net_Http_CurlHttpTransport_ShouldInjectUploadContinuationRegistrationFailure(
    void) {
#ifdef CXNET_TESTING
    if (cxnet_http_fail_next_upload_continuation_registration) {
        cxnet_http_fail_next_upload_continuation_registration = 0;
        cxnet_http_upload_continuation_failure_consumed = 1;
        return 1;
    }
#endif
    return 0;
}

cx_bool cxnet_System_Net_Http_CurlHttpTransport_ShouldInjectBeginSendSetupFailure(
    void) {
#ifdef CXNET_TESTING
    if (cxnet_http_fail_next_begin_send_setup) {
        cxnet_http_fail_next_begin_send_setup = 0;
        cxnet_http_begin_send_setup_failure_consumed = 1;
        return 1;
    }
#endif
    return 0;
}

cx_bool cxnet_System_Net_Http_HttpRequestMessage_ShouldInjectEndSendFailure(
    void) {
#ifdef CXNET_TESTING
    if (cxnet_http_fail_next_redirect_end_send) {
        cxnet_http_fail_next_redirect_end_send = 0;
        cxnet_http_redirect_cleanup_failures_consumed |= 1u;
        return 1;
    }
#endif
    return 0;
}

cx_bool cxnet_System_Net_Http_ByteArrayContent_ShouldInjectDisposeFailure(
    void) {
#ifdef CXNET_TESTING
    if (cxnet_http_fail_next_byte_array_content_dispose) {
        cxnet_http_fail_next_byte_array_content_dispose = CX_FALSE;
        cxnet_http_byte_array_content_dispose_failure_consumed = CX_TRUE;
        return 1;
    }
    if (cxnet_http_fail_next_redirect_content_dispose) {
        cxnet_http_fail_next_redirect_content_dispose = 0;
        cxnet_http_redirect_cleanup_failures_consumed |= 2u;
        return 1;
    }
#endif
    return 0;
}

extern void cxcore_System_String___constructor__2(
    struct cxcore_System_String *value, cx_uint length, cx_ptr data);

static struct cxcore_System_String *cxnet_http_string(const char *text) {
    size_t length = strlen(text);
    cx_ptr bytes = length == 0u ? NULL : cxcore_System_Memory_Alloc((cx_uint)length);
    struct cxcore_System_String *value =
        (struct cxcore_System_String *)cxcore_System_Memory_Alloc(
            cxcore_System_String___typeinfo.Size);
    if (length != 0u) memcpy(bytes, text, length);
    cxcore_System_String___constructor__2(value, (cx_uint)length, bytes);
    return value;
}

static struct cxcore_System_String *cxnet_http_latin1_string(const char *text) {
    size_t input_length = strlen(text);
    size_t output_length = input_length;
    size_t input_index;
    size_t output_index = 0u;
    cx_byte *bytes;
    struct cxcore_System_String *value;

    if (input_length > (size_t)UINT_MAX) return NULL;
    for (input_index = 0u; input_index < input_length; ++input_index) {
        if ((unsigned char)text[input_index] >= 0x80u) {
            if (output_length >= (size_t)UINT_MAX) return NULL;
            ++output_length;
        }
    }

    bytes = output_length == 0u ? NULL :
        (cx_byte *)cxcore_System_Memory_Alloc((cx_uint)output_length);
    value = (struct cxcore_System_String *)cxcore_System_Memory_Alloc(
        cxcore_System_String___typeinfo.Size);
    for (input_index = 0u; input_index < input_length; ++input_index) {
        unsigned char byte = (unsigned char)text[input_index];
        if (byte < 0x80u) {
            bytes[output_index++] = byte;
        } else {
            bytes[output_index++] = (cx_byte)(0xc0u | (byte >> 6));
            bytes[output_index++] = (cx_byte)(0x80u | (byte & 0x3fu));
        }
    }
    cxcore_System_String___constructor__2(value, (cx_uint)output_length, bytes);
    return value;
}

#ifndef CXNET_WITH_CURL
void cxnet_System_Net_Http_CurlHttpTransport_BeginSend(
    struct cxnet_System_Net_Http_HttpRequestMessage *request,
    struct cxcore_System_Threading_Tasks_CancellationToken cancellationToken,
    struct cxcore_System_Threading_Tasks_Task *completion,
    struct cxnet_System_Net_Http_CurlHttpRequestState *state,
    cx_uint maxResponseContentBufferSize,
    cx_bool automaticDecompression,
    struct cxcore_System_String *caCertificateFile,
    cx_bool allowUnknownCertificateRevocation,
    struct cxcore_System_String *proxyCaCertificateFile,
    struct cxcore_System_String *proxy,
    struct cxcore_System_String *proxyBypassList,
    struct cxcore_System_String *proxyUsername,
    struct cxcore_System_String *proxyPassword,
    cx_bool responseHeadersRead,
    struct cxnet_System_Net_Http_CurlResponseStream *responseStream,
    struct cxnet_System_Net_Http_CurlUploadStream *uploadStream,
    cx_uint maxConnectionsPerServer,
    cx_uint pooledConnectionIdleTimeoutMilliseconds,
    cx_uint pooledConnectionLifetimeMilliseconds,
    cx_ptr connectionShare) {
    struct cxnet_System_Net_Http_HttpRequestException *exception;
    (void)request;
    (void)cancellationToken;
    (void)state;
    (void)maxResponseContentBufferSize;
    (void)automaticDecompression;
    (void)caCertificateFile;
    (void)allowUnknownCertificateRevocation;
    (void)proxyCaCertificateFile;
    (void)proxy;
    (void)proxyBypassList;
    (void)proxyUsername;
    (void)proxyPassword;
    (void)responseHeadersRead;
    (void)responseStream;
    (void)uploadStream;
    (void)maxConnectionsPerServer;
    (void)pooledConnectionIdleTimeoutMilliseconds;
    (void)pooledConnectionLifetimeMilliseconds;
    (void)connectionShare;
    exception = (struct cxnet_System_Net_Http_HttpRequestException *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpRequestException___typeinfo.Size);
    cxnet_System_Net_Http_HttpRequestException___constructor(
        exception, cxnet_http_string(
            "cxnet was built without its pinned libcurl transport package."));
    cxcore_System_Threading_Tasks_TaskRuntime_SetFault(
        (struct cxcore_System_Object *)completion,
        (struct cxcore_System_Exception *)exception);
}
void cxnet_System_Net_Http_CurlHttpTransport_CancelSend(
    struct cxnet_System_Net_Http_CurlHttpRequestState *state) {
    (void)state;
}
void cxnet_System_Net_Http_CurlHttpTransport_ResumeStream(
    struct cxnet_System_Net_Http_CurlHttpRequestState *state) {
    (void)state;
}
void cxnet_System_Net_Http_CurlHttpTransport_ResumeUpload(
    struct cxnet_System_Net_Http_CurlHttpRequestState *state) {
    (void)state;
}
cx_ptr cxnet_System_Net_Http_CurlHttpTransport_CreateConnectionShare(void) {
    return NULL;
}
void cxnet_System_Net_Http_CurlHttpTransport_ReleaseConnectionShare(
    cx_ptr connectionShare) {
    (void)connectionShare;
}
#else
#include <curl/curl.h>
#include "System/Threading/Thread.internal.h"
#include <limits.h>
#include <stdio.h>
#ifndef _WIN32
extern void *realloc(void *memory, size_t size);
#endif

extern uint64_t cx_task_scheduler_post_after_cancellable_with_shutdown(
    uint32_t milliseconds, cx_action continuation,
    cx_action shutdown_continuation);
extern cx_bool cx_task_scheduler_cancel_timer(uint64_t registration_id);
extern uint64_t cxnet_socket_post_on_readiness_mask(uintptr_t handle,
    uint32_t interest_mask, uint32_t *ready_mask,
    cx_action continuation, cx_action shutdown_continuation);
extern int cxnet_socket_cancel_wait(uint64_t registration_id);
#define CXNET_SOCKET_READABLE 1u
#define CXNET_SOCKET_WRITABLE 2u
#define CXNET_SOCKET_ERROR 4u

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#endif

struct cxnet_http_origin_usage {
    char *scheme;
    char *host;
    int port;
    uint32_t active;
    struct cxnet_http_origin_usage *next;
};

struct cxnet_http_connection_share {
    CURLSH *share;
    volatile long references;
    struct cxnet_http_origin_usage *origins;
#ifdef _WIN32
    CRITICAL_SECTION lock;
#else
    pthread_mutex_t lock;
#endif
};

#ifdef _WIN32
static INIT_ONCE cxnet_http_curl_init_once = INIT_ONCE_STATIC_INIT;
static int cxnet_http_curl_init_result = (int)CURLE_FAILED_INIT;
static BOOL CALLBACK cxnet_http_initialize_curl_once(
    PINIT_ONCE once, PVOID parameter, PVOID *context) {
    (void)once; (void)parameter; (void)context;
    cxnet_http_curl_init_result = (int)curl_global_init(CURL_GLOBAL_DEFAULT);
    return TRUE;
}
static int cxnet_http_initialize_curl(void) {
    if (!InitOnceExecuteOnce(&cxnet_http_curl_init_once,
            cxnet_http_initialize_curl_once, NULL, NULL)) return (int)CURLE_FAILED_INIT;
    return cxnet_http_curl_init_result;
}
#else
static pthread_once_t cxnet_http_curl_init_once = PTHREAD_ONCE_INIT;
static int cxnet_http_curl_init_result = (int)CURLE_FAILED_INIT;
static void cxnet_http_initialize_curl_once(void) {
    cxnet_http_curl_init_result = (int)curl_global_init(CURL_GLOBAL_DEFAULT);
}
static int cxnet_http_initialize_curl(void) {
    if (pthread_once(&cxnet_http_curl_init_once,
            cxnet_http_initialize_curl_once) != 0) return (int)CURLE_FAILED_INIT;
    return cxnet_http_curl_init_result;
}
#endif

#define CXNET_HTTP_MAX_BODY (64u * 1024u * 1024u)
#define CXNET_HTTP_MAX_SOCKETS 64u
#define CXNET_HTTP_MAX_HEADERS 128u
#define CXNET_HTTP_MAX_HEADER_BYTES 16384u

struct cxnet_http_operation;

struct cxnet_http_socket_registration {
    struct cxnet_http_operation *operation;
    curl_socket_t socket;
    uint64_t registration_id;
    uint32_t ready_mask;
    uint32_t interest_mask;
    int active;
    int assigned; /* Stored as a live socket's libcurl socket data. */
};

struct cxnet_http_header_value {
    char *name;
    char *value;
};

struct cxnet_http_operation {
    struct cxcore_System_Threading_Tasks_Task *completion;
    struct cxnet_System_Net_Http_CurlHttpRequestState *state;
    struct cxcore_System_Collections_Array *request_body;
    struct cxnet_System_Net_Http_CurlUploadStream *request_upload;
    struct cxnet_http_connection_share *connection_share;
    char *origin_scheme;
    char *origin_host;
    int origin_port;
    long max_connections_per_server;
    uint64_t gate_timer_id;
    int origin_slot_acquired;
    long pooled_connection_idle_timeout_seconds;
    long pooled_connection_lifetime_seconds;
    long timeout_milliseconds;
    uint64_t request_started_milliseconds;
    CURLM *multi;
    CURL *easy;
    struct curl_slist *request_headers;
    struct cxnet_http_socket_registration *registrations[CXNET_HTTP_MAX_SOCKETS];
    struct cxnet_http_header_value response_headers[CXNET_HTTP_MAX_HEADERS];
    uint32_t registration_count;
    uint32_t response_header_count;
    uint32_t response_header_bytes;
    size_t max_body_length;
    size_t response_content_length;
    size_t chunk_remaining;
    size_t chunk_size_value;
    size_t chunk_line_bytes;
    size_t chunk_trailer_bytes;
    size_t chunk_trailer_name_length;
    size_t paused_body_chunk_length; /* Exact callback chunk libcurl will replay. */
    uint64_t timer_id;
    int running;
    int completed;
    int cleaning;
    int callback_error;
    int curl_failed;
    int timed_out;
    int response_headers_read;
    int response_headers_completed;
    int response_header_block_complete;
    int body_paused;
    int upload_paused;
    int response_limit_exceeded;
    int response_header_limit_exceeded;
    int has_response_content_length;
    int invalid_response_framing;
    int invalid_request_framing;
    int request_is_head;
    int response_is_chunked;
    int automatic_decompression;
    int response_was_content_encoded;
    int chunk_state;
    int chunk_size_has_digit;
    int chunk_extension_state;
    int chunk_trailer_line_has_data;
    int chunk_trailer_saw_cr;
    int chunk_trailer_saw_colon;
    long status_code;
    int curl_error_code;
    char response_version[4];
    char *reason_phrase;
    char *body;
    size_t body_length;
    size_t body_capacity;
    struct cxnet_System_Net_Http_CurlResponseStream *response_stream;
    char curl_error[CURL_ERROR_SIZE];
};

static void cxnet_http_share_lock(CURL *easy, curl_lock_data data,
    curl_lock_access access, void *user_value) {
    struct cxnet_http_connection_share *pool =
        (struct cxnet_http_connection_share *)user_value;
    (void)easy; (void)data; (void)access;
#ifdef _WIN32
    EnterCriticalSection(&pool->lock);
#else
    (void)pthread_mutex_lock(&pool->lock);
#endif
}

static void cxnet_http_share_unlock(CURL *easy, curl_lock_data data,
    void *user_value) {
    struct cxnet_http_connection_share *pool =
        (struct cxnet_http_connection_share *)user_value;
    (void)easy; (void)data;
#ifdef _WIN32
    LeaveCriticalSection(&pool->lock);
#else
    (void)pthread_mutex_unlock(&pool->lock);
#endif
}

static char *cxnet_http_copy_range(const char *start, size_t length);

static int cxnet_http_try_acquire_origin(
    struct cxnet_http_operation *operation) {
    struct cxnet_http_connection_share *pool = operation->connection_share;
    struct cxnet_http_origin_usage *usage;
    int acquired = 0;
    if (pool == NULL || operation->origin_scheme == NULL ||
        operation->origin_host == NULL) return -1;
    cxnet_http_share_lock(NULL, CURL_LOCK_DATA_CONNECT,
        CURL_LOCK_ACCESS_SINGLE, pool);
    usage = pool->origins;
    while (usage != NULL) {
        if (usage->port == operation->origin_port &&
            strcmp(usage->scheme, operation->origin_scheme) == 0 &&
            strcmp(usage->host, operation->origin_host) == 0) break;
        usage = usage->next;
    }
    if (usage == NULL) {
        usage = (struct cxnet_http_origin_usage *)malloc(sizeof(*usage));
        if (usage != NULL) {
            memset(usage, 0, sizeof(*usage));
            usage->scheme = cxnet_http_copy_range(operation->origin_scheme,
                strlen(operation->origin_scheme));
            usage->host = cxnet_http_copy_range(operation->origin_host,
                strlen(operation->origin_host));
            usage->port = operation->origin_port;
            if (usage->scheme == NULL || usage->host == NULL) {
                free(usage->scheme);
                free(usage->host);
                free(usage);
                usage = NULL;
            } else {
                usage->next = pool->origins;
                pool->origins = usage;
            }
        }
    }
    if (usage != NULL && usage->active <
            (uint32_t)operation->max_connections_per_server) {
        usage->active++;
        acquired = 1;
    }
    cxnet_http_share_unlock(NULL, CURL_LOCK_DATA_CONNECT, pool);
    if (usage == NULL) return -1;
    return acquired;
}

static void cxnet_http_release_origin(
    struct cxnet_http_operation *operation) {
    struct cxnet_http_connection_share *pool = operation->connection_share;
    struct cxnet_http_origin_usage **cursor;
    if (pool == NULL || !operation->origin_slot_acquired) return;
    operation->origin_slot_acquired = 0;
    cxnet_http_share_lock(NULL, CURL_LOCK_DATA_CONNECT,
        CURL_LOCK_ACCESS_SINGLE, pool);
    cursor = &pool->origins;
    while (*cursor != NULL) {
        struct cxnet_http_origin_usage *usage = *cursor;
        if (usage->port == operation->origin_port &&
            strcmp(usage->scheme, operation->origin_scheme) == 0 &&
            strcmp(usage->host, operation->origin_host) == 0) {
            if (usage->active != 0u) usage->active--;
            if (usage->active == 0u) {
                *cursor = usage->next;
                free(usage->scheme);
                free(usage->host);
                free(usage);
            }
            break;
        }
        cursor = &usage->next;
    }
    cxnet_http_share_unlock(NULL, CURL_LOCK_DATA_CONNECT, pool);
}

static void cxnet_http_share_retain(
    struct cxnet_http_connection_share *pool) {
    if (pool == NULL) return;
#ifdef _WIN32
    (void)InterlockedIncrement(&pool->references);
#else
    (void)__sync_add_and_fetch(&pool->references, 1);
#endif
}

static void cxnet_http_share_release(
    struct cxnet_http_connection_share *pool) {
    long remaining;
    if (pool == NULL) return;
#ifdef _WIN32
    remaining = InterlockedDecrement(&pool->references);
#else
    remaining = __sync_sub_and_fetch(&pool->references, 1);
#endif
    if (remaining != 0) return;
    if (pool->share != NULL) (void)curl_share_cleanup(pool->share);
    while (pool->origins != NULL) {
        struct cxnet_http_origin_usage *usage = pool->origins;
        pool->origins = usage->next;
        free(usage->scheme);
        free(usage->host);
        free(usage);
    }
#ifdef _WIN32
    DeleteCriticalSection(&pool->lock);
#else
    (void)pthread_mutex_destroy(&pool->lock);
#endif
    free(pool);
}

cx_ptr cxnet_System_Net_Http_CurlHttpTransport_CreateConnectionShare(void) {
    struct cxnet_http_connection_share *pool;
    if (cxnet_http_initialize_curl() != CURLE_OK) return NULL;
    pool = (struct cxnet_http_connection_share *)malloc(sizeof(*pool));
    if (pool == NULL) return NULL;
    memset(pool, 0, sizeof(*pool));
    pool->references = 1;
#ifdef _WIN32
    InitializeCriticalSection(&pool->lock);
#else
    if (pthread_mutex_init(&pool->lock, NULL) != 0) {
        free(pool);
        return NULL;
    }
#endif
    pool->share = curl_share_init();
    if (pool->share == NULL ||
        curl_share_setopt(pool->share, CURLSHOPT_LOCKFUNC,
            cxnet_http_share_lock) != CURLSHE_OK ||
        curl_share_setopt(pool->share, CURLSHOPT_UNLOCKFUNC,
            cxnet_http_share_unlock) != CURLSHE_OK ||
        curl_share_setopt(pool->share, CURLSHOPT_USERDATA, pool) != CURLSHE_OK ||
        curl_share_setopt(pool->share, CURLSHOPT_SHARE,
            CURL_LOCK_DATA_CONNECT) != CURLSHE_OK) {
        cxnet_http_share_release(pool);
        return NULL;
    }
    return (cx_ptr)pool;
}

void cxnet_System_Net_Http_CurlHttpTransport_ReleaseConnectionShare(
    cx_ptr connectionShare) {
    cxnet_http_share_release(
        (struct cxnet_http_connection_share *)connectionShare);
}

static char *cxnet_http_copy_string(const struct cxcore_System_String *value) {
    char *copy;
    if (value == NULL) return NULL;
    copy = (char *)malloc((size_t)value->_length + 1u);
    if (copy == NULL) return NULL;
    if (value->_length != 0u) memcpy(copy, value->_data, value->_length);
    copy[value->_length] = '\0';
    return copy;
}

static char *cxnet_http_copy_range(const char *start, size_t length) {
    char *copy = (char *)malloc(length + 1u);
    if (copy == NULL) return NULL;
    memcpy(copy, start, length);
    copy[length] = '\0';
    return copy;
}

static int cxnet_http_equal_header(const char *name, const char *expected) {
    while (*name != '\0' && *expected != '\0') {
        char left = *name++;
        char right = *expected++;
        if (left >= 'A' && left <= 'Z') left = (char)(left + ('a' - 'A'));
        if (right >= 'A' && right <= 'Z') right = (char)(right + ('a' - 'A'));
        if (left != right) return 0;
    }
    return *name == '\0' && *expected == '\0';
}

static int cxnet_http_equal_header_range(const char *name, size_t length,
    const char *expected) {
    size_t index;
    if (strlen(expected) != length) return 0;
    for (index = 0u; index < length; ++index) {
        char left = name[index];
        char right = expected[index];
        if (left >= 'A' && left <= 'Z') left = (char)(left + ('a' - 'A'));
        if (right >= 'A' && right <= 'Z') right = (char)(right + ('a' - 'A'));
        if (left != right) return 0;
    }
    return 1;
}

static int cxnet_http_has_content_type_header(const struct curl_slist *headers) {
    const struct curl_slist *header;
    for (header = headers; header != NULL; header = header->next) {
        const char *separator = strchr(header->data, ':');
        if (separator == NULL) separator = strchr(header->data, ';');
        if (separator != NULL && cxnet_http_equal_header_range(header->data,
                (size_t)(separator - header->data), "Content-Type"))
            return 1;
    }
    return 0;
}

static int cxnet_http_equal_value_range(const char *value, size_t length,
    const char *expected) {
    size_t index;
    if (strlen(expected) != length) return 0;
    for (index = 0u; index < length; ++index) {
        char left = value[index];
        char right = expected[index];
        if (left >= 'A' && left <= 'Z') left = (char)(left + ('a' - 'A'));
        if (right >= 'A' && right <= 'Z') right = (char)(right + ('a' - 'A'));
        if (left != right) return 0;
    }
    return 1;
}

static int cxnet_http_supported_content_encoding(const char *value,
    size_t length) {
    size_t index = 0u;
    int found = 0;
    while (index < length) {
        size_t start;
        size_t token_length;
        while (index < length &&
            (value[index] == ' ' || value[index] == '\t')) index++;
        start = index;
        while (index < length && value[index] != ',') index++;
        token_length = index - start;
        while (token_length != 0u &&
            (value[start + token_length - 1u] == ' ' ||
             value[start + token_length - 1u] == '\t')) token_length--;
        if (token_length == 0u ||
            !(cxnet_http_equal_value_range(value + start, token_length, "gzip") ||
              cxnet_http_equal_value_range(value + start, token_length, "deflate") ||
              cxnet_http_equal_value_range(value + start, token_length, "br") ||
              cxnet_http_equal_value_range(value + start, token_length, "zstd")))
            return 0;
        found = 1;
        if (index < length) {
            index++;
            if (index == length) return 0;
        }
    }
    return found;
}

static int cxnet_http_has_protocol(const curl_version_info_data *version,
    const char *expected) {
    const char *const *protocol;
    if (version == NULL || version->protocols == NULL) return 0;
    for (protocol = version->protocols; *protocol != NULL; ++protocol) {
        if (strcmp(*protocol, expected) == 0) return 1;
    }
    return 0;
}

static const char *cxnet_http_backend_error(int automatic_decompression) {
    const curl_version_info_data *version = curl_version_info(CURLVERSION_NOW);
    if (version == NULL) return "Could not inspect the libcurl feature set.";
    if ((version->features & CURL_VERSION_ASYNCHDNS) == 0)
        return "The libcurl transport requires asynchronous DNS support.";
    if ((version->features & CURL_VERSION_SSL) == 0)
        return "The libcurl transport requires TLS support.";
    if (!cxnet_http_has_protocol(version, "http") ||
        !cxnet_http_has_protocol(version, "https"))
        return "The libcurl transport requires HTTP and HTTPS protocol support.";
    if (automatic_decompression &&
        (version->features & CURL_VERSION_LIBZ) == 0)
        return "Automatic HTTP decompression requires a libcurl build with zlib.";
    if (automatic_decompression &&
        (version->features & CURL_VERSION_BROTLI) == 0)
        return "Automatic HTTP decompression requires a libcurl build with Brotli.";
    if (automatic_decompression &&
        (version->features & CURL_VERSION_ZSTD) == 0)
        return "Automatic HTTP decompression requires a libcurl build with zstd.";
    return NULL;
}

#ifndef _WIN32
static const char *cxnet_http_find_default_ca_bundle(CURL *easy) {
    static const char *const candidates[] = {
        "/etc/ssl/certs/ca-certificates.crt",
        "/etc/pki/tls/certs/ca-bundle.crt",
        "/etc/ssl/ca-bundle.pem",
        "/etc/ssl/cert.pem",
        "/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem"
    };
    const char *configured = NULL;
    size_t index;
    if (curl_easy_getinfo(easy, CURLINFO_CAINFO, &configured) == CURLE_OK &&
        configured != NULL && access(configured, R_OK) == 0)
        return NULL;
    for (index = 0u; index < sizeof(candidates) / sizeof(candidates[0]);
        index++) {
        if (access(candidates[index], R_OK) == 0) return candidates[index];
    }
    return NULL;
}
#endif

static int cxnet_http_is_content_header(const char *name) {
    return cxnet_http_equal_header(name, "Content-Type") ||
        cxnet_http_equal_header(name, "Content-Length") ||
        cxnet_http_equal_header(name, "Content-Encoding") ||
        cxnet_http_equal_header(name, "Content-Language") ||
        cxnet_http_equal_header(name, "Content-Location") ||
        cxnet_http_equal_header(name, "Content-Disposition") ||
        cxnet_http_equal_header(name, "Content-MD5") ||
        cxnet_http_equal_header(name, "Expires") ||
        cxnet_http_equal_header(name, "Last-Modified");
}

static void cxnet_http_append_headers(struct cxnet_http_operation *operation,
    struct cxnet_System_Net_Http_HttpHeaders *headers) {
    uint32_t index;
    for (index = 0u; index < cxnet_System_Net_Http_HttpHeaders_Count___const_get(headers); ++index) {
        struct cxnet_System_Net_Http_HttpHeader header =
            cxnet_System_Net_Http_HttpHeaders_Get(headers, index);
        char *name = cxnet_http_copy_string(header.Name);
        char *value = cxnet_http_copy_string(header.Value);
        char *line;
        size_t name_length;
        size_t value_length;
        size_t separator_length;
        struct curl_slist *replacement;
        if (name == NULL || value == NULL) {
            free(name); free(value);
            operation->callback_error = 1;
            break;
        }
        if (cxnet_http_equal_header(name, "Content-Length") ||
            cxnet_http_equal_header(name, "Transfer-Encoding")) {
            free(name); free(value);
            operation->invalid_request_framing = 1;
            operation->callback_error = 1;
            break;
        }
        name_length = strlen(name); value_length = strlen(value);
        separator_length = value_length == 0u ? 1u : 2u;
        line = (char *)malloc(name_length + separator_length + value_length + 1u);
        if (line == NULL) { free(name); free(value); operation->callback_error = 1; break; }
        memcpy(line, name, name_length);
        if (value_length == 0u) {
            // libcurl uses a trailing semicolon to preserve an explicitly empty header.
            line[name_length] = ';';
            line[name_length + 1u] = '\0';
        } else {
            line[name_length] = ':'; line[name_length + 1u] = ' ';
            memcpy(line + name_length + 2u, value, value_length + 1u);
        }
        replacement = curl_slist_append(operation->request_headers, line);
        free(line); free(name); free(value);
        if (replacement == NULL) { operation->callback_error = 1; break; }
        operation->request_headers = replacement;
    }
}

static void cxnet_http_clear_headers(struct cxnet_http_operation *operation) {
    uint32_t index;
    for (index = 0u; index < operation->response_header_count; ++index) {
        free(operation->response_headers[index].name);
        free(operation->response_headers[index].value);
    }
    operation->response_header_count = 0u;
    operation->response_header_bytes = 0u;
}

static struct cxcore_System_Collections_Array *cxnet_http_make_stream_chunk(
    const char *data, size_t amount) {
    struct cxcore_System_Collections_Array *array = cx_array_with_type(
        cx_array_new((cx_uint)amount, (cx_uint)sizeof(cx_byte)),
        "cx_generic_9c4b0dcf5365274a639812f507806a7475845f1a6e6b8c085532c41d4210ef4b");
    if (amount != 0u) memcpy(array->_data, data, amount);
    return array;
}

static int cxnet_http_append_body(struct cxnet_http_operation *operation,
    const char *data, size_t amount) {
    size_t capacity;
    char *replacement;
    if ((!operation->response_headers_read &&
            (operation->body_length > operation->max_body_length ||
             amount > operation->max_body_length - operation->body_length)) ||
        amount > ((size_t)-1) - operation->body_length) {
        operation->response_limit_exceeded = 1;
        return 0;
    }
    if (operation->response_headers_read) {
        if (operation->response_stream == NULL) return 0;
        cxnet_System_Net_Http_CurlResponseStream_AcceptChunk(
            operation->response_stream,
            cxnet_http_make_stream_chunk(data, amount));
        operation->body_length += amount;
        return 1;
    }
    if (operation->body_length + amount > operation->body_capacity) {
        capacity = operation->body_capacity == 0u
            ? (operation->max_body_length < 4096u
                ? operation->max_body_length : 4096u)
            : operation->body_capacity;
        while (capacity < operation->body_length + amount) {
            if (capacity > operation->max_body_length / 2u) {
                capacity = operation->max_body_length;
                break;
            }
            capacity *= 2u;
        }
        replacement = (char *)realloc(operation->body, capacity);
        if (replacement == NULL) return 0;
        operation->body = replacement;
        operation->body_capacity = capacity;
    }
    if (amount != 0u) memcpy(operation->body + operation->body_length, data, amount);
    operation->body_length += amount;
    return 1;
}

static int cxnet_http_is_field_name_char(unsigned char value);

enum cxnet_http_chunk_extension_state {
    CXNET_HTTP_CHUNK_EXTENSION_NONE = 0,
    CXNET_HTTP_CHUNK_EXTENSION_BEFORE_SEMICOLON = 1,
    CXNET_HTTP_CHUNK_EXTENSION_NAME_REQUIRED = 2,
    CXNET_HTTP_CHUNK_EXTENSION_NAME = 3,
    CXNET_HTTP_CHUNK_EXTENSION_NAME_WHITESPACE = 4,
    CXNET_HTTP_CHUNK_EXTENSION_VALUE_REQUIRED = 5,
    CXNET_HTTP_CHUNK_EXTENSION_TOKEN_VALUE = 6,
    CXNET_HTTP_CHUNK_EXTENSION_VALUE_WHITESPACE = 7,
    CXNET_HTTP_CHUNK_EXTENSION_QUOTED_VALUE = 8,
    CXNET_HTTP_CHUNK_EXTENSION_QUOTED_ESCAPE = 9,
    CXNET_HTTP_CHUNK_EXTENSION_QUOTED_COMPLETE = 10
};

static int cxnet_http_is_chunk_quoted_text(unsigned char value) {
    return value == '\t' || value == ' ' || value == '!' ||
        (value >= '#' && value <= '[') ||
        (value >= ']' && value <= '~') || value >= 0x80u;
}

static int cxnet_http_is_chunk_quoted_pair(unsigned char value) {
    return value == '\t' || value == ' ' ||
        (value >= '!' && value <= '~') || value >= 0x80u;
}

static int cxnet_http_receive_chunked(struct cxnet_http_operation *operation,
    const char *data, size_t length) {
    size_t index = 0u;
    while (index < length) {
        unsigned char value = (unsigned char)data[index++];
        if (operation->chunk_state == 0) {
            if (operation->chunk_line_bytes >= CXNET_HTTP_MAX_HEADER_BYTES)
                return 0;
            operation->chunk_line_bytes++;
            if (value == '\r') {
                if (!operation->chunk_size_has_digit ||
                    (operation->chunk_extension_state !=
                        CXNET_HTTP_CHUNK_EXTENSION_NONE &&
                     operation->chunk_extension_state !=
                        CXNET_HTTP_CHUNK_EXTENSION_NAME &&
                     operation->chunk_extension_state !=
                        CXNET_HTTP_CHUNK_EXTENSION_NAME_WHITESPACE &&
                     operation->chunk_extension_state !=
                        CXNET_HTTP_CHUNK_EXTENSION_TOKEN_VALUE &&
                     operation->chunk_extension_state !=
                        CXNET_HTTP_CHUNK_EXTENSION_VALUE_WHITESPACE &&
                     operation->chunk_extension_state !=
                        CXNET_HTTP_CHUNK_EXTENSION_QUOTED_COMPLETE)) return 0;
                operation->chunk_state = 5;
            } else if (operation->chunk_extension_state !=
                CXNET_HTTP_CHUNK_EXTENSION_NONE) {
                switch (operation->chunk_extension_state) {
                case CXNET_HTTP_CHUNK_EXTENSION_BEFORE_SEMICOLON:
                    if (value == ' ' || value == '\t') break;
                    if (value != ';') return 0;
                    operation->chunk_extension_state =
                        CXNET_HTTP_CHUNK_EXTENSION_NAME_REQUIRED;
                    break;
                case CXNET_HTTP_CHUNK_EXTENSION_NAME_REQUIRED:
                    if (value == ' ' || value == '\t') break;
                    if (!cxnet_http_is_field_name_char(value)) return 0;
                    operation->chunk_extension_state =
                        CXNET_HTTP_CHUNK_EXTENSION_NAME;
                    break;
                case CXNET_HTTP_CHUNK_EXTENSION_NAME:
                    if (cxnet_http_is_field_name_char(value)) break;
                    if (value == '=') {
                        operation->chunk_extension_state =
                            CXNET_HTTP_CHUNK_EXTENSION_VALUE_REQUIRED;
                    } else if (value == ';') {
                        operation->chunk_extension_state =
                            CXNET_HTTP_CHUNK_EXTENSION_NAME_REQUIRED;
                    } else if (value == ' ' || value == '\t') {
                        operation->chunk_extension_state =
                            CXNET_HTTP_CHUNK_EXTENSION_NAME_WHITESPACE;
                    } else return 0;
                    break;
                case CXNET_HTTP_CHUNK_EXTENSION_NAME_WHITESPACE:
                    if (value == ' ' || value == '\t') break;
                    if (value == '=') {
                        operation->chunk_extension_state =
                            CXNET_HTTP_CHUNK_EXTENSION_VALUE_REQUIRED;
                    } else if (value == ';') {
                        operation->chunk_extension_state =
                            CXNET_HTTP_CHUNK_EXTENSION_NAME_REQUIRED;
                    } else return 0;
                    break;
                case CXNET_HTTP_CHUNK_EXTENSION_VALUE_REQUIRED:
                    if (value == ' ' || value == '\t') break;
                    if (value == '"') {
                        operation->chunk_extension_state =
                            CXNET_HTTP_CHUNK_EXTENSION_QUOTED_VALUE;
                    } else if (cxnet_http_is_field_name_char(value)) {
                        operation->chunk_extension_state =
                            CXNET_HTTP_CHUNK_EXTENSION_TOKEN_VALUE;
                    } else return 0;
                    break;
                case CXNET_HTTP_CHUNK_EXTENSION_TOKEN_VALUE:
                    if (cxnet_http_is_field_name_char(value)) break;
                    if (value == ';') {
                        operation->chunk_extension_state =
                            CXNET_HTTP_CHUNK_EXTENSION_NAME_REQUIRED;
                    } else if (value == ' ' || value == '\t') {
                        operation->chunk_extension_state =
                            CXNET_HTTP_CHUNK_EXTENSION_VALUE_WHITESPACE;
                    } else return 0;
                    break;
                case CXNET_HTTP_CHUNK_EXTENSION_VALUE_WHITESPACE:
                    if (value == ' ' || value == '\t') break;
                    if (value != ';') return 0;
                    operation->chunk_extension_state =
                        CXNET_HTTP_CHUNK_EXTENSION_NAME_REQUIRED;
                    break;
                case CXNET_HTTP_CHUNK_EXTENSION_QUOTED_VALUE:
                    if (value == '"') {
                        operation->chunk_extension_state =
                            CXNET_HTTP_CHUNK_EXTENSION_QUOTED_COMPLETE;
                    } else if (value == '\\') {
                        operation->chunk_extension_state =
                            CXNET_HTTP_CHUNK_EXTENSION_QUOTED_ESCAPE;
                    } else if (!cxnet_http_is_chunk_quoted_text(value)) return 0;
                    break;
                case CXNET_HTTP_CHUNK_EXTENSION_QUOTED_ESCAPE:
                    if (!cxnet_http_is_chunk_quoted_pair(value)) return 0;
                    operation->chunk_extension_state =
                        CXNET_HTTP_CHUNK_EXTENSION_QUOTED_VALUE;
                    break;
                case CXNET_HTTP_CHUNK_EXTENSION_QUOTED_COMPLETE:
                    if (value == ';') {
                        operation->chunk_extension_state =
                            CXNET_HTTP_CHUNK_EXTENSION_NAME_REQUIRED;
                    } else if (value == ' ' || value == '\t') {
                        operation->chunk_extension_state =
                            CXNET_HTTP_CHUNK_EXTENSION_VALUE_WHITESPACE;
                    } else return 0;
                    break;
                default:
                    return 0;
                }
            } else if (value == ';') {
                if (!operation->chunk_size_has_digit) return 0;
                operation->chunk_extension_state =
                    CXNET_HTTP_CHUNK_EXTENSION_NAME_REQUIRED;
            } else if (value == ' ' || value == '\t') {
                if (!operation->chunk_size_has_digit) return 0;
                operation->chunk_extension_state =
                    CXNET_HTTP_CHUNK_EXTENSION_BEFORE_SEMICOLON;
            } else {
                size_t digit;
                if (value >= '0' && value <= '9') digit = (size_t)(value - '0');
                else if (value >= 'a' && value <= 'f') digit = (size_t)(value - 'a' + 10u);
                else if (value >= 'A' && value <= 'F') digit = (size_t)(value - 'A' + 10u);
                else return 0;
                if (operation->chunk_size_value > (((size_t)-1) - digit) / 16u) return 0;
                operation->chunk_size_value = operation->chunk_size_value * 16u + digit;
                operation->chunk_size_has_digit = 1;
            }
        } else if (operation->chunk_state == 1) {
            size_t remaining_input = length - index + 1u;
            size_t amount = operation->chunk_remaining < remaining_input
                ? operation->chunk_remaining : remaining_input;
            if (amount != 0u &&
                (operation->status_code == 204L ||
                 operation->status_code == 205L ||
                 operation->status_code == 304L)) return 0;
            if (!cxnet_http_append_body(operation, data + index - 1u, amount)) return 0;
            index += amount - 1u;
            operation->chunk_remaining -= amount;
            if (operation->chunk_remaining == 0u) operation->chunk_state = 2;
        } else if (operation->chunk_state == 2) {
            if (value != '\r') return 0;
            operation->chunk_state = 6;
        } else if (operation->chunk_state == 5) {
            if (operation->chunk_line_bytes >= CXNET_HTTP_MAX_HEADER_BYTES)
                return 0;
            operation->chunk_line_bytes++;
            if (value != '\n') return 0;
            operation->chunk_remaining = operation->chunk_size_value;
            operation->chunk_state = operation->chunk_remaining == 0u ? 3 : 1;
            operation->chunk_size_value = 0u;
            operation->chunk_size_has_digit = 0;
            operation->chunk_extension_state = CXNET_HTTP_CHUNK_EXTENSION_NONE;
            operation->chunk_line_bytes = 0u;
        } else if (operation->chunk_state == 6) {
            if (value != '\n') return 0;
            operation->chunk_state = 0;
        } else if (operation->chunk_state == 3) {
            if (operation->chunk_trailer_bytes >= CXNET_HTTP_MAX_HEADER_BYTES)
                return 0;
            operation->chunk_trailer_bytes++;
            if (operation->chunk_trailer_saw_cr) {
                if (value != '\n') return 0;
                if (operation->chunk_trailer_line_has_data) {
                    operation->chunk_trailer_line_has_data = 0;
                    operation->chunk_trailer_saw_cr = 0;
                    operation->chunk_trailer_name_length = 0u;
                    operation->chunk_trailer_saw_colon = 0;
                } else {
                    operation->chunk_state = 4;
                    operation->chunk_trailer_saw_cr = 0;
                }
            } else if (value == '\r') {
                if (operation->chunk_trailer_line_has_data &&
                    (!operation->chunk_trailer_saw_colon ||
                     operation->chunk_trailer_name_length == 0u)) return 0;
                operation->chunk_trailer_saw_cr = 1;
            } else if (value == '\n') {
                return 0;
            } else {
                if (!operation->chunk_trailer_saw_colon) {
                    if (value == ':') {
                        if (operation->chunk_trailer_name_length == 0u) return 0;
                        operation->chunk_trailer_saw_colon = 1;
                    } else {
                        if (!cxnet_http_is_field_name_char(value)) return 0;
                        operation->chunk_trailer_name_length++;
                    }
                } else if ((value < 0x20u && value != '\t') ||
                    value == 0x7fu) {
                    return 0;
                }
                operation->chunk_trailer_line_has_data = 1;
            }
        } else {
            return 0;
        }
    }
    return 1;
}

#ifdef CXNET_TESTING
CX_CXNET_API int cxnet_http_fuzz_chunk_input(const uint8_t *data,
    size_t length, size_t fragment_size,
    struct cxnet_http_chunk_fuzz_result *result) {
    struct cxnet_http_operation operation;
    size_t offset = 0u;
    size_t index;
    uint32_t body_hash = 2166136261u;
    int accepted = 1;

    if (result == NULL || (length != 0u && data == NULL)) return 0;
    memset(&operation, 0, sizeof(operation));
    memset(result, 0, sizeof(*result));
    operation.status_code = 200L;
    operation.max_body_length = 65536u;
    if (fragment_size == 0u) fragment_size = 1u;
    while (offset < length) {
        size_t amount = length - offset;
        if (amount > fragment_size) amount = fragment_size;
        if (!cxnet_http_receive_chunked(&operation,
                (const char *)data + offset, amount)) {
            accepted = 0;
            break;
        }
        offset += amount;
    }
    for (index = 0u; index < operation.body_length; index += 1u) {
        body_hash ^= (unsigned char)operation.body[index];
        body_hash *= 16777619u;
    }
    result->body_length = operation.body_length;
    result->chunk_remaining = operation.chunk_remaining;
    result->chunk_size_value = operation.chunk_size_value;
    result->chunk_line_bytes = operation.chunk_line_bytes;
    result->chunk_trailer_bytes = operation.chunk_trailer_bytes;
    result->chunk_trailer_name_length = operation.chunk_trailer_name_length;
    result->body_hash = body_hash;
    result->chunk_state = (uint32_t)operation.chunk_state;
    result->chunk_extension_state =
        (uint32_t)operation.chunk_extension_state;
    result->chunk_size_has_digit =
        (uint32_t)operation.chunk_size_has_digit;
    result->chunk_trailer_line_has_data =
        (uint32_t)operation.chunk_trailer_line_has_data;
    result->chunk_trailer_saw_cr =
        (uint32_t)operation.chunk_trailer_saw_cr;
    result->chunk_trailer_saw_colon =
        (uint32_t)operation.chunk_trailer_saw_colon;
    result->accepted = accepted ? 1u : 0u;
    free(operation.body);
    return accepted;
}
#endif

static void cxnet_http_complete_headers(struct cxnet_http_operation *operation);

static size_t cxnet_http_receive_body(char *data, size_t size, size_t count,
    void *context) {
    struct cxnet_http_operation *operation =
        (struct cxnet_http_operation *)context;
    size_t amount;
    if (size != 0u && count > (size_t)-1 / size) {
        operation->callback_error = 1;
        snprintf(operation->curl_error, sizeof(operation->curl_error), "%s",
            "The HTTP response callback size overflowed.");
        return 0u;
    }
    amount = size * count;
    if (amount != 0u && !operation->response_is_chunked &&
        !operation->request_is_head &&
        (operation->status_code == 204L || operation->status_code == 205L ||
         operation->status_code == 304L)) {
        operation->invalid_response_framing = 1;
        snprintf(operation->curl_error, sizeof(operation->curl_error), "%s",
            "A bodyless HTTP response included body bytes.");
        return 0u;
    }
    if (operation->response_headers_read && amount > (size_t)UINT_MAX) {
        operation->callback_error = 1;
        snprintf(operation->curl_error, sizeof(operation->curl_error), "%s",
            "The HTTP response chunk exceeds the CX stream length range.");
        return 0u;
    }
    if (operation->response_headers_read &&
        !cxnet_System_Net_Http_CurlResponseStream_CanAcceptChunk(
            operation->response_stream, (cx_uint)amount)) {
        operation->body_paused = 1;
        operation->paused_body_chunk_length = amount;
        return CURL_WRITEFUNC_PAUSE;
    }
    if (operation->response_is_chunked) {
        if (!cxnet_http_receive_chunked(operation, data, amount)) {

            operation->invalid_response_framing = 1;
            snprintf(operation->curl_error, sizeof(operation->curl_error), "%s",
                "The HTTP response body stream rejected chunk data.");
            return 0u;
        }
    } else if (!cxnet_http_append_body(operation, data, amount)) {
        snprintf(operation->curl_error, sizeof(operation->curl_error), "%s",
            "The HTTP response body stream rejected data.");
        return 0u;
    }
    return amount;
}

static size_t cxnet_http_read_upload(char *data, size_t size, size_t count,
    void *context) {
    struct cxnet_http_operation *operation =
        (struct cxnet_http_operation *)context;
    struct cxcore_System_Collections_Array *chunk;
    size_t capacity;
    size_t amount;
    if (size != 0u && count > (size_t)-1 / size) {
        operation->callback_error = 1;
        return CURL_READFUNC_ABORT;
    }
    capacity = size * count;
    if (operation->request_upload == NULL || capacity == 0u) return 0u;
    chunk = cxnet_System_Net_Http_CurlUploadStream_ReadForCurl(
        operation->request_upload,
        capacity > (size_t)UINT_MAX ? UINT_MAX : (cx_uint)capacity);
    if (chunk == NULL) {
        if (cxnet_System_Net_Http_CurlUploadStream_HasFailure___const_get(
                operation->request_upload)) {
            operation->callback_error = 1;
            return CURL_READFUNC_ABORT;
        }
        operation->upload_paused = 1;
        return CURL_READFUNC_PAUSE;
    }
    amount = (size_t)chunk->_length;
    if (amount == 0u &&
        cxnet_System_Net_Http_CurlUploadStream_HasFailure___const_get(
            operation->request_upload)) {
        operation->callback_error = 1;
        return CURL_READFUNC_ABORT;
    }
    if (amount != 0u) memcpy(data, chunk->_data, amount);
    return amount;
}

static int cxnet_http_is_field_name_char(unsigned char value) {
    if ((value >= '0' && value <= '9') ||
        (value >= 'A' && value <= 'Z') ||
        (value >= 'a' && value <= 'z')) return 1;
    switch (value) {
    case '!': case '#': case '$': case '%': case '&': case '\'': case '*':
    case '+': case '-': case '.': case '^': case '_': case '`': case '|': case '~':
        return 1;
    default:
        return 0;
    }
}

static int cxnet_http_parse_status_line(const char *data, size_t length,
    long *status_code) {
    const char *line_end;
    const char *cursor;
    if (length < 15u) return 0;
    line_end = data + length;
    if (memcmp(data, "HTTP/", 5u) != 0 ||
        data[5] != '1' || data[6] != '.' ||
        (data[7] != '0' && data[7] != '1') || data[8] != ' ' ||
        data[9] < '0' || data[9] > '9' ||
        data[10] < '0' || data[10] > '9' ||
        data[11] < '0' || data[11] > '9' ||
        line_end[-2] != '\r' || line_end[-1] != '\n' ||
        data[12] != ' ') return 0;
    for (cursor = data + 13;
         cursor < line_end - 2; ++cursor) {
        unsigned char value = (unsigned char)*cursor;
        if ((value < 0x20u && value != '\t') || value == 0x7fu) return 0;
    }
    *status_code = (long)(data[9] - '0') * 100L +
        (long)(data[10] - '0') * 10L + (long)(data[11] - '0');
    return *status_code >= 100L && *status_code <= 599L;
}

static int cxnet_http_valid_response_field(const char *data, size_t length,
    const char *colon) {
    const char *line_end;
    const char *cursor;
    if (length < 2u) return 0;
    line_end = data + length;
    if (line_end[-2] != '\r' || line_end[-1] != '\n' ||
        colon == NULL || colon == data || colon >= line_end - 2) return 0;
    for (cursor = data; cursor < colon; ++cursor) {
        if (!cxnet_http_is_field_name_char((unsigned char)*cursor)) return 0;
    }
    for (cursor = colon + 1; cursor < line_end - 2; ++cursor) {
        unsigned char value = (unsigned char)*cursor;
        if ((value < 0x20u && value != '\t') || value == 0x7fu) return 0;
    }
    return 1;
}

static int cxnet_http_parse_content_length(const char *value_start,
    const char *line_end, size_t *content_length) {
    const char *cursor = value_start;
    size_t common_length = 0u;
    int has_value = 0;
    while (cursor < line_end) {
        size_t parsed_length = 0u;
        int has_digit = 0;
        while (cursor < line_end && (*cursor == ' ' || *cursor == '\t'))
            cursor++;
        while (cursor < line_end && *cursor >= '0' && *cursor <= '9') {
            size_t digit = (size_t)(*cursor - '0');
            if (parsed_length > (((size_t)-1) - digit) / 10u) return 0;
            parsed_length = parsed_length * 10u + digit;
            has_digit = 1;
            cursor++;
        }
        if (!has_digit) return 0;
        if (has_value && common_length != parsed_length) return 0;
        common_length = parsed_length;
        has_value = 1;
        while (cursor < line_end && (*cursor == ' ' || *cursor == '\t'))
            cursor++;
        if (cursor == line_end) break;
        if (*cursor != ',') return 0;
        cursor++;
        if (cursor == line_end) return 0;
    }
    if (!has_value) return 0;
    *content_length = common_length;
    return 1;
}

static size_t cxnet_http_receive_header(char *data, size_t size, size_t count,
    void *context) {
    struct cxnet_http_operation *operation =
        (struct cxnet_http_operation *)context;
    size_t length;
    const char *colon;
    const char *value_start;
    const char *line_end;
    size_t name_length;
    size_t value_length;
    struct cxnet_http_header_value *header;
    if (size != 0u && count > (size_t)-1 / size) {
        operation->callback_error = 1;
        snprintf(operation->curl_error, sizeof(operation->curl_error), "%s",
            "The HTTP response header callback size overflowed.");
        return 0u;
    }
    length = size * count;
    if (operation->response_header_bytes > CXNET_HTTP_MAX_HEADER_BYTES ||
        length > CXNET_HTTP_MAX_HEADER_BYTES - operation->response_header_bytes) {

        operation->response_header_limit_exceeded = 1;
        return 0u;
    }
    line_end = data + length;
    operation->response_header_bytes += (uint32_t)length;
    if (length >= 5u && memcmp(data, "HTTP/", 5u) == 0) {
        const char *first_space;
        const char *second_space;
        if (!cxnet_http_parse_status_line(data, length,
                &operation->status_code)) {
            operation->invalid_response_framing = 1;
            return 0u;
        }
        first_space = data + 8;
        operation->response_version[0] = data[5];
        operation->response_version[1] = '.';
        operation->response_version[2] = data[7];
        operation->response_version[3] = '\0';
        free(operation->reason_phrase);
        operation->reason_phrase = NULL;
        second_space = memchr(first_space + 4, ' ',
            (size_t)(line_end - 2 - (first_space + 4)));
        if (second_space != NULL) {
            const char *phrase = second_space + 1;
            size_t phrase_length = (size_t)(line_end - 2 - phrase);
            operation->reason_phrase = cxnet_http_copy_range(
                phrase, phrase_length);
            if (operation->reason_phrase == NULL) {
                operation->callback_error = 1;
                snprintf(operation->curl_error,
                    sizeof(operation->curl_error), "%s",
                    "Could not allocate the HTTP response reason phrase.");
                return 0u;
            }
        } else {
            operation->reason_phrase = cxnet_http_copy_range("", 0u);
            if (operation->reason_phrase == NULL) {
                operation->callback_error = 1;
                snprintf(operation->curl_error,
                    sizeof(operation->curl_error), "%s",
                    "Could not allocate the HTTP response reason phrase.");
                return 0u;
            }
        }
        cxnet_http_clear_headers(operation);
        operation->response_header_block_complete = 0;
        operation->has_response_content_length = 0;
        operation->response_content_length = 0u;
        operation->response_is_chunked = 0;
        operation->response_was_content_encoded = 0;
        operation->invalid_response_framing = 0;
        operation->chunk_remaining = 0u;
        operation->chunk_size_value = 0u;
        operation->chunk_line_bytes = 0u;
        operation->chunk_trailer_bytes = 0u;
        operation->chunk_state = 0;
        operation->chunk_size_has_digit = 0;
        operation->chunk_extension_state = CXNET_HTTP_CHUNK_EXTENSION_NONE;
        operation->chunk_trailer_line_has_data = 0;
        operation->chunk_trailer_saw_cr = 0;
        operation->chunk_trailer_name_length = 0u;
        operation->chunk_trailer_saw_colon = 0;
        return length;
    }
    if (length == 2u && data[0] == '\r' && data[1] == '\n') {
        if (operation->status_code == 101L) {
            operation->invalid_response_framing = 1;
            snprintf(operation->curl_error, sizeof(operation->curl_error), "%s",
                "HTTP protocol upgrades are not supported.");
            return 0u;
        }
        if (operation->status_code >= 200L) {
            operation->response_header_block_complete = 1;
            if (operation->response_headers_read)
                cxnet_http_complete_headers(operation);
        }
        return length;
    }
    // The public response type has no trailing-header collection. Once the
    // final response header block ends, consume any chunked trailers without
    // merging them into HttpResponseMessage.Headers or content headers.
    if (operation->response_header_block_complete) return length;
    if (operation->status_code < 100L || operation->status_code > 599L) {
        operation->invalid_response_framing = 1;
        return 0u;
    }
    colon = memchr(data, ':', length);
    if (operation->response_header_count >= CXNET_HTTP_MAX_HEADERS) {
        operation->response_header_limit_exceeded = 1;
        return 0u;
    }
    if (!cxnet_http_valid_response_field(data, length, colon)) {
        operation->invalid_response_framing = 1;
        return 0u;
    }
    name_length = (size_t)(colon - data);
    value_start = colon + 1;
    while (value_start < line_end && (*value_start == ' ' || *value_start == '\t')) value_start++;
    while (line_end > value_start && (line_end[-1] == '\r' || line_end[-1] == '\n' || line_end[-1] == ' ' || line_end[-1] == '\t')) line_end--;
    value_length = (size_t)(line_end - value_start);
    if (cxnet_http_equal_header_range(data, name_length, "Content-Length")) {
        size_t parsed_length = 0u;
        if (operation->status_code == 204L)
            operation->invalid_response_framing = 1;
        if (value_length == 0u ||
            !cxnet_http_parse_content_length(value_start, line_end,
                &parsed_length)) {
            operation->invalid_response_framing = 1;
            return 0u;
        }
        if (operation->status_code == 205L && parsed_length != 0u)
            operation->invalid_response_framing = 1;
        if (operation->has_response_content_length &&
            operation->response_content_length != parsed_length) {
            operation->invalid_response_framing = 1;
            return 0u;
        }
        if (operation->response_is_chunked) {
            operation->invalid_response_framing = 1;
            return 0u;
        }
        operation->has_response_content_length = 1;
        operation->response_content_length = parsed_length;
    } else if (cxnet_http_equal_header_range(data, name_length,
            "Transfer-Encoding")) {
        if (operation->status_code == 204L)
            operation->invalid_response_framing = 1;
        if (!cxnet_http_equal_value_range(value_start, value_length, "chunked") ||
            operation->response_is_chunked ||
            operation->has_response_content_length) {
            operation->invalid_response_framing = 1;
            return 0u;
        }
        operation->response_is_chunked = 1;
    } else if (operation->automatic_decompression &&
        cxnet_http_equal_header_range(data, name_length, "Content-Encoding") &&
        cxnet_http_supported_content_encoding(value_start, value_length)) {
        operation->response_was_content_encoded = 1;
    }
    header = &operation->response_headers[operation->response_header_count];
    header->name = cxnet_http_copy_range(data, name_length);
    header->value = cxnet_http_copy_range(value_start, value_length);
    if (header->name == NULL || header->value == NULL) {
        free(header->name); free(header->value);
        header->name = NULL; header->value = NULL;
        operation->callback_error = 1;
        return 0u;
    }
    operation->response_header_count++;
    return length;
}

static void cxnet_http_collect_completions(struct cxnet_http_operation *operation) {
    int remaining = 0;
    CURLMsg *message;
    while ((message = curl_multi_info_read(operation->multi, &remaining)) != NULL) {
        if (message->msg == CURLMSG_DONE && !operation->completed) {
            operation->completed = 1;
            if (message->data.result == CURLE_OPERATION_TIMEDOUT) {
                operation->timed_out = 1;
            }
            if (message->data.result != CURLE_OK) {
                operation->curl_failed = 1;
                operation->curl_error_code = (int)message->data.result;
                if (operation->curl_error[0] == '\0') {
                    const char *description = curl_easy_strerror(message->data.result);
                    if (description == NULL || description[0] == '\0')
                        description = "The HTTP request failed in libcurl.";
                    snprintf(operation->curl_error, sizeof(operation->curl_error), "%s",
                        description);
                }
            }
            (void)curl_easy_getinfo(operation->easy, CURLINFO_RESPONSE_CODE,
                &operation->status_code);
            if (!operation->request_is_head &&
                operation->status_code != 204L && operation->status_code != 304L &&
                !(operation->status_code >= 100L && operation->status_code < 200L) &&
                !operation->response_was_content_encoded &&
                operation->has_response_content_length &&
                operation->body_length != operation->response_content_length) {
                operation->invalid_response_framing = 1;
            }
            if (operation->response_is_chunked && !operation->request_is_head &&
                operation->status_code != 204L && operation->status_code != 304L &&
                !(operation->status_code >= 100L && operation->status_code < 200L) &&
                operation->chunk_state != 4) {

                operation->invalid_response_framing = 1;
            }
        }
    }
}

static void cxnet_http_socket_ready(void *environment);
static void cxnet_http_timer_ready(void *environment);
static void cxnet_http_start_on_scheduler(void *environment);
static void cxnet_http_finish(struct cxnet_http_operation *operation);
static void cxnet_http_cancel_on_scheduler(void *environment);

static void cxnet_http_shutdown_on_scheduler(void *environment) {
    cxnet_http_cancel_on_scheduler(environment);
}

static int cxnet_http_replace_wait(struct cxnet_http_socket_registration *registration,
    uint32_t interest) {
    cx_action continuation = {0};
    cx_action shutdown_continuation = {0};
    if (registration->registration_id != 0u) {
        (void)cxnet_socket_cancel_wait(registration->registration_id);
        registration->registration_id = 0u;
    }
    registration->interest_mask = interest;
    registration->active = interest != 0u;
    registration->ready_mask = 0u;
    if (interest == 0u) return 1;
    continuation.__function = cxnet_http_socket_ready;
    continuation.__environment = registration;
    continuation.__target = (cx_ptr)registration;
    shutdown_continuation.__function = cxnet_http_shutdown_on_scheduler;
    shutdown_continuation.__environment = registration->operation->state;
    shutdown_continuation.__target = (cx_ptr)registration->operation->state;
    registration->registration_id = cxnet_socket_post_on_readiness_mask(
        (uintptr_t)registration->socket, interest, &registration->ready_mask,
        continuation, shutdown_continuation);
    if (registration->registration_id == 0u) {
        registration->operation->callback_error = 1;
        snprintf(registration->operation->curl_error,
            sizeof(registration->operation->curl_error), "%s",
            "Could not register HTTP socket readiness.");
        return 0;
    }
    return 1;
}

static int cxnet_http_socket_event(CURL *easy, curl_socket_t socket, int what,
    void *user_value, void *socket_value) {
    struct cxnet_http_operation *operation =
        (struct cxnet_http_operation *)user_value;
    struct cxnet_http_socket_registration *registration =
        (struct cxnet_http_socket_registration *)socket_value;
    uint32_t interest = 0u;
    (void)easy;
    if (what == CURL_POLL_REMOVE) {
        if (registration != NULL) {
            (void)cxnet_http_replace_wait(registration, 0u);
            if (curl_multi_assign(operation->multi, socket, NULL) != CURLM_OK) {
                operation->callback_error = 1;
                snprintf(operation->curl_error, sizeof(operation->curl_error), "%s",
                    "Could not detach an HTTP socket registration.");
                return -1;
            }
            registration->assigned = 0;
        }
        return 0;
    }
    if (registration == NULL) {
        for (uint32_t index = 0u; index < operation->registration_count; ++index) {
            struct cxnet_http_socket_registration *candidate =
                operation->registrations[index];
            if (candidate != NULL && !candidate->assigned && !candidate->active) {
                registration = candidate;
                break;
            }
        }
        if (registration == NULL &&
            operation->registration_count == CXNET_HTTP_MAX_SOCKETS) {
            operation->callback_error = 1;
            snprintf(operation->curl_error, sizeof(operation->curl_error), "%s",
                "HTTP transfer exceeded its concurrent socket registration limit.");
            return -1;
        }
        if (registration == NULL) {
            registration = (struct cxnet_http_socket_registration *)
                cxcore_System_Memory_Alloc((cx_uint)sizeof(*registration));
        }
        if (registration == NULL) {
            operation->callback_error = 1;
            snprintf(operation->curl_error, sizeof(operation->curl_error), "%s",
                "Could not allocate an HTTP socket registration.");
            return -1;
        }
        if (registration->operation == NULL) {
            memset(registration, 0, sizeof(*registration));
            cx_gc_store_reference((cx_ptr *)&registration->operation,
                (cx_ptr)operation);
            cx_gc_store_reference(
                (cx_ptr *)&operation->registrations[operation->registration_count],
                (cx_ptr)registration);
            operation->registration_count++;
        }
        registration->socket = socket;
        if (curl_multi_assign(operation->multi, socket, registration) != CURLM_OK) {
                operation->callback_error = 1;
            snprintf(operation->curl_error, sizeof(operation->curl_error), "%s",
                "Could not attach an HTTP socket registration.");
            return -1;
        }
    }
    registration->assigned = 1;
    if (what == CURL_POLL_IN || what == CURL_POLL_INOUT) interest |= CXNET_SOCKET_READABLE;
    if (what == CURL_POLL_OUT || what == CURL_POLL_INOUT) interest |= CXNET_SOCKET_WRITABLE;
    return cxnet_http_replace_wait(registration, interest) ? 0 : -1;
}

static int cxnet_http_timer_event(CURLM *multi, long timeout_ms, void *context) {
    struct cxnet_http_operation *operation =
        (struct cxnet_http_operation *)context;
    cx_action continuation = {0};
    cx_action shutdown_continuation = {0};
    (void)multi;
    if (operation->timer_id != 0u) {
        (void)cx_task_scheduler_cancel_timer(operation->timer_id);
        operation->timer_id = 0u;
    }
    if (timeout_ms < 0) return 0;
    continuation.__function = cxnet_http_timer_ready;
    continuation.__environment = operation;
    continuation.__target = (cx_ptr)operation;
    shutdown_continuation.__function = cxnet_http_shutdown_on_scheduler;
    shutdown_continuation.__environment = operation->state;
    shutdown_continuation.__target = (cx_ptr)operation->state;
    operation->timer_id =
        cx_task_scheduler_post_after_cancellable_with_shutdown(
            (uint32_t)timeout_ms, continuation, shutdown_continuation);
    if (operation->timer_id == 0u) {
        operation->callback_error = 1;
        snprintf(operation->curl_error, sizeof(operation->curl_error), "%s",
            "Could not register HTTP transfer timer.");
        return -1;
    }
    return 0;
}

static void cxnet_http_complete_from_curl(struct cxnet_http_operation *operation) {
    uint32_t index;
    cxnet_http_collect_completions(operation);
    if (operation->completed || operation->callback_error) {
        cxnet_http_finish(operation);
        return;
    }
    // Scheduler readiness waits are one-shot. libcurl need not repeat its
    // socket callback when the desired poll mask stays unchanged, so rearm any
    // wait consumed by a socket action unless the body callback paused curl.
    if (operation->body_paused) return;
    for (index = 0u; index < operation->registration_count; ++index) {
        struct cxnet_http_socket_registration *registration =
            operation->registrations[index];
        if (registration != NULL && !registration->active &&
            registration->interest_mask != 0u &&
            !cxnet_http_replace_wait(registration,
                registration->interest_mask)) {
            operation->callback_error = 1;
            if (operation->curl_error[0] == '\0') {
                snprintf(operation->curl_error, sizeof(operation->curl_error), "%s",
                    "Could not rearm HTTP socket readiness.");
            }
            break;
        }
    }
    if (operation->callback_error) cxnet_http_finish(operation);
}

static void cxnet_http_start_on_scheduler(void *environment) {
    struct cxnet_http_operation *operation =
        (struct cxnet_http_operation *)environment;
    CURLMcode result;
    if (operation == NULL || operation->cleaning) return;
    operation->gate_timer_id = 0u;
    {
        uint64_t now = cx_platform_monotonic_time_milliseconds();
        uint64_t elapsed = now >= operation->request_started_milliseconds
            ? now - operation->request_started_milliseconds : 0u;
        if (elapsed >= (uint64_t)operation->timeout_milliseconds) {
            operation->timed_out = 1;
            cxnet_http_finish(operation);
            return;
        }
    }
    if (!operation->origin_slot_acquired) {
        int slot = cxnet_http_try_acquire_origin(operation);
        if (slot < 0) {
            operation->callback_error = 1;
            snprintf(operation->curl_error, sizeof(operation->curl_error), "%s",
                "Could not allocate the per-origin HTTP connection slot.");
            cxnet_http_finish(operation);
            return;
        }
        if (slot == 0) {
            cx_action retry = {0};
            cx_action shutdown_continuation = {0};
            retry.__function = cxnet_http_start_on_scheduler;
            retry.__environment = operation;
            retry.__target = (cx_ptr)operation;
            shutdown_continuation.__function =
                cxnet_http_shutdown_on_scheduler;
            shutdown_continuation.__environment = operation->state;
            shutdown_continuation.__target = (cx_ptr)operation->state;
            operation->gate_timer_id =
                cx_task_scheduler_post_after_cancellable_with_shutdown(
                    10u, retry, shutdown_continuation);
            if (operation->gate_timer_id == 0u) {
                operation->callback_error = 1;
                snprintf(operation->curl_error, sizeof(operation->curl_error), "%s",
                    "Could not wait for an available per-origin HTTP connection slot.");
                cxnet_http_finish(operation);
            }
            return;
        }
        operation->origin_slot_acquired = 1;
    }
    if (operation->connection_share == NULL ||
        curl_easy_setopt(operation->easy, CURLOPT_SHARE,
            operation->connection_share->share) != CURLE_OK) {
        operation->callback_error = 1;
        snprintf(operation->curl_error, sizeof(operation->curl_error), "%s",
            "Could not attach the HTTP connection pool.");
        cxnet_http_finish(operation);
        return;
    }
    {
        uint64_t now = cx_platform_monotonic_time_milliseconds();
        uint64_t elapsed = now >= operation->request_started_milliseconds
            ? now - operation->request_started_milliseconds : 0u;
        long remaining = operation->timeout_milliseconds - (long)elapsed;
        long connect_timeout = remaining < 15000L ? remaining : 15000L;
        if (remaining <= 0L ||
            curl_easy_setopt(operation->easy, CURLOPT_TIMEOUT_MS, remaining) != CURLE_OK ||
            curl_easy_setopt(operation->easy, CURLOPT_CONNECTTIMEOUT_MS,
                connect_timeout) != CURLE_OK) {
            operation->timed_out = remaining <= 0L;
            operation->callback_error = remaining > 0L;
            cxnet_http_finish(operation);
            return;
        }
    }
    result = curl_multi_add_handle(operation->multi, operation->easy);
    if (result != CURLM_OK) {
        operation->callback_error = 1;
        snprintf(operation->curl_error, sizeof(operation->curl_error), "%s",
            curl_multi_strerror(result));
        cxnet_http_finish(operation);
        return;
    }
    result = curl_multi_socket_action(operation->multi, CURL_SOCKET_TIMEOUT,
        0, &operation->running);
    if (result != CURLM_OK) operation->callback_error = 1;
    cxnet_http_complete_from_curl(operation);
}

static void cxnet_http_resume_stream_on_scheduler(void *environment) {
    struct cxnet_System_Net_Http_CurlHttpRequestState *state =
        (struct cxnet_System_Net_Http_CurlHttpRequestState *)environment;
    struct cxnet_http_operation *operation =
        (struct cxnet_http_operation *)
            cxnet_System_Net_Http_CurlHttpRequestState_NativeOperation___get(state);
    if (operation == NULL || operation->cleaning || !operation->body_paused ||
        operation->easy == NULL) return;
    if (!cxnet_System_Net_Http_CurlResponseStream_CanAcceptChunk(
            operation->response_stream,
            (cx_uint)operation->paused_body_chunk_length)) return;
    operation->body_paused = 0;
    operation->paused_body_chunk_length = 0u;
    int pause_mask = operation->upload_paused ? CURLPAUSE_SEND : CURLPAUSE_CONT;
    CURLcode resume_result = curl_easy_pause(operation->easy, pause_mask);
    if (resume_result != CURLE_OK &&
        !(resume_result == CURLE_ABORTED_BY_CALLBACK && operation->body_paused)) {
        operation->callback_error = 1;
        if (operation->curl_error[0] == '\0') {
            snprintf(operation->curl_error, sizeof(operation->curl_error),
                "Could not resume the HTTP response stream: %s",
                curl_easy_strerror(resume_result));
        }
    }
    cxnet_http_complete_from_curl(operation);
}

static void cxnet_http_resume_upload_on_scheduler(void *environment) {
    struct cxnet_System_Net_Http_CurlHttpRequestState *state =
        (struct cxnet_System_Net_Http_CurlHttpRequestState *)environment;
    struct cxnet_http_operation *operation =
        (struct cxnet_http_operation *)
            cxnet_System_Net_Http_CurlHttpRequestState_NativeOperation___get(state);
    if (operation == NULL || operation->cleaning || !operation->upload_paused ||
        operation->easy == NULL) return;
    operation->upload_paused = 0;
    int pause_mask = operation->body_paused ? CURLPAUSE_RECV : CURLPAUSE_CONT;
    if (curl_easy_pause(operation->easy, pause_mask) != CURLE_OK) {
        operation->callback_error = 1;
    }
    cxnet_http_complete_from_curl(operation);
}

void cxnet_System_Net_Http_CurlHttpTransport_ResumeStream(
    struct cxnet_System_Net_Http_CurlHttpRequestState *state) {
    cx_action continuation = {0};
    if (state == NULL) return;
    continuation.__function = cxnet_http_resume_stream_on_scheduler;
    continuation.__environment = state;
    continuation.__target = (cx_ptr)state;
    cxcore_System_Threading_Tasks_TaskScheduler_Post(continuation);
}

void cxnet_System_Net_Http_CurlHttpTransport_ResumeUpload(
    struct cxnet_System_Net_Http_CurlHttpRequestState *state) {
    cx_action continuation = {0};
    if (state == NULL) return;
    continuation.__function = cxnet_http_resume_upload_on_scheduler;
    continuation.__environment = state;
    continuation.__target = (cx_ptr)state;
    cxcore_System_Threading_Tasks_TaskScheduler_Post(continuation);
}

static void cxnet_http_socket_ready(void *environment) {
    struct cxnet_http_socket_registration *registration =
        (struct cxnet_http_socket_registration *)environment;
    struct cxnet_http_operation *operation = registration->operation;
    uint32_t ready = registration->ready_mask;
    int flags = 0;
    if (operation->cleaning) return;
    registration->registration_id = 0u;
    registration->active = 0;
    if (ready & CXNET_SOCKET_READABLE) flags |= CURL_CSELECT_IN;
    if (ready & CXNET_SOCKET_WRITABLE) flags |= CURL_CSELECT_OUT;
    if (ready & CXNET_SOCKET_ERROR) flags |= CURL_CSELECT_ERR;
    if (flags == 0 || curl_multi_socket_action(operation->multi,
            registration->socket, flags, &operation->running) != CURLM_OK) {
        operation->callback_error = 1;
        snprintf(operation->curl_error, sizeof(operation->curl_error), "%s",
            "HTTP socket readiness action failed.");
    }
    cxnet_http_complete_from_curl(operation);
}

static void cxnet_http_timer_ready(void *environment) {
    struct cxnet_http_operation *operation =
        (struct cxnet_http_operation *)environment;
    if (operation->cleaning) return;
    operation->timer_id = 0u;
    if (curl_multi_socket_action(operation->multi, CURL_SOCKET_TIMEOUT,
            0, &operation->running) != CURLM_OK) {
        operation->callback_error = 1;
        snprintf(operation->curl_error, sizeof(operation->curl_error), "%s",
            "HTTP timeout action failed.");
    }
    cxnet_http_complete_from_curl(operation);
}

static void cxnet_http_release_native(struct cxnet_http_operation *operation) {
    uint32_t index;
    operation->cleaning = 1;
    if (operation->timer_id != 0u) {
        (void)cx_task_scheduler_cancel_timer(operation->timer_id);
        operation->timer_id = 0u;
    }
    if (operation->gate_timer_id != 0u) {
        (void)cx_task_scheduler_cancel_timer(operation->gate_timer_id);
        operation->gate_timer_id = 0u;
    }
    for (index = 0u; index < operation->registration_count; ++index) {
        struct cxnet_http_socket_registration *registration = operation->registrations[index];
        if (registration != NULL) {
            (void)cxnet_http_replace_wait(registration, 0u);
            registration->active = 0;
        }
    }
    if (operation->multi != NULL && operation->easy != NULL) {
        (void)curl_multi_remove_handle(operation->multi, operation->easy);
        curl_easy_cleanup(operation->easy);
        operation->easy = NULL;
    }
    if (operation->multi != NULL) {
        curl_multi_cleanup(operation->multi);
        operation->multi = NULL;
    }
    cxnet_http_release_origin(operation);
    free(operation->origin_scheme);
    operation->origin_scheme = NULL;
    free(operation->origin_host);
    operation->origin_host = NULL;
    free(operation->reason_phrase);
    operation->reason_phrase = NULL;
    if (operation->connection_share != NULL) {
        cxnet_http_share_release(operation->connection_share);
        operation->connection_share = NULL;
    }
    if (operation->request_upload != NULL) {
        cxnet_System_Net_Http_CurlUploadStream_CancelTransport(
            operation->request_upload);
        operation->request_upload = NULL;
    }
    curl_slist_free_all(operation->request_headers);
    operation->request_headers = NULL;
    free(operation->body);
    operation->body = NULL;
    cxnet_http_clear_headers(operation);
    if (operation->state != NULL) {
        cxnet_System_Net_Http_CurlHttpRequestState_NativeOperation___set(
            operation->state, NULL);
    }
}

static void cxnet_http_cancel_on_scheduler(void *environment) {
    struct cxnet_System_Net_Http_CurlHttpRequestState *state =
        (struct cxnet_System_Net_Http_CurlHttpRequestState *)environment;
    struct cxnet_http_operation *operation =
        (struct cxnet_http_operation *)
            cxnet_System_Net_Http_CurlHttpRequestState_NativeOperation___get(state);
    if (operation == NULL || operation->cleaning) return;
    operation->cleaning = 1;
    if (operation->response_headers_completed && operation->response_stream != NULL) {
        cxnet_System_Net_Http_CurlResponseStream_Cancelled(
            operation->response_stream);
    }
    cxnet_http_release_native(operation);
    if (!operation->response_headers_completed) {
        cxcore_System_Threading_Tasks_TaskRuntime_SetCanceled(
            (struct cxcore_System_Object *)operation->completion);
    }
}

void cxnet_System_Net_Http_CurlHttpTransport_CancelSend(
    struct cxnet_System_Net_Http_CurlHttpRequestState *state) {
    cx_action continuation = {0};
    if (state == NULL) return;
#ifdef CXNET_TESTING
    if (cxnet_http_fail_next_cancel_send) {
        struct cxcore_System_InvalidOperationException *exception =
            (struct cxcore_System_InvalidOperationException *)
                cxcore_System_Memory_Alloc(
                    cxcore_System_InvalidOperationException___typeinfo.Size);
        cxnet_http_fail_next_cancel_send = 0;
        cxcore_System_InvalidOperationException___constructor(exception);
        CX_THROW(exception);
    }
#endif
    continuation.__function = cxnet_http_cancel_on_scheduler;
    continuation.__environment = state;
    continuation.__target = (cx_ptr)state;
    cxcore_System_Threading_Tasks_TaskScheduler_Post(continuation);
}

static struct cxcore_System_Collections_Array *cxnet_http_make_byte_array(
    struct cxnet_http_operation *operation) {
    struct cxcore_System_Collections_Array *array = cx_array_with_type(
        cx_array_new((cx_uint)operation->body_length, (cx_uint)sizeof(cx_byte)),
        "cx_generic_9c4b0dcf5365274a639812f507806a7475845f1a6e6b8c085532c41d4210ef4b");
    if (operation->body_length != 0u) {
        memcpy(array->_data, operation->body, operation->body_length);
    }
    return array;
}

static struct cxnet_System_Net_Http_HttpResponseMessage *
cxnet_http_create_response(struct cxnet_http_operation *operation,
    int streaming) {
    struct cxnet_System_Net_Http_HttpResponseMessage *response =
        (struct cxnet_System_Net_Http_HttpResponseMessage *)
            cxcore_System_Memory_Alloc(
                cxnet_System_Net_Http_HttpResponseMessage___typeinfo.Size);
    struct cxnet_System_Net_Http_HttpContent *content;
    uint32_t index;
    cxnet_System_Net_Http_HttpResponseMessage___constructor__2(response,
        (cxnet_System_Net_Http_HttpStatusCode)operation->status_code);
    cxnet_System_Net_Http_HttpResponseMessage_ReasonPhrase___set(
        response, cxnet_http_latin1_string(operation->reason_phrase));
    cxnet_System_Net_Http_HttpResponseMessage_Version___set(response,
        cxnet_http_string(operation->response_version));
    if (streaming) {
        struct cxnet_System_Net_Http_CurlStreamingContent *stream_content =
            (struct cxnet_System_Net_Http_CurlStreamingContent *)
                cxcore_System_Memory_Alloc(
                    cxnet_System_Net_Http_CurlStreamingContent___typeinfo.Size);
        cxnet_System_Net_Http_CurlStreamingContent___constructor(
            stream_content, operation->response_stream);
        content = (struct cxnet_System_Net_Http_HttpContent *)stream_content;
    } else {
        struct cxnet_System_Net_Http_ByteArrayContent *buffered_content =
            (struct cxnet_System_Net_Http_ByteArrayContent *)
                cxcore_System_Memory_Alloc(
                    cxnet_System_Net_Http_ByteArrayContent___typeinfo.Size);
        cxnet_System_Net_Http_ByteArrayContent___constructor(buffered_content,
            cxnet_http_make_byte_array(operation));
        content = (struct cxnet_System_Net_Http_HttpContent *)buffered_content;
    }
    cxnet_System_Net_Http_HttpResponseMessage_Content___set(response, content);
    {
        struct cxnet_System_Net_Http_HttpHeaders *headers =
            cxnet_System_Net_Http_HttpResponseMessage_Headers___const_get(response);
        struct cxnet_System_Net_Http_HttpHeaders *content_headers =
            cxnet_System_Net_Http_HttpContent_Headers___const_get(content);
        for (index = 0u; index < operation->response_header_count; ++index) {
            if (operation->automatic_decompression &&
                operation->response_was_content_encoded &&
                (cxnet_http_equal_header(operation->response_headers[index].name,
                    "Content-Encoding") ||
                 cxnet_http_equal_header(operation->response_headers[index].name,
                    "Content-Length"))) {
                continue;
            }
            struct cxnet_System_Net_Http_HttpHeaders *target =
                cxnet_http_is_content_header(operation->response_headers[index].name)
                    ? content_headers : headers;
            cxnet_System_Net_Http_HttpHeaders_Add(target,
                cxnet_http_string(operation->response_headers[index].name),
                cxnet_http_latin1_string(operation->response_headers[index].value));
        }
    }
    return response;
}

static void cxnet_http_complete_headers(struct cxnet_http_operation *operation) {
    struct cxnet_System_Net_Http_HttpResponseMessage *response;
    if (operation->response_headers_completed || operation->cleaning ||
        operation->invalid_response_framing || operation->status_code < 200L ||
        operation->response_stream == NULL) return;
    operation->response_headers_completed = 1;
    response = cxnet_http_create_response(operation, 1);
    cx_generic_cffaa19f1c5c7eeac39eaf7e5d7657cd15ab974e1443642c08c9ac96b8d3bd61(
        operation->completion, response);
}

static void cxnet_http_finish(struct cxnet_http_operation *operation) {
    struct cxnet_System_Net_Http_HttpResponseMessage *response;
    if (operation->cleaning) return;
    operation->cleaning = 1;
    if (operation->callback_error || operation->timed_out ||
        operation->response_limit_exceeded ||
        operation->response_header_limit_exceeded ||
        operation->invalid_response_framing ||
        operation->invalid_request_framing ||
        operation->curl_failed ||
        (operation->response_headers_read &&
            !operation->response_headers_completed) ||
        operation->curl_error[0] != '\0') {
        struct cxcore_System_Exception *exception;
        if (operation->timed_out) {
            struct cxcore_System_TimeoutException *timeout =
                (struct cxcore_System_TimeoutException *)cxcore_System_Memory_Alloc(
                    cxcore_System_TimeoutException___typeinfo.Size);
            cxcore_System_TimeoutException___constructor__2(
                timeout, cxnet_http_string("The HTTP request timed out."));
            exception = (struct cxcore_System_Exception *)timeout;
        } else {
            struct cxnet_System_Net_Http_HttpRequestException *requestException =
                (struct cxnet_System_Net_Http_HttpRequestException *)
                    cxcore_System_Memory_Alloc(
                        cxnet_System_Net_Http_HttpRequestException___typeinfo.Size);
            struct cxcore_System_IO_IOException *cause =
                (struct cxcore_System_IO_IOException *)cxcore_System_Memory_Alloc(
                    cxcore_System_IO_IOException___typeinfo.Size);
            const char *message = operation->response_limit_exceeded
                ? "The response exceeds MaxResponseContentBufferSize."
                : (operation->response_header_limit_exceeded
                    ? "The response exceeds the configured header limits."
                    : (operation->status_code == 101L
                        ? "HTTP protocol upgrades are not supported."
                        : (operation->invalid_response_framing
                        ? "The HTTP response has invalid or truncated framing."
                        : (operation->invalid_request_framing
                            ? "Request Content-Length and Transfer-Encoding headers are managed by the HTTP transport."
                            : (operation->curl_error[0] != '\0'
                                ? operation->curl_error : "The HTTP transport callback failed.")))));
            if (operation->curl_error_code != 0) {
                cxcore_System_IO_IOException___constructor__4(
                    cause, cxnet_http_string("The libcurl operation failed."),
                    cxnet_http_string("HttpClient.SendAsync"), NULL,
                    cxnet_http_string("libcurl"), operation->curl_error_code,
                    NULL);
            } else {
                cxcore_System_IO_IOException___constructor__2(cause,
                    cxnet_http_string(
                        "The HTTP transport could not complete the request."));
            }
            if ((operation->status_code == 101L ||
                 operation->status_code >= 200L) &&
                operation->status_code <= 599L) {
                cxnet_System_Net_Http_HttpRequestException___constructor__4(
                    requestException,
                    (cxnet_System_Net_Http_HttpStatusCode)operation->status_code,
                    cxnet_http_string(message),
                    (struct cxcore_System_Exception *)cause);
            } else {
                cxnet_System_Net_Http_HttpRequestException___constructor__2(
                    requestException, cxnet_http_string(message),
                    (struct cxcore_System_Exception *)cause);
            }
            exception = (struct cxcore_System_Exception *)requestException;
        }
        if (operation->response_headers_completed &&
            operation->response_stream != NULL) {
            cxnet_System_Net_Http_CurlResponseStream_Fail(
                operation->response_stream, exception);
            cxnet_http_release_native(operation);
            return;
        }
        cxnet_http_release_native(operation);
        cxcore_System_Threading_Tasks_TaskRuntime_SetFault(
            (struct cxcore_System_Object *)operation->completion,
            exception);
        return;
    }
    if (operation->response_headers_completed &&
        operation->response_stream != NULL) {
        cxnet_System_Net_Http_CurlResponseStream_Complete(
            operation->response_stream);
        cxnet_http_release_native(operation);
        return;
    }
    response = cxnet_http_create_response(operation, 0);
    cxnet_http_release_native(operation);
    cx_generic_cffaa19f1c5c7eeac39eaf7e5d7657cd15ab974e1443642c08c9ac96b8d3bd61(
        operation->completion, response);
}

void cxnet_System_Net_Http_CurlHttpTransport_BeginSend(
    struct cxnet_System_Net_Http_HttpRequestMessage *request,
    struct cxcore_System_Threading_Tasks_CancellationToken cancellationToken,
    struct cxcore_System_Threading_Tasks_Task *completion,
    struct cxnet_System_Net_Http_CurlHttpRequestState *state,
    cx_uint maxResponseContentBufferSize,
    cx_bool automaticDecompression,
    struct cxcore_System_String *caCertificateFile,
    cx_bool allowUnknownCertificateRevocation,
    struct cxcore_System_String *proxyCaCertificateFile,
    struct cxcore_System_String *proxy,
    struct cxcore_System_String *proxyBypassList,
    struct cxcore_System_String *proxyUsername,
    struct cxcore_System_String *proxyPassword,
    cx_bool responseHeadersRead,
    struct cxnet_System_Net_Http_CurlResponseStream *responseStream,
    struct cxnet_System_Net_Http_CurlUploadStream *uploadStream,
    cx_uint maxConnectionsPerServer,
    cx_uint pooledConnectionIdleTimeoutMilliseconds,
    cx_uint pooledConnectionLifetimeMilliseconds,
    cx_ptr connectionShare) {
    struct cxnet_http_operation *operation;
    struct cxcore_System_Uri *uri;
    struct cxcore_System_String *url_value;
    struct cxnet_System_Net_Http_HttpMethod *method_value;
    char *url = NULL;
    char *method = NULL;
    char *ca_certificate_file = NULL;
    char *proxy_ca_certificate_file = NULL;
    char *proxy_url = NULL;
    char *proxy_bypass_list = NULL;
    char *proxy_username = NULL;
    char *proxy_password = NULL;
    struct cxnet_System_Net_Http_HttpHeaders *request_headers;
    struct cxnet_System_Net_Http_HttpHeaders *content_headers;
    struct cxnet_System_Net_Http_HttpHeaders *default_request_headers;
    struct cxnet_System_Net_Http_HttpContent *content;
    struct cxcore_System_Collections_Array *body = NULL;
    cx_uint timeout_milliseconds;
    long timeout_option;
    long connect_timeout_option;
    long max_connections_per_server_option;
    cx_action start_action = {0};
    (void)cancellationToken;
    operation = (struct cxnet_http_operation *)cxcore_System_Memory_Alloc(
        (cx_uint)sizeof(*operation));
    memset(operation, 0, sizeof(*operation));
    operation->max_body_length = maxResponseContentBufferSize;
    operation->automatic_decompression = automaticDecompression != 0;
    operation->response_headers_read = responseHeadersRead != 0;
    operation->response_headers_completed = 0;
    operation->body_paused = 0;
    operation->connection_share =
        (struct cxnet_http_connection_share *)connectionShare;
    cxnet_http_share_retain(operation->connection_share);
    operation->max_connections_per_server = maxConnectionsPerServer;
    operation->pooled_connection_idle_timeout_seconds =
        (long)((pooledConnectionIdleTimeoutMilliseconds + 999u) / 1000u);
    operation->pooled_connection_lifetime_seconds =
        (long)((pooledConnectionLifetimeMilliseconds + 999u) / 1000u);
    cx_gc_store_reference((cx_ptr *)&operation->response_stream,
        (cx_ptr)responseStream);
    cx_gc_store_reference((cx_ptr *)&operation->request_upload,
        (cx_ptr)uploadStream);
    if (maxResponseContentBufferSize == 0u ||
        maxResponseContentBufferSize > CXNET_HTTP_MAX_BODY) {
        operation->callback_error = 1;
        snprintf(operation->curl_error, sizeof(operation->curl_error),
            "%s", "Invalid MaxResponseContentBufferSize value.");
    }
    cx_gc_store_reference((cx_ptr *)&operation->completion, (cx_ptr)completion);
    cx_gc_store_reference((cx_ptr *)&operation->state, (cx_ptr)state);
    cxnet_System_Net_Http_CurlHttpRequestState_NativeOperation___set(
        state, (struct cxcore_System_Object *)operation);
    uri = cxnet_System_Net_Http_HttpRequestMessage_RequestUri___get(request);
    url_value = cxcore_System_Uri_AbsoluteUri___const_get(uri);
    url = cxnet_http_copy_string(url_value);
    if (url != NULL) {
        char *fragment = strchr(url, '#');
        if (fragment != NULL) *fragment = '\0';
    }
    operation->origin_scheme = cxnet_http_copy_string(
        cxcore_System_Uri_Scheme___const_get(uri));
    operation->origin_host = cxnet_http_copy_string(
        cxcore_System_Uri_Host___const_get(uri));
    operation->origin_port = (int)cxcore_System_Uri_Port___const_get(uri);
    if (operation->origin_scheme != NULL) {
        char *character = operation->origin_scheme;
        while (*character != '\0') {
            if (*character >= 'A' && *character <= 'Z')
                *character = (char)(*character + ('a' - 'A'));
            character++;
        }
    }
    if (operation->origin_host != NULL) {
        char *character = operation->origin_host;
        while (*character != '\0') {
            if (*character >= 'A' && *character <= 'Z')
                *character = (char)(*character + ('a' - 'A'));
            character++;
        }
    }
    ca_certificate_file = cxnet_http_copy_string(caCertificateFile);
    proxy_ca_certificate_file = cxnet_http_copy_string(proxyCaCertificateFile);
    proxy_url = cxnet_http_copy_string(proxy);
    proxy_bypass_list = cxnet_http_copy_string(proxyBypassList);
    proxy_username = cxnet_http_copy_string(proxyUsername);
    proxy_password = cxnet_http_copy_string(proxyPassword);
    method_value = cxnet_System_Net_Http_HttpRequestMessage_Method___get(request);
    method = cxnet_http_copy_string(
        cxnet_System_Net_Http_HttpMethod_Method___const_get(method_value));
    if (url == NULL || method == NULL || ca_certificate_file == NULL ||
        proxy_ca_certificate_file == NULL ||
        operation->origin_scheme == NULL || operation->origin_host == NULL ||
        (proxy != NULL && proxy_url == NULL) ||
        (proxyBypassList != NULL && proxy_bypass_list == NULL) ||
        (proxyUsername != NULL && proxy_username == NULL) ||
        (proxyPassword != NULL && proxy_password == NULL)) {
        operation->callback_error = 1;
        operation->curl_error[0] = '\0';
        cxnet_http_finish(operation);
        free(url); free(method); free(ca_certificate_file);
        free(proxy_ca_certificate_file);
        free(proxy_url); free(proxy_bypass_list);
        free(proxy_username); free(proxy_password);
        return;
    }
    request_headers =
        cxnet_System_Net_Http_HttpRequestMessage_TransportRequestHeaders___get(
            request);
    if (request_headers == NULL) {
        request_headers =
            cxnet_System_Net_Http_HttpRequestMessage_Headers___const_get(request);
    }
    cxnet_http_append_headers(operation, request_headers);
    default_request_headers =
        cxnet_System_Net_Http_HttpRequestMessage_TransportDefaultHeaders___get(request);
    if (default_request_headers != NULL && !operation->callback_error) {
        cxnet_http_append_headers(operation, default_request_headers);
    }
    content = cxnet_System_Net_Http_HttpRequestMessage_Content___get(request);
    if (content != NULL && !operation->callback_error) {
        content_headers =
            cxnet_System_Net_Http_HttpRequestMessage_TransportContentHeaders___get(
                request);
        if (content_headers == NULL) {
            content_headers =
                cxnet_System_Net_Http_HttpContent_Headers___const_get(content);
        }
        cxnet_http_append_headers(operation,
            content_headers);
    }
    if (content != NULL && uploadStream == NULL && !operation->callback_error) {
        body = (struct cxcore_System_Collections_Array *)
            cxnet_System_Net_Http_CurlHttpTransport_ReadContent(content);
        cx_gc_store_reference((cx_ptr *)&operation->request_body, (cx_ptr)body);
    }
    if ((body != NULL || uploadStream != NULL || strcmp(method, "POST") == 0) &&
        !cxnet_http_has_content_type_header(operation->request_headers)) {
        struct curl_slist *replacement = curl_slist_append(
            operation->request_headers, "Content-Type:");
        if (replacement == NULL) {
            operation->callback_error = 1;
        } else {
            operation->request_headers = replacement;
        }
    }
    timeout_milliseconds =
        cxnet_System_Net_Http_HttpRequestMessage_TransportTimeoutMilliseconds___get(request);
    timeout_option = timeout_milliseconds > (cx_uint)LONG_MAX
        ? LONG_MAX : (long)timeout_milliseconds;
    operation->timeout_milliseconds = timeout_option;
    operation->request_started_milliseconds =
        cx_platform_monotonic_time_milliseconds();
    connect_timeout_option = timeout_option < 15000L ? timeout_option : 15000L;
    max_connections_per_server_option = maxConnectionsPerServer > (cx_uint)LONG_MAX
        ? LONG_MAX : (long)maxConnectionsPerServer;
    if (max_connections_per_server_option <= 0L) {
        operation->callback_error = 1;
        snprintf(operation->curl_error, sizeof(operation->curl_error), "%s",
            "Invalid MaxConnectionsPerServer value.");
    }
    operation->max_connections_per_server = max_connections_per_server_option;
    if (!operation->callback_error && cxnet_http_initialize_curl() != CURLE_OK) {
        operation->callback_error = 1;
    }
    if (!operation->callback_error) {
        const char *backend_error = cxnet_http_backend_error(
            operation->automatic_decompression);
        if (backend_error != NULL) {
            operation->callback_error = 1;
            snprintf(operation->curl_error, sizeof(operation->curl_error), "%s",
                backend_error);
        }
    }
    if (!operation->callback_error) {
        operation->multi = curl_multi_init();
        operation->easy = curl_easy_init();
        if (operation->multi == NULL || operation->easy == NULL) operation->callback_error = 1;
    }
    if (!operation->callback_error) {
        (void)curl_multi_setopt(operation->multi, CURLMOPT_SOCKETFUNCTION, cxnet_http_socket_event);
        (void)curl_multi_setopt(operation->multi, CURLMOPT_SOCKETDATA, operation);
        (void)curl_multi_setopt(operation->multi, CURLMOPT_TIMERFUNCTION, cxnet_http_timer_event);
        (void)curl_multi_setopt(operation->multi, CURLMOPT_TIMERDATA, operation);
        (void)curl_easy_setopt(operation->easy, CURLOPT_URL, url);
        (void)curl_easy_setopt(operation->easy, CURLOPT_PROTOCOLS_STR, "http,https");
        (void)curl_easy_setopt(operation->easy, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
        (void)curl_easy_setopt(operation->easy, CURLOPT_FOLLOWLOCATION, 0L);
        if (curl_easy_setopt(operation->easy, CURLOPT_SSL_VERIFYPEER, 1L) != CURLE_OK ||
            curl_easy_setopt(operation->easy, CURLOPT_SSL_VERIFYHOST, 2L) != CURLE_OK) {
            operation->callback_error = 1;
        }
#ifdef _WIN32
        if (allowUnknownCertificateRevocation &&
            curl_easy_setopt(operation->easy, CURLOPT_SSL_OPTIONS,
                (long)CURLSSLOPT_REVOKE_BEST_EFFORT) != CURLE_OK) {
            operation->callback_error = 1;
            snprintf(operation->curl_error, sizeof(operation->curl_error), "%s",
                "Could not configure Schannel certificate revocation handling.");
        }
        if (allowUnknownCertificateRevocation && proxy_url != NULL &&
            curl_easy_setopt(operation->easy, CURLOPT_PROXY_SSL_OPTIONS,
                (long)CURLSSLOPT_REVOKE_BEST_EFFORT) != CURLE_OK) {
            operation->callback_error = 1;
            snprintf(operation->curl_error, sizeof(operation->curl_error), "%s",
                "Could not configure Schannel HTTPS proxy certificate revocation handling.");
        }
#else
        (void)allowUnknownCertificateRevocation;
#endif
        if (ca_certificate_file[0] != '\0' &&
            curl_easy_setopt(operation->easy, CURLOPT_CAINFO,
                ca_certificate_file) != CURLE_OK) {
            operation->callback_error = 1;
            snprintf(operation->curl_error, sizeof(operation->curl_error), "%s",
                "Could not configure the HTTP CA certificate file.");
        }
#ifndef _WIN32
        if (ca_certificate_file[0] == '\0') {
            const char *fallback_ca_bundle =
                cxnet_http_find_default_ca_bundle(operation->easy);
            if (fallback_ca_bundle != NULL &&
                curl_easy_setopt(operation->easy, CURLOPT_CAINFO,
                    fallback_ca_bundle) != CURLE_OK) {
                operation->callback_error = 1;
                snprintf(operation->curl_error, sizeof(operation->curl_error),
                    "%s", "Could not configure the platform HTTP CA bundle.");
            }
        }
#endif
        if (proxy_url != NULL &&
            curl_easy_setopt(operation->easy, CURLOPT_PROXY, proxy_url) != CURLE_OK) {
            operation->callback_error = 1;
            snprintf(operation->curl_error, sizeof(operation->curl_error), "%s",
                "Could not configure the HTTP proxy.");
        }
        if (proxy_username != NULL && proxy_password != NULL &&
            (curl_easy_setopt(operation->easy, CURLOPT_PROXYUSERNAME,
                 proxy_username) != CURLE_OK ||
             curl_easy_setopt(operation->easy, CURLOPT_PROXYPASSWORD,
                 proxy_password) != CURLE_OK ||
             curl_easy_setopt(operation->easy, CURLOPT_PROXYAUTH,
                 (long)CURLAUTH_ANY) != CURLE_OK)) {
            operation->callback_error = 1;
            snprintf(operation->curl_error, sizeof(operation->curl_error), "%s",
                "Could not configure HTTP proxy authentication.");
        }
        if (proxy_url != NULL &&
            (curl_easy_setopt(operation->easy, CURLOPT_PROXY_SSL_VERIFYPEER, 1L) != CURLE_OK ||
             curl_easy_setopt(operation->easy, CURLOPT_PROXY_SSL_VERIFYHOST, 2L) != CURLE_OK)) {
            operation->callback_error = 1;
            snprintf(operation->curl_error, sizeof(operation->curl_error), "%s",
                "Could not require TLS certificate verification for the HTTPS proxy.");
        }
        if (proxy_ca_certificate_file[0] != '\0' &&
            curl_easy_setopt(operation->easy, CURLOPT_PROXY_CAINFO,
                proxy_ca_certificate_file) != CURLE_OK) {
            operation->callback_error = 1;
            snprintf(operation->curl_error, sizeof(operation->curl_error), "%s",
                "Could not configure the HTTPS proxy CA bundle.");
        }
        if (proxy_bypass_list != NULL &&
            curl_easy_setopt(operation->easy, CURLOPT_NOPROXY,
                proxy_bypass_list) != CURLE_OK) {
            operation->callback_error = 1;
            snprintf(operation->curl_error, sizeof(operation->curl_error), "%s",
                "Could not configure the HTTP proxy bypass list.");
        }
        (void)curl_easy_setopt(operation->easy, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
        (void)curl_easy_setopt(operation->easy, CURLOPT_CONNECTTIMEOUT_MS, connect_timeout_option);
        (void)curl_easy_setopt(operation->easy, CURLOPT_TIMEOUT_MS, timeout_option);
        if (operation->pooled_connection_idle_timeout_seconds > 0L)
            (void)curl_easy_setopt(operation->easy, CURLOPT_MAXAGE_CONN,
                operation->pooled_connection_idle_timeout_seconds);
        if (operation->pooled_connection_lifetime_seconds > 0L)
            (void)curl_easy_setopt(operation->easy, CURLOPT_MAXLIFETIME_CONN,
                operation->pooled_connection_lifetime_seconds);
        (void)curl_easy_setopt(operation->easy, CURLOPT_WRITEFUNCTION, cxnet_http_receive_body);
        (void)curl_easy_setopt(operation->easy, CURLOPT_WRITEDATA, operation);
        (void)curl_easy_setopt(operation->easy, CURLOPT_HTTP_TRANSFER_DECODING, 0L);
        if (operation->automatic_decompression)
            (void)curl_easy_setopt(operation->easy, CURLOPT_ACCEPT_ENCODING,
                "gzip, deflate, br, zstd");
        (void)curl_easy_setopt(operation->easy, CURLOPT_HEADERFUNCTION, cxnet_http_receive_header);
        (void)curl_easy_setopt(operation->easy, CURLOPT_HEADERDATA, operation);
        if (uploadStream != NULL) {
            struct curl_slist *replacement;
            (void)curl_easy_setopt(operation->easy, CURLOPT_READFUNCTION,
                cxnet_http_read_upload);
            (void)curl_easy_setopt(operation->easy, CURLOPT_READDATA, operation);
            replacement = curl_slist_append(operation->request_headers,
                "Expect:");
            if (replacement == NULL) {
                operation->callback_error = 1;
            } else {
                operation->request_headers = replacement;
            }
            if (strcmp(method, "POST") == 0) {
                (void)curl_easy_setopt(operation->easy, CURLOPT_POST, 1L);
                (void)curl_easy_setopt(operation->easy, CURLOPT_POSTFIELDS, NULL);
                replacement = curl_slist_append(operation->request_headers,
                    "Transfer-Encoding: chunked");
                if (replacement == NULL) {
                    operation->callback_error = 1;
                } else {
                    operation->request_headers = replacement;
                }
            } else {
                (void)curl_easy_setopt(operation->easy, CURLOPT_UPLOAD, 1L);
                (void)curl_easy_setopt(operation->easy,
                    CURLOPT_INFILESIZE_LARGE, (curl_off_t)-1);
            }
        }
        (void)curl_easy_setopt(operation->easy, CURLOPT_ERRORBUFFER, operation->curl_error);
        (void)curl_easy_setopt(operation->easy, CURLOPT_NOSIGNAL, 1L);
        if (operation->request_headers != NULL)
            (void)curl_easy_setopt(operation->easy, CURLOPT_HTTPHEADER, operation->request_headers);
        if (body != NULL) {
            (void)curl_easy_setopt(operation->easy, CURLOPT_POSTFIELDS, body->_data);
            (void)curl_easy_setopt(operation->easy, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)body->_length);
        }
        if (strcmp(method, "GET") != 0) (void)curl_easy_setopt(operation->easy, CURLOPT_CUSTOMREQUEST, method);
        if (strcmp(method, "HEAD") == 0) {
            operation->request_is_head = 1;
            (void)curl_easy_setopt(operation->easy, CURLOPT_NOBODY, 1L);
        }
        if (strcmp(method, "POST") == 0 && body == NULL && uploadStream == NULL) {
            (void)curl_easy_setopt(operation->easy, CURLOPT_POSTFIELDS, "");
            (void)curl_easy_setopt(operation->easy, CURLOPT_POSTFIELDSIZE, 0L);
        }
    }
    free(url); free(method); free(ca_certificate_file);
    free(proxy_ca_certificate_file);
    free(proxy_url); free(proxy_bypass_list);
    free(proxy_username); free(proxy_password);
    if (operation->callback_error) {
        if (operation->curl_error[0] == '\0')
            snprintf(operation->curl_error, sizeof(operation->curl_error), "%s", "Could not initialize the HTTP transport.");
        cxnet_http_finish(operation);
        return;
    }
    start_action.__function = cxnet_http_start_on_scheduler;
    start_action.__environment = operation;
    start_action.__target = (cx_ptr)operation;
    cxcore_System_Threading_Tasks_TaskScheduler_Post(start_action);
}

static void cxnet_http_finish(struct cxnet_http_operation *operation);
#endif

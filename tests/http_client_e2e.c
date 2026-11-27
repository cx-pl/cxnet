#include "cxnet.internal.h"
#include "cxruntime.h"
#include <errno.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
typedef SOCKET test_socket;
typedef HANDLE test_thread;
typedef int test_socklen;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
typedef int test_socket;
typedef pthread_t test_thread;
typedef socklen_t test_socklen;
#endif

static uint64_t test_monotonic_milliseconds(void) {
#ifdef _WIN32
    return (uint64_t)GetTickCount64();
#else
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return 0u;
    return (uint64_t)now.tv_sec * 1000u +
        (uint64_t)now.tv_nsec / 1000000u;
#endif
}

extern void cxcore_System_String___constructor__2(
    struct cxcore_System_String *value, cx_uint length, cx_ptr data);
extern void cxnet_System_Net_Http_CurlHttpTransport_TestFailNextCancelSend(void);
extern void cxnet_System_Net_Http_HttpRequestMessage_TestInjectRedirectCleanupFailures(
    void);
extern void cxnet_System_Net_Http_HttpClient_TestFailNextHandlerContinuationRegistration(
    void);
extern cx_bool cxnet_System_Net_Http_HttpClient_TestHandlerContinuationRegistrationFailureConsumed(
    void);
extern void cxnet_System_Net_Http_CurlUploadStream_TestFailNextContinuationRegistration(
    void);
extern cx_bool cxnet_System_Net_Http_CurlUploadStream_TestContinuationRegistrationFailureConsumed(
    void);
extern void cxnet_System_Net_Http_CurlHttpTransport_TestFailNextBeginSendSetup(
    void);
extern cx_bool cxnet_System_Net_Http_CurlHttpTransport_TestBeginSendSetupFailureConsumed(
    void);
extern cx_uint cxnet_System_Net_Http_HttpRequestMessage_TestRedirectCleanupFailuresConsumed(
    void);
extern void cxnet_System_Net_Http_ByteArrayContent_TestInjectDisposeFailure(
    void);
extern cx_bool cxnet_System_Net_Http_ByteArrayContent_TestDisposeFailureConsumed(
    void);
extern void cxnet_System_Net_Http_HttpClient_TestCleanupRedirectRequest(
    struct cxnet_System_Net_Http_HttpRequestMessage *request);
extern void cxcore_System_Threading_Tasks_TaskRuntime_TestFailCancellationRegistrationAfter(
    cx_uint successful_registrations);
extern void cxcore_System_Threading_Tasks_TaskRuntime_TestFailContinuationRegistrationAfter(
    cx_uint successful_registrations);
extern void cx_task_scheduler_test_fail_next_post(void);
extern cx_bool cx_task_scheduler_test_post_failure_was_consumed(void);
extern void cxnet_System_Net_Http_Rest_RestClient_TestCompleteStringResponse(
    struct cxnet_System_Net_Http_Rest_RestClient *client,
    struct cxcore_System_Threading_Tasks_Task *pending,
    struct cxcore_System_Threading_Tasks_Task *completion);
extern void cxnet_System_Net_Http_Rest_RestClient_TestCompleteByteArrayResponse(
    struct cxnet_System_Net_Http_Rest_RestClient *client,
    struct cxcore_System_Threading_Tasks_Task *pending,
    struct cxcore_System_Threading_Tasks_Task *completion);

struct fixture {
    test_socket listener;
    int failed;
    int stall_response;
    volatile int body_sent;
    volatile size_t body_bytes_sent;
    volatile int client_closed;
    volatile int expect_client_close;
    volatile int request_received;
    volatile int request_body_read;
    volatile int partial_headers_sent;
    volatile int hold_response;
    volatile int release_response;
    volatile int hold_stream_body;
    volatile int release_stream_body;
    const char *expected_request_line;
    const char *expected_body;
    const char *expected_content_type;
    char observed_headers[256];
    size_t observed_body_bytes;
    size_t early_reject_body_bytes;
};

static const char chunk_delimiter_mutation_response[] =
    "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n"
    "Connection: close\r\n\r\n"
    "4;note=token\r\npong\r\n0\r\nX-CX-Trailer: value\r\n\r\n";

struct reuse_fixture {
    test_socket listener;
    volatile int failed;
    volatile int requests;
    volatile int accepted_connections;
    int first_response_mode;
};

struct abandoned_body_fixture {
    test_socket listener;
    volatile int failed;
    volatile int accepted_connections;
};

struct limit_fixture {
    test_socket listener;
    volatile int failed;
    volatile int accepted_connections;
    volatile int accepted_before_first_closed;
    volatile int client_closed;
    int expect_first_closed;
    int expect_second;
};

struct upload_cancel_fixture {
    test_socket listener;
    volatile int failed;
    volatile int client_closed;
    volatile int headers_received;
    volatile int body_started;
    volatile int allow_body_drain;
    int allow_empty_cancel;
    volatile size_t body_bytes;
};

struct request_send_race {
    struct cxnet_System_Net_Http_HttpRequestMessage *request;
    volatile int ready;
    volatile int start;
    volatile int completed;
    volatile int winners;
    volatile int rejected;
    volatile int failed;
};

struct request_state_race {
    struct cxnet_System_Net_Http_HttpRequestMessage *request;
    struct cxcore_System_Uri *uri;
    struct cxnet_System_Net_Http_HttpMethod *method;
    volatile int ready;
    volatile int start;
    volatile int completed;
    volatile int send_started;
    volatile int mutation_succeeded;
    volatile int mutation_rejected;
    volatile int failed;
};

struct request_state_race_worker {
    struct request_state_race *race;
    int operation;
};

struct content_stream_read_race {
    struct cxnet_System_Net_Http_ByteArrayContent *content;
    struct cxcore_System_Threading_Tasks_Task *tasks[2];
    volatile int disposed_reads[2];
    volatile int dispose_completed;
    volatile int ready;
    volatile int start;
    volatile int completed;
    volatile int failed;
};

struct content_stream_read_race_worker {
    struct content_stream_read_race *race;
    int index;
};

struct response_owned_stream_race {
    struct cxnet_System_Net_Http_ResponseOwnedStream *stream;
    struct cxcore_System_Collections_Array *bytes;
    volatile int ready;
    volatile int start;
    volatile int completed;
    volatile int read_status;
    volatile int dispose_completed;
    volatile int failed;
};

struct response_owned_stream_race_worker {
    struct response_owned_stream_race *race;
    int operation;
};

struct curl_response_stream_dispose_race {
    struct cxnet_System_Net_Http_CurlResponseStream *stream;
    volatile int ready;
    volatile int start;
    volatile int completed;
    volatile int disposed;
    volatile int failed;
};

struct curl_upload_stream_dispose_race {
    struct cxnet_System_Net_Http_CurlUploadStream *upload;
    volatile int ready;
    volatile int start;
    volatile int completed;
    volatile int disposed;
    volatile int failed;
};

struct response_send_lifetime_race {
    struct cxnet_System_Net_Http_HttpResponseMessage *response;
    struct cxnet_System_Net_Http_HttpResponseMessage *lifetime;
    volatile int ready;
    volatile int start;
    volatile int completed;
    volatile int failed;
};

struct response_send_lifetime_race_worker {
    struct response_send_lifetime_race *race;
    int operation;
};

struct response_content_race {
    struct cxnet_System_Net_Http_HttpResponseMessage *response;
    struct cxnet_System_Net_Http_HttpContent *content;
    volatile int ready;
    volatile int start;
    volatile int completed;
    volatile int assigned;
    volatile int rejected;
    volatile int failed;
};

struct multipart_serialization_race {
    struct cxnet_System_Net_Http_MultipartFormDataContent *content;
    struct cxcore_System_Collections_Array *serialized;
    volatile int ready;
    volatile int start;
    volatile int completed;
    volatile int read_status;
    volatile int dispose_completed;
    volatile int failed;
};

struct header_mutation_race {
    struct cxnet_System_Net_Http_HttpHeaders *headers;
    struct cxcore_System_String *raceName;
    struct cxcore_System_String *initialValue;
    struct cxcore_System_String *replacementValue;
    struct cxcore_System_String *lateName;
    struct cxcore_System_String *lateValue;
    struct cxcore_System_String *stableName;
    struct cxcore_System_String *stableValue;
    volatile int ready;
    volatile int start;
    volatile int completed;
    volatile int failed;
};

struct client_setting_race {
    struct cxnet_System_Net_Http_HttpClient *client;
    struct cxcore_System_Uri *newBaseAddress;
    struct cxcore_System_String *newProxy;
    struct cxcore_System_String *newProxyBypassList;
    struct cxnet_System_Net_Http_ProxyCredentials *newProxyCredentials;
    cx_uint newTimeoutMilliseconds;
    cx_uint newMaxResponseContentBufferSize;
    cx_uint newMaxConnectionsPerServer;
    cx_uint newPooledConnectionIdleTimeoutMilliseconds;
    cx_uint newPooledConnectionLifetimeMilliseconds;
    int newUseCookies;
    volatile int ready;
    volatile int start;
    volatile int completed;
    volatile int failed;
};


static void join_request_send_race_worker(test_thread thread);
static int start_response_content_race_worker(test_thread *thread,
    struct response_content_race *race);
static int response_content_set_attempt(
    struct cxnet_System_Net_Http_HttpResponseMessage *response,
    struct cxnet_System_Net_Http_HttpContent *content);
static int request_race_load(volatile int *value);
static int request_race_increment(volatile int *value);
static void request_race_store(volatile int *value, int next);
static int start_header_mutation_worker(test_thread *thread,
    struct header_mutation_race *race);
static int start_client_setting_worker(test_thread *thread,
    struct client_setting_race *race);

struct client_dispose_action {
    struct cxnet_System_Net_Http_HttpClient *client;
    volatile int started;
    volatile int dispose_started;
    volatile int fired;
    volatile int failed;
};

#ifdef _WIN32
static DWORD WINAPI reuse_fixture_main(void *context) {
#else
static void *reuse_fixture_main(void *context) {
#endif
    struct reuse_fixture *fixture = (struct reuse_fixture *)context;
    const char response[] = "HTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\npong";
    const char truncated_response[] =
        "HTTP/1.1 200 OK\r\nContent-Length: 8\r\n\r\npart";
    const char malformed_framing_response[] =
        "HTTP/1.1 200 OK\r\nContent-Length: 4\r\n"
        "Transfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
        "4\r\npong\r\n0\r\n\r\n";
    const char duplicate_transfer_encoding_response[] =
        "HTTP/1.1 200 OK\r\nContent-Length: 4\r\n"
        "Transfer-Encoding: chunked\r\nTransfer-Encoding: chunked\r\n"
        "Connection: close\r\n\r\n4\r\npong\r\n0\r\n\r\n";
    const char repeated_transfer_coding_response[] =
        "HTTP/1.1 200 OK\r\nContent-Length: 4\r\n"
        "Transfer-Encoding: chunked, chunked\r\n"
        "Connection: close\r\n\r\n4\r\npong\r\n0\r\n\r\n";
    const char conflicting_length_and_chunked_response[] =
        "HTTP/1.1 200 OK\r\nContent-Length: 4\r\nContent-Length: 5\r\n"
        "Transfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
        "4\r\npong\r\n0\r\n\r\n";
    const char identical_duplicate_length_and_chunked_response[] =
        "HTTP/1.1 200 OK\r\nContent-Length: 4\r\nContent-Length: 4\r\n"
        "Transfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
        "4\r\npong\r\n0\r\n\r\n";
    const char invalid_length_and_chunked_response[] =
        "HTTP/1.1 200 OK\r\nContent-Length: four\r\n"
        "Transfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
        "4\r\npong\r\n0\r\n\r\n";
    const char comma_length_and_chunked_response[] =
        "HTTP/1.1 200 OK\r\nContent-Length: 4, 5\r\n"
        "Transfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
        "4\r\npong\r\n0\r\n\r\n";
    const char matching_comma_length_and_chunked_response[] =
        "HTTP/1.1 200 OK\r\nContent-Length: 4, 4\r\n"
        "Transfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
        "4\r\npong\r\n0\r\n\r\n";
    const char empty_length_and_chunked_response[] =
        "HTTP/1.1 200 OK\r\nContent-Length:\r\n"
        "Transfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
        "4\r\npong\r\n0\r\n\r\n";
    const char whitespace_length_and_chunked_response[] =
        "HTTP/1.1 200 OK\r\nContent-Length: \t \r\n"
        "Transfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
        "4\r\npong\r\n0\r\n\r\n";
    const char overflowing_length_and_chunked_response[] =
        "HTTP/1.1 200 OK\r\nContent-Length: 18446744073709551616\r\n"
        "Transfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
        "4\r\npong\r\n0\r\n\r\n";
    const char signed_length_and_chunked_response[] =
        "HTTP/1.1 200 OK\r\nContent-Length: +4\r\n"
        "Transfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
        "4\r\npong\r\n0\r\n\r\n";
    const char chunked_parameter_response[] =
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked;foo=bar\r\n"
        "Connection: close\r\n\r\n4\r\npong\r\n0\r\n\r\n";
    const char transfer_encoding_leading_empty_response[] =
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: , chunked\r\n"
        "Connection: close\r\n\r\n4\r\npong\r\n0\r\n\r\n";
    const char transfer_encoding_trailing_empty_response[] =
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked,\r\n"
        "Connection: close\r\n\r\n4\r\npong\r\n0\r\n\r\n";
    test_socket client = (test_socket)-1;
    int total = 0;
    while (total < 2) {
        char request[4096];
        size_t used = 0u;
        int timed_out = 0;
        if (total == 0 || client == (test_socket)-1) {
            client = accept(fixture->listener, NULL, NULL);
#ifdef _WIN32
            if (client == INVALID_SOCKET) { fixture->failed = 1; break; }
            {
                DWORD timeout = 2500u;
                (void)setsockopt(client, SOL_SOCKET, SO_RCVTIMEO,
                    (const char *)&timeout, (int)sizeof(timeout));
            }
#else
            if (client < 0) { fixture->failed = 1; break; }
            {
                struct timeval timeout;
                timeout.tv_sec = 2;
                timeout.tv_usec = 500000;
                (void)setsockopt(client, SOL_SOCKET, SO_RCVTIMEO,
                    &timeout, sizeof(timeout));
            }
#endif
            fixture->accepted_connections++;
        }
        request[0] = '\0';
        while (strstr(request, "\r\n\r\n") == NULL) {
#ifdef _WIN32
            int amount = recv(client, request + used,
                (int)(sizeof(request) - used - 1u), 0);
            if (amount == SOCKET_ERROR) {
                int error = WSAGetLastError();
                timed_out = error == WSAETIMEDOUT || error == WSAEWOULDBLOCK;
            }
#else
            ssize_t amount = recv(client, request + used,
                sizeof(request) - used - 1u, 0);
            if (amount < 0) timed_out = errno == EAGAIN || errno == EWOULDBLOCK;
#endif
            if (amount <= 0) break;
            used += (size_t)amount;
            request[used] = '\0';
            if (used == sizeof(request) - 1u) { fixture->failed = 1; break; }
        }
        if (timed_out) {
#ifdef _WIN32
            closesocket(client);
#else
            close(client);
#endif
            client = (test_socket)-1;
            continue;
        }
        if (fixture->failed) break;
        if (used == 0u) {
#ifdef _WIN32
            closesocket(client);
#else
            close(client);
#endif
            client = (test_socket)-1;
            continue;
        }
        if (strstr(request, "GET /cx-http-reuse HTTP/1.1\r\n") == NULL) {
            fixture->failed = 1;
            break;
        }
        {
            const char *current_response = response;
            if (total == 0 && fixture->first_response_mode == 1)
                current_response = truncated_response;
            else if (total == 0 && fixture->first_response_mode == 2)
                current_response = malformed_framing_response;
            else if (total == 0 && fixture->first_response_mode == 3)
                current_response = duplicate_transfer_encoding_response;
            else if (total == 0 && fixture->first_response_mode == 4)
                current_response = repeated_transfer_coding_response;
            else if (total == 0 && fixture->first_response_mode == 5)
                current_response = conflicting_length_and_chunked_response;
            else if (total == 0 && fixture->first_response_mode == 6)
                current_response = invalid_length_and_chunked_response;
            else if (total == 0 && fixture->first_response_mode == 7)
                current_response = comma_length_and_chunked_response;
            else if (total == 0 && fixture->first_response_mode == 8)
                current_response = matching_comma_length_and_chunked_response;
            else if (total == 0 && fixture->first_response_mode == 9)
                current_response = empty_length_and_chunked_response;
            else if (total == 0 && fixture->first_response_mode == 10)
                current_response = whitespace_length_and_chunked_response;
            else if (total == 0 && fixture->first_response_mode == 11)
                current_response = overflowing_length_and_chunked_response;
            else if (total == 0 && fixture->first_response_mode == 12)
                current_response = signed_length_and_chunked_response;
            else if (total == 0 && fixture->first_response_mode == 13)
                current_response = identical_duplicate_length_and_chunked_response;
            else if (total == 0 && fixture->first_response_mode == 14)
                current_response = chunked_parameter_response;
            else if (total == 0 && fixture->first_response_mode == 15)
                current_response = transfer_encoding_leading_empty_response;
            else if (total == 0 && fixture->first_response_mode == 16)
                current_response = transfer_encoding_trailing_empty_response;
            size_t response_length = strlen(current_response);
            size_t sent = 0u;
            while (sent < response_length) {
#ifdef _WIN32
                int amount = send(client, current_response + sent,
                    (int)(response_length - sent), 0);
#else
                ssize_t amount = send(client, current_response + sent,
                    response_length - sent, 0);
#endif
                if (amount <= 0) { fixture->failed = 1; break; }
                sent += (size_t)amount;
            }
        }
        if (fixture->failed) break;
        total++;
        fixture->requests = total;
        if (fixture->first_response_mode != 0 && total == 1) {
#ifdef _WIN32
            closesocket(client);
#else
            close(client);
#endif
            client = (test_socket)-1;
        }
    }
#ifdef _WIN32
    if (client != INVALID_SOCKET && client != (test_socket)-1) closesocket(client);
    return 0;
#else
    if (client >= 0) close(client);
    return NULL;
#endif
}

#ifdef _WIN32
static DWORD WINAPI abandoned_body_fixture_main(void *context) {
#else
static void *abandoned_body_fixture_main(void *context) {
#endif
    struct abandoned_body_fixture *fixture =
        (struct abandoned_body_fixture *)context;
    test_socket client = accept(fixture->listener, NULL, NULL);
    const char first[] = "HTTP/1.1 200 OK\r\nContent-Length: 1048576\r\nConnection: close\r\n\r\n";
    char request[1024];
    char discard[128];
    size_t sent = 0u;
    size_t request_used = 0u;
    if (client == (test_socket)-1
#ifdef _WIN32
        || client == INVALID_SOCKET
#endif
        ) {
        fixture->failed = 1;
        goto done;
    }
    fixture->accepted_connections++;
    request[0] = '\0';
    while (strstr(request, "\r\n\r\n") == NULL &&
        request_used < sizeof(request) - 1u) {
#ifdef _WIN32
        int amount = recv(client, request + request_used,
            (int)(sizeof(request) - request_used - 1u), 0);
#else
        ssize_t amount = recv(client, request + request_used,
            sizeof(request) - request_used - 1u, 0);
#endif
        if (amount <= 0) { fixture->failed = 1; break; }
        request_used += (size_t)amount;
        request[request_used] = '\0';
    }
    if (strstr(request, "GET /cx-http-abandoned HTTP/1.1\r\n") == NULL) {
        fixture->failed = 1;
    }
#ifdef _WIN32
    {
        DWORD timeout = 4000u;
        (void)setsockopt(client, SOL_SOCKET, SO_RCVTIMEO,
            (const char *)&timeout, (int)sizeof(timeout));
    }
#else
    {
        struct timeval timeout = {4, 0};
        (void)setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &timeout,
            (socklen_t)sizeof(timeout));
    }
#endif
    while (sent < sizeof(first) - 1u) {
#ifdef _WIN32
        int amount = send(client, first + sent, (int)(sizeof(first) - 1u - sent), 0);
#else
        ssize_t amount = send(client, first + sent, sizeof(first) - 1u - sent, 0);
#endif
        if (amount <= 0) { fixture->failed = 1; break; }
        sent += (size_t)amount;
    }
    /* The response body never arrives. Disposal should close this transfer. */
    if (!fixture->failed) {
#ifdef _WIN32
        int amount = recv(client, discard, (int)sizeof(discard), 0);
#else
        ssize_t amount = recv(client, discard, sizeof(discard), 0);
#endif
        if (amount != 0) fixture->failed = 1;
    }
#ifdef _WIN32
    closesocket(client);
#else
    close(client);
#endif
    if (fixture->failed) goto done;
    client = accept(fixture->listener, NULL, NULL);
    if (client == (test_socket)-1
#ifdef _WIN32
        || client == INVALID_SOCKET
#endif
        ) {
        fixture->failed = 1;
        goto done;
    }
    fixture->accepted_connections++;
    {
        const char response[] =
            "HTTP/1.1 200 OK\r\nContent-Length: 4\r\nConnection: close\r\n\r\npong";
        request_used = 0u;
        request[0] = '\0';
        while (strstr(request, "\r\n\r\n") == NULL &&
            request_used < sizeof(request) - 1u) {
#ifdef _WIN32
            int amount = recv(client, request + request_used,
                (int)(sizeof(request) - request_used - 1u), 0);
#else
            ssize_t amount = recv(client, request + request_used,
                sizeof(request) - request_used - 1u, 0);
#endif
            if (amount <= 0) { fixture->failed = 1; break; }
            request_used += (size_t)amount;
            request[request_used] = '\0';
        }
        if (strstr(request, "GET /cx-http-abandoned HTTP/1.1\r\n") == NULL) {
            fixture->failed = 1;
        }
        sent = 0u;
        while (!fixture->failed && sent < sizeof(response) - 1u) {
#ifdef _WIN32
            int amount = send(client, response + sent,
                (int)(sizeof(response) - 1u - sent), 0);
#else
            ssize_t amount = send(client, response + sent,
                sizeof(response) - 1u - sent, 0);
#endif
            if (amount <= 0) { fixture->failed = 1; break; }
            sent += (size_t)amount;
        }
    }
#ifdef _WIN32
    closesocket(client);
#else
    close(client);
#endif
done:
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

static struct cxcore_System_String *make_string(const char *text) {
    size_t length = strlen(text);
    cx_ptr data = length == 0u ? NULL : cxcore_System_Memory_Alloc((cx_uint)length);
    struct cxcore_System_String *value =
        (struct cxcore_System_String *)cxcore_System_Memory_Alloc(
            cxcore_System_String___typeinfo.Size);
    if (length != 0u) memcpy(data, text, length);
    cxcore_System_String___constructor__2(value,
        (cx_uint)length, data);
    return value;
}

static struct cxcore_System_String *make_scalar_string(
    const cx_char *scalars, cx_uint length) {
    struct cxcore_System_StringBuilder *builder =
        (struct cxcore_System_StringBuilder *)cxcore_System_Memory_Alloc(
            cxcore_System_StringBuilder___typeinfo.Size);
    cx_uint index;
    cxcore_System_StringBuilder___constructor(builder);
    for (index = 0u; index < length; index += 1u) {
        cxcore_System_StringBuilder_Append__2(builder, scalars[index]);
    }
    return cxcore_System_StringBuilder_ToString(builder);
}

static int string_equals(const struct cxcore_System_String *value,
    const char *expected) {
    size_t length = strlen(expected);
    return value != NULL && value->_length == length &&
        memcmp(value->_data, expected, length) == 0;
}

#ifdef _WIN32
static DWORD WINAPI fixture_main(void *context) {
#else
static void *fixture_main(void *context) {
#endif
    static const char response_with_body[] =
        "HTTP/1.1 200 OK\r\nContent-Length: 4\r\nContent-Type: text/plain\r\n"
        "X-CX-Test: passed\r\nConnection: close\r\n\r\npong";
    static const char head_response[] =
        "HTTP/1.1 200 OK\r\nContent-Length: 4\r\nContent-Type: text/plain\r\n"
        "X-CX-Test: passed\r\nConnection: close\r\n\r\n";
    static const char no_content_response[] =
        "HTTP/1.1 204 No Content\r\nX-CX-Test: passed\r\nConnection: close\r\n\r\n";
    static const char no_content_with_length_response[] =
        "HTTP/1.1 204 No Content\r\nContent-Length: 0\r\n"
        "Connection: close\r\n\r\n";
    static const char no_content_with_transfer_encoding_response[] =
        "HTTP/1.1 204 No Content\r\nTransfer-Encoding: chunked\r\n"
        "Connection: close\r\n\r\n0\r\n\r\n";
    static const char reset_content_response[] =
        "HTTP/1.1 205 Reset Content\r\nContent-Length: 0\r\n"
        "Connection: close\r\n\r\n";
    static const char reset_content_chunked_response[] =
        "HTTP/1.1 205 Reset Content\r\nTransfer-Encoding: chunked\r\n"
        "Connection: close\r\n\r\n0\r\n\r\n";
    static const char reset_content_with_body_response[] =
        "HTTP/1.1 205 Reset Content\r\nContent-Length: 4\r\n"
        "Connection: close\r\n\r\n"
        "fail";
    static const char reset_content_chunked_with_body_response[] =
        "HTTP/1.1 205 Reset Content\r\nTransfer-Encoding: chunked\r\n"
        "Connection: close\r\n\r\n4\r\nfail\r\n0\r\n\r\n";
    static const char not_modified_response[] =
        "HTTP/1.1 304 Not Modified\r\nContent-Length: 4\r\n"
        "ETag: \"cx-fixture\"\r\nConnection: close\r\n\r\n";
    static const char cookie_set_response[] =
        "HTTP/1.1 200 OK\r\nSet-Cookie: session=blue; Path=/cx-http-cookie-area; HttpOnly; Max-Age=60\r\n"
        "Content-Length: 3\r\nConnection: close\r\n\r\nset";
    static const char cookie_delete_response[] =
        "HTTP/1.1 200 OK\r\nSet-Cookie: session=; Path=/cx-http-cookie-area; Max-Age=0\r\n"
        "Content-Length: 7\r\nConnection: close\r\n\r\ndeleted";
    static const char cookie_expired_response[] =
        "HTTP/1.1 200 OK\r\nSet-Cookie: past=old; Path=/cx-http-cookie-area; Expires=Wed, 21 Oct 2015 07:28:00 GMT\r\n"
        "Content-Length: 7\r\nConnection: close\r\n\r\nexpired";
    static const char cookie_found_response[] =
        "HTTP/1.1 200 OK\r\nContent-Length: 5\r\nConnection: close\r\n\r\nfound";
    static const char cookie_missing_response[] =
        "HTTP/1.1 200 OK\r\nContent-Length: 7\r\nConnection: close\r\n\r\nmissing";
    static const char truncated_body_response[] =
        "HTTP/1.1 200 OK\r\nContent-Length: 10\r\nConnection: close\r\n\r\npart";
    static const char conflicting_length_response[] =
        "HTTP/1.1 200 OK\r\nContent-Length: 4\r\nContent-Length: 5\r\n"
        "Connection: close\r\n\r\npong";
    static const char comma_length_response[] =
        "HTTP/1.1 200 OK\r\nContent-Length: 4, 4\r\n"
        "Connection: close\r\n\r\npong";
    static const char conflicting_comma_length_response[] =
        "HTTP/1.1 200 OK\r\nContent-Length: 4, 5\r\n"
        "Connection: close\r\n\r\npong";
    static const char content_length_and_chunked_response[] =
        "HTTP/1.1 200 OK\r\nContent-Length: 4\r\n"
        "Transfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
        "4\r\npong\r\n0\r\n\r\n";
    static const char conflicting_length_and_chunked_response[] =
        "HTTP/1.1 200 OK\r\nContent-Length: 4\r\nContent-Length: 5\r\n"
        "Transfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
        "4\r\npong\r\n0\r\n\r\n";
    static const char repeated_chunked_encoding_response[] =
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n"
        "Transfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
        "4\r\npong\r\n0\r\n\r\n";
    static const char comma_repeated_chunked_encoding_response[] =
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked, chunked\r\n"
        "Connection: close\r\n\r\n4\r\npong\r\n0\r\n\r\n";
    static const char malformed_status_response[] =
        "HTTP/1.1 20X Invalid\r\nContent-Length: 0\r\n"
        "Connection: close\r\n\r\n";
    static const char switching_protocols_response[] =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Connection: Upgrade\r\nUpgrade: cx-fixture\r\n\r\n";
    static const char malformed_field_name_response[] =
        "HTTP/1.1 200 OK\r\nBad Header: value\r\nContent-Length: 0\r\n"
        "Connection: close\r\n\r\n";
    static const char malformed_field_value_response[] =
        "HTTP/1.1 200 OK\r\nX-Bad: value\001tail\r\nContent-Length: 0\r\n"
        "Connection: close\r\n\r\n";
    static const char high_byte_response[] =
        "HTTP/1.1 200 \x80\xff\r\nX-CX-High: \x80\xff\r\n"
        "Content-Length: 4\r\nConnection: close\r\n\r\npong";
    static const char latin1_charset_response[] =
        "HTTP/1.1 200 OK\r\nContent-Type: text/plain; charset=iso-8859-1\r\n"
        "Content-Length: 4\r\nConnection: close\r\n\r\ncaf\xe9";
    static const char ascii_charset_response[] =
        "HTTP/1.1 200 OK\r\nContent-Type: text/plain; charset=us-ascii\r\n"
        "Content-Length: 5\r\nConnection: close\r\n\r\nascii";
    static const char unsupported_charset_response[] =
        "HTTP/1.1 200 OK\r\nContent-Type: text/plain; charset=x-unknown\r\n"
        "Content-Length: 1\r\nConnection: close\r\n\r\nx";
    static const char duplicate_charset_response[] =
        "HTTP/1.1 200 OK\r\nContent-Type: text/plain; charset=utf-8; charset=us-ascii\r\n"
        "Content-Length: 1\r\nConnection: close\r\n\r\nx";
    static const char truncated_chunk_response[] =
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n4\r\npong\r\n";
    static const char truncated_chunk_data_response[] =
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
        "5\r\npong";
    static const char lf_chunk_size_response[] =
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
        "4\npong\r\n0\r\n\r\n";
    static const char missing_chunk_data_cr_response[] =
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
        "4\r\npong\n0\r\n\r\n";
    static const char doubled_chunk_size_cr_response[] =
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
        "4\r\r\npong\r\n0\r\n\r\n";
    static const char doubled_chunk_data_cr_response[] =
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
        "4\r\npong\r\r\n0\r\n\r\n";
    static const char lf_chunk_trailer_response[] =
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
        "0\r\nX-CX-Trailer: value\n\r\n";
    static const char malformed_zero_chunk_terminator_response[] =
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
        "0\r\n\rX";
    static const char valid_chunk_response[] =
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
        "4;source=fixture;note=\"a\\\"b\"\r\npong\r\n"
        "0\r\nX-CX-Trailer: passed\r\n\r\n";
    static const char segmented_chunk_response[] =
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
        "4;source=fixture;note=\"split\"\r\npong\r\n"
        "0\r\nX-CX-Trailer: passed\r\n\r\n";
    static const char malformed_chunk_trailer_response[] =
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
        "4\r\npong\r\n0\r\nBad Header: value\r\n\r\n";
    static const char malformed_chunk_extension_response[] =
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
        "4;note=\"unterminated\r\npong\r\n0\r\n\r\n";
    static const char chunk_extension_control_response[] =
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
        "4;note=\"a\001b\"\r\npong\r\n0\r\n\r\n";
    static const char chunk_extension_escaped_control_response[] =
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
        "4;note=\"a\\\001b\"\r\npong\r\n0\r\n\r\n";
    static const char chunk_trailer_del_response[] =
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
        "0\r\nX-Tag: a\177b\r\n\r\n";
    static const char chunk_trailer_control_response[] =
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
        "0\r\nX-Tag: a\001b\r\n\r\n";
    static const char chunk_trailer_obs_fold_response[] =
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
        "0\r\nX-Tag: value\r\n continuation\r\n\r\n";
    static const char empty_chunk_size_response[] =
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
        "\r\n";
    static const char signed_chunk_size_response[] =
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
        "+4\r\npong\r\n0\r\n\r\n";
    static const char prefixed_chunk_size_response[] =
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
        "0x4\r\npong\r\n0\r\n\r\n";
    static const char non_hex_chunk_size_response[] =
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
        "4G\r\npong\r\n0\r\n\r\n";
    static const char leading_space_chunk_size_response[] =
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
        " 4\r\npong\r\n0\r\n\r\n";
    static const char overflowing_chunk_size_response[] =
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
        "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF\r\n";
    static const char malformed_chunk_data_terminator_response[] =
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
        "4\r\npongX\n0\r\n\r\n";
    static const char malformed_chunk_extension_name_response[] =
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
        "4;=empty\r\npong\r\n0\r\n\r\n";
    static const char malformed_chunk_extension_value_response[] =
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
        "4;note=\r\npong\r\n0\r\n\r\n";
    static const char malformed_chunk_extension_token_response[] =
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
        "4;note=token/value\r\npong\r\n0\r\n\r\n";
    static const char truncated_chunk_trailer_response[] =
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
        "0\r\nX-CX-Trailer: incomplete\r\n";
    static const char close_delimited_response[] =
        "HTTP/1.1 200 OK\r\nConnection: close\r\n\r\npong";
    static const char interim_response[] =
        "HTTP/1.1 103 Early Hints\r\nLink: </style.css>; rel=preload; as=style\r\n\r\n"
        "HTTP/1.1 100 Continue\r\n\r\n"
        "HTTP/1.1 200 OK\r\nContent-Length: 4\r\nConnection: close\r\n\r\npong";
    static const char early_reject_response[] =
        "HTTP/1.1 417 Expectation Failed\r\nContent-Length: 0\r\n"
        "Connection: close\r\n\r\n";
    static const char early_body_reject_response[] =
        "HTTP/1.1 413 Payload Too Large\r\nContent-Length: 0\r\n"
        "Connection: close\r\n\r\n";
    static const char gzip_response[] =
        "HTTP/1.1 200 OK\r\nContent-Length: 24\r\nContent-Encoding: gzip\r\nConnection: close\r\n\r\n"
        "\x1f\x8b\x08\x00\x00\x00\x00\x00\x02\xff\x2b\xc8\xcf\x4b\x07\x00\x4f\x41\x58\x21\x04\x00\x00\x00";
    static const char gzip_large_response[] =
        "HTTP/1.1 200 OK\r\nContent-Length: 24\r\nContent-Encoding: gzip\r\nConnection: close\r\n\r\n"
        "\x1f\x8b\x08\x00\x00\x00\x00\x00\x02\xff\xab\xa8\xa0\x0c\x00\x00\x12\xac\xd2\x3a\x40\x00\x00\x00";
    static const char gzip_stacked_response[] =
        "HTTP/1.1 200 OK\r\nContent-Length: 40\r\nContent-Encoding: gzip ,  gzip\r\nConnection: close\r\n\r\n"
        "\x1f\x8b\x08\x00\x00\x00\x00\x00\x02\xff\x93\xef\xe6\x60\x00\x01\xa6\xff\xda\x27\xce\x7b\xb3\x33\xf8\x3b\x46\x28\xb2\x00\xf9\x00\xe4\x4f\x25\x11\x18\x00\x00\x00\x00";
    static const char brotli_response[] =
        "HTTP/1.1 200 OK\r\nContent-Length: 8\r\nContent-Encoding: br\r\nConnection: close\r\n\r\n"
        "\x8b\x01\x80\x70\x6f\x6e\x67\x03";
    static const char zstd_response[] =
        "HTTP/1.1 200 OK\r\nContent-Length: 13\r\nContent-Encoding: zstd\r\nConnection: close\r\n\r\n"
        "\x28\xb5\x2f\xfd\x20\x04\x21\x00\x00\x70\x6f\x6e\x67";
    struct fixture *fixture = (struct fixture *)context;
    char oversized_header_response[20000];
    char oversized_chunk_line_response[20000];
    char chunk_delimiter_mutated_response[sizeof(chunk_delimiter_mutation_response)];
    const char *response = response_with_body;
    size_t response_length = sizeof(response_with_body) - 1u;
    int stream_response = 0;
    int early_reject_upload = strstr(fixture->expected_request_line,
        "/cx-http-early-reject ") != NULL;
    int early_body_reject_upload = strstr(fixture->expected_request_line,
        "/cx-http-early-body-reject ") != NULL;
    test_socket client = accept(fixture->listener, NULL, NULL);
    char request[150000];
    size_t used = 0u;
    size_t sent = 0u;
    request[0] = '\0';
    if (strstr(fixture->expected_request_line, "HEAD ") != NULL) {
        response = head_response;
        response_length = sizeof(head_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-stream-backpressure ") != NULL) {
        stream_response = 4;
        response_length = 0u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-stream-large ") != NULL) {
        stream_response = 2;
        response_length = 0u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-stream-headers ") != NULL) {
        stream_response = 1;
        response_length = 0u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-stream-cancel ") != NULL) {
        stream_response = 3;
        response_length = 0u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-stream-close-delimited ") != NULL) {
        stream_response = 5;
        response_length = 0u;
    } else if (strstr(fixture->expected_request_line, "/cx-http-stream ") != NULL) {
        stream_response = 1;
        response_length = 0u;
    } else if (strstr(fixture->expected_request_line, "/cx-http-header-limit ") != NULL) {
        size_t offset = (size_t)snprintf(oversized_header_response,
            sizeof(oversized_header_response), "HTTP/1.1 200 OK\r\n");
        unsigned int header_index;
        for (header_index = 0u; header_index < 129u &&
                offset < sizeof(oversized_header_response); header_index++) {
            int amount = snprintf(oversized_header_response + offset,
                sizeof(oversized_header_response) - offset,
                "X-Fill-%03u: x\r\n", header_index);
            if (amount < 0 || (size_t)amount >=
                    sizeof(oversized_header_response) - offset) {
                fixture->failed = 1;
                break;
            }
            offset += (size_t)amount;
        }
        if (!fixture->failed) {
            int amount = snprintf(oversized_header_response + offset,
                sizeof(oversized_header_response) - offset,
                "Content-Length: 0\r\nConnection: close\r\n\r\n");
            if (amount < 0 || (size_t)amount >=
                    sizeof(oversized_header_response) - offset) {
                fixture->failed = 1;
            } else {
                offset += (size_t)amount;
                response = oversized_header_response;
                response_length = offset;
            }
        }
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-header-bytes-limit ") != NULL) {
        const char header_prefix[] = "HTTP/1.1 200 OK\r\nX-Fill: ";
        const char header_suffix[] = "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        size_t prefix_length = sizeof(header_prefix) - 1u;
        size_t filler_length = 17000u;
        size_t suffix_length = sizeof(header_suffix) - 1u;
        if (prefix_length + filler_length + suffix_length >
                sizeof(oversized_header_response)) {
            fixture->failed = 1;
        } else {
            memcpy(oversized_header_response, header_prefix, prefix_length);
            memset(oversized_header_response + prefix_length, 'x', filler_length);
            memcpy(oversized_header_response + prefix_length + filler_length,
                header_suffix, suffix_length);
            response = oversized_header_response;
            response_length = prefix_length + filler_length + suffix_length;
        }
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-chunk-size-line-limit ") != NULL) {
        const char line_prefix[] =
            "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n"
            "Connection: close\r\n\r\n4;fill=";
        const char line_suffix[] = "\r\npong\r\n0\r\n\r\n";
        size_t prefix_length = sizeof(line_prefix) - 1u;
        size_t fill_length = 17000u;
        size_t suffix_length = sizeof(line_suffix) - 1u;
        if (prefix_length + fill_length + suffix_length >
                sizeof(oversized_chunk_line_response)) {
            fixture->failed = 1;
        } else {
            memcpy(oversized_chunk_line_response, line_prefix, prefix_length);
            memset(oversized_chunk_line_response + prefix_length, 'x', fill_length);
            memcpy(oversized_chunk_line_response + prefix_length + fill_length,
                line_suffix, suffix_length);
            response = oversized_chunk_line_response;
            response_length = prefix_length + fill_length + suffix_length;
        }
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-malformed-status ") != NULL) {
        response = malformed_status_response;
        response_length = sizeof(malformed_status_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-101 ") != NULL) {
        response = switching_protocols_response;
        response_length = sizeof(switching_protocols_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-malformed-field-name ") != NULL) {
        response = malformed_field_name_response;
        response_length = sizeof(malformed_field_name_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-malformed-field-value ") != NULL) {
        response = malformed_field_value_response;
        response_length = sizeof(malformed_field_value_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-high-byte-text ") != NULL) {
        response = high_byte_response;
        response_length = sizeof(high_byte_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-charset-latin1 ") != NULL) {
        response = latin1_charset_response;
        response_length = sizeof(latin1_charset_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-charset-ascii ") != NULL) {
        response = ascii_charset_response;
        response_length = sizeof(ascii_charset_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-charset-unsupported ") != NULL) {
        response = unsupported_charset_response;
        response_length = sizeof(unsupported_charset_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-charset-duplicate ") != NULL) {
        response = duplicate_charset_response;
        response_length = sizeof(duplicate_charset_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-truncated-body ") != NULL) {
        response = truncated_body_response;
        response_length = sizeof(truncated_body_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-conflicting-length ") != NULL) {
        response = conflicting_length_response;
        response_length = sizeof(conflicting_length_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-length-and-chunked ") != NULL) {
        response = content_length_and_chunked_response;
        response_length = sizeof(content_length_and_chunked_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-conflicting-length-and-chunked ") != NULL) {
        response = conflicting_length_and_chunked_response;
        response_length = sizeof(conflicting_length_and_chunked_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-repeated-chunked-encoding ") != NULL) {
        response = repeated_chunked_encoding_response;
        response_length = sizeof(repeated_chunked_encoding_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-comma-repeated-chunked-encoding ") != NULL) {
        response = comma_repeated_chunked_encoding_response;
        response_length =
            sizeof(comma_repeated_chunked_encoding_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-comma-length-conflict ") != NULL) {
        response = conflicting_comma_length_response;
        response_length = sizeof(conflicting_comma_length_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-comma-length ") != NULL) {
        response = comma_length_response;
        response_length = sizeof(comma_length_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-truncated-chunk ") != NULL) {
        response = truncated_chunk_response;
        response_length = sizeof(truncated_chunk_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-chunk-size-byte-") != NULL) {
        const char marker[] = "/cx-http-chunk-size-byte-";
        const char *target = strstr(fixture->expected_request_line, marker);
        const char *header_end = strstr(chunk_delimiter_mutation_response,
            "\r\n\r\n");
        char *number_end;
        unsigned long byte_value = target == NULL ? 0ul :
            strtoul(target + sizeof(marker) - 1u, &number_end, 10);
        size_t header_length = header_end == NULL ? 0u :
            (size_t)(header_end - chunk_delimiter_mutation_response) + 4u;
        if (target == NULL || header_end == NULL ||
            number_end == target + sizeof(marker) - 1u ||
            *number_end != ' ' || byte_value > 255ul) {
            fixture->failed = 1;
        } else {
            memcpy(chunk_delimiter_mutated_response,
                chunk_delimiter_mutation_response,
                sizeof(chunk_delimiter_mutation_response));
            ((unsigned char *)chunk_delimiter_mutated_response)[header_length] =
                (unsigned char)byte_value;
            response = chunk_delimiter_mutated_response;
            response_length = sizeof(chunk_delimiter_mutated_response) - 1u;
        }
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-chunk-delimiter-mutation-") != NULL) {
        const char marker[] = "/cx-http-chunk-delimiter-mutation-";
        const char *target = strstr(fixture->expected_request_line, marker);
        const char *header_end = strstr(chunk_delimiter_mutation_response,
            "\r\n\r\n");
        char *number_end;
        unsigned long body_offset = target == NULL ? 0ul :
            strtoul(target + sizeof(marker) - 1u, &number_end, 10);
        size_t header_length = header_end == NULL ? 0u :
            (size_t)(header_end - chunk_delimiter_mutation_response) + 4u;
        size_t body_length = sizeof(chunk_delimiter_mutation_response) - 1u -
            header_length;
        if (target == NULL || header_end == NULL ||
            number_end == target + sizeof(marker) - 1u ||
            *number_end != ' ' || (size_t)body_offset >= body_length ||
            (chunk_delimiter_mutation_response[header_length + body_offset] !=
                '\r' && chunk_delimiter_mutation_response[
                    header_length + body_offset] != '\n')) {
            fixture->failed = 1;
        } else {
            memcpy(chunk_delimiter_mutated_response,
                chunk_delimiter_mutation_response,
                sizeof(chunk_delimiter_mutation_response));
            chunk_delimiter_mutated_response[header_length + body_offset] =
                chunk_delimiter_mutation_response[
                    header_length + body_offset] == '\r' ? '\n' : '\r';
            response = chunk_delimiter_mutated_response;
            response_length = sizeof(chunk_delimiter_mutated_response) - 1u;
        }
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-chunk-truncation-") != NULL) {
        const char marker[] = "/cx-http-chunk-truncation-";
        const char *target = strstr(fixture->expected_request_line, marker);
        const char *header_end = strstr(chunk_delimiter_mutation_response,
            "\r\n\r\n");
        char *number_end;
        unsigned long body_prefix = target == NULL ? 0ul :
            strtoul(target + sizeof(marker) - 1u, &number_end, 10);
        size_t header_length = header_end == NULL ? 0u :
            (size_t)(header_end - chunk_delimiter_mutation_response) + 4u;
        size_t body_length = sizeof(chunk_delimiter_mutation_response) - 1u -
            header_length;
        if (target == NULL || header_end == NULL ||
            number_end == target + sizeof(marker) - 1u ||
            *number_end != ' ' || (size_t)body_prefix >= body_length) {
            fixture->failed = 1;
        } else {
            memcpy(chunk_delimiter_mutated_response,
                chunk_delimiter_mutation_response,
                sizeof(chunk_delimiter_mutation_response));
            response = chunk_delimiter_mutated_response;
            response_length = header_length + (size_t)body_prefix;
        }
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-truncated-chunk-data ") != NULL) {
        response = truncated_chunk_data_response;
        response_length = sizeof(truncated_chunk_data_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-lf-chunk-size ") != NULL) {
        response = lf_chunk_size_response;
        response_length = sizeof(lf_chunk_size_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-missing-chunk-data-cr ") != NULL) {
        response = missing_chunk_data_cr_response;
        response_length = sizeof(missing_chunk_data_cr_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-doubled-chunk-size-cr ") != NULL) {
        response = doubled_chunk_size_cr_response;
        response_length = sizeof(doubled_chunk_size_cr_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-doubled-chunk-data-cr ") != NULL) {
        response = doubled_chunk_data_cr_response;
        response_length = sizeof(doubled_chunk_data_cr_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-lf-chunk-trailer ") != NULL) {
        response = lf_chunk_trailer_response;
        response_length = sizeof(lf_chunk_trailer_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-malformed-zero-chunk-terminator ") != NULL) {
        response = malformed_zero_chunk_terminator_response;
        response_length = sizeof(malformed_zero_chunk_terminator_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-malformed-trailer ") != NULL) {
        response = malformed_chunk_trailer_response;
        response_length = sizeof(malformed_chunk_trailer_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-malformed-chunk-extension ") != NULL) {
        response = malformed_chunk_extension_response;
        response_length = sizeof(malformed_chunk_extension_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-chunk-extension-control ") != NULL) {
        response = chunk_extension_control_response;
        response_length = sizeof(chunk_extension_control_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-chunk-extension-escaped-control ") != NULL) {
        response = chunk_extension_escaped_control_response;
        response_length = sizeof(chunk_extension_escaped_control_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-chunk-trailer-del ") != NULL) {
        response = chunk_trailer_del_response;
        response_length = sizeof(chunk_trailer_del_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-chunk-trailer-control ") != NULL) {
        response = chunk_trailer_control_response;
        response_length = sizeof(chunk_trailer_control_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-chunk-trailer-obs-fold ") != NULL) {
        response = chunk_trailer_obs_fold_response;
        response_length = sizeof(chunk_trailer_obs_fold_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-empty-chunk-size ") != NULL) {
        response = empty_chunk_size_response;
        response_length = sizeof(empty_chunk_size_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-signed-chunk-size ") != NULL) {
        response = signed_chunk_size_response;
        response_length = sizeof(signed_chunk_size_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-prefixed-chunk-size ") != NULL) {
        response = prefixed_chunk_size_response;
        response_length = sizeof(prefixed_chunk_size_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-non-hex-chunk-size ") != NULL) {
        response = non_hex_chunk_size_response;
        response_length = sizeof(non_hex_chunk_size_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-leading-space-chunk-size ") != NULL) {
        response = leading_space_chunk_size_response;
        response_length = sizeof(leading_space_chunk_size_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-overflowing-chunk-size ") != NULL) {
        response = overflowing_chunk_size_response;
        response_length = sizeof(overflowing_chunk_size_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-malformed-chunk-data-terminator ") != NULL) {
        response = malformed_chunk_data_terminator_response;
        response_length = sizeof(malformed_chunk_data_terminator_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-malformed-chunk-extension-name ") != NULL) {
        response = malformed_chunk_extension_name_response;
        response_length = sizeof(malformed_chunk_extension_name_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-malformed-chunk-extension-value ") != NULL) {
        response = malformed_chunk_extension_value_response;
        response_length = sizeof(malformed_chunk_extension_value_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-malformed-chunk-extension-token ") != NULL) {
        response = malformed_chunk_extension_token_response;
        response_length = sizeof(malformed_chunk_extension_token_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-truncated-chunk-trailer ") != NULL) {
        response = truncated_chunk_trailer_response;
        response_length = sizeof(truncated_chunk_trailer_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-segmented-chunked ") != NULL) {
        response = segmented_chunk_response;
        response_length = sizeof(segmented_chunk_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-chunked ") != NULL) {
        response = valid_chunk_response;
        response_length = sizeof(valid_chunk_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-close-delimited ") != NULL) {
        response = close_delimited_response;
        response_length = sizeof(close_delimited_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-interim ") != NULL) {
        response = interim_response;
        response_length = sizeof(interim_response) - 1u;
    } else if (early_reject_upload) {
        response = early_reject_response;
        response_length = sizeof(early_reject_response) - 1u;
    } else if (early_body_reject_upload) {
        response = early_body_reject_response;
        response_length = sizeof(early_body_reject_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-gzip ") != NULL ||
        strstr(fixture->expected_request_line,
            "/cx-http-gzip-buffer-limit ") != NULL) {
        response = gzip_response;
        response_length = sizeof(gzip_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-gzip-expanded-limit ") != NULL) {
        response = gzip_large_response;
        response_length = sizeof(gzip_large_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-gzip-stacked ") != NULL) {
        response = gzip_stacked_response;
        response_length = sizeof(gzip_stacked_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-brotli ") != NULL) {
        response = brotli_response;
        response_length = sizeof(brotli_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-zstd ") != NULL) {
        response = zstd_response;
        response_length = sizeof(zstd_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-205-chunked-invalid ") != NULL) {
        response = reset_content_chunked_with_body_response;
        response_length = sizeof(reset_content_chunked_with_body_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-205-invalid ") != NULL) {
        response = reset_content_with_body_response;
        response_length = sizeof(reset_content_with_body_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-204-length-invalid ") != NULL) {
        response = no_content_with_length_response;
        response_length = sizeof(no_content_with_length_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-204-transfer-invalid ") != NULL) {
        response = no_content_with_transfer_encoding_response;
        response_length = sizeof(no_content_with_transfer_encoding_response) - 1u;
    } else if (strstr(fixture->expected_request_line, "/cx-http-205 ") != NULL) {
        response = reset_content_response;
        response_length = sizeof(reset_content_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-205-chunked ") != NULL) {
        response = reset_content_chunked_response;
        response_length = sizeof(reset_content_chunked_response) - 1u;
    } else if (strstr(fixture->expected_request_line, "/cx-http-304 ") != NULL) {
        response = not_modified_response;
        response_length = sizeof(not_modified_response) - 1u;
    } else if (strstr(fixture->expected_request_line, "/cx-http-204 ") != NULL ||
        strstr(fixture->expected_request_line, "/cx-http-options ") != NULL) {
        response = no_content_response;
        response_length = sizeof(no_content_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-cookie-area/set ") != NULL) {
        response = cookie_set_response;
        response_length = sizeof(cookie_set_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-cookie-area/delete ") != NULL) {
        response = cookie_delete_response;
        response_length = sizeof(cookie_delete_response) - 1u;
    } else if (strstr(fixture->expected_request_line,
            "/cx-http-cookie-area/expired ") != NULL) {
        response = cookie_expired_response;
        response_length = sizeof(cookie_expired_response) - 1u;
    }
#ifdef _WIN32
    if (client == INVALID_SOCKET) fixture->failed = 1;
#else
    if (client < 0) fixture->failed = 1;
#endif
    if (!fixture->failed && strstr(fixture->expected_request_line,
            "/cx-http-stream-backpressure ") != NULL) {
        int send_buffer_size = 16384;
        (void)setsockopt(client, SOL_SOCKET, SO_SNDBUF,
            (const char *)&send_buffer_size, (int)sizeof(send_buffer_size));
#ifdef _WIN32
        {
            int send_timeout_ms = 15000;
            (void)setsockopt(client, SOL_SOCKET, SO_SNDTIMEO,
                (const char *)&send_timeout_ms, (int)sizeof(send_timeout_ms));
        }
#else
        {
            struct timeval send_timeout = {15, 0};
            (void)setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, &send_timeout,
                (socklen_t)sizeof(send_timeout));
        }
#endif
    }
    while (!fixture->failed && strstr(request, "\r\n\r\n") == NULL) {
#ifdef _WIN32
        int amount = recv(client, request + used, (int)(sizeof(request) - used - 1u), 0);
#else
        ssize_t amount = recv(client, request + used, sizeof(request) - used - 1u, 0);
#endif
        if (amount <= 0 || used + (size_t)amount >= sizeof(request)) {
            fixture->failed = 1;
            break;
        }
        used += (size_t)amount;
        request[used] = '\0';
    }
    if (!fixture->failed && strstr(request, fixture->expected_request_line) == NULL) {
        fixture->failed = 1;
    }
    if (!fixture->failed && strstr(fixture->expected_request_line,
            "/cx-http-default-header ") != NULL &&
        (strstr(request, "X-CX-Default: from-client\r\n") == NULL ||
         strstr(request, "X-CX-Override: request\r\n") == NULL ||
         strstr(request, "X-CX-Override: default\r\n") != NULL ||
         strstr(request, "X-CX-Default: after-send\r\n") != NULL)) {
        fixture->failed = 1;
    }
    if (!fixture->failed &&
        strstr(fixture->expected_request_line, "check HTTP/1.1") != NULL) {
        int checks_past_cookie = strstr(fixture->expected_request_line,
            "past-check HTTP/1.1") != NULL;
        int cookie_found = checks_past_cookie
            ? strstr(request, "past=old") != NULL
            : strstr(request, "Cookie: session=blue\r\n") != NULL;
        if (cookie_found) {
            response = cookie_found_response;
            response_length = sizeof(cookie_found_response) - 1u;
        } else {
            response = cookie_missing_response;
            response_length = sizeof(cookie_missing_response) - 1u;
        }
    }
    {
        char *header_end = strstr(request, "\r\n\r\n");
        char *content_length_header = strstr(request, "Content-Length: ");
        char *transfer_encoding_header = strstr(request,
            "Transfer-Encoding: chunked");
        size_t header_bytes;
        size_t content_length = content_length_header == NULL
            ? 0u : (size_t)strtoul(content_length_header + 16u, NULL, 10);
        if (header_end == NULL) {
            fixture->failed = 1;
        } else {
            header_bytes = (size_t)(header_end - request) + 4u;
            {
                size_t observed_length = header_bytes < sizeof(fixture->observed_headers) - 1u
                    ? header_bytes : sizeof(fixture->observed_headers) - 1u;
                memcpy(fixture->observed_headers, request, observed_length);
                fixture->observed_headers[observed_length] = '\0';
                fixture->request_received = 1;
            }
            if (early_reject_upload) {
                if (strstr(fixture->observed_headers,
                        "Expect: 100-continue\r\n") == NULL) {
                    fixture->failed = 1;
                }
                fixture->early_reject_body_bytes = used - header_bytes;
            } else if (early_body_reject_upload) {
                static const char continue_response[] =
                    "HTTP/1.1 100 Continue\r\n\r\n";
                size_t continue_sent = 0u;
                if (strstr(fixture->observed_headers,
                        "Expect: 100-continue\r\n") == NULL) {
                    fixture->failed = 1;
                }
                while (!fixture->failed &&
                    continue_sent < sizeof(continue_response) - 1u) {
#ifdef _WIN32
                    int amount = send(client, continue_response + continue_sent,
                        (int)(sizeof(continue_response) - 1u - continue_sent), 0);
#else
                    ssize_t amount = send(client, continue_response + continue_sent,
                        sizeof(continue_response) - 1u - continue_sent, 0);
#endif
                    if (amount <= 0) {
                        fixture->failed = 1;
                        break;
                    }
                    continue_sent += (size_t)amount;
                }
                while (!fixture->failed && used == header_bytes) {
#ifdef _WIN32
                    int amount = recv(client, request + used,
                        (int)(sizeof(request) - used - 1u), 0);
#else
                    ssize_t amount = recv(client, request + used,
                        sizeof(request) - used - 1u, 0);
#endif
                    if (amount <= 0 || used + (size_t)amount >= sizeof(request)) {
                        fixture->failed = 1;
                        break;
                    }
                    used += (size_t)amount;
                    request[used] = '\0';
                }
                fixture->early_reject_body_bytes = used - header_bytes;
                if ((strstr(fixture->observed_headers, "Content-Length: ") == NULL &&
                     strstr(fixture->observed_headers,
                        "Transfer-Encoding: chunked\r\n") == NULL) ||
                    fixture->early_reject_body_bytes == 0u) {
                    fixture->failed = 1;
                }
            } else if (transfer_encoding_header != NULL) {
                char *body_start = request + header_bytes;
                while (!fixture->failed && strstr(body_start, "\r\n0\r\n\r\n") == NULL) {
#ifdef _WIN32
                    int amount = recv(client, request + used,
                        (int)(sizeof(request) - used - 1u), 0);
#else
                    ssize_t amount = recv(client, request + used,
                        sizeof(request) - used - 1u, 0);
#endif
                    if (amount <= 0 || used + (size_t)amount >= sizeof(request)) {
                        fixture->failed = 1;
                        break;
                    }
                    used += (size_t)amount;
                    request[used] = '\0';
                }
                fixture->observed_body_bytes = used - header_bytes;
                if (!fixture->failed && fixture->expected_body != NULL) {
                    char decoded_body[140000];
                    char *cursor = body_start;
                    size_t decoded_length = 0u;
                    char *terminator = strstr(body_start, "\r\n0\r\n\r\n");
                    while (!fixture->failed && cursor < terminator) {
                        char *line_end = strstr(cursor, "\r\n");
                        unsigned long chunk_length;
                        if (line_end == NULL || line_end >= terminator) {
                            fixture->failed = 1;
                            break;
                        }
                        *line_end = '\0';
                        chunk_length = strtoul(cursor, NULL, 16);
                        cursor = line_end + 2;
                        if (chunk_length == 0u ||
                            chunk_length > sizeof(decoded_body) - decoded_length ||
                            cursor + chunk_length + 2 > terminator + 5) {
                            fixture->failed = 1;
                            break;
                        }
                        memcpy(decoded_body + decoded_length, cursor,
                            (size_t)chunk_length);
                        decoded_length += (size_t)chunk_length;
                        cursor += chunk_length;
                        if (cursor[0] != '\r' || cursor[1] != '\n') {
                            fixture->failed = 1;
                            break;
                        }
                        cursor += 2;
                    }
                    if (!fixture->failed &&
                        (decoded_length != strlen(fixture->expected_body) ||
                         memcmp(decoded_body, fixture->expected_body,
                            decoded_length) != 0)) fixture->failed = 1;
                }
            } else {
                while (!fixture->failed && used < header_bytes + content_length) {
#ifdef _WIN32
                    int amount = recv(client, request + used,
                        (int)(sizeof(request) - used - 1u), 0);
#else
                    ssize_t amount = recv(client, request + used,
                        sizeof(request) - used - 1u, 0);
#endif
                    if (amount <= 0 || used + (size_t)amount >= sizeof(request)) {
                        fixture->failed = 1;
                        break;
                    }
                    used += (size_t)amount;
                    request[used] = '\0';
                }
                if (!fixture->failed && fixture->expected_body != NULL &&
                    (content_length != strlen(fixture->expected_body) ||
                     memcmp(request + header_bytes, fixture->expected_body,
                        content_length) != 0)) {
                    fixture->failed = 1;
                }
            }
            if (!fixture->failed && fixture->expected_content_type != NULL &&
                strstr(request, fixture->expected_content_type) == NULL) {
                fixture->failed = 1;
            }
            if (!fixture->failed) fixture->request_body_read = 1;
        }
    }
    if (!fixture->failed && fixture->stall_response) {
        if (fixture->stall_response == 2) {
            static const char partial_headers[] =
                "HTTP/1.1 200 OK\r\nContent-Length: 4\r\nX-Incomplete:";
            size_t partial_sent = 0u;
            while (!fixture->failed && partial_sent < sizeof(partial_headers) - 1u) {
#ifdef _WIN32
                int amount = send(client, partial_headers + partial_sent,
                    (int)(sizeof(partial_headers) - 1u - partial_sent), 0);
#else
                ssize_t amount = send(client, partial_headers + partial_sent,
                    sizeof(partial_headers) - 1u - partial_sent, 0);
#endif
                if (amount <= 0) { fixture->failed = 1; break; }
                partial_sent += (size_t)amount;
            }
            if (!fixture->failed) fixture->partial_headers_sent = 1;
        }
        char discard[64];
#ifdef _WIN32
        /* The caller may be forcing GC before it cancels this pending request. */
        DWORD timeout = 10000u;
        (void)setsockopt(client, SOL_SOCKET, SO_RCVTIMEO,
            (const char *)&timeout, (int)sizeof(timeout));
#else
        struct timeval timeout = {10, 0};
        (void)setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &timeout,
            (socklen_t)sizeof(timeout));
#endif
        for (;;) {
#ifdef _WIN32
            int amount = recv(client, discard, (int)sizeof(discard), 0);
#else
            ssize_t amount = recv(client, discard, sizeof(discard), 0);
#endif
            if (amount == 0) {
                fixture->client_closed = 1;
                break;
            }
            if (amount < 0) {
#ifdef _WIN32
                {
                    int socket_error = WSAGetLastError();
                    if (socket_error == WSAECONNRESET ||
                        socket_error == WSAECONNABORTED) {
                        fixture->client_closed = 1;
                        break;
                    }
                }
#else
                if (errno == ECONNRESET || errno == ECONNABORTED) {
                    fixture->client_closed = 1;
                    break;
                }
#endif
                if (fixture->expect_client_close) {
                    fixture->failed = 1;
                    break;
                }
                continue;
            }
        }
#ifdef _WIN32
        if (client != INVALID_SOCKET) closesocket(client);
        return 0;
#else
        if (client >= 0) close(client);
        return NULL;
#endif
    }
    while (!fixture->failed && fixture->hold_response &&
        !fixture->release_response) {
#ifdef _WIN32
        Sleep(1u);
#else
        usleep(1000u);
#endif
    }
    if (!fixture->failed && stream_response) {
        static const char headers[] =
            "HTTP/1.1 200 OK\r\nContent-Length: 4\r\nConnection: close\r\n\r\n";
        static const char close_delimited_headers[] =
            "HTTP/1.1 200 OK\r\nConnection: close\r\n\r\n";
        static const char body[] = "pong";
        char large_headers[128];
        char large_body_chunk[8192];
        const char *response_headers = headers;
        size_t response_headers_length = sizeof(headers) - 1u;
        size_t response_body_length = sizeof(body) - 1u;
        size_t body_offset = 0u;
        if (stream_response == 2) {
            int header_length = snprintf(large_headers, sizeof(large_headers),
                "HTTP/1.1 200 OK\r\nContent-Length: 262144\r\nConnection: close\r\n\r\n");
            if (header_length < 0 || (size_t)header_length >= sizeof(large_headers)) {
                fixture->failed = 1;
            } else {
                response_headers = large_headers;
                response_headers_length = (size_t)header_length;
                response_body_length = 262144u;
            }
        } else if (stream_response == 3) {
            int header_length = snprintf(large_headers, sizeof(large_headers),
                "HTTP/1.1 200 OK\r\nContent-Length: 1048576\r\nConnection: close\r\n\r\n");
            if (header_length < 0 || (size_t)header_length >= sizeof(large_headers)) {
                fixture->failed = 1;
            } else {
                response_headers = large_headers;
                response_headers_length = (size_t)header_length;
                response_body_length = sizeof(body) - 1u;
            }
        } else if (stream_response == 4) {
            int header_length = snprintf(large_headers, sizeof(large_headers),
                "HTTP/1.1 200 OK\r\nContent-Length: 16777216\r\nConnection: close\r\n\r\n");
            if (header_length < 0 || (size_t)header_length >= sizeof(large_headers)) {
                fixture->failed = 1;
            } else {
                response_headers = large_headers;
                response_headers_length = (size_t)header_length;
                response_body_length = 16777216u;
            }
        } else if (stream_response == 5) {
            response_headers = close_delimited_headers;
            response_headers_length = sizeof(close_delimited_headers) - 1u;
        }
        sent = 0u;
        while (!fixture->failed && sent < response_headers_length) {
#ifdef _WIN32
            int amount = send(client, response_headers + sent,
                (int)(response_headers_length - sent), 0);
#else
            ssize_t amount = send(client, response_headers + sent,
                response_headers_length - sent, 0);
#endif
            if (amount <= 0) { fixture->failed = 1; break; }
            sent += (size_t)amount;
        }
        if (stream_response != 4) {
#ifdef _WIN32
            Sleep(300u);
#else
            usleep(300000u);
#endif
        }
        while (!fixture->failed && fixture->hold_stream_body &&
            !fixture->release_stream_body) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        if (stream_response == 1 || stream_response == 3 ||
            stream_response == 5) {
            sent = 0u;
            while (!fixture->failed && sent < response_body_length) {
#ifdef _WIN32
                int amount = send(client, body + sent,
                    (int)(response_body_length - sent), 0);
#else
                ssize_t amount = send(client, body + sent,
                    response_body_length - sent, 0);
#endif
                if (amount <= 0) { fixture->failed = 1; break; }
                sent += (size_t)amount;
            }
        } else {
            while (!fixture->failed && body_offset < response_body_length) {
                size_t amount_to_send = response_body_length - body_offset;
                size_t index;
                if (amount_to_send > sizeof(large_body_chunk)) {
                    amount_to_send = sizeof(large_body_chunk);
                }
                for (index = 0u; index < amount_to_send; index++) {
                    large_body_chunk[index] = (char)((body_offset + index) % 251u);
                }
#ifdef _WIN32
                int amount = send(client, large_body_chunk, (int)amount_to_send, 0);
#else
                ssize_t amount = send(client, large_body_chunk, amount_to_send, 0);
#endif
                if (amount <= 0) { fixture->failed = 1; break; }
                body_offset += (size_t)amount;
                if (stream_response == 4) {
                    fixture->body_bytes_sent = body_offset;
                }
                if (stream_response == 4) continue;
#ifdef _WIN32
                Sleep(1u);
#else
                usleep(1000u);
#endif
            }
        }
        if (!fixture->failed) fixture->body_sent = 1;
        if (!fixture->failed && stream_response == 3) {
            char discard[64];
#ifdef _WIN32
            DWORD timeout = 5000u;
            (void)setsockopt(client, SOL_SOCKET, SO_RCVTIMEO,
                (const char *)&timeout, (int)sizeof(timeout));
            {
                int amount = recv(client, discard, (int)sizeof(discard), 0);
                if (amount != 0) fixture->failed = 1;
                else fixture->client_closed = 1;
            }
#else
            struct timeval timeout = {5, 0};
            (void)setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &timeout,
                (socklen_t)sizeof(timeout));
            {
                ssize_t amount = recv(client, discard, sizeof(discard), 0);
                if (amount != 0) fixture->failed = 1;
                else fixture->client_closed = 1;
            }
#endif
        }
    }
    if (!fixture->failed && strstr(fixture->expected_request_line,
            "/cx-http-segmented-chunked ") != NULL) {
        int no_delay = 1;
        if (setsockopt(client, IPPROTO_TCP, TCP_NODELAY,
                (const char *)&no_delay, (int)sizeof(no_delay)) != 0) {
            fixture->failed = 1;
        }
    }
    while (!fixture->failed && sent < response_length) {
        size_t amount_to_send = response_length - sent;
        int segmented_body = strstr(fixture->expected_request_line,
            "/cx-http-segmented-chunked ") != NULL;
        char *header_end = segmented_body ? strstr((char *)response, "\r\n\r\n") : NULL;
        size_t header_length = header_end == NULL ? response_length :
            (size_t)(header_end - response) + 4u;
        if (segmented_body && sent >= header_length && amount_to_send > 1u) {
            amount_to_send = 1u;
        }
#ifdef _WIN32
        int amount = send(client, response + sent, (int)amount_to_send, 0);
#else
        ssize_t amount = send(client, response + sent, amount_to_send, 0);
#endif
        if (amount <= 0) { fixture->failed = 1; break; }
        sent += (size_t)amount;
        if (segmented_body && sent > header_length) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
    }
    if (!fixture->failed && early_reject_upload) {
        char discard[4096];
#ifdef _WIN32
        DWORD timeout = 250u;
        (void)setsockopt(client, SOL_SOCKET, SO_RCVTIMEO,
            (const char *)&timeout, (int)sizeof(timeout));
        for (;;) {
            int amount = recv(client, discard, (int)sizeof(discard), 0);
            if (amount <= 0) break;
            fixture->early_reject_body_bytes += (size_t)amount;
        }
#else
        struct timeval timeout = {0, 250000};
        (void)setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &timeout,
            (socklen_t)sizeof(timeout));
        for (;;) {
            ssize_t amount = recv(client, discard, sizeof(discard), 0);
            if (amount <= 0) break;
            fixture->early_reject_body_bytes += (size_t)amount;
        }
#endif
        if (fixture->early_reject_body_bytes != 0u) fixture->failed = 1;
    }
    if (!fixture->failed && early_body_reject_upload) {
        char discard[4096];
#ifdef _WIN32
        DWORD timeout = 250u;
        (void)setsockopt(client, SOL_SOCKET, SO_RCVTIMEO,
            (const char *)&timeout, (int)sizeof(timeout));
        for (;;) {
            int amount = recv(client, discard, (int)sizeof(discard), 0);
            if (amount <= 0) {
                if (amount == 0) fixture->client_closed = 1;
                break;
            }
            fixture->early_reject_body_bytes += (size_t)amount;
        }
#else
        struct timeval timeout = {0, 250000};
        (void)setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &timeout,
            (socklen_t)sizeof(timeout));
        for (;;) {
            ssize_t amount = recv(client, discard, sizeof(discard), 0);
            if (amount <= 0) {
                if (amount == 0) fixture->client_closed = 1;
                break;
            }
            fixture->early_reject_body_bytes += (size_t)amount;
        }
#endif
        if (fixture->early_reject_body_bytes >= 1024u * 1024u) {
            fixture->failed = 1;
        }
    }
#ifdef _WIN32
    if (client != INVALID_SOCKET) closesocket(client);
    return 0;
#else
    if (client >= 0) close(client);
    return NULL;
#endif
}

static int start_fixture(struct fixture *fixture, uint16_t *port,
    test_thread *thread, int stall_response, const char *expected_request_line,
    const char *expected_body, const char *expected_content_type);
static void stop_fixture(struct fixture *fixture, test_thread thread);
static void cancel_token_on_scheduler(void *environment);

static int start_reuse_fixture_mode(struct reuse_fixture *fixture, uint16_t *port,
    test_thread *thread, int first_response_mode) {
    struct sockaddr_in address;
    test_socklen length = (test_socklen)sizeof(address);
#ifdef _WIN32
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return 0;
    fixture->listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fixture->listener == INVALID_SOCKET) return 0;
#else
    fixture->listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fixture->listener < 0) return 0;
#endif
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(fixture->listener, (struct sockaddr *)&address,
            (test_socklen)sizeof(address)) != 0 || listen(fixture->listener, 2) != 0 ||
        getsockname(fixture->listener, (struct sockaddr *)&address, &length) != 0) return 0;
    fixture->failed = 0;
    fixture->requests = 0;
    fixture->accepted_connections = 0;
    fixture->first_response_mode = first_response_mode;
    *port = ntohs(address.sin_port);
#ifdef _WIN32
    *thread = CreateThread(NULL, 0, reuse_fixture_main, fixture, 0, NULL);
    return *thread != NULL;
#else
    return pthread_create(thread, NULL, reuse_fixture_main, fixture) == 0;
#endif
}

static int start_reuse_fixture(struct reuse_fixture *fixture, uint16_t *port,
    test_thread *thread) {
    return start_reuse_fixture_mode(fixture, port, thread, 0);
}

static int start_abandoned_body_fixture(struct abandoned_body_fixture *fixture,
    uint16_t *port, test_thread *thread) {
    struct sockaddr_in address;
    test_socklen length = (test_socklen)sizeof(address);
#ifdef _WIN32
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return 0;
    fixture->listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fixture->listener == INVALID_SOCKET) return 0;
#else
    fixture->listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fixture->listener < 0) return 0;
#endif
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(fixture->listener, (struct sockaddr *)&address,
            (test_socklen)sizeof(address)) != 0 || listen(fixture->listener, 2) != 0 ||
        getsockname(fixture->listener, (struct sockaddr *)&address, &length) != 0) return 0;
    fixture->failed = 0;
    fixture->accepted_connections = 0;
    *port = ntohs(address.sin_port);
#ifdef _WIN32
    *thread = CreateThread(NULL, 0, abandoned_body_fixture_main, fixture, 0, NULL);
    return *thread != NULL;
#else
    return pthread_create(thread, NULL, abandoned_body_fixture_main, fixture) == 0;
#endif
}

static void stop_abandoned_body_fixture(struct abandoned_body_fixture *fixture,
    test_thread thread) {
#ifdef _WIN32
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    closesocket(fixture->listener);
    WSACleanup();
#else
    pthread_join(thread, NULL);
    close(fixture->listener);
#endif
}

static int run_abandoned_response_recovery_case(int disposal_order) {
    struct abandoned_body_fixture fixture;
    test_thread thread;
    uint16_t port = 0u;
    char url[160];
    struct cxcore_System_Uri *uri;
    struct cxnet_System_Net_Http_HttpClient *client;
    struct cxnet_System_Net_Http_HttpRequestMessage *request;
    struct cxcore_System_Threading_Tasks_Task *first;
    struct cxcore_System_Threading_Tasks_Task *second;
    struct cxnet_System_Net_Http_HttpResponseMessage *response;
    struct cxnet_System_Net_Http_CurlStreamingContent *content;
    struct cxcore_System_IO_Stream *stream;
    struct cxnet_System_Net_Http_CurlHttpRequestState *cleanup_state = NULL;
    struct cxcore_System_Collections_Array *bytes;
    struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *read_task;
    struct cx_gc_root_handle *uri_root;
    struct cx_gc_root_handle *client_root;
    struct cx_gc_root_handle *request_root;
    struct cx_gc_root_handle *first_root;
    struct cx_gc_root_handle *response_root;
    struct cx_gc_root_handle *content_root;
    struct cx_gc_root_handle *stream_root;
    struct cx_gc_root_handle *bytes_root;
    struct cx_gc_root_handle *read_task_root = NULL;
    struct cx_gc_root_handle * volatile cleanup_state_root = NULL;
    struct cx_gc_root_handle *second_root;
    uint32_t steps = 0u;
    if (!start_abandoned_body_fixture(&fixture, &port, &thread)) return 0;
    (void)snprintf(url, sizeof(url), "http://localhost:%u/cx-http-abandoned",
        (unsigned)port);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri, make_string(url));
    uri_root = cx_gc_add_root(&uri, (cx_uint)sizeof(uri));
    client = (struct cxnet_System_Net_Http_HttpClient *)cxcore_System_Memory_Alloc(
        cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    client_root = cx_gc_add_root(&client, (cx_uint)sizeof(client));
    request = (struct cxnet_System_Net_Http_HttpRequestMessage *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Http_HttpRequestMessage___typeinfo.Size);
    cxnet_System_Net_Http_HttpRequestMessage___constructor(request,
        cxnet_System_Net_Http_HttpMethod_Get___const_get(), uri);
    request_root = cx_gc_add_root(&request, (cx_uint)sizeof(request));
    first = cxnet_System_Net_Http_HttpClient_SendAsync__4(client, request,
        cxnet_System_Net_Http_HttpCompletionOption_ResponseHeadersRead);
    first_root = cx_gc_add_root(&first, (cx_uint)sizeof(first));
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(first) &&
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
    if (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(first) ||
        first->_isFaulted || first->_isCanceled) {
        fputs("abandoned-body response headers did not arrive\n", stderr);
        return 0;
    }
    response = cx_generic_640110dc1a526c1c94f059cac4ed8d246fe089fbe535a78bc1997c7adde4688b(first);
    if (response == NULL) return 0;
    response_root = cx_gc_add_root(&response, (cx_uint)sizeof(response));
    content = (struct cxnet_System_Net_Http_CurlStreamingContent *)
        cxnet_System_Net_Http_HttpResponseMessage_Content___get(response);
    content_root = cx_gc_add_root(&content, (cx_uint)sizeof(content));
    stream = cxnet_System_Net_Http_CurlStreamingContent_ReadAsStream(content);
    stream_root = cx_gc_add_root(&stream, (cx_uint)sizeof(stream));
    bytes = cx_array_with_type(cx_array_new(4u, (cx_uint)sizeof(cx_byte)),
        "cx_generic_9c4b0dcf5365274a639812f507806a7475845f1a6e6b8c085532c41d4210ef4b");
    bytes_root = cx_gc_add_root(&bytes, (cx_uint)sizeof(bytes));
    read_task = cxcore_System_IO_Stream_ReadAsync__2(stream, bytes, 0u, 4u);
    read_task_root = cx_gc_add_root(&read_task, (cx_uint)sizeof(read_task));
    if (read_task == NULL || read_task->_isCompleted) {
        fputs("abandoned response body read was not pending\n", stderr);
        return 0;
    }
    cx_gc_collect();
    if (read_task->_isCompleted) {
        fputs("forced GC completed an abandoned response body read\n", stderr);
        return 0;
    }
    if (disposal_order == 1) {
        cxnet_System_Net_Http_CurlResponseStream_Dispose(
            (struct cxnet_System_Net_Http_CurlResponseStream *)stream);
    }
    if (disposal_order == 3) {
        struct cx_exception_frame frame;
        struct cxcore_System_Exception * volatile exception = NULL;
        struct cxnet_System_Net_Http_CurlResponseStream *response_stream =
            (struct cxnet_System_Net_Http_CurlResponseStream *)stream;
        cleanup_state = response_stream->_state;
        cleanup_state_root = cx_gc_add_root(&cleanup_state,
            (cx_uint)sizeof(cleanup_state));
        cxnet_System_Net_Http_CurlHttpTransport_TestFailNextCancelSend();
        cx_exception_push(&frame);
        if (setjmp(frame.environment) == 0) {
            cxnet_System_Net_Http_CurlResponseStream_Dispose(response_stream);
            cx_exception_pop(&frame);
        } else {
            exception = (struct cxcore_System_Exception *)
                cx_exception_current();
            cx_exception_pop(&frame);
            cx_exception_clear();
        }
        if (exception == NULL || !cxcore_System_Exception_Matches(exception,
                cxcore_System_InvalidOperationException___typeinfo) ||
            cxnet_System_Net_Http_CurlResponseStream_CanRead___get(
                response_stream) || !read_task->_isCompleted ||
            !read_task->_isCanceled) {
            fputs("injected transport cancellation failure interrupted stream cleanup\n",
                stderr);
            return 0;
        }
        cxnet_System_Net_Http_CurlHttpTransport_CancelSend(cleanup_state);
    }
    if (disposal_order == 2) {
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        if (!read_task->_isCompleted || !read_task->_isCanceled) {
            fputs("client disposal did not cancel its pending response-body read\n",
                stderr);
            return 0;
        }
    }
    cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
    cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
    if (!read_task->_isCompleted || !read_task->_isCanceled) {
        fputs("repeated response disposal did not preserve pending-read cancellation\n",
            stderr);
        return 0;
    }
    if (disposal_order != 1) {
        cxnet_System_Net_Http_CurlResponseStream_Dispose(
            (struct cxnet_System_Net_Http_CurlResponseStream *)stream);
    }
    if (disposal_order == 2) {
        client = (struct cxnet_System_Net_Http_HttpClient *)
            cxcore_System_Memory_Alloc(
                cxnet_System_Net_Http_HttpClient___typeinfo.Size);
        cxnet_System_Net_Http_HttpClient___constructor__2(client);
    }
    second = cxnet_System_Net_Http_HttpClient_GetAsync__2(client, uri);
    second_root = cx_gc_add_root(&second, (cx_uint)sizeof(second));
    steps = 0u;
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(second) &&
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
    stop_abandoned_body_fixture(&fixture, thread);
    cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    if (fixture.failed || fixture.accepted_connections != 2 ||
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(second) ||
        second->_isFaulted || second->_isCanceled) {
        fprintf(stderr, "abandoned response recovery failed=%d accepts=%d complete=%d\n",
            fixture.failed, fixture.accepted_connections,
            cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(second));
        fprintf(stderr, "second request faulted=%d canceled=%d\n",
            second->_isFaulted, second->_isCanceled);
        if (second->_exception != NULL) {
            struct cxcore_System_String *message =
                cxcore_System_Exception_Message___const_get(second->_exception);
            if (message != NULL) {
                fprintf(stderr, "second request exception: %.*s\n",
                    (int)message->_length, (char *)message->_data);
            }
        }
        return 0;
    }
    response = cx_generic_640110dc1a526c1c94f059cac4ed8d246fe089fbe535a78bc1997c7adde4688b(second);
    if (response == NULL || !string_equals(
            cxnet_System_Net_Http_HttpContent_ReadAsString(
                cxnet_System_Net_Http_HttpResponseMessage_Content___get(response)), "pong")) {
        fputs("request after abandoned response returned the wrong body\n", stderr);
        return 0;
    }
    cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
    cx_gc_remove_root(second_root);
    cx_gc_remove_root(read_task_root);
    if (cleanup_state_root != NULL) cx_gc_remove_root(
        (struct cx_gc_root_handle *)cleanup_state_root);
    cx_gc_remove_root(bytes_root);
    cx_gc_remove_root(stream_root);
    cx_gc_remove_root(content_root);
    cx_gc_remove_root(response_root);
    cx_gc_remove_root(first_root);
    cx_gc_remove_root(request_root);
    cx_gc_remove_root(client_root);
    cx_gc_remove_root(uri_root);
    return 1;
}

static int run_redirect_cleanup_injected_failure_case(void) {
    struct cxcore_System_Uri *uri =
        (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
            cxcore_System_Uri___typeinfo.Size);
    struct cxnet_System_Net_Http_HttpRequestMessage *request =
        (struct cxnet_System_Net_Http_HttpRequestMessage *)
            cxcore_System_Memory_Alloc(
                cxnet_System_Net_Http_HttpRequestMessage___typeinfo.Size);
    struct cxnet_System_Net_Http_StringContent *content =
        (struct cxnet_System_Net_Http_StringContent *)
            cxcore_System_Memory_Alloc(
                cxnet_System_Net_Http_StringContent___typeinfo.Size);
    struct cx_gc_root_handle *uri_root;
    struct cx_gc_root_handle *request_root;
    struct cx_gc_root_handle *content_root;
    cxcore_System_Uri___constructor(uri,
        make_string("https://redirect.example.test/cleanup"));
    uri_root = cx_gc_add_root(&uri, (cx_uint)sizeof(uri));
    cxnet_System_Net_Http_HttpRequestMessage___constructor(request,
        cxnet_System_Net_Http_HttpMethod_Post___const_get(), uri);
    request_root = cx_gc_add_root(&request, (cx_uint)sizeof(request));
    cxnet_System_Net_Http_StringContent___constructor(content,
        make_string("redirect-body"));
    content_root = cx_gc_add_root(&content, (cx_uint)sizeof(content));
    cxnet_System_Net_Http_HttpRequestMessage_Content___set(request,
        (struct cxnet_System_Net_Http_HttpContent *)content);
    cxnet_System_Net_Http_HttpRequestMessage_BeginSend(request, 1000u);
    cxnet_System_Net_Http_HttpRequestMessage_TestInjectRedirectCleanupFailures();
    cxnet_System_Net_Http_HttpClient_TestCleanupRedirectRequest(request);
    if (cxnet_System_Net_Http_HttpRequestMessage_TestRedirectCleanupFailuresConsumed() !=
            3u || !request->_disposed) {
        fputs("redirect cleanup did not attempt EndSend and request disposal after injected failures\n",
            stderr);
        return 0;
    }
    {
        struct cx_exception_frame frame;
        struct cxcore_System_Exception * volatile exception = NULL;
        cx_exception_push(&frame);
        if (setjmp(frame.environment) == 0) {
            (void)cxnet_System_Net_Http_ByteArrayContent_ReadAsByteArray(
                (struct cxnet_System_Net_Http_ByteArrayContent *)content);
            cx_exception_pop(&frame);
        } else {
            exception = (struct cxcore_System_Exception *)
                cx_exception_current();
            cx_exception_pop(&frame);
            cx_exception_clear();
        }
        if (exception == NULL || !cxcore_System_Exception_Matches(exception,
                cxcore_System_ObjectDisposedException___typeinfo)) {
            fputs("injected request disposal failure skipped redirect-content disposal\n",
                stderr);
            return 0;
        }
    }
    cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
    cx_gc_remove_root(content_root);
    cx_gc_remove_root(request_root);
    cx_gc_remove_root(uri_root);
    return 1;
}

static int run_http_send_cancellation_registration_rollback_case(void) {
    struct cxcore_System_Threading_Tasks_CancellationTokenSource *lifetimeSource =
        (struct cxcore_System_Threading_Tasks_CancellationTokenSource *)
            cxcore_System_Memory_Alloc(
                cxcore_System_Threading_Tasks_CancellationTokenSource___typeinfo.Size);
    struct cxcore_System_Threading_Tasks_CancellationTokenSource *callerSource =
        (struct cxcore_System_Threading_Tasks_CancellationTokenSource *)
            cxcore_System_Memory_Alloc(
                cxcore_System_Threading_Tasks_CancellationTokenSource___typeinfo.Size);
    struct cxnet_System_Net_Http_HttpSendCancellation *sendCancellation =
        (struct cxnet_System_Net_Http_HttpSendCancellation *)
            cxcore_System_Memory_Alloc(
                cxnet_System_Net_Http_HttpSendCancellation___typeinfo.Size);
    struct cx_gc_root_handle *lifetime_root = cx_gc_add_root(
        &lifetimeSource, (cx_uint)sizeof(lifetimeSource));
    struct cx_gc_root_handle *caller_root = cx_gc_add_root(
        &callerSource, (cx_uint)sizeof(callerSource));
    struct cx_gc_root_handle *send_root = cx_gc_add_root(
        &sendCancellation, (cx_uint)sizeof(sendCancellation));
    struct cxcore_System_Threading_Tasks_CancellationToken lifetimeToken;
    struct cxcore_System_Threading_Tasks_CancellationToken callerToken;
    struct cxcore_System_Exception * volatile exception = NULL;

    cxcore_System_Threading_Tasks_CancellationTokenSource___constructor(
        lifetimeSource);
    cxcore_System_Threading_Tasks_CancellationTokenSource___constructor(
        callerSource);
    lifetimeToken =
        cxcore_System_Threading_Tasks_CancellationTokenSource_Token___const_get(
            lifetimeSource);
    callerToken =
        cxcore_System_Threading_Tasks_CancellationTokenSource_Token___const_get(
            callerSource);
    cxcore_System_Threading_Tasks_TaskRuntime_TestFailCancellationRegistrationAfter(
        2u);
    {
        struct cx_exception_frame frame;
        cx_exception_push(&frame);
        if (setjmp(frame.environment) == 0) {
            cxnet_System_Net_Http_HttpSendCancellation___constructor(
                sendCancellation, lifetimeToken, callerToken);
            cx_exception_pop(&frame);
        } else {
            exception = (struct cxcore_System_Exception *)
                cx_exception_current();
            cx_exception_pop(&frame);
            cx_exception_clear();
        }
    }
    if (exception == NULL || !cxcore_System_Exception_Matches(exception,
            cxcore_System_InvalidOperationException___typeinfo) ||
        lifetimeSource->_callbackCount != 0u ||
        callerSource->_callbackCount != 0u) {
        fputs("HttpSendCancellation did not roll back its lifetime registration after caller registration failed\n",
            stderr);
        return 0;
    }
    cx_gc_remove_root(send_root);
    cx_gc_remove_root(caller_root);
    cx_gc_remove_root(lifetime_root);
    return 1;
}

static int run_http_send_completion_registration_failure_case(void) {
    struct cxcore_System_Uri *uri =
        (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
            cxcore_System_Uri___typeinfo.Size);
    struct cxnet_System_Net_Http_HttpRequestMessage *request =
        (struct cxnet_System_Net_Http_HttpRequestMessage *)
            cxcore_System_Memory_Alloc(
                cxnet_System_Net_Http_HttpRequestMessage___typeinfo.Size);
    struct cxnet_System_Net_Http_HttpClient *client =
        (struct cxnet_System_Net_Http_HttpClient *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    struct cx_gc_root_handle *uri_root = cx_gc_add_root(
        &uri, (cx_uint)sizeof(uri));
    struct cx_gc_root_handle *request_root = cx_gc_add_root(
        &request, (cx_uint)sizeof(request));
    struct cx_gc_root_handle *client_root = cx_gc_add_root(
        &client, (cx_uint)sizeof(client));
    struct cxcore_System_Exception * volatile exception = NULL;

    cxcore_System_Uri___constructor(uri,
        make_string("http://127.0.0.1:1/continuation-registration-failure"));
    cxnet_System_Net_Http_HttpRequestMessage___constructor(request,
        cxnet_System_Net_Http_HttpMethod_Get___const_get(), uri);
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    cxcore_System_Threading_Tasks_TaskRuntime_TestFailContinuationRegistrationAfter(
        0u);
    {
        struct cx_exception_frame frame;
        cx_exception_push(&frame);
        if (setjmp(frame.environment) == 0) {
            (void)cxnet_System_Net_Http_HttpClient_SendAsync(client, request);
            cx_exception_pop(&frame);
        } else {
            exception = (struct cxcore_System_Exception *)
                cx_exception_current();
            cx_exception_pop(&frame);
            cx_exception_clear();
        }
    }
    if (exception == NULL || !cxcore_System_Exception_Matches(exception,
            cxcore_System_InvalidOperationException___typeinfo) ||
        request->_sendInProgress || request->_disposed) {
        fprintf(stderr,
            "HttpClient completion-registration failure state: exception=%p invalid_operation=%d in_progress=%d disposed=%d\n",
            (void *)exception,
            exception != NULL && cxcore_System_Exception_Matches(exception,
                cxcore_System_InvalidOperationException___typeinfo),
            request->_sendInProgress, request->_disposed);
        return 0;
    }
    // The failure occurs before the first handler call; mutating the request
    // confirms BeginSend's reservation was released during setup rollback.
    cxnet_System_Net_Http_HttpRequestMessage_Method___set(request,
        cxnet_System_Net_Http_HttpMethod_Head___const_get());
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
    cx_gc_remove_root(client_root);
    cx_gc_remove_root(request_root);
    cx_gc_remove_root(uri_root);
    return 1;
}

static int run_rest_result_continuation_registration_failure_case(void) {
    struct cxcore_System_Uri *uri =
        (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
            cxcore_System_Uri___typeinfo.Size);
    struct cxnet_System_Net_Http_HttpRequestMessage *request =
        (struct cxnet_System_Net_Http_HttpRequestMessage *)
            cxcore_System_Memory_Alloc(
                cxnet_System_Net_Http_HttpRequestMessage___typeinfo.Size);
    struct cxnet_System_Net_Http_StringContent *request_content =
        (struct cxnet_System_Net_Http_StringContent *)
            cxcore_System_Memory_Alloc(
                cxnet_System_Net_Http_StringContent___typeinfo.Size);
    struct cxnet_System_Net_Http_Rest_RestClient *client =
        (struct cxnet_System_Net_Http_Rest_RestClient *)
            cxcore_System_Memory_Alloc(
                cxnet_System_Net_Http_Rest_RestClient___typeinfo.Size);
    struct cxcore_System_Threading_Tasks_Task *task = NULL;
    struct cx_gc_root_handle *uri_root = cx_gc_add_root(
        &uri, (cx_uint)sizeof(uri));
    struct cx_gc_root_handle *request_root = cx_gc_add_root(
        &request, (cx_uint)sizeof(request));
    struct cx_gc_root_handle *request_content_root = cx_gc_add_root(
        &request_content, (cx_uint)sizeof(request_content));
    struct cx_gc_root_handle *client_root = cx_gc_add_root(
        &client, (cx_uint)sizeof(client));
    struct cx_gc_root_handle *task_root = NULL;

    cxcore_System_Uri___constructor(uri,
        make_string("http://127.0.0.1:1/rest-result-continuation-registration-failure"));
    cxnet_System_Net_Http_HttpRequestMessage___constructor(request,
        cxnet_System_Net_Http_HttpMethod_Get___const_get(), uri);
    cxnet_System_Net_Http_StringContent___constructor(request_content,
        make_string("owned REST request content"));
    cxnet_System_Net_Http_HttpRequestMessage_Content___set(request,
        (struct cxnet_System_Net_Http_HttpContent *)request_content);
    cxnet_System_Net_Http_Rest_RestClient___constructor(client);

    // SendWithRedirects first registers request cleanup, then the REST result
    // callback. Fail the second registration to exercise that ownership edge.
    cxcore_System_Threading_Tasks_TaskRuntime_TestFailContinuationRegistrationAfter(
        1u);
    task = cxnet_System_Net_Http_Rest_RestClient_SendAsStringAsync(
        client, request);
    task_root = cx_gc_add_root(&task, (cx_uint)sizeof(task));

    cx_int request_content_disposed =
        cxcore_System_Threading_AtomicInt_Value___const_get(
            &request_content->__base.__base._disposed);

    if (task == NULL || !task->_isCompleted || !task->_isFaulted ||
        !cxcore_System_Exception_Matches(task->_exception,
            cxcore_System_InvalidOperationException___typeinfo) ||
        !request->_disposed || request->_sendInProgress ||
        request_content_disposed == 0) {
        fprintf(stderr,
            "REST result-continuation failure state: task=%p completed=%d faulted=%d invalid_operation=%d request_disposed=%d in_progress=%d content_disposed=%d\n",
            (void *)task, task == NULL ? 0 : task->_isCompleted,
            task == NULL ? 0 : task->_isFaulted,
            task != NULL && cxcore_System_Exception_Matches(task->_exception,
                cxcore_System_InvalidOperationException___typeinfo),
            request->_disposed, request->_sendInProgress,
            (int)(request_content_disposed != 0));
        return 0;
    }

    cxnet_System_Net_Http_Rest_RestClient_Dispose(client);
    cx_gc_remove_root(task_root);
    cx_gc_remove_root(client_root);
    cx_gc_remove_root(request_content_root);
    cx_gc_remove_root(request_root);
    cx_gc_remove_root(uri_root);
    return 1;
}

static int run_http_convenience_continuation_registration_failure_case(
    int operation) {
    struct cxcore_System_Uri *uri =
        (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
            cxcore_System_Uri___typeinfo.Size);
    struct cxnet_System_Net_Http_HttpClient *client =
        (struct cxnet_System_Net_Http_HttpClient *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    struct cxcore_System_Threading_Tasks_Task *task = NULL;
    struct cx_gc_root_handle *uri_root = cx_gc_add_root(
        &uri, (cx_uint)sizeof(uri));
    struct cx_gc_root_handle *client_root = cx_gc_add_root(
        &client, (cx_uint)sizeof(client));
    struct cx_gc_root_handle *task_root = NULL;

    cxcore_System_Uri___constructor(uri,
        make_string("http://127.0.0.1:1/convenience-registration-failure"));
    cxnet_System_Net_Http_HttpClient___constructor__2(client);

    // SendWithRedirects registers request cleanup first and the convenience
    // result callback second. Failure must be caught before transport dispatch
    // and returned as a faulted task.
    cxcore_System_Threading_Tasks_TaskRuntime_TestFailContinuationRegistrationAfter(
        1u);
    if (operation == 0) {
        task = cxnet_System_Net_Http_HttpClient_GetStringAsync__2(client, uri);
    } else if (operation == 1) {
        task = cxnet_System_Net_Http_HttpClient_GetByteArrayAsync__2(client, uri);
    } else {
        task = cxnet_System_Net_Http_HttpClient_GetStreamAsync__2(client, uri);
    }
    task_root = cx_gc_add_root(&task, (cx_uint)sizeof(task));

    if (task == NULL || !task->_isCompleted || !task->_isFaulted ||
        !cxcore_System_Exception_Matches(task->_exception,
            cxcore_System_InvalidOperationException___typeinfo)) {
        fprintf(stderr,
            "HttpClient convenience operation %d did not return a faulted task after result callback registration failed\n",
            operation);
        if (task_root != NULL) cx_gc_remove_root(task_root);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        cx_gc_remove_root(client_root);
        cx_gc_remove_root(uri_root);
        return 0;
    }

    cxnet_System_Net_Http_HttpClient_Dispose(client);
    cx_gc_remove_root(task_root);
    cx_gc_remove_root(client_root);
    cx_gc_remove_root(uri_root);
    return 1;
}

static int run_http_convenience_continuation_registration_failure_cases(void) {
    return run_http_convenience_continuation_registration_failure_case(0) &&
        run_http_convenience_continuation_registration_failure_case(1) &&
        run_http_convenience_continuation_registration_failure_case(2);
}

static int run_http_send_handler_continuation_failure_case(void) {
    struct cxcore_System_Uri *uri =
        (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
            cxcore_System_Uri___typeinfo.Size);
    struct cxnet_System_Net_Http_HttpRequestMessage *request =
        (struct cxnet_System_Net_Http_HttpRequestMessage *)
            cxcore_System_Memory_Alloc(
                cxnet_System_Net_Http_HttpRequestMessage___typeinfo.Size);
    struct cxnet_System_Net_Http_HttpClient *client =
        (struct cxnet_System_Net_Http_HttpClient *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    struct cxcore_System_Threading_Tasks_Task *task = NULL;
    struct cx_gc_root_handle *uri_root = cx_gc_add_root(
        &uri, (cx_uint)sizeof(uri));
    struct cx_gc_root_handle *request_root = cx_gc_add_root(
        &request, (cx_uint)sizeof(request));
    struct cx_gc_root_handle *client_root = cx_gc_add_root(
        &client, (cx_uint)sizeof(client));
    struct cx_gc_root_handle *task_root = NULL;
    uint32_t steps = 0u;

    cxcore_System_Uri___constructor(uri,
        make_string("http://127.0.0.1:1/handler-continuation-registration-failure"));
    cxnet_System_Net_Http_HttpRequestMessage___constructor(request,
        cxnet_System_Net_Http_HttpMethod_Get___const_get(), uri);
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    cxnet_System_Net_Http_HttpClient_TestFailNextHandlerContinuationRegistration();
    task = cxnet_System_Net_Http_HttpClient_SendAsync(client, request);
    task_root = cx_gc_add_root(&task, (cx_uint)sizeof(task));
    while (request->_sendInProgress && steps < 10000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (task == NULL || !task->_isCompleted || !task->_isFaulted ||
        !cxcore_System_Exception_Matches(task->_exception,
            cxcore_System_InvalidOperationException___typeinfo) ||
        request->_sendInProgress || !cxnet_System_Net_Http_HttpClient_TestHandlerContinuationRegistrationFailureConsumed()) {
        fputs("HttpClient did not cancel and clean up after handler continuation registration failed\n",
            stderr);
        return 0;
    }
    cxnet_System_Net_Http_HttpRequestMessage_Method___set(request,
        cxnet_System_Net_Http_HttpMethod_Head___const_get());
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
    cx_gc_remove_root(task_root);
    cx_gc_remove_root(client_root);
    cx_gc_remove_root(request_root);
    cx_gc_remove_root(uri_root);
    return 1;
}

static int run_upload_setup_failure_reservation_rollback_case(void) {
    struct fixture fixture;
    test_thread server_thread;
    uint16_t port = 0u;
    char url[160];
    struct cxcore_System_Uri *active_uri = NULL;
    struct cxnet_System_Net_Http_HttpRequestMessage *active_request = NULL;
    struct cxcore_System_Threading_Tasks_Task *active_task = NULL;
    struct cxnet_System_Net_Http_HttpResponseMessage *active_response = NULL;
    struct cxcore_System_Uri *uri = NULL;
    struct cxnet_System_Net_Http_HttpRequestMessage *request = NULL;
    struct cxnet_System_Net_Http_HttpClient *client = NULL;
    struct cxnet_System_Net_Http_CurlResponseStream *source = NULL;
    struct cxnet_System_Net_Http_StreamContent *content = NULL;
    struct cxcore_System_Threading_Tasks_Task *task = NULL;
    struct cx_gc_root_handle *active_uri_root = NULL;
    struct cx_gc_root_handle *active_request_root = NULL;
    struct cx_gc_root_handle *active_task_root = NULL;
    struct cx_gc_root_handle *active_response_root = NULL;
    struct cx_gc_root_handle *uri_root = NULL;
    struct cx_gc_root_handle *request_root = NULL;
    struct cx_gc_root_handle *client_root = NULL;
    struct cx_gc_root_handle *source_root = NULL;
    struct cx_gc_root_handle *content_root = NULL;
    struct cx_gc_root_handle *task_root = NULL;
    uint32_t steps = 0u;
    int fixture_started = 0;
    int result = 0;

    if (!start_fixture(&fixture, &port, &server_thread, 0,
            "GET /cx-http-upload-setup-failure-active HTTP/1.1", NULL, NULL)) {
        fputs("could not start upload setup rollback overlap fixture\n", stderr);
        return 0;
    }
    fixture_started = 1;
    fixture.hold_response = 1;
    (void)snprintf(url, sizeof(url),
        "http://127.0.0.1:%u/cx-http-upload-setup-failure-active",
        (unsigned)port);
    active_uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    active_uri_root = cx_gc_add_root(&active_uri, (cx_uint)sizeof(active_uri));
    active_request = (struct cxnet_System_Net_Http_HttpRequestMessage *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpRequestMessage___typeinfo.Size);
    active_request_root = cx_gc_add_root(&active_request,
        (cx_uint)sizeof(active_request));
    client = (struct cxnet_System_Net_Http_HttpClient *)cxcore_System_Memory_Alloc(
        cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    client_root = cx_gc_add_root(&client, (cx_uint)sizeof(client));
    if (active_uri == NULL || active_uri_root == NULL || active_request == NULL ||
        active_request_root == NULL || client == NULL || client_root == NULL) goto cleanup;
    cxcore_System_Uri___constructor(active_uri, make_string(url));
    cxnet_System_Net_Http_HttpRequestMessage___constructor(active_request,
        cxnet_System_Net_Http_HttpMethod_Get___const_get(), active_uri);
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    active_task = cxnet_System_Net_Http_HttpClient_SendAsync(client, active_request);
    active_task_root = cx_gc_add_root(&active_task, (cx_uint)sizeof(active_task));
    if (active_task == NULL || active_task_root == NULL) goto cleanup;
    while (!fixture.request_received && steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (fixture.failed || !fixture.request_received ||
        cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(active_task) ||
        !active_request->_sendInProgress) {
        fputs("overlapping HTTP request did not remain pending before upload setup failure\n",
            stderr);
        goto cleanup;
    }

    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    uri_root = cx_gc_add_root(&uri, (cx_uint)sizeof(uri));
    request = (struct cxnet_System_Net_Http_HttpRequestMessage *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpRequestMessage___typeinfo.Size);
    request_root = cx_gc_add_root(&request, (cx_uint)sizeof(request));
    source = (struct cxnet_System_Net_Http_CurlResponseStream *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_CurlResponseStream___typeinfo.Size);
    source_root = cx_gc_add_root(&source, (cx_uint)sizeof(source));
    content = (struct cxnet_System_Net_Http_StreamContent *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_StreamContent___typeinfo.Size);
    content_root = cx_gc_add_root(&content, (cx_uint)sizeof(content));
    if (uri == NULL || uri_root == NULL || request == NULL || request_root == NULL ||
        source == NULL || source_root == NULL || content == NULL ||
        content_root == NULL) goto cleanup;
    cxcore_System_Uri___constructor(uri,
        make_string("http://127.0.0.1:1/upload-setup-failure"));
    cxnet_System_Net_Http_CurlResponseStream___constructor(source, NULL,
        cxcore_System_Threading_Tasks_CancellationToken_None___const_get(), 16u);
    cxnet_System_Net_Http_StreamContent___constructor__2(content,
        (struct cxcore_System_IO_Stream *)source, 16u, 0);
    cxnet_System_Net_Http_HttpRequestMessage___constructor(request,
        cxnet_System_Net_Http_HttpMethod_Post___const_get(), uri);
    cxnet_System_Net_Http_HttpRequestMessage_Content___set(request,
        (struct cxnet_System_Net_Http_HttpContent *)content);
    cxnet_System_Net_Http_CurlHttpTransport_TestFailNextBeginSendSetup();
    task = cxnet_System_Net_Http_HttpClient_SendAsync(client, request);
    task_root = cx_gc_add_root(&task, (cx_uint)sizeof(task));
    steps = 0u;
    while (task != NULL && (!task->_isCompleted || request->_sendInProgress) &&
        steps < 10000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (task == NULL || task_root == NULL || !task->_isCompleted || !task->_isFaulted ||
        !cxcore_System_Exception_Matches(task->_exception,
            cxcore_System_InvalidOperationException___typeinfo) ||
        request->_sendInProgress || content->_transportReserved ||
        !cxnet_System_Net_Http_CurlHttpTransport_TestBeginSendSetupFailureConsumed() ||
        cxnet_System_Net_Http_StreamContent_ReadAsStream(content) !=
            (struct cxcore_System_IO_Stream *)source) {
        fputs("curl transport setup failure did not release the upload source reservation\n",
            stderr);
        goto cleanup;
    }
    if (cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(active_task) ||
        !active_request->_sendInProgress) {
        fputs("upload setup failure disturbed the overlapping request\n", stderr);
        goto cleanup;
    }

    fixture.release_response = 1;
    steps = 0u;
    while ((!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(active_task) ||
            active_request->_sendInProgress) && steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(active_task) ||
        active_task->_isFaulted || active_task->_isCanceled ||
        active_request->_sendInProgress) {
        fputs("overlapping HTTP request did not complete after releasing fixture\n",
            stderr);
        goto cleanup;
    }
    active_response = cx_generic_640110dc1a526c1c94f059cac4ed8d246fe089fbe535a78bc1997c7adde4688b(
        active_task);
    active_response_root = cx_gc_add_root(&active_response,
        (cx_uint)sizeof(active_response));
    if (active_response == NULL || active_response_root == NULL || fixture.failed ||
        cxnet_System_Net_Http_HttpResponseMessage_StatusCode___get(
            active_response) != 200 ||
        !string_equals(cxnet_System_Net_Http_HttpContent_ReadAsString(
            cxnet_System_Net_Http_HttpResponseMessage_Content___get(
                active_response)), "pong")) {
        fputs("overlapping HTTP request returned an invalid response\n", stderr);
        goto cleanup;
    }
    result = 1;

cleanup:
    if (fixture_started) {
        fixture.release_response = 1;
        steps = 0u;
        while (active_task != NULL &&
            (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(active_task) ||
             (active_request != NULL && active_request->_sendInProgress)) &&
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
        stop_fixture(&fixture, server_thread);
    }
    if (active_response != NULL)
        cxnet_System_Net_Http_HttpResponseMessage_Dispose(active_response);
    if (client != NULL) cxnet_System_Net_Http_HttpClient_Dispose(client);
    if (request != NULL) cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
    if (active_request != NULL)
        cxnet_System_Net_Http_HttpRequestMessage_Dispose(active_request);
    if (task_root != NULL) cx_gc_remove_root(task_root);
    if (content_root != NULL) cx_gc_remove_root(content_root);
    if (source_root != NULL) cx_gc_remove_root(source_root);
    if (active_response_root != NULL) cx_gc_remove_root(active_response_root);
    if (active_task_root != NULL) cx_gc_remove_root(active_task_root);
    if (active_request_root != NULL) cx_gc_remove_root(active_request_root);
    if (active_uri_root != NULL) cx_gc_remove_root(active_uri_root);
    if (client_root != NULL) cx_gc_remove_root(client_root);
    if (request_root != NULL) cx_gc_remove_root(request_root);
    if (uri_root != NULL) cx_gc_remove_root(uri_root);
    return result;
}
static int run_http_content_buffer_continuation_registration_failure_case(void) {
    struct cxcore_System_Collections_Array *bytes = cx_array_with_type(
        cx_array_new(1u, (cx_uint)sizeof(cx_byte)),
        "cx_generic_9c4b0dcf5365274a639812f507806a7475845f1a6e6b8c085532c41d4210ef4b");
    struct cxcore_System_IO_MemoryStream *source =
        (struct cxcore_System_IO_MemoryStream *)cxcore_System_Memory_Alloc(
            cxcore_System_IO_MemoryStream___typeinfo.Size);
    struct cxnet_System_Net_Http_StreamContent *content =
        (struct cxnet_System_Net_Http_StreamContent *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_StreamContent___typeinfo.Size);
    struct cxcore_System_Threading_Tasks_Task *task = NULL;
    struct cx_gc_root_handle *bytes_root = cx_gc_add_root(
        &bytes, (cx_uint)sizeof(bytes));
    struct cx_gc_root_handle *source_root = cx_gc_add_root(
        &source, (cx_uint)sizeof(source));
    struct cx_gc_root_handle *content_root = cx_gc_add_root(
        &content, (cx_uint)sizeof(content));
    struct cx_gc_root_handle *task_root = NULL;

    ((cx_byte *)cx_array_at(bytes, 0u, (cx_uint)sizeof(cx_byte)))[0] = 'x';
    cxcore_System_IO_MemoryStream___constructor__3(source, bytes);
    cxnet_System_Net_Http_StreamContent___constructor__2(content,
        (struct cxcore_System_IO_Stream *)source, 16u, 1);
    cxcore_System_Threading_Tasks_TaskRuntime_TestFailContinuationRegistrationAfter(
        0u);
    task = cxnet_System_Net_Http_HttpContent_ReadAsByteArrayAsync(
        (struct cxnet_System_Net_Http_HttpContent *)content);
    task_root = cx_gc_add_root(&task, (cx_uint)sizeof(task));
    if (task == NULL || !task->_isCompleted || !task->_isFaulted ||
        task->_isCanceled || !cxcore_System_Exception_Matches(
            task->_exception,
            cxcore_System_InvalidOperationException___typeinfo)) {
        fputs("content buffering did not fault when read-continuation registration failed\n",
            stderr);
        return 0;
    }
    if (cxnet_System_Net_Http_HttpContent_ReadAsByteArrayAsync(
            (struct cxnet_System_Net_Http_HttpContent *)content) != task) {
        fputs("content buffering did not retain its terminal read failure\n",
            stderr);
        return 0;
    }
    cxnet_System_Net_Http_HttpContent_Dispose(
        (struct cxnet_System_Net_Http_HttpContent *)content);
    cxcore_System_IO_MemoryStream_Dispose(source);
    cx_gc_remove_root(task_root);
    cx_gc_remove_root(content_root);
    cx_gc_remove_root(source_root);
    cx_gc_remove_root(bytes_root);
    return 1;
}

static int run_stream_copy_continuation_registration_failure_case(void) {
    struct cxcore_System_Collections_Array *bytes = cx_array_with_type(
        cx_array_new(1u, (cx_uint)sizeof(cx_byte)),
        "cx_generic_9c4b0dcf5365274a639812f507806a7475845f1a6e6b8c085532c41d4210ef4b");
    struct cxcore_System_IO_MemoryStream *source =
        (struct cxcore_System_IO_MemoryStream *)cxcore_System_Memory_Alloc(
            cxcore_System_IO_MemoryStream___typeinfo.Size);
    struct cxcore_System_IO_MemoryStream *destination =
        (struct cxcore_System_IO_MemoryStream *)cxcore_System_Memory_Alloc(
            cxcore_System_IO_MemoryStream___typeinfo.Size);
    struct cxcore_System_Threading_Tasks_TaskHandle *task = NULL;
    struct cx_gc_root_handle *bytes_root = cx_gc_add_root(
        &bytes, (cx_uint)sizeof(bytes));
    struct cx_gc_root_handle *source_root = cx_gc_add_root(
        &source, (cx_uint)sizeof(source));
    struct cx_gc_root_handle *destination_root = cx_gc_add_root(
        &destination, (cx_uint)sizeof(destination));
    struct cx_gc_root_handle *task_root = NULL;
    cx_bool failed = CX_FALSE;

    ((cx_byte *)cx_array_at(bytes, 0u, (cx_uint)sizeof(cx_byte)))[0] = 'x';
    cxcore_System_IO_MemoryStream___constructor__3(source, bytes);
    cxcore_System_IO_MemoryStream___constructor(destination);
    cxcore_System_Threading_Tasks_TaskRuntime_TestFailContinuationRegistrationAfter(
        0u);
    task = cxcore_System_IO_Stream_CopyToAsync(
        &source->__base, &destination->__base);
    task_root = cx_gc_add_root(&task, (cx_uint)sizeof(task));
    failed = task == NULL ||
        !cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(task) ||
        !cxcore_System_Threading_Tasks_TaskHandle_IsFaulted___const_get(task);
    if (failed) {
        fputs("stream copy did not fault when read-continuation registration failed\n",
            stderr);
        goto cleanup;
    }

    cxcore_System_IO_MemoryStream_Position___set(source, 0L);
    cxcore_System_Threading_Tasks_TaskRuntime_TestFailContinuationRegistrationAfter(
        1u);
    task = cxcore_System_IO_Stream_CopyToAsync(
        &source->__base, &destination->__base);
    if (task_root != NULL) cx_gc_remove_root(task_root);
    task_root = cx_gc_add_root(&task, (cx_uint)sizeof(task));
    cx_uint steps = 0u;
    while (task != NULL &&
        !cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(task) &&
        steps < 10000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) break;
        steps++;
    }
    failed = task == NULL ||
        !cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(task) ||
        !cxcore_System_Threading_Tasks_TaskHandle_IsFaulted___const_get(task) ||
        cxcore_System_IO_MemoryStream_Position___get(source) != 1L;
    if (failed) {
        fputs("stream copy did not stop after write-continuation registration failed\n",
            stderr);
    }

cleanup:
    if (task_root != NULL) cx_gc_remove_root(task_root);
    cxcore_System_IO_MemoryStream_Dispose(source);
    cxcore_System_IO_MemoryStream_Dispose(destination);
    cx_gc_remove_root(destination_root);
    cx_gc_remove_root(source_root);
    cx_gc_remove_root(bytes_root);
    return failed ? 0 : 1;
}

#ifdef _WIN32
static DWORD WINAPI curl_upload_stream_dispose_worker(void *argument) {
#else
static void *curl_upload_stream_dispose_worker(void *argument) {
#endif
    struct curl_upload_stream_dispose_race *race =
        (struct curl_upload_stream_dispose_race *)argument;
    struct cxnet_System_Net_Http_CurlUploadStream *upload = race->upload;
    struct cx_gc_root_handle *upload_root = NULL;
    cx_bool attached = cx_gc_enter_thread();
    if (!attached) goto completed;
    upload_root = cx_gc_add_root(&upload, (cx_uint)sizeof(upload));
    if (upload_root == NULL) {
        request_race_increment(&race->failed);
        goto cleanup;
    }
    request_race_increment(&race->ready);
    while (!request_race_load(&race->start)) {
#ifdef _WIN32
        Sleep(0u);
#else
        usleep(100u);
#endif
    }
    {
        struct cx_exception_frame frame;
        cx_exception_push(&frame);
        if (setjmp(frame.environment) == 0) {
            cxnet_System_Net_Http_CurlUploadStream_CancelTransport(upload);
            cx_exception_pop(&frame);
            request_race_store(&race->disposed, 1);
        } else {
            struct cxcore_System_Exception *exception =
                (struct cxcore_System_Exception *)cx_exception_current();
            const struct cxcore_System_String *message = exception == NULL
                ? NULL : cxcore_System_Exception_Message___const_get(exception);
            cx_exception_pop(&frame);
            if (message != NULL) {
                fprintf(stderr, "curl upload-stream dispose threw: %.*s\n",
                    (int)message->_length, (char *)message->_data);
            }
            cx_exception_clear();
            request_race_increment(&race->failed);
        }
    }
cleanup:
    if (upload_root != NULL) cx_gc_remove_root(upload_root);
    cx_gc_leave_thread(attached);
completed:
    request_race_increment(&race->completed);
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

static int start_curl_upload_stream_dispose_worker(test_thread *thread,
    struct curl_upload_stream_dispose_race *race) {
#ifdef _WIN32
    *thread = CreateThread(NULL, 0, curl_upload_stream_dispose_worker,
        race, 0, NULL);
    return *thread != NULL;
#else
    return pthread_create(thread, NULL, curl_upload_stream_dispose_worker,
        race) == 0;
#endif
}

static int run_curl_upload_stream_dispose_read_race_case(void) {
    const uint32_t source_length = 1024u * 1024u;
    struct cxcore_System_Collections_Array *bytes = cx_array_with_type(
        cx_array_new(source_length, (cx_uint)sizeof(cx_byte)),
        "cx_generic_9c4b0dcf5365274a639812f507806a7475845f1a6e6b8c085532c41d4210ef4b");
    struct cxcore_System_IO_MemoryStream *source =
        (struct cxcore_System_IO_MemoryStream *)cxcore_System_Memory_Alloc(
            cxcore_System_IO_MemoryStream___typeinfo.Size);
    struct cxnet_System_Net_Http_CurlUploadStream *upload =
        (struct cxnet_System_Net_Http_CurlUploadStream *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_CurlUploadStream___typeinfo.Size);
    struct cxcore_System_Threading_Tasks_TaskCompletionSourceVoid *space_source = NULL;
    struct cxcore_System_Threading_Tasks_TaskHandle *space_task = NULL;
    struct cxcore_System_Collections_Array *consumed = NULL;
    struct curl_upload_stream_dispose_race race;
    struct cx_gc_root_handle *bytes_root = cx_gc_add_root(
        &bytes, (cx_uint)sizeof(bytes));
    struct cx_gc_root_handle *source_root = cx_gc_add_root(
        &source, (cx_uint)sizeof(source));
    struct cx_gc_root_handle *upload_root = cx_gc_add_root(
        &upload, (cx_uint)sizeof(upload));
    struct cx_gc_root_handle *space_source_root = NULL;
    struct cx_gc_root_handle *space_task_root = NULL;
    struct cx_gc_root_handle *consumed_root = NULL;
    test_thread worker;
    int worker_started = 0;
    uint32_t steps = 0u;
    int result = 0;
    memset(&race, 0, sizeof(race));
    memset(bytes->_data, 'u', source_length);
    cxcore_System_IO_MemoryStream___constructor__3(source, bytes);
    cxnet_System_Net_Http_CurlUploadStream___constructor(upload,
        (struct cxcore_System_IO_Stream *)source, NULL,
        cxcore_System_Threading_Tasks_CancellationToken_None___const_get());
    cxnet_System_Net_Http_CurlUploadStream_BeginPump(upload);
    while (upload->_spaceAvailableSource == NULL && steps < 300000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (upload->_spaceAvailableSource == NULL) {
        fputs("curl upload producer did not reach its backpressure wait\n", stderr);
        goto cleanup;
    }
    space_source = upload->_spaceAvailableSource;
    space_source_root = cx_gc_add_root(
        &space_source, (cx_uint)sizeof(space_source));
    space_task = cxcore_System_Threading_Tasks_TaskCompletionSourceVoid_Task___const_get(
        space_source);
    space_task_root = cx_gc_add_root(&space_task, (cx_uint)sizeof(space_task));
    if (cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(space_task)) {
        fputs("curl upload backpressure task completed before space was available\n",
            stderr);
        goto cleanup;
    }
    race.upload = upload;
    if (!start_curl_upload_stream_dispose_worker(&worker, &race)) {
        fputs("could not start curl upload-stream dispose worker\n", stderr);
        goto cleanup;
    }
    worker_started = 1;
    while (!request_race_load(&race.ready) &&
        !request_race_load(&race.completed) && steps < 400000u) {
#ifdef _WIN32
        Sleep(1u);
#else
        usleep(1000u);
#endif
        steps++;
    }
    if (!request_race_load(&race.ready)) {
        fputs("curl upload-stream dispose worker did not reach its barrier\n",
            stderr);
        goto cleanup;
    }
    request_race_store(&race.start, 1);
    consumed = cxnet_System_Net_Http_CurlUploadStream_ReadForCurl(upload, 4096u);
    consumed_root = cx_gc_add_root(&consumed, (cx_uint)sizeof(consumed));
    while ((!request_race_load(&race.completed) ||
            !cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(space_task)) &&
        steps < 600000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    join_request_send_race_worker(worker);
    worker_started = 0;
    if (request_race_load(&race.completed) != 1 ||
        request_race_load(&race.failed) != 0 || !race.disposed ||
        !upload->_disposed || upload->_buffer != NULL ||
        !cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(space_task) ||
        (!cxcore_System_Threading_Tasks_TaskHandle_IsCanceled___const_get(space_task) &&
            cxcore_System_Threading_Tasks_TaskHandle_IsFaulted___const_get(space_task)) ||
        consumed == NULL ||
        cxcore_System_Collections_Array_Length___const_get(consumed) > 4096u) {
        fprintf(stderr,
            "curl upload-stream dispose/read race failed: completed=%d failed=%d disposed=%d taskCompleted=%d taskCanceled=%d taskFaulted=%d\n",
            request_race_load(&race.completed), request_race_load(&race.failed),
            race.disposed,
            cxcore_System_Threading_Tasks_TaskHandle_IsCompleted___const_get(space_task),
            cxcore_System_Threading_Tasks_TaskHandle_IsCanceled___const_get(space_task),
            cxcore_System_Threading_Tasks_TaskHandle_IsFaulted___const_get(space_task));
        goto cleanup;
    }
    result = 1;
cleanup:
    request_race_store(&race.start, 1);
    if (worker_started) join_request_send_race_worker(worker);
    if (upload != NULL)
        cxnet_System_Net_Http_CurlUploadStream_CancelTransport(upload);
    if (source != NULL) cxcore_System_IO_MemoryStream_Dispose(source);
    if (consumed_root != NULL) cx_gc_remove_root(consumed_root);
    if (space_task_root != NULL) cx_gc_remove_root(space_task_root);
    if (space_source_root != NULL) cx_gc_remove_root(space_source_root);
    if (upload_root != NULL) cx_gc_remove_root(upload_root);
    if (source_root != NULL) cx_gc_remove_root(source_root);
    if (bytes_root != NULL) cx_gc_remove_root(bytes_root);
    return result;
}

static int run_curl_upload_continuation_registration_failure_case(void) {
    cx_gc_check_thread();
    struct cxcore_System_Collections_Array *bytes = cx_array_with_type(
        cx_array_new(1u, (cx_uint)sizeof(cx_byte)),
        "cx_generic_9c4b0dcf5365274a639812f507806a7475845f1a6e6b8c085532c41d4210ef4b");
    struct cxcore_System_IO_MemoryStream *source =
        (struct cxcore_System_IO_MemoryStream *)cxcore_System_Memory_Alloc(
            cxcore_System_IO_MemoryStream___typeinfo.Size);
    struct cxnet_System_Net_Http_CurlUploadStream *upload =
        (struct cxnet_System_Net_Http_CurlUploadStream *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_CurlUploadStream___typeinfo.Size);
    struct cx_gc_root_handle *bytes_root = cx_gc_add_root(
        &bytes, (cx_uint)sizeof(bytes));
    struct cx_gc_root_handle *source_root = cx_gc_add_root(
        &source, (cx_uint)sizeof(source));
    struct cx_gc_root_handle *upload_root = cx_gc_add_root(
        &upload, (cx_uint)sizeof(upload));

    ((cx_byte *)cx_array_at(bytes, 0u, (cx_uint)sizeof(cx_byte)))[0] = 'x';
    cxcore_System_IO_MemoryStream___constructor__3(source, bytes);
    cxnet_System_Net_Http_CurlUploadStream___constructor(upload,
        (struct cxcore_System_IO_Stream *)source, NULL,
        cxcore_System_Threading_Tasks_CancellationToken_None___const_get());
    cxnet_System_Net_Http_CurlUploadStream_TestFailNextContinuationRegistration();
    cxnet_System_Net_Http_CurlUploadStream_BeginPump(upload);
    if (!cxnet_System_Net_Http_CurlUploadStream_TestContinuationRegistrationFailureConsumed()) {
        fputs("curl upload continuation-registration injection was not consumed\n",
            stderr);
        return 0;
    }
    if (!cxnet_System_Net_Http_CurlUploadStream_HasFailure___const_get(upload)) {
        fputs("curl upload producer did not record continuation-registration failure\n",
            stderr);
        return 0;
    }
    if (!cxcore_System_Exception_Matches(upload->_error,
            cxcore_System_InvalidOperationException___typeinfo)) {
        fputs("curl upload producer did not record continuation-registration failure\n",
            stderr);
        return 0;
    }
    cxnet_System_Net_Http_CurlUploadStream_CancelTransport(upload);
    cxcore_System_IO_MemoryStream_Dispose(source);
    cx_gc_remove_root(upload_root);
    cx_gc_remove_root(source_root);
    cx_gc_remove_root(bytes_root);
    return 1;
}

static int request_content_set_expected_failure(
    struct cxnet_System_Net_Http_HttpRequestMessage *request,
    struct cxnet_System_Net_Http_HttpContent *content,
    struct cxcore_System_Reflection_TypeInfo expectedType) {
    struct cx_exception_frame frame;
    struct cxcore_System_Exception * volatile exception = NULL;
    volatile int result = 0;
    cx_exception_push(&frame);
    if (setjmp(frame.environment) == 0) {
        cxnet_System_Net_Http_HttpRequestMessage_Content___set(request, content);
        cx_exception_pop(&frame);
        result = 0;
    } else {
        exception = (struct cxcore_System_Exception *)cx_exception_current();
        cx_exception_pop(&frame);
        cx_exception_clear();
        result = exception != NULL &&
            cxcore_System_Exception_Matches(exception, expectedType) ? 1 : -1;
    }
    return result;
}

static int run_request_content_replacement_disposal_case(void) {
    struct cxcore_System_Collections_Array *firstBytes = cx_array_with_type(
        cx_array_new(1u, (cx_uint)sizeof(cx_byte)),
        "cx_generic_9c4b0dcf5365274a639812f507806a7475845f1a6e6b8c085532c41d4210ef4b");
    struct cxcore_System_Collections_Array *secondBytes = cx_array_with_type(
        cx_array_new(1u, (cx_uint)sizeof(cx_byte)),
        "cx_generic_9c4b0dcf5365274a639812f507806a7475845f1a6e6b8c085532c41d4210ef4b");
    struct cxcore_System_Uri *uri = (struct cxcore_System_Uri *)
        cxcore_System_Memory_Alloc(cxcore_System_Uri___typeinfo.Size);
    struct cxnet_System_Net_Http_HttpRequestMessage *request =
        (struct cxnet_System_Net_Http_HttpRequestMessage *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpRequestMessage___typeinfo.Size);
    struct cxnet_System_Net_Http_ByteArrayContent *firstContent =
        (struct cxnet_System_Net_Http_ByteArrayContent *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_ByteArrayContent___typeinfo.Size);
    struct cxnet_System_Net_Http_ByteArrayContent *secondContent =
        (struct cxnet_System_Net_Http_ByteArrayContent *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_ByteArrayContent___typeinfo.Size);
    struct cx_gc_root_handle *first_bytes_root = cx_gc_add_root(
        &firstBytes, (cx_uint)sizeof(firstBytes));
    struct cx_gc_root_handle *second_bytes_root = cx_gc_add_root(
        &secondBytes, (cx_uint)sizeof(secondBytes));
    struct cx_gc_root_handle *uri_root = cx_gc_add_root(
        &uri, (cx_uint)sizeof(uri));
    struct cx_gc_root_handle *request_root = cx_gc_add_root(
        &request, (cx_uint)sizeof(request));
    struct cx_gc_root_handle *first_content_root = cx_gc_add_root(
        &firstContent, (cx_uint)sizeof(firstContent));
    struct cx_gc_root_handle *second_content_root = cx_gc_add_root(
        &secondContent, (cx_uint)sizeof(secondContent));
    int result = 0;

    ((cx_byte *)cx_array_at(firstBytes, 0u, (cx_uint)sizeof(cx_byte)))[0] = 'a';
    ((cx_byte *)cx_array_at(secondBytes, 0u, (cx_uint)sizeof(cx_byte)))[0] = 'b';
    cxcore_System_Uri___constructor(uri, make_string("http://localhost/content"));
    cxnet_System_Net_Http_HttpRequestMessage___constructor(request,
        cxnet_System_Net_Http_HttpMethod_Get___const_get(), uri);
    cxnet_System_Net_Http_ByteArrayContent___constructor(firstContent,
        firstBytes);
    cxnet_System_Net_Http_ByteArrayContent___constructor(secondContent,
        secondBytes);

    cxnet_System_Net_Http_HttpRequestMessage_Content___set(request,
        (struct cxnet_System_Net_Http_HttpContent *)firstContent);
    cxnet_System_Net_Http_HttpRequestMessage_BeginSend(request, 1000u);
    if (request_content_set_expected_failure(request,
            (struct cxnet_System_Net_Http_HttpContent *)secondContent,
            cxcore_System_InvalidOperationException___typeinfo) != 1) {
        fputs("request content replacement was not rejected during send\n",
            stderr);
        goto cleanup;
    }
    cxnet_System_Net_Http_HttpRequestMessage_EndSend(request);
    if (cxnet_System_Net_Http_HttpRequestMessage_Content___get(request) !=
            (struct cxnet_System_Net_Http_HttpContent *)firstContent) {
        fputs("rejected request content replacement changed the content\n",
            stderr);
        goto cleanup;
    }
    cxnet_System_Net_Http_HttpRequestMessage_Content___set(request,
        (struct cxnet_System_Net_Http_HttpContent *)secondContent);
    if (cxcore_System_Threading_AtomicInt_Value___const_get(
            &firstContent->__base._disposed) != 0) {
        fputs("replacing request content disposed the previous content\n",
            stderr);
        goto cleanup;
    }

    cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
    if (cxcore_System_Threading_AtomicInt_Value___const_get(
            &secondContent->__base._disposed) == 0 ||
        cxcore_System_Threading_AtomicInt_Value___const_get(
            &firstContent->__base._disposed) != 0) {
        fputs("request disposal did not dispose only its current content\n",
            stderr);
        goto cleanup;
    }
    if (request_content_set_expected_failure(request,
            (struct cxnet_System_Net_Http_HttpContent *)firstContent,
            cxcore_System_ObjectDisposedException___typeinfo) != 1) {
        fputs("disposed request accepted a content replacement\n", stderr);
        goto cleanup;
    }
    cxnet_System_Net_Http_HttpContent_Dispose(
        (struct cxnet_System_Net_Http_HttpContent *)firstContent);
    result = 1;

cleanup:
    if (result == 0) {
        cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
        cxnet_System_Net_Http_HttpContent_Dispose(
            (struct cxnet_System_Net_Http_HttpContent *)firstContent);
        cxnet_System_Net_Http_HttpContent_Dispose(
            (struct cxnet_System_Net_Http_HttpContent *)secondContent);
    }
    cx_gc_remove_root(second_content_root);
    cx_gc_remove_root(first_content_root);
    cx_gc_remove_root(request_root);
    cx_gc_remove_root(uri_root);
    cx_gc_remove_root(second_bytes_root);
    cx_gc_remove_root(first_bytes_root);
    return result;
}

static int run_response_content_replacement_disposal_race_case(void) {
    struct response_content_race race;
    test_thread worker;
    struct cxcore_System_Collections_Array *bytes = NULL;
    struct cxnet_System_Net_Http_HttpResponseMessage *response = NULL;
    struct cxnet_System_Net_Http_ByteArrayContent *original = NULL;
    struct cxnet_System_Net_Http_ByteArrayContent *replacement = NULL;
    struct cx_gc_root_handle *bytes_root = NULL;
    struct cx_gc_root_handle *response_root = NULL;
    struct cx_gc_root_handle *original_root = NULL;
    struct cx_gc_root_handle *replacement_root = NULL;
    struct cx_gc_root_handle *race_root = NULL;
    int worker_started = 0;
    int response_initialized = 0;
    int original_initialized = 0;
    int replacement_initialized = 0;
    int result = 0;
    uint32_t wait_steps = 0u;
    memset(&race, 0, sizeof(race));
    bytes = cx_array_with_type(
        cx_array_new(1u, (cx_uint)sizeof(cx_byte)),
        "cx_generic_9c4b0dcf5365274a639812f507806a7475845f1a6e6b8c085532c41d4210ef4b");
    bytes_root = cx_gc_add_root(&bytes, (cx_uint)sizeof(bytes));
    if (bytes == NULL || bytes_root == NULL) goto cleanup;
    response =
        (struct cxnet_System_Net_Http_HttpResponseMessage *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpResponseMessage___typeinfo.Size);
    response_root = cx_gc_add_root(&response, (cx_uint)sizeof(response));
    if (response == NULL || response_root == NULL) goto cleanup;
    original =
        (struct cxnet_System_Net_Http_ByteArrayContent *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_ByteArrayContent___typeinfo.Size);
    original_root = cx_gc_add_root(&original, (cx_uint)sizeof(original));
    if (original == NULL || original_root == NULL) goto cleanup;
    replacement =
        (struct cxnet_System_Net_Http_ByteArrayContent *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_ByteArrayContent___typeinfo.Size);
    replacement_root = cx_gc_add_root(&replacement,
        (cx_uint)sizeof(replacement));
    race_root = cx_gc_add_root(&race, (cx_uint)sizeof(race));
    if (replacement == NULL || replacement_root == NULL || race_root == NULL) {
        fputs("could not allocate response-content race state\n", stderr);
        goto cleanup;
    }
    cxnet_System_Net_Http_HttpResponseMessage___constructor(response);
    response_initialized = 1;
    cxnet_System_Net_Http_ByteArrayContent___constructor(original, bytes);
    original_initialized = 1;
    cxnet_System_Net_Http_ByteArrayContent___constructor(replacement, bytes);
    replacement_initialized = 1;
    cxnet_System_Net_Http_HttpResponseMessage_Content___set(response,
        (struct cxnet_System_Net_Http_HttpContent *)original);
    cxnet_System_Net_Http_HttpResponseMessage_Content___set(response,
        (struct cxnet_System_Net_Http_HttpContent *)replacement);
    if (cxnet_System_Net_Http_HttpResponseMessage_Content___get(response) !=
            (struct cxnet_System_Net_Http_HttpContent *)replacement ||
        cxcore_System_Threading_AtomicInt_Value___const_get(
            &original->__base._disposed) != 0) {
        fputs("response content assignment disposed detached caller-owned content\n",
            stderr);
        goto cleanup;
    }
    cxnet_System_Net_Http_HttpResponseMessage_Content___set(response,
        (struct cxnet_System_Net_Http_HttpContent *)original);
    race.response = response;
    race.content = (struct cxnet_System_Net_Http_HttpContent *)replacement;
    if (!start_response_content_race_worker(&worker, &race)) {
        fputs("could not start response-content replacement worker\n", stderr);
        goto cleanup;
    }
    worker_started = 1;
    while (!request_race_load(&race.ready) &&
        !request_race_load(&race.completed) && wait_steps < 200000u) {
#ifdef _WIN32
        Sleep(1u);
#else
        usleep(1000u);
#endif
        wait_steps++;
    }
    if (!request_race_load(&race.ready)) {
        fputs("response-content worker did not reach the race barrier\n", stderr);
        goto cleanup;
    }
    request_race_store(&race.start, 1);
    cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
    join_request_send_race_worker(worker);
    worker_started = 0;

    if (request_race_load(&race.failed) != 0 ||
        request_race_load(&race.completed) != 1 ||
        request_race_load(&race.assigned) + request_race_load(&race.rejected) != 1) {
        fputs("response-content setter did not complete with a defined outcome\n",
            stderr);
        goto cleanup;
    }
    if (request_race_load(&race.assigned) == 1) {
        if (cxnet_System_Net_Http_HttpResponseMessage_Content___get(response) !=
                (struct cxnet_System_Net_Http_HttpContent *)replacement ||
            cxcore_System_Threading_AtomicInt_Value___const_get(
                &original->__base._disposed) != 0 ||
            cxcore_System_Threading_AtomicInt_Value___const_get(
                &replacement->__base._disposed) == 0) {
            fputs("response disposal did not dispose the content selected by the setter\n",
                stderr);
            goto cleanup;
        }
    } else if (cxnet_System_Net_Http_HttpResponseMessage_Content___get(response) !=
            (struct cxnet_System_Net_Http_HttpContent *)original ||
        cxcore_System_Threading_AtomicInt_Value___const_get(
            &original->__base._disposed) == 0 ||
        cxcore_System_Threading_AtomicInt_Value___const_get(
            &replacement->__base._disposed) != 0) {
        fputs("disposed response replacement changed or disposed caller-owned content\n",
            stderr);
        goto cleanup;
    }
    if (response_content_set_attempt(response,
            (struct cxnet_System_Net_Http_HttpContent *)replacement) != 0) {
        fputs("disposed response accepted a content replacement\n", stderr);
        goto cleanup;
    }
    result = 1;
cleanup:
    if (worker_started) join_request_send_race_worker(worker);
    if (response_initialized) cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
    if (original_initialized) cxnet_System_Net_Http_HttpContent_Dispose(
        (struct cxnet_System_Net_Http_HttpContent *)original);
    if (replacement_initialized) cxnet_System_Net_Http_HttpContent_Dispose(
        (struct cxnet_System_Net_Http_HttpContent *)replacement);
    if (race_root != NULL) cx_gc_remove_root(race_root);
    if (replacement_root != NULL) cx_gc_remove_root(replacement_root);
    if (original_root != NULL) cx_gc_remove_root(original_root);
    if (response_root != NULL) cx_gc_remove_root(response_root);
    if (bytes_root != NULL) cx_gc_remove_root(bytes_root);
    return result;
}

static void stop_reuse_fixture(struct reuse_fixture *fixture, test_thread thread) {
#ifdef _WIN32
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    closesocket(fixture->listener);
    WSACleanup();
#else
    pthread_join(thread, NULL);
    close(fixture->listener);
#endif
}

static void wake_reuse_fixture(uint16_t port) {
    static const char request[] = "GET /cx-http-reuse HTTP/1.1\r\nHost: localhost\r\n\r\n";
    struct sockaddr_in address;
    test_socket wake_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (wake_socket == (test_socket)-1
#ifdef _WIN32
        || wake_socket == INVALID_SOCKET
#endif
        ) return;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);
    if (connect(wake_socket, (struct sockaddr *)&address,
            (test_socklen)sizeof(address)) == 0) {
#ifdef _WIN32
        (void)send(wake_socket, request, (int)(sizeof(request) - 1u), 0);
        closesocket(wake_socket);
#else
        (void)send(wake_socket, request, sizeof(request) - 1u, 0);
        close(wake_socket);
#endif
    } else {
#ifdef _WIN32
        closesocket(wake_socket);
#else
        close(wake_socket);
#endif
    }
}

static int run_connection_reuse_case(void) {
    struct reuse_fixture fixture;
    test_thread thread;
    uint16_t port = 0u;
    char url[160];
    struct cxcore_System_Uri *uri;
    struct cxnet_System_Net_Http_HttpClient *client;
    struct cxcore_System_Threading_Tasks_Task *first;
    struct cxcore_System_Threading_Tasks_Task *second;
    uint32_t steps = 0u;
    if (!start_reuse_fixture(&fixture, &port, &thread)) return 0;
    (void)snprintf(url, sizeof(url), "http://localhost:%u/cx-http-reuse", (unsigned)port);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri, make_string(url));
    client = (struct cxnet_System_Net_Http_HttpClient *)cxcore_System_Memory_Alloc(
        cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    first = cxnet_System_Net_Http_HttpClient_GetAsync__2(client, uri);
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(first) &&
        steps < 100000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) break;
        steps++;
    }
    if (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(first)) {
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        stop_reuse_fixture(&fixture, thread);
        return 0;
    }
    second = cxnet_System_Net_Http_HttpClient_GetAsync__2(client, uri);
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(second) &&
        steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) break;
        steps++;
    }
    stop_reuse_fixture(&fixture, thread);
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    if (fixture.failed || fixture.requests != 2 ||
        fixture.accepted_connections != 1 ||
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(second)) {
        fprintf(stderr, "connection reuse evidence failed=%d requests=%d connections=%d completed=%d\n",
            fixture.failed, fixture.requests, fixture.accepted_connections,
            cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(second));
        fputs("HttpClient did not reuse its persistent connection\n", stderr);
        return 0;
    }
    return 1;
}

static int run_malformed_response_connection_recovery_case(int framing_mode) {
    struct reuse_fixture fixture;
    test_thread thread;
    uint16_t port = 0u;
    char url[160];
    struct cxcore_System_Uri *uri;
    struct cxnet_System_Net_Http_HttpClient *client;
    struct cxcore_System_Threading_Tasks_Task *first;
    struct cxcore_System_Threading_Tasks_Task *second;
    struct cxnet_System_Net_Http_HttpResponseMessage *response;
    uint32_t steps = 0u;

    if (!start_reuse_fixture_mode(&fixture, &port, &thread, framing_mode))
        return 0;
    (void)snprintf(url, sizeof(url), "http://localhost:%u/cx-http-reuse",
        (unsigned)port);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri, make_string(url));
    client = (struct cxnet_System_Net_Http_HttpClient *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    cxnet_System_Net_Http_HttpClient___constructor__2(client);

    first = cxnet_System_Net_Http_HttpClient_GetAsync__2(client, uri);
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(first) &&
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
    if (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(first) ||
        !first->_isFaulted || first->_isCanceled) {
        fprintf(stderr,
            "malformed response framing mode %d did not fault the first request\n",
            framing_mode);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        wake_reuse_fixture(port);
        stop_reuse_fixture(&fixture, thread);
        return 0;
    }

    second = cxnet_System_Net_Http_HttpClient_GetAsync__2(client, uri);
    steps = 0u;
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(second) &&
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
    stop_reuse_fixture(&fixture, thread);
    if (fixture.failed || fixture.requests != 2 ||
        fixture.accepted_connections != 2 ||
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(second) ||
        second->_isFaulted || second->_isCanceled) {
        fprintf(stderr,
            "malformed response recovery mode=%d fixture=%d requests=%d connections=%d completed=%d faulted=%d canceled=%d\n",
            framing_mode, fixture.failed, fixture.requests,
            fixture.accepted_connections,
            cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(second),
            second->_isFaulted, second->_isCanceled);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        return 0;
    }
    response = cx_generic_640110dc1a526c1c94f059cac4ed8d246fe089fbe535a78bc1997c7adde4688b(
        second);
    if (response == NULL) {
        fputs("recovered request did not produce an HTTP response\n", stderr);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        return 0;
    }
    cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    return 1;
}

static int run_truncated_stream_connection_recovery_case(void) {
    struct reuse_fixture fixture;
    test_thread thread;
    uint16_t port = 0u;
    char url[160];
    struct cxcore_System_Uri *uri;
    struct cxnet_System_Net_Http_HttpClient *client;
    struct cxcore_System_Threading_Tasks_Task *first;
    struct cxcore_System_Threading_Tasks_Task *second;
    struct cxcore_System_IO_Stream *stream;
    struct cxcore_System_Collections_Array *bytes;
    struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *read_task;
    uint32_t steps = 0u;
    if (!start_reuse_fixture_mode(&fixture, &port, &thread, 1)) return 0;
    (void)snprintf(url, sizeof(url), "http://localhost:%u/cx-http-reuse",
        (unsigned)port);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri, make_string(url));
    client = (struct cxnet_System_Net_Http_HttpClient *)cxcore_System_Memory_Alloc(
        cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    first = cxnet_System_Net_Http_HttpClient_GetStreamAsync__2(client, uri);
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(first) &&
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
    if (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(first) ||
        first->_isFaulted || first->_isCanceled) {
        fputs("truncated streaming response did not complete at headers\n", stderr);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        wake_reuse_fixture(port);
        stop_reuse_fixture(&fixture, thread);
        return 0;
    }
    stream = cx_generic_b7a000e5a1e82c2cc28bb7d06efab618c555997e8e29f1d0e05edd0237738424(first);
    bytes = cx_array_with_type(cx_array_new(8u, (cx_uint)sizeof(cx_byte)),
        "cx_generic_9c4b0dcf5365274a639812f507806a7475845f1a6e6b8c085532c41d4210ef4b");
    read_task = cxcore_System_IO_Stream_ReadAsync__2(stream, bytes, 0u, 8u);
    steps = 0u;
    while (!read_task->_isCompleted && steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (!read_task->_isCompleted || read_task->_isFaulted || read_task->_isCanceled ||
        cx_generic_32f22edd4c9ba40c64ed2658909ec8fb1d6e85d2c4276d46eebd01030eefa80f(
            read_task) != 4u ||
        memcmp(cx_array_at(bytes, 0u, (cx_uint)sizeof(cx_byte)), "part", 4u) != 0) {
        fputs("truncated response did not preserve the partial stream bytes\n", stderr);
        cxnet_System_Net_Http_ResponseOwnedStream_Dispose(
            (struct cxnet_System_Net_Http_ResponseOwnedStream *)stream);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        wake_reuse_fixture(port);
        stop_reuse_fixture(&fixture, thread);
        return 0;
    }
    read_task = cxcore_System_IO_Stream_ReadAsync__2(stream, bytes, 0u, 1u);
    steps = 0u;
    while (!read_task->_isCompleted && steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (!read_task->_isCompleted || !read_task->_isFaulted || read_task->_isCanceled) {
        fputs("truncated response did not fault the following stream read\n", stderr);
        cxnet_System_Net_Http_ResponseOwnedStream_Dispose(
            (struct cxnet_System_Net_Http_ResponseOwnedStream *)stream);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        wake_reuse_fixture(port);
        stop_reuse_fixture(&fixture, thread);
        return 0;
    }
    cxnet_System_Net_Http_ResponseOwnedStream_Dispose(
        (struct cxnet_System_Net_Http_ResponseOwnedStream *)stream);
    second = cxnet_System_Net_Http_HttpClient_GetAsync__2(client, uri);
    steps = 0u;
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(second) &&
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
    stop_reuse_fixture(&fixture, thread);
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    if (fixture.failed || fixture.requests != 2 ||
        fixture.accepted_connections != 2 ||
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(second) ||
        second->_isFaulted || second->_isCanceled) {
        fprintf(stderr, "truncated stream recovery failed fixture=%d requests=%d connections=%d completed=%d\n",
            fixture.failed, fixture.requests, fixture.accepted_connections,
            cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(second));
        return 0;
    }
    return 1;
}

static int run_connection_expiry_case(int lifetime) {
    struct reuse_fixture fixture;
    test_thread thread;
    uint16_t port = 0u;
    char url[160];
    struct cxcore_System_Uri *uri;
    struct cxnet_System_Net_Http_HttpClient *client;
    struct cxcore_System_Threading_Tasks_Task *first;
    struct cxcore_System_Threading_Tasks_Task *second;
    uint32_t steps = 0u;
    if (!start_reuse_fixture(&fixture, &port, &thread)) return 0;
    (void)snprintf(url, sizeof(url), "http://localhost:%u/cx-http-reuse", (unsigned)port);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri, make_string(url));
    client = (struct cxnet_System_Net_Http_HttpClient *)cxcore_System_Memory_Alloc(
        cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    if (lifetime) {
        cxnet_System_Net_Http_HttpClient_PooledConnectionLifetimeMilliseconds___set(
            client, 1000u);
    } else {
        cxnet_System_Net_Http_HttpClient_PooledConnectionIdleTimeoutMilliseconds___set(
            client, 1000u);
        cxnet_System_Net_Http_HttpClient_PooledConnectionLifetimeMilliseconds___set(
            client, 0u);
    }
    first = cxnet_System_Net_Http_HttpClient_GetAsync__2(client, uri);
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(first) &&
        steps < 100000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) break;
        steps++;
    }
    if (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(first)) {
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        stop_reuse_fixture(&fixture, thread);
        return 0;
    }
#ifdef _WIN32
    Sleep(1300u);
#else
    usleep(1300000u);
#endif
    second = cxnet_System_Net_Http_HttpClient_GetAsync__2(client, uri);
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(second) &&
        steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) break;
        steps++;
    }
    stop_reuse_fixture(&fixture, thread);
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    if (fixture.failed || fixture.requests != 2 ||
        fixture.accepted_connections != 2 ||
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(second)) {
        fprintf(stderr, "connection lifetime evidence failed=%d requests=%d connections=%d completed=%d\n",
            fixture.failed, fixture.requests, fixture.accepted_connections,
            cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(second));
        fputs("HttpClient reused a connection past its configured expiry setting\n", stderr);
        return 0;
    }
    return 1;
}

static int receive_request_headers(test_socket client) {
    char request[4096];
    size_t used = 0u;
    request[0] = '\0';
    while (strstr(request, "\r\n\r\n") == NULL) {
#ifdef _WIN32
        int amount = recv(client, request + used,
            (int)(sizeof(request) - used - 1u), 0);
#else
        ssize_t amount = recv(client, request + used,
            sizeof(request) - used - 1u, 0);
#endif
        if (amount <= 0 || used + (size_t)amount >= sizeof(request)) {
            fprintf(stderr,
                "limit fixture request read failed amount=%d errno=%d bytes=%zu data=%.*s\n",
                (int)amount, errno, used, (int)used, request);
            return 0;
        }
        used += (size_t)amount;
        request[used] = '\0';
    }
    if (strstr(request, "GET /cx-http-limit HTTP/1.1\r\n") == NULL) {
        fprintf(stderr, "limit fixture request line mismatch: %s\n", request);
        return 0;
    }
    return 1;
}

static int listener_wait_for_connection(test_socket listener,
    uint32_t timeout_milliseconds) {
    fd_set readable;
    struct timeval timeout;
    FD_ZERO(&readable);
    FD_SET(listener, &readable);
    timeout.tv_sec = (long)(timeout_milliseconds / 1000u);
    timeout.tv_usec = (long)((timeout_milliseconds % 1000u) * 1000u);
#ifdef _WIN32
    return select(0, &readable, NULL, NULL, &timeout) > 0;
#else
    return select(listener + 1, &readable, NULL, NULL, &timeout) > 0;
#endif
}

static int listener_has_pending_connection(test_socket listener) {
    return listener_wait_for_connection(listener, 350u);
}

#ifdef _WIN32
static DWORD WINAPI limit_fixture_main(void *context) {
#else
static void *limit_fixture_main(void *context) {
#endif
    struct limit_fixture *fixture = (struct limit_fixture *)context;
    const char response[] = "HTTP/1.1 200 OK\r\nContent-Length: 4\r\nConnection: close\r\n\r\npong";
    test_socket first = accept(fixture->listener, NULL, NULL);
    test_socket second = (test_socket)-1;
#ifdef _WIN32
    if (first == INVALID_SOCKET) fixture->failed = 1;
#else
    if (first < 0) fixture->failed = 1;
#endif
    if (!fixture->failed) {
        fixture->accepted_connections = 1;
        if (!receive_request_headers(first)) fixture->failed = 1;
    }
    if (!fixture->failed && listener_has_pending_connection(fixture->listener)) {
        second = accept(fixture->listener, NULL, NULL);
#ifdef _WIN32
        if (second == INVALID_SOCKET) fixture->failed = 1;
#else
        if (second < 0) fixture->failed = 1;
#endif
        if (!fixture->failed) {
            fixture->accepted_connections++;
            fixture->accepted_before_first_closed = 1;
            if (!receive_request_headers(second)) fixture->failed = 1;
        }
    }
    if (!fixture->failed && fixture->expect_first_closed) {
        char discard[64];
#ifdef _WIN32
        DWORD timeout = 30000u;
        (void)setsockopt(first, SOL_SOCKET, SO_RCVTIMEO,
            (const char *)&timeout, (int)sizeof(timeout));
        {
            int amount = recv(first, discard, (int)sizeof(discard), 0);
            if (amount == 0) fixture->client_closed = 1;
            else fixture->failed = 1;
        }
#else
        struct timeval timeout = {30, 0};
        (void)setsockopt(first, SOL_SOCKET, SO_RCVTIMEO, &timeout,
            (socklen_t)sizeof(timeout));
        {
            ssize_t amount = recv(first, discard, sizeof(discard), 0);
            if (amount == 0) fixture->client_closed = 1;
            else fixture->failed = 1;
        }
#endif
    } else if (!fixture->failed) {
        size_t sent = 0u;
        while (sent < sizeof(response) - 1u) {
#ifdef _WIN32
            int amount = send(first, response + sent,
                (int)(sizeof(response) - sent - 1u), 0);
#else
            ssize_t amount = send(first, response + sent,
                sizeof(response) - sent - 1u, 0);
#endif
            if (amount <= 0) { fixture->failed = 1; break; }
            sent += (size_t)amount;
        }
    }
#ifdef _WIN32
    if (first != INVALID_SOCKET) closesocket(first);
#else
    if (first >= 0) close(first);
#endif
    if (!fixture->failed && fixture->expect_first_closed &&
        listener_has_pending_connection(fixture->listener)) {
        second = accept(fixture->listener, NULL, NULL);
#ifdef _WIN32
        if (second == INVALID_SOCKET) fixture->failed = 1;
#else
        if (second < 0) fixture->failed = 1;
#endif
        if (!fixture->failed) {
            fixture->accepted_connections++;
            fixture->failed = 1;
        }
    }
    if (!fixture->failed && second == (test_socket)-1 && fixture->expect_second) {
        if (!listener_wait_for_connection(fixture->listener, 5000u))
            fixture->failed = 1;
    }
    if (!fixture->failed && second == (test_socket)-1 && fixture->expect_second) {
        second = accept(fixture->listener, NULL, NULL);
#ifdef _WIN32
        if (second == INVALID_SOCKET) fixture->failed = 1;
#else
        if (second < 0) fixture->failed = 1;
#endif
        if (!fixture->failed) {
            fixture->accepted_connections++;
            if (!receive_request_headers(second)) fixture->failed = 1;
        }
    }
    if (!fixture->failed && second != (test_socket)-1) {
        size_t sent = 0u;
        while (sent < sizeof(response) - 1u) {
#ifdef _WIN32
            int amount = send(second, response + sent,
                (int)(sizeof(response) - sent - 1u), 0);
#else
            ssize_t amount = send(second, response + sent,
                sizeof(response) - sent - 1u, 0);
#endif
            if (amount <= 0) { fixture->failed = 1; break; }
            sent += (size_t)amount;
        }
    }
#ifdef _WIN32
    if (second != INVALID_SOCKET && second != (test_socket)-1) closesocket(second);
    return 0;
#else
    if (second >= 0) close(second);
    return NULL;
#endif
}

static int start_limit_fixture(struct limit_fixture *fixture, uint16_t *port,
    test_thread *thread, int expect_second, int expect_first_closed) {
    struct sockaddr_in address;
    test_socklen length = (test_socklen)sizeof(address);
#ifdef _WIN32
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return 0;
    fixture->listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fixture->listener == INVALID_SOCKET) return 0;
#else
    fixture->listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fixture->listener < 0) return 0;
#endif
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(fixture->listener, (struct sockaddr *)&address,
            (test_socklen)sizeof(address)) != 0 || listen(fixture->listener, 2) != 0 ||
        getsockname(fixture->listener, (struct sockaddr *)&address, &length) != 0) return 0;
    fixture->failed = 0;
    fixture->accepted_connections = 0;
    fixture->accepted_before_first_closed = 0;
    fixture->client_closed = 0;
    fixture->expect_first_closed = expect_first_closed;
    fixture->expect_second = expect_second;
    *port = ntohs(address.sin_port);
#ifdef _WIN32
    *thread = CreateThread(NULL, 0, limit_fixture_main, fixture, 0, NULL);
    return *thread != NULL;
#else
    return pthread_create(thread, NULL, limit_fixture_main, fixture) == 0;
#endif
}

static void stop_limit_fixture(struct limit_fixture *fixture, test_thread thread) {
#ifdef _WIN32
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    closesocket(fixture->listener);
    WSACleanup();
#else
    pthread_join(thread, NULL);
    close(fixture->listener);
#endif
}

#ifdef _WIN32
static DWORD WINAPI upload_cancel_fixture_main(void *context) {
#else
static void *upload_cancel_fixture_main(void *context) {
#endif
    struct upload_cancel_fixture *fixture =
        (struct upload_cancel_fixture *)context;
    test_socket client = accept(fixture->listener, NULL, NULL);
    char request[4096];
    size_t used = 0u;
#ifdef _WIN32
    if (client == INVALID_SOCKET) fixture->failed = 1;
#else
    if (client < 0) fixture->failed = 1;
#endif
    if (!fixture->failed) {
#ifdef _WIN32
        DWORD timeout = 1500u;
        (void)setsockopt(client, SOL_SOCKET, SO_RCVTIMEO,
            (const char *)&timeout, (int)sizeof(timeout));
#else
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 500000;
        (void)setsockopt(client, SOL_SOCKET, SO_RCVTIMEO,
            &timeout, sizeof(timeout));
#endif
        request[0] = '\0';
        while (strstr(request, "\r\n\r\n") == NULL) {
#ifdef _WIN32
            int amount = recv(client, request + used,
                (int)(sizeof(request) - used - 1u), 0);
#else
            ssize_t amount = recv(client, request + used,
                sizeof(request) - used - 1u, 0);
#endif
            if (amount <= 0 || used + (size_t)amount >= sizeof(request)) {
                fixture->failed = 1;
                break;
            }
            used += (size_t)amount;
            request[used] = '\0';
        }
        if (!fixture->failed &&
            strstr(request, "POST /cx-http-upload-cancel HTTP/1.1\r\n") == NULL) {
            fixture->failed = 1;
        }
        if (!fixture->failed) {
            if (fixture->allow_empty_cancel &&
                strstr(request, "Expect: 100-continue\r\n") == NULL) {
                fixture->failed = 1;
            }
            fixture->headers_received = 1;
        }
    }
    if (!fixture->failed) {
        char body[4096];
        char *header_end = strstr(request, "\r\n\r\n");
        if (header_end != NULL) {
            size_t header_bytes = (size_t)(header_end + 4 - request);
            fixture->body_bytes = used > header_bytes ? used - header_bytes : 0u;
            if (fixture->body_bytes != 0u) fixture->body_started = 1;
        }
        if (!fixture->body_started) {
            int amount;
#ifdef _WIN32
            amount = recv(client, body, (int)sizeof(body), 0);
#else
            amount = (int)recv(client, body, sizeof(body), 0);
#endif
            if (amount > 0) {
                fixture->body_bytes += (size_t)amount;
                fixture->body_started = 1;
            } else if (amount == 0 && fixture->allow_empty_cancel) {
                fixture->client_closed = 1;
            } else {
                fixture->failed = 1;
            }
        }
        while (fixture->body_started && !fixture->allow_body_drain &&
            !fixture->failed) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        for (;;) {
#ifdef _WIN32
            int amount = recv(client, body, (int)sizeof(body), 0);
#else
            ssize_t amount = recv(client, body, sizeof(body), 0);
#endif
            if (amount == 0) {
                fixture->client_closed = 1;
                break;
            }
            if (amount < 0) break;
            fixture->body_bytes += (size_t)amount;
        }
    }
#ifdef _WIN32
    if (client != INVALID_SOCKET) closesocket(client);
    return 0;
#else
    if (client >= 0) close(client);
    return NULL;
#endif
}

static int start_upload_cancel_fixture(struct upload_cancel_fixture *fixture,
    uint16_t *port, test_thread *thread, int allow_empty_cancel) {
    struct sockaddr_in address;
    test_socklen length = (test_socklen)sizeof(address);
#ifdef _WIN32
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return 0;
    fixture->listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fixture->listener == INVALID_SOCKET) return 0;
#else
    fixture->listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fixture->listener < 0) return 0;
#endif
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(fixture->listener, (struct sockaddr *)&address,
            (test_socklen)sizeof(address)) != 0 || listen(fixture->listener, 1) != 0 ||
        getsockname(fixture->listener, (struct sockaddr *)&address, &length) != 0) return 0;
    fixture->failed = 0;
    fixture->client_closed = 0;
    fixture->headers_received = 0;
    fixture->body_started = 0;
    fixture->allow_body_drain = 0;
    fixture->allow_empty_cancel = allow_empty_cancel;
    fixture->body_bytes = 0u;
    *port = ntohs(address.sin_port);
#ifdef _WIN32
    *thread = CreateThread(NULL, 0, upload_cancel_fixture_main, fixture, 0, NULL);
    return *thread != NULL;
#else
    return pthread_create(thread, NULL, upload_cancel_fixture_main, fixture) == 0;
#endif
}

static void stop_upload_cancel_fixture(
    struct upload_cancel_fixture *fixture, test_thread thread) {
#ifdef _WIN32
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    closesocket(fixture->listener);
    WSACleanup();
#else
    pthread_join(thread, NULL);
    close(fixture->listener);
#endif
}

struct upload_cancel_race_action {
    struct cxcore_System_Threading_Tasks_CancellationTokenSource *source;
    struct cxnet_System_Net_Http_HttpClient *client;
    int dispose_order;
    volatile int fired;
};

static void upload_cancel_and_dispose_on_scheduler(void *environment) {
    struct upload_cancel_race_action *race =
        (struct upload_cancel_race_action *)environment;
    if (race->dispose_order == 2)
        cxnet_System_Net_Http_HttpClient_Dispose(race->client);
    cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(race->source);
    if (race->dispose_order == 1)
        cxnet_System_Net_Http_HttpClient_Dispose(race->client);
    race->fired = 1;
}

static void upload_cancel_token_on_scheduler(void *environment) {
    struct upload_cancel_race_action *race =
        (struct upload_cancel_race_action *)environment;
    cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(race->source);
    race->fired = 1;
}

static void upload_dispose_client_on_scheduler(void *environment) {
    struct upload_cancel_race_action *race =
        (struct upload_cancel_race_action *)environment;
    cxnet_System_Net_Http_HttpClient_Dispose(race->client);
    race->fired = 1;
}

static int run_streaming_upload_cancellation_case(int dispose_order,
    uint32_t cancel_delay_ms, int cancel_after_headers) {
    const uint32_t upload_length = 8388608u;
    struct upload_cancel_fixture fixture;
    test_thread thread;
    uint16_t port = 0u;
    char url[160];
    struct cxcore_System_Uri *uri;
    struct cxcore_System_Collections_Array *bytes;
    struct cxcore_System_IO_MemoryStream *source;
    struct cxnet_System_Net_Http_StreamContent *content;
    struct cxnet_System_Net_Http_HttpClient *client;
    struct cxnet_System_Net_Http_HttpRequestMessage *request;
    struct cxcore_System_Threading_Tasks_CancellationTokenSource *cancellationSource;
    struct cxcore_System_Threading_Tasks_CancellationToken cancellationToken;
    struct cxcore_System_Threading_Tasks_Task *task;
    uint32_t steps = 0u;
    if (!start_upload_cancel_fixture(&fixture, &port, &thread,
            cancel_after_headers)) return 0;
    (void)snprintf(url, sizeof(url), "http://localhost:%u/cx-http-upload-cancel",
        (unsigned)port);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri, make_string(url));
    bytes = cx_array_with_type(cx_array_new(upload_length,
        (cx_uint)sizeof(cx_byte)),
        "cx_generic_9c4b0dcf5365274a639812f507806a7475845f1a6e6b8c085532c41d4210ef4b");
    memset(bytes->_data, 'u', upload_length);
    source = (struct cxcore_System_IO_MemoryStream *)cxcore_System_Memory_Alloc(
        cxcore_System_IO_MemoryStream___typeinfo.Size);
    cxcore_System_IO_MemoryStream___constructor__3(source, bytes);
    content = (struct cxnet_System_Net_Http_StreamContent *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Http_StreamContent___typeinfo.Size);
    cxnet_System_Net_Http_StreamContent___constructor__2(content,
        (struct cxcore_System_IO_Stream *)source, 1024u, 1);
    request = (struct cxnet_System_Net_Http_HttpRequestMessage *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Http_HttpRequestMessage___typeinfo.Size);
    cxnet_System_Net_Http_HttpRequestMessage___constructor(request,
        cxnet_System_Net_Http_HttpMethod_Post___const_get(), uri);
    cxnet_System_Net_Http_HttpRequestMessage_Content___set(request,
        (struct cxnet_System_Net_Http_HttpContent *)content);
    client = (struct cxnet_System_Net_Http_HttpClient *)cxcore_System_Memory_Alloc(
        cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    if (cancel_after_headers &&
        !cxnet_System_Net_Http_HttpHeaders_TryAddWithoutValidation(
            cxnet_System_Net_Http_HttpRequestMessage_Headers___const_get(request),
            make_string("Expect"), make_string("100-continue"))) {
        fixture.allow_body_drain = 1;
        stop_upload_cancel_fixture(&fixture, thread);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        fputs("could not add Expect header for pre-body cancellation case\n",
            stderr);
        return 0;
    }
    cancellationSource = (struct cxcore_System_Threading_Tasks_CancellationTokenSource *)
        cxcore_System_Memory_Alloc(
            cxcore_System_Threading_Tasks_CancellationTokenSource___typeinfo.Size);
    cxcore_System_Threading_Tasks_CancellationTokenSource___constructor(cancellationSource);
    cancellationToken = cxcore_System_Threading_Tasks_CancellationTokenSource_Token___const_get(
        cancellationSource);
    task = cxnet_System_Net_Http_HttpClient_SendAsync__2(client, request,
        cancellationToken);
    while ((cancel_after_headers ? !fixture.headers_received :
            !fixture.body_started) &&
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        steps < 300000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (cancel_after_headers ? !fixture.headers_received : !fixture.body_started) {
        fixture.allow_body_drain = 1;
        stop_upload_cancel_fixture(&fixture, thread);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        fputs(cancel_after_headers
            ? "upload cancellation began before request headers were observed\n"
            : "streaming upload cancellation began before body transfer was observed\n",
            stderr);
        return 0;
    }
    if (cancel_delay_ms == 0xffffffffu) {
        if (dispose_order == 2)
            cxnet_System_Net_Http_HttpClient_Dispose(client);
        cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(
            cancellationSource);
        if (dispose_order == 1)
            cxnet_System_Net_Http_HttpClient_Dispose(client);
    } else {
        struct upload_cancel_race_action *race =
            (struct upload_cancel_race_action *)cxcore_System_Memory_Alloc(
                (cx_uint)sizeof(struct upload_cancel_race_action));
        cx_action race_action = {0};
        if (race == NULL) {
            fixture.allow_body_drain = 1;
            stop_upload_cancel_fixture(&fixture, thread);
            cxnet_System_Net_Http_HttpClient_Dispose(client);
            fputs("could not allocate scheduled upload cancellation context\n", stderr);
            return 0;
        }
        cx_gc_store_reference((cx_ptr *)&race->source, (cx_ptr)cancellationSource);
        cx_gc_store_reference((cx_ptr *)&race->client, (cx_ptr)client);
        race->dispose_order = dispose_order;
        race->fired = 0;
        race_action.__function = upload_cancel_and_dispose_on_scheduler;
        race_action.__environment = race;
        race_action.__target = (cx_ptr)race;
        cxcore_System_Threading_Tasks_TaskScheduler_PostAfter(
            cancel_delay_ms, race_action);
        while (!race->fired && steps < 600000u) {
            if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                Sleep(1u);
#else
                usleep(1000u);
#endif
            }
            steps++;
        }
        if (!race->fired) {
            fixture.allow_body_drain = 1;
            stop_upload_cancel_fixture(&fixture, thread);
            cxnet_System_Net_Http_HttpClient_Dispose(client);
            fputs("scheduled upload cancellation/disposal did not run\n", stderr);
            return 0;
        }
    }
    fixture.allow_body_drain = 1;
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        steps < 500000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    stop_upload_cancel_fixture(&fixture, thread);
    if (dispose_order == 0) {
        cxnet_System_Net_Http_HttpClient_Dispose(client);
    }
    if (fixture.failed || !fixture.client_closed ||
        (cancel_after_headers ? fixture.body_bytes >= upload_length :
            (fixture.body_bytes == 0u || fixture.body_bytes >= upload_length)) ||
        !cxnet_System_Net_Http_CurlHttpTransport_WasCanceled(task) ||
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task)) {
        fprintf(stderr, "streaming upload cancellation evidence order=%d afterHeaders=%d failed=%d closed=%d bodyBytes=%zu completed=%d canceled=%d\n",
            dispose_order, cancel_after_headers, fixture.failed,
            fixture.client_closed, fixture.body_bytes,
            cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task),
            cxnet_System_Net_Http_CurlHttpTransport_WasCanceled(task));
        fputs("streaming upload cancellation did not close its transport cleanly\n", stderr);
        return 0;
    }
    return 1;
}

static int run_streaming_upload_header_cancel_cases(void) {
    return run_streaming_upload_cancellation_case(0, 0xffffffffu, 1) &&
        run_streaming_upload_cancellation_case(1, 0xffffffffu, 1) &&
        run_streaming_upload_cancellation_case(2, 0xffffffffu, 1);
}

static int run_streaming_upload_timeout_case(void) {
    const uint32_t upload_length = 8388608u;
    struct upload_cancel_fixture fixture;
    test_thread thread;
    uint16_t port = 0u;
    char url[160];
    struct cxcore_System_Uri *uri;
    struct cxcore_System_Collections_Array *bytes;
    struct cxcore_System_IO_MemoryStream *source;
    struct cxnet_System_Net_Http_StreamContent *content;
    struct cxnet_System_Net_Http_HttpClient *client;
    struct cxnet_System_Net_Http_HttpRequestMessage *request;
    struct cxcore_System_Threading_Tasks_Task *task;
    uint32_t steps = 0u;

    if (!start_upload_cancel_fixture(&fixture, &port, &thread, 0)) return 0;
    (void)snprintf(url, sizeof(url), "http://localhost:%u/cx-http-upload-cancel",
        (unsigned)port);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri, make_string(url));
    bytes = cx_array_with_type(cx_array_new(upload_length,
        (cx_uint)sizeof(cx_byte)),
        "cx_generic_9c4b0dcf5365274a639812f507806a7475845f1a6e6b8c085532c41d4210ef4b");
    memset(bytes->_data, 'u', upload_length);
    source = (struct cxcore_System_IO_MemoryStream *)cxcore_System_Memory_Alloc(
        cxcore_System_IO_MemoryStream___typeinfo.Size);
    cxcore_System_IO_MemoryStream___constructor__3(source, bytes);
    content = (struct cxnet_System_Net_Http_StreamContent *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Http_StreamContent___typeinfo.Size);
    cxnet_System_Net_Http_StreamContent___constructor__2(content,
        (struct cxcore_System_IO_Stream *)source, 1024u, 1);
    request = (struct cxnet_System_Net_Http_HttpRequestMessage *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpRequestMessage___typeinfo.Size);
    cxnet_System_Net_Http_HttpRequestMessage___constructor(request,
        cxnet_System_Net_Http_HttpMethod_Post___const_get(), uri);
    cxnet_System_Net_Http_HttpRequestMessage_Content___set(request,
        (struct cxnet_System_Net_Http_HttpContent *)content);
    client = (struct cxnet_System_Net_Http_HttpClient *)cxcore_System_Memory_Alloc(
        cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    cxnet_System_Net_Http_HttpClient_TimeoutMilliseconds___set(client, 250u);
    task = cxnet_System_Net_Http_HttpClient_SendAsync(client, request);

    while (!fixture.body_started &&
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        steps < 300000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (!fixture.body_started) {
        fixture.allow_body_drain = 1;
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
        stop_upload_cancel_fixture(&fixture, thread);
        fputs("streaming upload timeout expired before body transfer was observed\n",
            stderr);
        return 0;
    }

    /* The request, upload producer, and timeout must survive collection. */
    cx_gc_collect();
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        steps < 600000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    fixture.allow_body_drain = 1;
    while (!fixture.client_closed && steps < 700000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    stop_upload_cancel_fixture(&fixture, thread);
    cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    if (fixture.failed || !fixture.client_closed || fixture.body_bytes == 0u ||
        fixture.body_bytes >= upload_length ||
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) ||
        !cxnet_System_Net_Http_CurlHttpTransport_WasTimedOut(task)) {
        fprintf(stderr,
            "streaming upload timeout evidence failed=%d closed=%d bodyBytes=%zu completed=%d timedOut=%d\n",
            fixture.failed, fixture.client_closed, fixture.body_bytes,
            cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task),
            cxnet_System_Net_Http_CurlHttpTransport_WasTimedOut(task));
        fputs("streaming upload timeout did not close its transport cleanly\n",
            stderr);
        return 0;
    }
    return 1;
}

static int run_upload_timeout_cancel_race_case(uint32_t cancel_delay_ms,
    int expect_timeout, int dispose_client) {
    const uint32_t upload_length = 8388608u;
    struct upload_cancel_fixture fixture;
    struct upload_cancel_race_action *race;
    cx_action cancel_action = {0};
    test_thread thread;
    uint16_t port = 0u;
    char url[160];
    struct cxcore_System_Uri *uri;
    struct cxcore_System_Collections_Array *bytes;
    struct cxcore_System_IO_MemoryStream *source;
    struct cxnet_System_Net_Http_StreamContent *content;
    struct cxnet_System_Net_Http_HttpClient *client;
    struct cxnet_System_Net_Http_HttpRequestMessage *request;
    struct cxcore_System_Threading_Tasks_CancellationTokenSource *cancellationSource;
    struct cxcore_System_Threading_Tasks_Task *task;
    uint32_t steps = 0u;

    if (!start_upload_cancel_fixture(&fixture, &port, &thread, 0)) return 0;
    (void)snprintf(url, sizeof(url), "http://localhost:%u/cx-http-upload-cancel",
        (unsigned)port);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri, make_string(url));
    bytes = cx_array_with_type(cx_array_new(upload_length,
        (cx_uint)sizeof(cx_byte)),
        "cx_generic_9c4b0dcf5365274a639812f507806a7475845f1a6e6b8c085532c41d4210ef4b");
    memset(bytes->_data, 'u', upload_length);
    source = (struct cxcore_System_IO_MemoryStream *)cxcore_System_Memory_Alloc(
        cxcore_System_IO_MemoryStream___typeinfo.Size);
    cxcore_System_IO_MemoryStream___constructor__3(source, bytes);
    content = (struct cxnet_System_Net_Http_StreamContent *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Http_StreamContent___typeinfo.Size);
    cxnet_System_Net_Http_StreamContent___constructor__2(content,
        (struct cxcore_System_IO_Stream *)source, 1024u, 1);
    request = (struct cxnet_System_Net_Http_HttpRequestMessage *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpRequestMessage___typeinfo.Size);
    cxnet_System_Net_Http_HttpRequestMessage___constructor(request,
        cxnet_System_Net_Http_HttpMethod_Post___const_get(), uri);
    cxnet_System_Net_Http_HttpRequestMessage_Content___set(request,
        (struct cxnet_System_Net_Http_HttpContent *)content);
    client = (struct cxnet_System_Net_Http_HttpClient *)cxcore_System_Memory_Alloc(
        cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    cxnet_System_Net_Http_HttpClient_TimeoutMilliseconds___set(client, 1000u);
    cancellationSource = (struct cxcore_System_Threading_Tasks_CancellationTokenSource *)
        cxcore_System_Memory_Alloc(
            cxcore_System_Threading_Tasks_CancellationTokenSource___typeinfo.Size);
    cxcore_System_Threading_Tasks_CancellationTokenSource___constructor(
        cancellationSource);
    task = cxnet_System_Net_Http_HttpClient_SendAsync__2(client, request,
        cxcore_System_Threading_Tasks_CancellationTokenSource_Token___const_get(
            cancellationSource));
    while (!fixture.body_started &&
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        steps < 300000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (!fixture.body_started) {
        fixture.allow_body_drain = 1;
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
        stop_upload_cancel_fixture(&fixture, thread);
        fputs("upload cancellation/timeout race started without body transfer\n",
            stderr);
        return 0;
    }
    race = (struct upload_cancel_race_action *)cxcore_System_Memory_Alloc(
        (cx_uint)sizeof(struct upload_cancel_race_action));
    if (race == NULL) {
        fixture.allow_body_drain = 1;
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        stop_upload_cancel_fixture(&fixture, thread);
        cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
        fputs("could not allocate upload cancellation race context\n", stderr);
        return 0;
    }
    cx_gc_store_reference((cx_ptr *)&race->source,
        (cx_ptr)cancellationSource);
    race->client = NULL;
    race->dispose_order = 0;
    race->fired = 0;
    if (dispose_client) {
        cx_gc_store_reference((cx_ptr *)&race->client, (cx_ptr)client);
        cancel_action.__function = upload_dispose_client_on_scheduler;
    } else {
        cancel_action.__function = upload_cancel_token_on_scheduler;
    }
    cancel_action.__environment = race;
    cancel_action.__target = (cx_ptr)race;
    cxcore_System_Threading_Tasks_TaskScheduler_PostAfter(cancel_delay_ms,
        cancel_action);
    cx_gc_collect();
    while ((!race->fired ||
            !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task)) &&
        steps < 1600000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    fixture.allow_body_drain = 1;
    while (!fixture.client_closed && steps < 1800000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (!fixture.client_closed) {
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        while (!fixture.client_closed && steps < 1900000u) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
            steps++;
        }
    }
    int was_canceled = cxnet_System_Net_Http_CurlHttpTransport_WasCanceled(task);
    int was_timed_out = cxnet_System_Net_Http_CurlHttpTransport_WasTimedOut(task);
    int passed = race->fired && fixture.client_closed && !fixture.failed &&
        fixture.body_bytes > 0u && fixture.body_bytes < upload_length &&
        cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        was_canceled != was_timed_out &&
        was_timed_out == expect_timeout;
    stop_upload_cancel_fixture(&fixture, thread);
    cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    if (!passed) {
        fprintf(stderr,
            "upload %s race evidence delay=%u expectedTimeout=%d fired=%d closed=%d failed=%d bodyBytes=%zu completed=%d canceled=%d timedOut=%d\n",
            dispose_client ? "disposal/timeout" : "cancellation/timeout",
            cancel_delay_ms, expect_timeout, race->fired, fixture.client_closed,
            fixture.failed, fixture.body_bytes,
            cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task),
            was_canceled, was_timed_out);
        return 0;
    }
    return 1;
}

static int run_upload_timeout_cancel_race_cases(void) {
    return run_upload_timeout_cancel_race_case(200u, 0, 0) &&
        run_upload_timeout_cancel_race_case(1100u, 1, 0);
}

static int run_upload_timeout_dispose_race_cases(void) {
    return run_upload_timeout_cancel_race_case(200u, 0, 1) &&
        run_upload_timeout_cancel_race_case(1100u, 1, 1);
}

static int run_upload_response_wait_request_disposal_case(void) {
    const uint32_t upload_length = 65536u;
    struct fixture fixture;
    test_thread thread;
    uint16_t port = 0u;
    char url[160];
    char *expected = (char *)malloc((size_t)upload_length + 1u);
    struct cxcore_System_Uri *uri;
    struct cxcore_System_Collections_Array *bytes;
    struct cxcore_System_IO_MemoryStream *source;
    struct cxnet_System_Net_Http_StreamContent *content;
    struct cxnet_System_Net_Http_HttpRequestMessage *request;
    struct cxnet_System_Net_Http_HttpClient *client;
    struct cxcore_System_Threading_Tasks_CancellationTokenSource *cancellationSource;
    struct cxcore_System_Threading_Tasks_CancellationToken cancellationToken;
    struct cxcore_System_Threading_Tasks_Task *task;
    uint32_t steps = 0u;
    int result = 0;
    if (expected == NULL) return 0;
    memset(expected, 'u', upload_length);
    expected[upload_length] = '\0';
    if (!start_fixture(&fixture, &port, &thread, 1,
            "POST /cx-http-upload-response-wait HTTP/1.1", expected,
            "Content-Type: application/octet-stream")) {
        free(expected);
        return 0;
    }
    (void)snprintf(url, sizeof(url),
        "http://localhost:%u/cx-http-upload-response-wait", (unsigned)port);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri, make_string(url));
    bytes = cx_array_with_type(cx_array_new(upload_length,
        (cx_uint)sizeof(cx_byte)),
        "cx_generic_9c4b0dcf5365274a639812f507806a7475845f1a6e6b8c085532c41d4210ef4b");
    memset(bytes->_data, 'u', upload_length);
    source = (struct cxcore_System_IO_MemoryStream *)cxcore_System_Memory_Alloc(
        cxcore_System_IO_MemoryStream___typeinfo.Size);
    cxcore_System_IO_MemoryStream___constructor__3(source, bytes);
    content = (struct cxnet_System_Net_Http_StreamContent *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Http_StreamContent___typeinfo.Size);
    cxnet_System_Net_Http_StreamContent___constructor__2(content,
        (struct cxcore_System_IO_Stream *)source, 1024u, 1);
    cxnet_System_Net_Http_HttpHeaders_Add(
        cxnet_System_Net_Http_HttpContent_Headers___const_get(
            (struct cxnet_System_Net_Http_HttpContent *)content),
        make_string("Content-Type"), make_string("application/octet-stream"));
    request = (struct cxnet_System_Net_Http_HttpRequestMessage *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpRequestMessage___typeinfo.Size);
    cxnet_System_Net_Http_HttpRequestMessage___constructor(request,
        cxnet_System_Net_Http_HttpMethod_Post___const_get(), uri);
    cxnet_System_Net_Http_HttpRequestMessage_Content___set(request,
        (struct cxnet_System_Net_Http_HttpContent *)content);
    client = (struct cxnet_System_Net_Http_HttpClient *)cxcore_System_Memory_Alloc(
        cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    cancellationSource =
        (struct cxcore_System_Threading_Tasks_CancellationTokenSource *)
            cxcore_System_Memory_Alloc(
                cxcore_System_Threading_Tasks_CancellationTokenSource___typeinfo.Size);
    cxcore_System_Threading_Tasks_CancellationTokenSource___constructor(
        cancellationSource);
    cancellationToken =
        cxcore_System_Threading_Tasks_CancellationTokenSource_Token___const_get(
            cancellationSource);
    task = cxnet_System_Net_Http_HttpClient_SendAsync__2(client, request,
        cancellationToken);
    while (!fixture.request_body_read &&
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        steps < 300000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (!fixture.request_body_read ||
        cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task)) {
        fputs("upload did not reach the pending-response cancellation state\n",
            stderr);
        goto cleanup;
    }
    cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
    if (!request->_disposed ||
        cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task)) {
        fputs("disposing the request changed a send awaiting its response\n",
            stderr);
        goto cleanup;
    }
    cx_gc_collect();
    if (cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task)) {
        fputs("upload response completed during pending-work collection\n",
            stderr);
        goto cleanup;
    }
    cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(
        cancellationSource);
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(
        cancellationSource);
    while ((!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) ||
            !fixture.client_closed) && steps < 700000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (!fixture.request_body_read || fixture.failed || !fixture.client_closed ||
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) ||
        !cxnet_System_Net_Http_CurlHttpTransport_WasCanceled(task)) {
        fprintf(stderr, "upload response-wait cancellation evidence failed=%d bodyRead=%d closed=%d completed=%d canceled=%d\n",
            fixture.failed, fixture.request_body_read, fixture.client_closed,
            cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task),
            cxnet_System_Net_Http_CurlHttpTransport_WasCanceled(task));
        goto cleanup;
    }
    result = 1;
cleanup:
    cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(
        cancellationSource);
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    stop_fixture(&fixture, thread);
    cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
    free(expected);
    return result;
}

static int run_connection_limit_case(void) {
    struct limit_fixture fixture;
    test_thread thread;
    uint16_t port = 0u;
    char url[160];
    struct cxcore_System_Uri *uri;
    struct cxnet_System_Net_Http_HttpClient *client;
    struct cxcore_System_Threading_Tasks_Task *first;
    struct cxcore_System_Threading_Tasks_Task *second;
    uint32_t steps = 0u;
    if (!start_limit_fixture(&fixture, &port, &thread, 1, 0)) return 0;
    (void)snprintf(url, sizeof(url), "http://127.0.0.1:%u/cx-http-limit", (unsigned)port);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri, make_string(url));
    client = (struct cxnet_System_Net_Http_HttpClient *)cxcore_System_Memory_Alloc(
        cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    cxnet_System_Net_Http_HttpClient_MaxConnectionsPerServer___set(client, 1u);
    first = cxnet_System_Net_Http_HttpClient_GetAsync__2(client, uri);
    second = cxnet_System_Net_Http_HttpClient_GetAsync__2(client, uri);
    while ((!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(first) ||
            !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(second)) &&
        steps < 300000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) break;
        steps++;
    }
    stop_limit_fixture(&fixture, thread);
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    if (fixture.failed || fixture.accepted_connections != 2 ||
        fixture.accepted_before_first_closed ||
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(first) ||
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(second)) {
        fprintf(stderr, "connection limit evidence failed=%d accepted=%d early=%d first=%d second=%d\n",
            fixture.failed, fixture.accepted_connections,
            fixture.accepted_before_first_closed,
            cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(first),
            cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(second));
        fputs("HttpClient exceeded its per-origin connection limit or failed to finish requests\n", stderr);
        return 0;
    }
    return 1;
}

static int run_queued_timeout_case(void) {
    struct limit_fixture fixture;
    test_thread thread;
    uint16_t port = 0u;
    char url[160];
    struct cxcore_System_Uri *uri;
    struct cxnet_System_Net_Http_HttpClient *client;
    struct cxcore_System_Threading_Tasks_Task *first;
    struct cxcore_System_Threading_Tasks_Task *second;
    uint32_t steps = 0u;
    if (!start_limit_fixture(&fixture, &port, &thread, 0, 0)) return 0;
    (void)snprintf(url, sizeof(url), "http://127.0.0.1:%u/cx-http-limit", (unsigned)port);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri, make_string(url));
    client = (struct cxnet_System_Net_Http_HttpClient *)cxcore_System_Memory_Alloc(
        cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    cxnet_System_Net_Http_HttpClient_MaxConnectionsPerServer___set(client, 1u);
    first = cxnet_System_Net_Http_HttpClient_GetAsync__2(client, uri);
    cxnet_System_Net_Http_HttpClient_TimeoutMilliseconds___set(client, 100u);
    second = cxnet_System_Net_Http_HttpClient_GetAsync__2(client, uri);
    while ((!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(first) ||
            !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(second)) &&
        steps < 300000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) break;
        steps++;
    }
    stop_limit_fixture(&fixture, thread);
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    if (fixture.failed ||
        fixture.accepted_before_first_closed ||
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(first) ||
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(second) ||
        !cxnet_System_Net_Http_CurlHttpTransport_WasTimedOut(second)) {
        fprintf(stderr, "queued timeout evidence failed=%d accepted=%d early=%d first=%d firstTimedOut=%d second=%d timedOut=%d\n",
            fixture.failed, fixture.accepted_connections,
            fixture.accepted_before_first_closed,
            cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(first),
            cxnet_System_Net_Http_CurlHttpTransport_WasTimedOut(first),
            cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(second),
            cxnet_System_Net_Http_CurlHttpTransport_WasTimedOut(second));
        fputs("HttpClient did not apply the deadline while waiting for a connection slot\n", stderr);
        return 0;
    }
    return 1;
}

static int run_queued_cancellation_case(void) {
    struct limit_fixture fixture;
    test_thread thread;
    uint16_t port = 0u;
    char url[160];
    struct cxcore_System_Uri *uri;
    struct cxnet_System_Net_Http_HttpClient *client;
    struct cxcore_System_Threading_Tasks_CancellationTokenSource *cancellationSource;
    struct cxcore_System_Threading_Tasks_CancellationToken cancellationToken;
    struct cxcore_System_Threading_Tasks_Task *first;
    struct cxcore_System_Threading_Tasks_Task *second;
    cx_action cancelAction = {0};
    uint32_t steps = 0u;
    if (!start_limit_fixture(&fixture, &port, &thread, 0, 0)) return 0;
    (void)snprintf(url, sizeof(url), "http://127.0.0.1:%u/cx-http-limit", (unsigned)port);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri, make_string(url));
    client = (struct cxnet_System_Net_Http_HttpClient *)cxcore_System_Memory_Alloc(
        cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    cxnet_System_Net_Http_HttpClient_MaxConnectionsPerServer___set(client, 1u);
    first = cxnet_System_Net_Http_HttpClient_GetAsync__2(client, uri);
    cancellationSource = (struct cxcore_System_Threading_Tasks_CancellationTokenSource *)
        cxcore_System_Memory_Alloc(
            cxcore_System_Threading_Tasks_CancellationTokenSource___typeinfo.Size);
    cxcore_System_Threading_Tasks_CancellationTokenSource___constructor(cancellationSource);
    cancellationToken = cxcore_System_Threading_Tasks_CancellationTokenSource_Token___const_get(
        cancellationSource);
    cancelAction.__function = cancel_token_on_scheduler;
    cancelAction.__environment = cancellationSource;
    cancelAction.__target = (cx_ptr)cancellationSource;
    cxcore_System_Threading_Tasks_TaskScheduler_PostAfter(100u, cancelAction);
    second = cxnet_System_Net_Http_HttpClient_GetAsync(client, uri, cancellationToken);
    while ((!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(first) ||
            !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(second)) &&
        steps < 300000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) break;
        steps++;
    }
    stop_limit_fixture(&fixture, thread);
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    if (fixture.failed || fixture.accepted_connections != 1 ||
        fixture.accepted_before_first_closed ||
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(first) ||
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(second) ||
        !cxnet_System_Net_Http_CurlHttpTransport_WasCanceled(second)) {
        fprintf(stderr, "queued cancellation evidence failed=%d accepted=%d early=%d first=%d second=%d canceled=%d\n",
            fixture.failed, fixture.accepted_connections,
            fixture.accepted_before_first_closed,
            cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(first),
            cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(second),
            cxnet_System_Net_Http_CurlHttpTransport_WasCanceled(second));
        fputs("HttpClient did not cancel a request waiting for a connection slot\n", stderr);
        return 0;
    }
    return 1;
}

static int run_queued_timeout_cancel_race_case(uint32_t cancel_delay_ms,
    int expect_timeout) {
    struct limit_fixture fixture;
    test_thread thread;
    uint16_t port = 0u;
    char url[160];
    struct cxcore_System_Uri *uri;
    struct cxnet_System_Net_Http_HttpClient *client;
    struct cxcore_System_Threading_Tasks_CancellationTokenSource *source;
    struct cxcore_System_Threading_Tasks_CancellationToken token;
    struct cxcore_System_Threading_Tasks_Task *first;
    struct cxcore_System_Threading_Tasks_Task *second;
    struct cxcore_System_Threading_Tasks_Task *recovery;
    struct cxnet_System_Net_Http_HttpResponseMessage *response;
    cx_action cancel_action = {0};
    uint32_t steps = 0u;
    int was_canceled;
    int was_timed_out;
    if (!start_limit_fixture(&fixture, &port, &thread, 1, 0)) return 0;
    (void)snprintf(url, sizeof(url), "http://127.0.0.1:%u/cx-http-limit",
        (unsigned)port);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri, make_string(url));
    client = (struct cxnet_System_Net_Http_HttpClient *)cxcore_System_Memory_Alloc(
        cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    cxnet_System_Net_Http_HttpClient_MaxConnectionsPerServer___set(client, 1u);
    first = cxnet_System_Net_Http_HttpClient_GetAsync__2(client, uri);
    source = (struct cxcore_System_Threading_Tasks_CancellationTokenSource *)
        cxcore_System_Memory_Alloc(
            cxcore_System_Threading_Tasks_CancellationTokenSource___typeinfo.Size);
    cxcore_System_Threading_Tasks_CancellationTokenSource___constructor(source);
    token = cxcore_System_Threading_Tasks_CancellationTokenSource_Token___const_get(
        source);
    cxnet_System_Net_Http_HttpClient_TimeoutMilliseconds___set(client, 100u);
    second = cxnet_System_Net_Http_HttpClient_GetAsync(client, uri, token);
    cancel_action.__function = cancel_token_on_scheduler;
    cancel_action.__environment = source;
    cancel_action.__target = (cx_ptr)source;
    cxcore_System_Threading_Tasks_TaskScheduler_PostAfter(cancel_delay_ms,
        cancel_action);
    while ((!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(first) ||
            !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(second)) &&
        steps < 500000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    was_canceled = cxnet_System_Net_Http_CurlHttpTransport_WasCanceled(second);
    was_timed_out = cxnet_System_Net_Http_CurlHttpTransport_WasTimedOut(second);
    recovery = cxnet_System_Net_Http_HttpClient_GetAsync__2(client, uri);
    steps = 0u;
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(recovery) &&
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
    stop_limit_fixture(&fixture, thread);
    if (fixture.failed || fixture.accepted_before_first_closed ||
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(first) ||
        cxcore_System_Threading_Tasks_Task_IsCanceled___const_get(first) ||
        first->_isFaulted ||
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(second) ||
        was_canceled == was_timed_out || was_timed_out != expect_timeout ||
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(recovery) ||
        recovery->_isFaulted || recovery->_isCanceled ||
        fixture.accepted_connections != 2) {
        fprintf(stderr,
            "queued timeout/cancellation recovery failed delay=%u expectedTimeout=%d failed=%d accepted=%d early=%d first=%d second=%d canceled=%d timedOut=%d recovery=%d recoveryFaulted=%d recoveryCanceled=%d\n",
            cancel_delay_ms, expect_timeout, fixture.failed,
            fixture.accepted_connections, fixture.accepted_before_first_closed,
            cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(first),
            cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(second),
            was_canceled, was_timed_out,
            cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(recovery),
            recovery->_isFaulted, recovery->_isCanceled);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        return 0;
    }
    response = cx_generic_640110dc1a526c1c94f059cac4ed8d246fe089fbe535a78bc1997c7adde4688b(
        recovery);
    if (response == NULL) {
        fputs("queued request completion did not release the connection slot\n",
            stderr);
        cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        return 0;
    }
    cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    return 1;
}

static int run_queued_timeout_cancel_race_cases(void) {
    return run_queued_timeout_cancel_race_case(50u, 0) &&
        run_queued_timeout_cancel_race_case(150u, 1);
}

static int run_concurrent_streaming_upload_cancellation_case(void) {
    const uint32_t canceled_upload_length = 8388608u;
    const uint32_t completed_upload_length = 65536u;
    struct upload_cancel_fixture canceled_fixture;
    struct fixture completed_fixture;
    test_thread canceled_thread;
    test_thread completed_thread;
    uint16_t canceled_port = 0u;
    uint16_t completed_port = 0u;
    char canceled_url[160];
    char completed_url[160];
    char *expected_body = (char *)malloc((size_t)completed_upload_length + 1u);
    struct cxcore_System_Uri *canceled_uri;
    struct cxcore_System_Uri *completed_uri;
    struct cxcore_System_Collections_Array *canceled_bytes;
    struct cxcore_System_Collections_Array *completed_bytes;
    struct cxcore_System_IO_MemoryStream *canceled_source;
    struct cxcore_System_IO_MemoryStream *completed_source;
    struct cxnet_System_Net_Http_StreamContent *canceled_content;
    struct cxnet_System_Net_Http_StreamContent *completed_content;
    struct cxnet_System_Net_Http_HttpRequestMessage *canceled_request;
    struct cxnet_System_Net_Http_HttpRequestMessage *completed_request;
    struct cxnet_System_Net_Http_HttpClient *client;
    struct cxcore_System_Threading_Tasks_CancellationTokenSource *source;
    struct cxcore_System_Threading_Tasks_CancellationToken token;
    struct cxcore_System_Threading_Tasks_Task *canceled_task;
    struct cxcore_System_Threading_Tasks_Task *completed_task;
    struct cxnet_System_Net_Http_HttpResponseMessage *response;
    uint32_t steps = 0u;
    int result = 0;
    if (expected_body == NULL) return 0;
    memset(expected_body, 'b', completed_upload_length);
    expected_body[completed_upload_length] = '\0';
    if (!start_upload_cancel_fixture(&canceled_fixture, &canceled_port,
            &canceled_thread, 0)) {
        free(expected_body);
        return 0;
    }
    if (!start_fixture(&completed_fixture, &completed_port, &completed_thread,
            0, "POST /cx-http-upload-peer HTTP/1.1", expected_body,
            "Content-Type: application/octet-stream")) {
        canceled_fixture.allow_body_drain = 1;
        return 0;
    }
    completed_fixture.hold_response = 1;
    (void)snprintf(canceled_url, sizeof(canceled_url),
        "http://localhost:%u/cx-http-upload-cancel", (unsigned)canceled_port);
    (void)snprintf(completed_url, sizeof(completed_url),
        "http://localhost:%u/cx-http-upload-peer", (unsigned)completed_port);
    canceled_uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(canceled_uri, make_string(canceled_url));
    completed_uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(completed_uri, make_string(completed_url));
    canceled_bytes = cx_array_with_type(cx_array_new(canceled_upload_length,
        (cx_uint)sizeof(cx_byte)),
        "cx_generic_9c4b0dcf5365274a639812f507806a7475845f1a6e6b8c085532c41d4210ef4b");
    memset(canceled_bytes->_data, 'u', canceled_upload_length);
    completed_bytes = cx_array_with_type(cx_array_new(completed_upload_length,
        (cx_uint)sizeof(cx_byte)),
        "cx_generic_9c4b0dcf5365274a639812f507806a7475845f1a6e6b8c085532c41d4210ef4b");
    memset(completed_bytes->_data, 'b', completed_upload_length);
    canceled_source = (struct cxcore_System_IO_MemoryStream *)cxcore_System_Memory_Alloc(
        cxcore_System_IO_MemoryStream___typeinfo.Size);
    cxcore_System_IO_MemoryStream___constructor__3(canceled_source, canceled_bytes);
    completed_source = (struct cxcore_System_IO_MemoryStream *)cxcore_System_Memory_Alloc(
        cxcore_System_IO_MemoryStream___typeinfo.Size);
    cxcore_System_IO_MemoryStream___constructor__3(completed_source, completed_bytes);
    canceled_content = (struct cxnet_System_Net_Http_StreamContent *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Http_StreamContent___typeinfo.Size);
    cxnet_System_Net_Http_StreamContent___constructor__2(canceled_content,
        (struct cxcore_System_IO_Stream *)canceled_source, 1024u, 1);
    completed_content = (struct cxnet_System_Net_Http_StreamContent *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Http_StreamContent___typeinfo.Size);
    cxnet_System_Net_Http_StreamContent___constructor__2(completed_content,
        (struct cxcore_System_IO_Stream *)completed_source, 1024u, 1);
    cxnet_System_Net_Http_HttpHeaders_Add(
        cxnet_System_Net_Http_HttpContent_Headers___const_get(
            (struct cxnet_System_Net_Http_HttpContent *)completed_content),
        make_string("Content-Type"), make_string("application/octet-stream"));
    canceled_request = (struct cxnet_System_Net_Http_HttpRequestMessage *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Http_HttpRequestMessage___typeinfo.Size);
    cxnet_System_Net_Http_HttpRequestMessage___constructor(canceled_request,
        cxnet_System_Net_Http_HttpMethod_Post___const_get(), canceled_uri);
    cxnet_System_Net_Http_HttpRequestMessage_Content___set(canceled_request,
        (struct cxnet_System_Net_Http_HttpContent *)canceled_content);
    completed_request = (struct cxnet_System_Net_Http_HttpRequestMessage *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Http_HttpRequestMessage___typeinfo.Size);
    cxnet_System_Net_Http_HttpRequestMessage___constructor(completed_request,
        cxnet_System_Net_Http_HttpMethod_Post___const_get(), completed_uri);
    cxnet_System_Net_Http_HttpRequestMessage_Content___set(completed_request,
        (struct cxnet_System_Net_Http_HttpContent *)completed_content);
    client = (struct cxnet_System_Net_Http_HttpClient *)cxcore_System_Memory_Alloc(
        cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    cxnet_System_Net_Http_HttpClient_TimeoutMilliseconds___set(client, 15000u);
    source = (struct cxcore_System_Threading_Tasks_CancellationTokenSource *)
        cxcore_System_Memory_Alloc(
            cxcore_System_Threading_Tasks_CancellationTokenSource___typeinfo.Size);
    cxcore_System_Threading_Tasks_CancellationTokenSource___constructor(source);
    token = cxcore_System_Threading_Tasks_CancellationTokenSource_Token___const_get(source);
    canceled_task = cxnet_System_Net_Http_HttpClient_SendAsync__2(client,
        canceled_request, token);
    completed_task = cxnet_System_Net_Http_HttpClient_SendAsync(client,
        completed_request);
    while ((!canceled_fixture.body_started || !completed_fixture.request_body_read) &&
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(canceled_task) &&
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(completed_task) &&
        steps < 500000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (!canceled_fixture.body_started || !completed_fixture.request_body_read ||
        cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(completed_task)) {
        fputs("concurrent streaming uploads did not reach the held-response state\n",
            stderr);
        goto cleanup;
    }
    cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(source);
    canceled_fixture.allow_body_drain = 1;
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(canceled_task) &&
        steps < 700000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    while (!canceled_fixture.client_closed && steps < 750000u) {
#ifdef _WIN32
        Sleep(1u);
#else
        usleep(1000u);
#endif
        steps++;
    }
    if (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(canceled_task) ||
        !cxnet_System_Net_Http_CurlHttpTransport_WasCanceled(canceled_task) ||
        !canceled_fixture.client_closed || canceled_fixture.failed ||
        canceled_fixture.body_bytes == 0u ||
        canceled_fixture.body_bytes >= canceled_upload_length ||
        cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(completed_task)) {
        fputs("canceling one streaming upload affected the held peer request\n", stderr);
        goto cleanup;
    }
    completed_fixture.release_response = 1;
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(completed_task) &&
        steps < 900000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(completed_task) ||
        cxcore_System_Threading_Tasks_Task_IsCanceled___const_get(completed_task) ||
        completed_task->_isFaulted) {
        fputs("peer streaming upload did not complete after isolated cancellation\n",
            stderr);
        goto cleanup;
    }
    response = cx_generic_640110dc1a526c1c94f059cac4ed8d246fe089fbe535a78bc1997c7adde4688b(
        completed_task);
    if (response == NULL ||
        !string_equals(cxnet_System_Net_Http_HttpContent_ReadAsString(
            cxnet_System_Net_Http_HttpResponseMessage_Content___get(response)),
            "pong")) {
        fputs("peer upload response did not contain the expected success body\n",
            stderr);
        goto cleanup;
    }
    cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
    result = 1;
cleanup:
    canceled_fixture.allow_body_drain = 1;
    completed_fixture.release_response = 1;
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    stop_upload_cancel_fixture(&canceled_fixture, canceled_thread);
    stop_fixture(&completed_fixture, completed_thread);
    cxnet_System_Net_Http_HttpRequestMessage_Dispose(canceled_request);
    cxnet_System_Net_Http_HttpRequestMessage_Dispose(completed_request);
    free(expected_body);
    return result;
}

static int run_streaming_upload_case(void) {
    const uint32_t upload_length = 131072u;
    struct fixture fixture;
    test_thread thread;
    uint16_t port = 0u;
    char url[160];
    char *expected = (char *)malloc((size_t)upload_length + 1u);
    struct cxcore_System_Uri *uri;
    struct cxcore_System_IO_MemoryStream *source;
    struct cxcore_System_Collections_Array *bytes;
    struct cxnet_System_Net_Http_StreamContent *content;
    struct cxnet_System_Net_Http_HttpClient *client;
    struct cxnet_System_Net_Http_HttpRequestMessage *request;
    struct cxcore_System_Threading_Tasks_Task *task;
    struct cxnet_System_Net_Http_HttpResponseMessage *response;
    uint32_t steps = 0u;
    if (expected == NULL) return 0;
    memset(expected, 'u', upload_length);
    expected[upload_length] = '\0';
    if (!start_fixture(&fixture, &port, &thread, 0,
            "POST /cx-http-stream-upload HTTP/1.1", expected,
            "Content-Type: application/octet-stream")) {
        free(expected);
        return 0;
    }
    (void)snprintf(url, sizeof(url), "http://localhost:%u/cx-http-stream-upload",
        (unsigned)port);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri, make_string(url));
    bytes = cx_array_with_type(cx_array_new(upload_length,
        (cx_uint)sizeof(cx_byte)),
        "cx_generic_9c4b0dcf5365274a639812f507806a7475845f1a6e6b8c085532c41d4210ef4b");
    memset(bytes->_data, 'u', upload_length);
    source = (struct cxcore_System_IO_MemoryStream *)cxcore_System_Memory_Alloc(
        cxcore_System_IO_MemoryStream___typeinfo.Size);
    cxcore_System_IO_MemoryStream___constructor__3(source, bytes);
    content = (struct cxnet_System_Net_Http_StreamContent *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Http_StreamContent___typeinfo.Size);
    cxnet_System_Net_Http_StreamContent___constructor__2(content,
        (struct cxcore_System_IO_Stream *)source, 1024u, 1);
    cxnet_System_Net_Http_HttpHeaders_Add(
        cxnet_System_Net_Http_HttpContent_Headers___const_get(
            (struct cxnet_System_Net_Http_HttpContent *)content),
        make_string("Content-Type"), make_string("application/octet-stream"));
    request = (struct cxnet_System_Net_Http_HttpRequestMessage *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Http_HttpRequestMessage___typeinfo.Size);
    cxnet_System_Net_Http_HttpRequestMessage___constructor(request,
        cxnet_System_Net_Http_HttpMethod_Post___const_get(), uri);
    cxnet_System_Net_Http_HttpRequestMessage_Content___set(request,
        (struct cxnet_System_Net_Http_HttpContent *)content);
    client = (struct cxnet_System_Net_Http_HttpClient *)cxcore_System_Memory_Alloc(
        cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    cxnet_System_Net_Http_HttpClient_TimeoutMilliseconds___set(client, 10000u);
    task = cxnet_System_Net_Http_HttpClient_SendAsync(client, request);
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        steps < 1000000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) break;
        steps++;
    }
    stop_fixture(&fixture, thread);
    free(expected);
    if (fixture.failed || !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task)) {
        fprintf(stderr, "streaming upload evidence fixture=%d completed=%d canceled=%d steps=%u\n",
            fixture.failed,
            cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task),
            cxcore_System_Threading_Tasks_Task_IsCanceled___const_get(task),
            steps);
        fprintf(stderr, "streaming upload request headers: %s\n", fixture.observed_headers);
        fprintf(stderr, "streaming upload wire body bytes=%zu\n", fixture.observed_body_bytes);
        if (task->_exception != NULL) {
            struct cxcore_System_String *message =
                cxcore_System_Exception_Message___const_get(task->_exception);
            if (message != NULL) {
                fprintf(stderr, "streaming upload exception: %.*s\n",
                    (int)message->_length, (char *)message->_data);
            }
        }
        if (cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
            cxnet_System_Net_Http_CurlHttpTransport_FailedResponseTransfer(task)) {
            fputs("streaming upload transport reported failure\n", stderr);
        }
        fputs("managed HttpClient did not stream the complete upload body\n", stderr);
        return 0;
    }
    response = cx_generic_640110dc1a526c1c94f059cac4ed8d246fe089fbe535a78bc1997c7adde4688b(task);
    cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
    return 1;
}

static int start_fixture(struct fixture *fixture, uint16_t *port,
    test_thread *thread, int stall_response, const char *expected_request_line,
    const char *expected_body, const char *expected_content_type) {
    struct sockaddr_in address;
    test_socklen length = (test_socklen)sizeof(address);
#ifdef _WIN32
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return 0;
    fixture->listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fixture->listener == INVALID_SOCKET) return 0;
#else
    fixture->listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fixture->listener < 0) return 0;
#endif
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(fixture->listener, (struct sockaddr *)&address,
            (test_socklen)sizeof(address)) != 0 || listen(fixture->listener, 1) != 0 ||
        getsockname(fixture->listener, (struct sockaddr *)&address, &length) != 0) return 0;
    fixture->failed = 0;
    fixture->stall_response = stall_response;
    fixture->body_sent = 0;
    fixture->body_bytes_sent = 0u;
    fixture->client_closed = 0;
    fixture->request_received = 0;
    fixture->request_body_read = 0;
    fixture->partial_headers_sent = 0;
    fixture->expect_client_close = stall_response == 2;
    fixture->hold_response = 0;
    fixture->release_response = 0;
    fixture->hold_stream_body = 0;
    fixture->release_stream_body = 0;
    fixture->expected_request_line = expected_request_line;
    fixture->expected_body = expected_body;
    fixture->expected_content_type = expected_content_type;
    fixture->observed_headers[0] = '\0';
    fixture->observed_body_bytes = 0u;
    fixture->early_reject_body_bytes = 0u;
    *port = ntohs(address.sin_port);
#ifdef _WIN32
    *thread = CreateThread(NULL, 0, fixture_main, fixture, 0, NULL);
    return *thread != NULL;
#else
    return pthread_create(thread, NULL, fixture_main, fixture) == 0;
#endif
}

static void cancel_token_on_scheduler(void *environment) {
    cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(
        (struct cxcore_System_Threading_Tasks_CancellationTokenSource *)environment);
}

static void wake_fixture_accept(uint16_t port) {
    struct sockaddr_in address;
    test_socket wake_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (wake_socket == (test_socket)-1
#ifdef _WIN32
        || wake_socket == INVALID_SOCKET
#endif
        ) return;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);
    (void)connect(wake_socket, (struct sockaddr *)&address,
        (test_socklen)sizeof(address));
#ifdef _WIN32
    closesocket(wake_socket);
#else
    close(wake_socket);
#endif
}

static void stop_fixture(struct fixture *fixture, test_thread thread) {
#ifdef _WIN32
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    closesocket(fixture->listener);
    WSACleanup();
#else
    pthread_join(thread, NULL);
    close(fixture->listener);
#endif
}

static void count_task_completion(void *environment) {
    volatile uint32_t *count = (volatile uint32_t *)environment;
    *count += 1u;
}

static int run_partial_response_header_cancellation_case(void) {
    struct fixture fixture;
    test_thread thread;
    uint16_t port = 0u;
    char url[160];
    struct cxcore_System_Uri *uri = NULL;
    struct cxnet_System_Net_Http_HttpClient *client = NULL;
    struct cxnet_System_Net_Http_HttpRequestMessage *request = NULL;
    struct cxcore_System_Threading_Tasks_CancellationTokenSource *source = NULL;
    struct cxcore_System_Threading_Tasks_CancellationToken token;
    struct cxcore_System_Threading_Tasks_Task *task = NULL;
    struct cx_gc_root_handle *uri_root = NULL;
    struct cx_gc_root_handle *client_root = NULL;
    struct cx_gc_root_handle *request_root = NULL;
    struct cx_gc_root_handle *source_root = NULL;
    struct cx_gc_root_handle *task_root = NULL;
    volatile uint32_t *completion_count = NULL;
    struct cx_gc_root_handle *completion_count_root = NULL;
    cx_action continuation = {0};
    uint32_t steps = 0u;
    uint64_t deadline = 0u;
    int fixture_started = 0;
    int passed = 0;

    if (!start_fixture(&fixture, &port, &thread, 2,
            "GET /cx-http-partial-response-headers HTTP/1.1", NULL, NULL)) {
        fputs("could not start partial-response-header fixture\n", stderr);
        return 0;
    }
    fixture_started = 1;
    (void)snprintf(url, sizeof(url),
        "http://localhost:%u/cx-http-partial-response-headers", (unsigned)port);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri, make_string(url));
    uri_root = cx_gc_add_root(&uri, (cx_uint)sizeof(uri));
    client = (struct cxnet_System_Net_Http_HttpClient *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    client_root = cx_gc_add_root(&client, (cx_uint)sizeof(client));
    source = (struct cxcore_System_Threading_Tasks_CancellationTokenSource *)
        cxcore_System_Memory_Alloc(
            cxcore_System_Threading_Tasks_CancellationTokenSource___typeinfo.Size);
    cxcore_System_Threading_Tasks_CancellationTokenSource___constructor(source);
    source_root = cx_gc_add_root(&source, (cx_uint)sizeof(source));
    token = cxcore_System_Threading_Tasks_CancellationTokenSource_Token___const_get(
        source);
    request = (struct cxnet_System_Net_Http_HttpRequestMessage *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpRequestMessage___typeinfo.Size);
    cxnet_System_Net_Http_HttpRequestMessage___constructor(request,
        cxnet_System_Net_Http_HttpMethod_Get___const_get(), uri);
    request_root = cx_gc_add_root(&request, (cx_uint)sizeof(request));
    task = cxnet_System_Net_Http_HttpClient_SendAsync__3(client, request,
        cxnet_System_Net_Http_HttpCompletionOption_ResponseHeadersRead, token);
    task_root = cx_gc_add_root(&task, (cx_uint)sizeof(task));
    completion_count = (volatile uint32_t *)cxcore_System_Memory_Alloc(
        (cx_uint)sizeof(*completion_count));
    if (completion_count != NULL) *completion_count = 0u;
    completion_count_root = cx_gc_add_root(&completion_count,
        (cx_uint)sizeof(completion_count));
    if (uri_root == NULL || client_root == NULL || source_root == NULL ||
        request_root == NULL || task_root == NULL || completion_count_root == NULL ||
        completion_count == NULL || task == NULL) {
        fputs("could not root partial-response-header cancellation state\n",
            stderr);
        goto cleanup;
    }
    continuation.__function = count_task_completion;
    continuation.__environment = (cx_ptr)completion_count;
    continuation.__target = (cx_ptr)completion_count;
    cxcore_System_Threading_Tasks_Task_OnCompleted(task, continuation);
    deadline = test_monotonic_milliseconds() + 10000u;
    while (!fixture.partial_headers_sent &&
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        steps < 200000u && test_monotonic_milliseconds() < deadline) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (!fixture.partial_headers_sent ||
        cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task)) {
        fprintf(stderr,
            "partial response headers did not hold the request task pending "
            "(sent=%d, request=%d, closed=%d, failed=%d, completed=%d)\n",
            fixture.partial_headers_sent, fixture.request_received,
            fixture.client_closed, fixture.failed,
            cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task));
        goto cleanup;
    }
    cx_gc_collect();
    if (cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task)) {
        fputs("forced GC completed a request with partial response headers\n",
            stderr);
        goto cleanup;
    }
    cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(source);
    steps = 0u;
    deadline = test_monotonic_milliseconds() + 10000u;
    while ((!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) ||
            !fixture.client_closed) && steps < 200000u &&
        test_monotonic_milliseconds() < deadline) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) ||
        !cxcore_System_Threading_Tasks_Task_IsCanceled___const_get(task) ||
        task->_isFaulted || !fixture.client_closed || fixture.failed) {
        fprintf(stderr,
            "canceling a pending partial-header response did not close and "
            "cancel (completed=%d, canceled=%d, faulted=%d, closed=%d, "
            "failed=%d)\n",
            cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task),
            cxcore_System_Threading_Tasks_Task_IsCanceled___const_get(task),
            task->_isFaulted, fixture.client_closed, fixture.failed);
        goto cleanup;
    }
    for (steps = 0u; steps < 32u; ++steps)
        (void)cxcore_System_Threading_Tasks_TaskScheduler_RunOne();
    if (*completion_count != 1u) {
        fprintf(stderr, "partial-header cancellation completed %u times\n",
            (unsigned)*completion_count);
        goto cleanup;
    }
    passed = 1;

cleanup:
    if (source != NULL && task != NULL &&
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task)) {
        cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(source);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        steps = 0u;
        deadline = test_monotonic_milliseconds() + 10000u;
        while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
            steps < 200000u && test_monotonic_milliseconds() < deadline) {
            if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                Sleep(1u);
#else
                usleep(1000u);
#endif
            }
            steps++;
        }
        if (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task))
            fputs("partial-header cleanup did not complete within 10 seconds\n",
                stderr);
    }
    if (fixture_started) stop_fixture(&fixture, thread);
    if (client != NULL) cxnet_System_Net_Http_HttpClient_Dispose(client);
    if (request != NULL) cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
    if (task_root != NULL) cx_gc_remove_root(task_root);
    if (source_root != NULL) cx_gc_remove_root(source_root);
    if (completion_count_root != NULL)
        cx_gc_remove_root(completion_count_root);
    if (request_root != NULL) cx_gc_remove_root(request_root);
    if (client_root != NULL) cx_gc_remove_root(client_root);
    if (uri_root != NULL) cx_gc_remove_root(uri_root);
    return passed;
}

static int run_https_certificate_case(const char *url, const char *ca_file,
    int expect_success, const char *case_name) {
    struct cxnet_System_Net_Http_CurlHttpMessageHandler *handler;
    struct cxnet_System_Net_Http_HttpClient *client;
    struct cxcore_System_Uri *uri;
    struct cxcore_System_Threading_Tasks_Task *task;
    uint32_t steps = 0u;
    handler = (struct cxnet_System_Net_Http_CurlHttpMessageHandler *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_CurlHttpMessageHandler___typeinfo.Size);
    cxnet_System_Net_Http_CurlHttpMessageHandler___constructor(handler);
    cxnet_System_Net_Http_CurlHttpMessageHandler_CaCertificateFile___set(
        handler, make_string(ca_file));
    cxnet_System_Net_Http_CurlHttpMessageHandler_AllowUnknownCertificateRevocation___set(
        handler, ca_file[0] != '\0');
    client = (struct cxnet_System_Net_Http_HttpClient *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    cxnet_System_Net_Http_HttpClient___constructor__3(client,
        (struct cxnet_System_Net_Http_HttpMessageHandler *)handler, 1);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri, make_string(url));
    task = cxnet_System_Net_Http_HttpClient_GetAsync__2(client, uri);
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) break;
        steps++;
    }
    if (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task)) {
        fprintf(stderr, "HTTPS %s did not complete\n", case_name);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        return 0;
    }
    {
        int failed = cxnet_System_Net_Http_CurlHttpTransport_FailedResponseTransfer(task);
        int passed = expect_success ? !failed : failed;
        if (expect_success && !failed) {
            struct cxnet_System_Net_Http_HttpResponseMessage *response =
                cx_generic_640110dc1a526c1c94f059cac4ed8d246fe089fbe535a78bc1997c7adde4688b(task);
            struct cxnet_System_Net_Http_HttpContent *content =
                cxnet_System_Net_Http_HttpResponseMessage_Content___get(response);
            passed = cxnet_System_Net_Http_HttpResponseMessage_StatusCode___get(response) ==
                    cxnet_System_Net_Http_HttpStatusCode_OK &&
                string_equals(cxnet_System_Net_Http_HttpContent_ReadAsString(content), "pong");
            cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
        } else if (!expect_success && !failed) {
            struct cxnet_System_Net_Http_HttpResponseMessage *response =
                cx_generic_640110dc1a526c1c94f059cac4ed8d246fe089fbe535a78bc1997c7adde4688b(task);
            struct cxnet_System_Net_Http_HttpContent *content =
                cxnet_System_Net_Http_HttpResponseMessage_Content___get(response);
            fprintf(stderr, "Unexpected HTTPS success for %s: status=%d body=%s\n",
                case_name,
                cxnet_System_Net_Http_HttpResponseMessage_StatusCode___get(response),
                content == NULL ? "<none>" : "<present>");
            cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
        }
        if (!passed) fprintf(stderr, "HTTPS certificate case failed: %s\n", case_name);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        return passed;
    }
}

static void disable_https_downgrade_on_scheduler(void *environment) {
    cxnet_System_Net_Http_HttpClient_AllowHttpsDowngrade___set(
        (struct cxnet_System_Net_Http_HttpClient *)environment, 0);
}

struct https_downgrade_setting_race {
    struct cxnet_System_Net_Http_HttpClient *client;
    volatile int ready;
    volatile int start;
    volatile int completed;
    volatile int failed;
};

#ifdef _WIN32
static DWORD WINAPI enable_https_downgrade_worker(void *environment) {
#else
static void *enable_https_downgrade_worker(void *environment) {
#endif
    struct https_downgrade_setting_race *race =
        (struct https_downgrade_setting_race *)environment;
    struct cxnet_System_Net_Http_HttpClient *client = race->client;
    struct cx_gc_root_handle *race_root = NULL;
    cx_bool attached = cx_gc_enter_thread();
    if (!attached) {
        request_race_store(&race->failed, 1);
        request_race_store(&race->completed, 1);
#ifdef _WIN32
        return 0;
#else
        return NULL;
#endif
    }
    race_root = cx_gc_add_root(&race, (cx_uint)sizeof(race));
    if (race_root == NULL) {
        request_race_store(&race->failed, 1);
        goto cleanup;
    }
    request_race_store(&race->ready, 1);
    while (!request_race_load(&race->start)) {
#ifdef _WIN32
        Sleep(0u);
#else
        usleep(100u);
#endif
    }
    cxnet_System_Net_Http_HttpClient_AllowHttpsDowngrade___set(client, 1);
cleanup:
    if (race_root != NULL) cx_gc_remove_root(race_root);
    cx_gc_leave_thread(attached);
    request_race_store(&race->completed, 1);
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

static int run_https_downgrade_case(const char *url, const char *ca_file) {
    struct cxnet_System_Net_Http_CurlHttpMessageHandler *handler;
    struct cxnet_System_Net_Http_HttpClient *client;
    struct cxcore_System_Uri *uri;
    struct cxcore_System_Threading_Tasks_Task *task;
    cx_action downgrade_action = {0};
    struct https_downgrade_setting_race race = {0};
    struct cx_gc_root_handle *race_root = NULL;
    test_thread setting_thread;
    uint32_t steps = 0u;
    handler = (struct cxnet_System_Net_Http_CurlHttpMessageHandler *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_CurlHttpMessageHandler___typeinfo.Size);
    cxnet_System_Net_Http_CurlHttpMessageHandler___constructor(handler);
    cxnet_System_Net_Http_CurlHttpMessageHandler_CaCertificateFile___set(
        handler, make_string(ca_file));
    cxnet_System_Net_Http_CurlHttpMessageHandler_AllowUnknownCertificateRevocation___set(
        handler, 1);
    client = (struct cxnet_System_Net_Http_HttpClient *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    cxnet_System_Net_Http_HttpClient___constructor__3(client,
        (struct cxnet_System_Net_Http_HttpMessageHandler *)handler, 1);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri, make_string(url));

    task = cxnet_System_Net_Http_HttpClient_GetAsync__2(client, uri);
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) break;
        steps++;
    }
    if (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) ||
        !cxnet_System_Net_Http_CurlHttpTransport_FailedResponseTransfer(task)) {
        fputs("HttpClient followed an HTTPS-to-HTTP redirect by default\n", stderr);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        return 0;
    }

    cxnet_System_Net_Http_HttpClient_AllowHttpsDowngrade___set(client, 1);
    downgrade_action.__function = disable_https_downgrade_on_scheduler;
    downgrade_action.__environment = client;
    downgrade_action.__target = (cx_ptr)client;
    cxcore_System_Threading_Tasks_TaskScheduler_PostAfter(
        20u, downgrade_action);
    task = cxnet_System_Net_Http_HttpClient_GetAsync__2(client, uri);
    steps = 0u;
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) break;
        steps++;
    }
    if (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) ||
        cxnet_System_Net_Http_CurlHttpTransport_FailedResponseTransfer(task)) {
        fputs("explicit HTTPS downgrade opt-in did not complete\n", stderr);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        return 0;
    }
    {
        struct cxnet_System_Net_Http_HttpResponseMessage *response =
            cx_generic_640110dc1a526c1c94f059cac4ed8d246fe089fbe535a78bc1997c7adde4688b(task);
        struct cxnet_System_Net_Http_HttpContent *content =
            cxnet_System_Net_Http_HttpResponseMessage_Content___get(response);
        int passed = content != NULL && string_equals(
            cxnet_System_Net_Http_HttpContent_ReadAsString(content), "pong");
        if (!passed) fputs("explicit HTTPS downgrade returned the wrong body\n", stderr);
        cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
        if (!passed) {
            cxnet_System_Net_Http_HttpClient_Dispose(client);
            return 0;
        }
    }
    cxnet_System_Net_Http_HttpClient_AllowHttpsDowngrade___set(client, 0);
    race_root = cx_gc_add_root(&race, (cx_uint)sizeof(race));
    if (race_root == NULL) goto race_failure;
    race.client = client;
#ifdef _WIN32
    setting_thread = CreateThread(NULL, 0, enable_https_downgrade_worker,
        &race, 0, NULL);
    if (setting_thread == NULL) goto race_failure;
#else
    if (pthread_create(&setting_thread, NULL, enable_https_downgrade_worker,
            &race) != 0) goto race_failure;
#endif
    steps = 0u;
    while (!request_race_load(&race.ready) &&
        !request_race_load(&race.completed) && steps < 200000u) {
#ifdef _WIN32
        Sleep(1u);
#else
        usleep(1000u);
#endif
        steps++;
    }
    if (!request_race_load(&race.ready)) {
        request_race_store(&race.start, 1);
#ifdef _WIN32
        WaitForSingleObject(setting_thread, INFINITE);
        CloseHandle(setting_thread);
#else
        pthread_join(setting_thread, NULL);
#endif
        goto race_failure;
    }
    request_race_store(&race.start, 1);
    task = cxnet_System_Net_Http_HttpClient_GetAsync__2(client, uri);
#ifdef _WIN32
    WaitForSingleObject(setting_thread, INFINITE);
    CloseHandle(setting_thread);
#else
    pthread_join(setting_thread, NULL);
#endif
    steps = 0u;
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
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
    if (request_race_load(&race.failed) ||
        !request_race_load(&race.completed) ||
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task)) {
        fputs("concurrent HTTPS downgrade setting/send race did not complete\n",
            stderr);
        goto race_failure;
    }
    if (task->_isFaulted) {
        if (!cxnet_System_Net_Http_CurlHttpTransport_FailedResponseTransfer(task)) {
            goto race_failure;
        }
    } else {
        struct cxnet_System_Net_Http_HttpResponseMessage *response =
            cx_generic_640110dc1a526c1c94f059cac4ed8d246fe089fbe535a78bc1997c7adde4688b(task);
        struct cxcore_System_String *body = response == NULL ? NULL :
            cxnet_System_Net_Http_HttpContent_ReadAsString(
                cxnet_System_Net_Http_HttpResponseMessage_Content___get(response));
        int passed = response != NULL && string_equals(body, "pong");
        if (response != NULL) cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
        if (!passed) goto race_failure;
    }
    cx_gc_remove_root(race_root);
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    return 1;

race_failure:
    if (race_root != NULL) cx_gc_remove_root(race_root);
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    return 0;
}

static int run_optional_https_certificate_cases(void) {
    const char *base_url = getenv("CXNET_TEST_HTTPS_URL");
    const char *wrong_host_url = getenv("CXNET_TEST_HTTPS_WRONG_HOST_URL");
    const char *expired_url = getenv("CXNET_TEST_HTTPS_EXPIRED_URL");
    const char *ca_file = getenv("CXNET_TEST_HTTPS_CA_FILE");
    const char *untrusted_ca_file = getenv("CXNET_TEST_HTTPS_UNTRUSTED_CA_FILE");
    const char *downgrade_url = getenv("CXNET_TEST_HTTPS_DOWNGRADE_URL");
    const char *downgrade_ca_file = getenv("CXNET_TEST_HTTPS_DOWNGRADE_CA_FILE");
    if (base_url == NULL) return 1;
    if (wrong_host_url == NULL || expired_url == NULL || ca_file == NULL ||
        untrusted_ca_file == NULL) {
        fputs("HTTPS fixture environment is incomplete\n", stderr);
        return 0;
    }
    if (!run_https_certificate_case(base_url, ca_file, 1, "trusted local CA")) return 0;
    if (!run_https_certificate_case(base_url, "", 0, "default trust rejects local CA")) return 0;
    if (!run_https_certificate_case(wrong_host_url, ca_file, 0, "wrong hostname")) return 0;
    if (!run_https_certificate_case(expired_url, ca_file, 0, "expired certificate")) return 0;
    if (!run_https_certificate_case(base_url, untrusted_ca_file, 0, "untrusted CA")) return 0;
    if (downgrade_url == NULL || downgrade_ca_file == NULL ||
        !run_https_downgrade_case(downgrade_url, downgrade_ca_file)) return 0;
    puts("PASS HTTPS local CA trust and certificate rejection cases");
    return 1;
}

static int run_empty_response_case(const char *request_line,
    const char *path, struct cxnet_System_Net_Http_HttpMethod *method,
    cxnet_System_Net_Http_HttpStatusCode expected_status, int convenience) {
    struct fixture fixture;
    test_thread thread;
    uint16_t port = 0u;
    char url[160];
    struct cxcore_System_Uri *uri;
    struct cxnet_System_Net_Http_HttpClient *client;
    struct cxnet_System_Net_Http_HttpRequestMessage *request;
    struct cxcore_System_Threading_Tasks_Task *task;
    struct cxnet_System_Net_Http_HttpResponseMessage *response;
    struct cxnet_System_Net_Http_HttpContent *content;
    struct cxcore_System_String *body;
    uint32_t steps = 0u;
    if (!start_fixture(&fixture, &port, &thread, 0, request_line, NULL, NULL)) {
        fputs("could not start bodyless-response fixture\n", stderr);
        return 0;
    }
    (void)snprintf(url, sizeof(url), "http://localhost:%u%s", (unsigned)port, path);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri, make_string(url));
    client = (struct cxnet_System_Net_Http_HttpClient *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    if (convenience == 1) {
        task = cxnet_System_Net_Http_HttpClient_HeadAsync__2(client, uri);
    } else if (convenience == 2) {
        task = cxnet_System_Net_Http_HttpClient_OptionsAsync__2(client, uri);
    } else {
        request = (struct cxnet_System_Net_Http_HttpRequestMessage *)
            cxcore_System_Memory_Alloc(cxnet_System_Net_Http_HttpRequestMessage___typeinfo.Size);
        cxnet_System_Net_Http_HttpRequestMessage___constructor(request, method, uri);
        task = cxnet_System_Net_Http_HttpClient_SendAsync(client, request);
    }
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) break;
        steps++;
    }
    stop_fixture(&fixture, thread);
    if (fixture.failed ||
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task)) {
        fputs("managed HttpClient did not complete a bodyless response\n", stderr);
        return 0;
    }
    response = cx_generic_640110dc1a526c1c94f059cac4ed8d246fe089fbe535a78bc1997c7adde4688b(task);
    content = cxnet_System_Net_Http_HttpResponseMessage_Content___get(response);
    body = cxnet_System_Net_Http_HttpContent_ReadAsString(content);
    if (cxnet_System_Net_Http_HttpResponseMessage_StatusCode___get(response) != expected_status ||
        body == NULL || body->_length != 0u) {
        fputs("managed HttpClient returned content for a bodyless response\n", stderr);
        return 0;
    }
    cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    return 1;
}

static int run_method_case(const char *request_line,
    const char *path, struct cxnet_System_Net_Http_HttpMethod *method,
    const char *body, const char *content_type) {
    struct fixture fixture;
    test_thread thread;
    uint16_t port = 0u;
    char url[160];
    struct cxcore_System_Uri *uri;
    struct cxnet_System_Net_Http_HttpClient *client;
    struct cxnet_System_Net_Http_HttpRequestMessage *request;
    struct cxcore_System_Threading_Tasks_Task *task;
    struct cxnet_System_Net_Http_HttpResponseMessage *response;
    struct cxnet_System_Net_Http_HttpContent *response_content;
    uint32_t steps = 0u;
    if (!start_fixture(&fixture, &port, &thread, 0, request_line,
            body, content_type)) {
        fputs("could not start HTTP method fixture\n", stderr);
        return 0;
    }
    (void)snprintf(url, sizeof(url), "http://localhost:%u%s", (unsigned)port, path);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri, make_string(url));
    request = (struct cxnet_System_Net_Http_HttpRequestMessage *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Http_HttpRequestMessage___typeinfo.Size);
    cxnet_System_Net_Http_HttpRequestMessage___constructor(request, method, uri);
    if (body != NULL) {
        struct cxnet_System_Net_Http_StringContent *content =
            (struct cxnet_System_Net_Http_StringContent *)cxcore_System_Memory_Alloc(
                cxnet_System_Net_Http_StringContent___typeinfo.Size);
        cxnet_System_Net_Http_StringContent___constructor(content, make_string(body));
        cxnet_System_Net_Http_HttpRequestMessage_Content___set(request,
            (struct cxnet_System_Net_Http_HttpContent *)content);
    }
    client = (struct cxnet_System_Net_Http_HttpClient *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    task = cxnet_System_Net_Http_HttpClient_SendAsync(client, request);
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) break;
        steps++;
    }
    stop_fixture(&fixture, thread);
    if (fixture.failed || !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task)) {
        fprintf(stderr, "HTTP method fixture failed for %s: fixture=%d completed=%d\n",
            request_line, fixture.failed,
            cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task));
        return 0;
    }
    if (task->_isFaulted) {
        struct cxcore_System_Exception *exception = task->_exception;
        struct cxcore_System_String *message = exception == NULL ? NULL :
            cxcore_System_Exception_Message___const_get(exception);
        fprintf(stderr, "HTTP method fixture unexpectedly faulted for %s: %.*s\n",
            request_line, message == NULL ? 0 : (int)message->_length,
            message == NULL ? "" : (const char *)message->_data);
        return 0;
    }
    response = cx_generic_640110dc1a526c1c94f059cac4ed8d246fe089fbe535a78bc1997c7adde4688b(task);
    if (cxnet_System_Net_Http_HttpResponseMessage_StatusCode___get(response) !=
            cxnet_System_Net_Http_HttpStatusCode_OK) {
        fputs("HTTP method fixture returned the wrong status\n", stderr);
        return 0;
    }
    response_content = cxnet_System_Net_Http_HttpResponseMessage_Content___get(response);
    if (!string_equals(cxnet_System_Net_Http_HttpContent_ReadAsString(response_content), "pong")) {
        fputs("HTTP method fixture returned the wrong response body\n", stderr);
        return 0;
    }
    if ((strstr(path, "/cx-http-gzip") != NULL ||
         strstr(path, "/cx-http-brotli") != NULL ||
         strstr(path, "/cx-http-zstd") != NULL) &&
        (cxnet_System_Net_Http_HttpHeaders_Contains(
            cxnet_System_Net_Http_HttpResponseMessage_Headers___const_get(response),
            make_string("content-encoding")) ||
         cxnet_System_Net_Http_HttpHeaders_Contains(
            cxnet_System_Net_Http_HttpContent_Headers___const_get(response_content),
            make_string("content-length")))) {
        fputs("managed HttpClient preserved wire encoding headers after decompression\n", stderr);
        return 0;
    }
    if ((strstr(path, "/cx-http-chunked") != NULL ||
         strstr(path, "/cx-http-segmented-chunked") != NULL) &&
        (cxnet_System_Net_Http_HttpHeaders_Contains(
            cxnet_System_Net_Http_HttpResponseMessage_Headers___const_get(response),
            make_string("X-CX-Trailer")) ||
         cxnet_System_Net_Http_HttpHeaders_Contains(
            cxnet_System_Net_Http_HttpContent_Headers___const_get(response_content),
            make_string("X-CX-Trailer")))) {
        fputs("managed HttpClient exposed a chunked trailer as an initial header\n",
            stderr);
        return 0;
    }
    if (strstr(path, "/cx-http-high-byte-text") != NULL &&
        (!string_equals(
            cxnet_System_Net_Http_HttpResponseMessage_ReasonPhrase___get(response),
            "\xc2\x80\xc3\xbf") ||
         !string_equals(cxnet_System_Net_Http_HttpHeaders_GetFirstValue(
            cxnet_System_Net_Http_HttpResponseMessage_Headers___const_get(response),
            make_string("X-CX-High")), "\xc2\x80\xc3\xbf"))) {
        fputs("managed HttpClient did not preserve high-byte response text as UTF-8\n",
            stderr);
        return 0;
    }
    cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
    return 1;
}

static int run_proxy_request_case(void) {
    struct fixture fixture;
    test_thread thread;
    uint16_t proxy_port = 0u;
    char proxy_url[96];
    struct cxcore_System_Uri *uri;
    struct cxnet_System_Net_Http_HttpClient *client;
    struct cxcore_System_Threading_Tasks_Task *task;
    struct cxnet_System_Net_Http_HttpResponseMessage *response;
    uint32_t steps = 0u;

    if (!start_fixture(&fixture, &proxy_port, &thread, 0,
            "GET http://cx-m26-proxy.invalid/cx-http-proxy HTTP/1.1",
            NULL, NULL)) {
        fputs("could not start HTTP proxy fixture\n", stderr);
        return 0;
    }
    (void)snprintf(proxy_url, sizeof(proxy_url), "http://127.0.0.1:%u",
        (unsigned)proxy_port);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri,
        make_string("http://cx-m26-proxy.invalid/cx-http-proxy"));
    client = (struct cxnet_System_Net_Http_HttpClient *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    cxnet_System_Net_Http_HttpClient_Proxy___set(client, make_string(proxy_url));
    cxnet_System_Net_Http_HttpClient_ProxyBypassList___set(client, make_string(""));
    task = cxnet_System_Net_Http_HttpClient_GetAsync__2(client, uri);
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) break;
        steps++;
    }
    stop_fixture(&fixture, thread);
    if (fixture.failed ||
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) ||
        task->_isFaulted) {
        fputs("managed HttpClient did not send the request through its proxy\n",
            stderr);
        return 0;
    }
    response = cx_generic_640110dc1a526c1c94f059cac4ed8d246fe089fbe535a78bc1997c7adde4688b(task);
    if (cxnet_System_Net_Http_HttpResponseMessage_StatusCode___get(response) !=
            cxnet_System_Net_Http_HttpStatusCode_OK ||
        !string_equals(cxnet_System_Net_Http_HttpContent_ReadAsString(
            cxnet_System_Net_Http_HttpResponseMessage_Content___get(response)),
            "pong")) {
        fputs("managed HttpClient returned the wrong proxied response\n", stderr);
        return 0;
    }
    cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    return 1;
}

static int run_client_verb_case(const char *request_line, const char *path,
    int verb, const char *body, const char *content_type) {
    struct fixture fixture;
    test_thread thread;
    uint16_t port = 0u;
    char url[160];
    struct cxcore_System_Uri *uri;
    struct cxnet_System_Net_Http_HttpClient *client;
    struct cxnet_System_Net_Http_StringContent *string_content = NULL;
    struct cxcore_System_Threading_Tasks_CancellationToken token =
        cxcore_System_Threading_Tasks_CancellationToken_None___const_get();
    struct cxcore_System_Threading_Tasks_Task *task;
    struct cxnet_System_Net_Http_HttpResponseMessage *response;
    uint32_t steps = 0u;

    if (!start_fixture(&fixture, &port, &thread, 0, request_line,
            body, content_type)) {
        fputs("could not start HttpClient convenience-verb fixture\n", stderr);
        return 0;
    }
    (void)snprintf(url, sizeof(url), "http://localhost:%u%s",
        (unsigned)port, path);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri, make_string(url));
    client = (struct cxnet_System_Net_Http_HttpClient *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    if (body != NULL) {
        string_content =
            (struct cxnet_System_Net_Http_StringContent *)
                cxcore_System_Memory_Alloc(
                    cxnet_System_Net_Http_StringContent___typeinfo.Size);
        cxnet_System_Net_Http_StringContent___constructor(
            string_content, make_string(body));
    }

    switch (verb) {
    case 0:
        task = cxnet_System_Net_Http_HttpClient_PostAsync(client, uri,
            (struct cxnet_System_Net_Http_HttpContent *)string_content, token);
        break;
    case 1:
        task = cxnet_System_Net_Http_HttpClient_PutAsync(client, uri,
            (struct cxnet_System_Net_Http_HttpContent *)string_content, token);
        break;
    case 2:
        task = cxnet_System_Net_Http_HttpClient_PatchAsync(client, uri,
            (struct cxnet_System_Net_Http_HttpContent *)string_content, token);
        break;
    case 3:
        task = cxnet_System_Net_Http_HttpClient_DeleteAsync(client, uri, token);
        break;
    default:
        stop_fixture(&fixture, thread);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        fputs("unknown HttpClient convenience verb\n", stderr);
        return 0;
    }

    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) break;
        steps++;
    }
    stop_fixture(&fixture, thread);
    if (fixture.failed ||
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) ||
        task->_isFaulted || task->_isCanceled) {
        fprintf(stderr, "HttpClient convenience verb failed for %s: fixture=%d completed=%d\n",
            request_line, fixture.failed,
            cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task));
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        return 0;
    }

    response = cx_generic_640110dc1a526c1c94f059cac4ed8d246fe089fbe535a78bc1997c7adde4688b(task);
    if (response == NULL ||
        cxnet_System_Net_Http_HttpResponseMessage_StatusCode___get(response) !=
            cxnet_System_Net_Http_HttpStatusCode_OK ||
        !string_equals(cxnet_System_Net_Http_HttpContent_ReadAsString(
            cxnet_System_Net_Http_HttpResponseMessage_Content___get(response)),
            "pong")) {
        fputs("HttpClient convenience verb returned an invalid response\n", stderr);
        cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        return 0;
    }
    cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    return 1;
}

static int run_http_utf8_supplementary_round_trip(void) {
    static const cx_char scalars[] = {0x41u, 0x1F30Du};
    static const unsigned char expected_utf8[] = {
        0x41u, 0xF0u, 0x9Fu, 0x8Cu, 0x8Du
    };
    struct cxnet_System_Net_Http_StringContent *string_content =
        (struct cxnet_System_Net_Http_StringContent *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_StringContent___typeinfo.Size);
    struct cxcore_System_Collections_Array *encoded;
    struct cxnet_System_Net_Http_ByteArrayContent *byte_content;
    struct cxcore_System_String *decoded;

    cxnet_System_Net_Http_StringContent___constructor(string_content,
        make_scalar_string(scalars, (cx_uint)(sizeof(scalars) / sizeof(scalars[0]))));
    encoded = cxnet_System_Net_Http_ByteArrayContent_ReadAsByteArray(
        (struct cxnet_System_Net_Http_ByteArrayContent *)string_content);
    if (encoded == NULL ||
        cxcore_System_Collections_Array_Length___const_get(encoded) !=
            (cx_uint)sizeof(expected_utf8) ||
        memcmp(cx_array_at(encoded, 0u, (cx_uint)sizeof(cx_byte)),
            expected_utf8, sizeof(expected_utf8)) != 0) {
        fputs("StringContent did not encode supplementary CX scalars as UTF-8\n",
            stderr);
        return 0;
    }

    byte_content = (struct cxnet_System_Net_Http_ByteArrayContent *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_ByteArrayContent___typeinfo.Size);
    cxnet_System_Net_Http_ByteArrayContent___constructor(byte_content, encoded);
    decoded = cxnet_System_Net_Http_HttpContent_ReadAsString(
        (struct cxnet_System_Net_Http_HttpContent *)byte_content);
    if (decoded == NULL || decoded->_length != (cx_uint)sizeof(expected_utf8) ||
        cxcore_System_String_Item___const_get(decoded, 0u) != scalars[0] ||
        cxcore_System_String_Item___const_get(decoded, 1u) != scalars[1]) {
        fputs("HTTP UTF-8 decoding did not preserve supplementary CX scalars\n",
            stderr);
        return 0;
    }
    cxnet_System_Net_Http_ByteArrayContent_Dispose(byte_content);
    cxnet_System_Net_Http_HttpContent_Dispose(
        (struct cxnet_System_Net_Http_HttpContent *)string_content);
    return 1;
}

static int run_string_charset_case(const char *path, const char *expected,
    int expect_fault) {
    struct fixture fixture;
    test_thread thread;
    uint16_t port = 0u;
    char url[160];
    struct cxcore_System_Uri *uri;
    struct cxnet_System_Net_Http_HttpClient *client;
    struct cxcore_System_Threading_Tasks_Task *task;
    uint32_t steps = 0u;
    char request_line[96];

    (void)snprintf(request_line, sizeof(request_line), "GET %s HTTP/1.1", path);
    if (!start_fixture(&fixture, &port, &thread, 0, request_line, NULL, NULL)) {
        fputs("could not start charset response fixture\n", stderr);
        return 0;
    }
    (void)snprintf(url, sizeof(url), "http://localhost:%u%s", (unsigned)port,
        path);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri, make_string(url));
    client = (struct cxnet_System_Net_Http_HttpClient *)cxcore_System_Memory_Alloc(
        cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    task = cxnet_System_Net_Http_HttpClient_GetStringAsync__2(client, uri);
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) break;
        steps++;
    }
    stop_fixture(&fixture, thread);
    if (fixture.failed ||
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) ||
        task->_isFaulted != expect_fault) {
        fprintf(stderr,
            "charset response did not match expectations for %s: fixture=%d completed=%d faulted=%d\n",
            path, fixture.failed,
            cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task),
            task->_isFaulted);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        return 0;
    }
    if (!expect_fault && !string_equals(
            cx_generic_61b4e22ba70d50cc885a0dac2d8bad266f92222a2650ca64de8edc4d369f0bd9(task),
            expected)) {
        fprintf(stderr, "charset response decoded incorrectly for %s\n", path);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        return 0;
    }
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    return 1;
}

static int run_early_upload_rejection_case(int after_body_started) {
    const uint32_t upload_length = 8u * 1024u * 1024u;
    struct fixture fixture;
    test_thread thread;
    uint16_t port = 0u;
    char url[160];
    struct cxcore_System_Uri *uri;
    struct cxcore_System_Collections_Array *bytes;
    struct cxcore_System_IO_MemoryStream *stream;
    struct cxnet_System_Net_Http_StreamContent *content;
    struct cxnet_System_Net_Http_HttpClient *client;
    struct cxnet_System_Net_Http_HttpRequestMessage *request;
    struct cxcore_System_Threading_Tasks_Task *task;
    struct cxnet_System_Net_Http_HttpResponseMessage *response;
    uint32_t steps = 0u;
    if (!start_fixture(&fixture, &port, &thread, 0,
            after_body_started
                ? "POST /cx-http-early-body-reject HTTP/1.1"
                : "POST /cx-http-early-reject HTTP/1.1",
            NULL, NULL)) {
        fputs("could not start early upload rejection fixture\n", stderr);
        return 0;
    }
    (void)snprintf(url, sizeof(url), "http://localhost:%u/%s", (unsigned)port,
        after_body_started ? "cx-http-early-body-reject" : "cx-http-early-reject");
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri, make_string(url));
    bytes = cx_array_with_type(cx_array_new(upload_length,
        (cx_uint)sizeof(cx_byte)),
        "cx_generic_9c4b0dcf5365274a639812f507806a7475845f1a6e6b8c085532c41d4210ef4b");
    memset(bytes->_data, 'u', upload_length);
    stream = (struct cxcore_System_IO_MemoryStream *)cxcore_System_Memory_Alloc(
        cxcore_System_IO_MemoryStream___typeinfo.Size);
    cxcore_System_IO_MemoryStream___constructor__3(stream, bytes);
    content = (struct cxnet_System_Net_Http_StreamContent *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Http_StreamContent___typeinfo.Size);
    cxnet_System_Net_Http_StreamContent___constructor__2(content,
        (struct cxcore_System_IO_Stream *)stream, 1024u, 1);
    request = (struct cxnet_System_Net_Http_HttpRequestMessage *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpRequestMessage___typeinfo.Size);
    cxnet_System_Net_Http_HttpRequestMessage___constructor(request,
        cxnet_System_Net_Http_HttpMethod_Post___const_get(), uri);
    cxnet_System_Net_Http_HttpRequestMessage_Content___set(request,
        (struct cxnet_System_Net_Http_HttpContent *)content);
    if (!cxnet_System_Net_Http_HttpHeaders_TryAddWithoutValidation(
            cxnet_System_Net_Http_HttpRequestMessage_Headers___const_get(request),
            make_string("Expect"), make_string("100-continue"))) {
        fputs("could not add Expect: 100-continue to streamed upload\n", stderr);
        stop_fixture(&fixture, thread);
        return 0;
    }
    client = (struct cxnet_System_Net_Http_HttpClient *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    task = cxnet_System_Net_Http_HttpClient_SendAsync(client, request);
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        steps < 300000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    stop_fixture(&fixture, thread);
    if (fixture.failed || !fixture.request_received ||
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) ||
        task->_isFaulted || task->_isCanceled) {
        fprintf(stderr, "early upload rejection failed: fixture=%d request=%d completed=%d faulted=%d canceled=%d bodyBytes=%zu\n",
            fixture.failed, fixture.request_received,
            cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task),
            task->_isFaulted, task->_isCanceled,
            fixture.early_reject_body_bytes);
        return 0;
    }
    response = cx_generic_640110dc1a526c1c94f059cac4ed8d246fe089fbe535a78bc1997c7adde4688b(
        task);
    if (response == NULL ||
        cxnet_System_Net_Http_HttpResponseMessage_StatusCode___get(response) !=
            (after_body_started ? 413 : 417) ||
        (!after_body_started && fixture.early_reject_body_bytes != 0u) ||
        (after_body_started && fixture.early_reject_body_bytes == 0u)) {
        fprintf(stderr, "early upload rejection returned the wrong status or body transfer: status=%d bytes=%zu\n",
            response == NULL ? 0 : cxnet_System_Net_Http_HttpResponseMessage_StatusCode___get(response),
            fixture.early_reject_body_bytes);
        return 0;
    }
    cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
    cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    return 1;
}

static int run_default_request_headers_case(void) {
    struct fixture fixture;
    test_thread thread;
    uint16_t port = 0u;
    char url[160];
    struct cxcore_System_Uri *uri;
    struct cxnet_System_Net_Http_HttpClient *client;
    struct cxnet_System_Net_Http_HttpRequestMessage *request;
    struct cxcore_System_Threading_Tasks_Task *task;
    struct cxnet_System_Net_Http_HttpResponseMessage *response;
    struct cxnet_System_Net_Http_HttpHeaders *default_headers;
    uint32_t steps = 0u;
    if (!start_fixture(&fixture, &port, &thread, 0,
            "GET /cx-http-default-header HTTP/1.1", NULL, NULL)) {
        fputs("could not start default-header fixture\n", stderr);
        return 0;
    }
    (void)snprintf(url, sizeof(url),
        "http://localhost:%u/cx-http-default-header", (unsigned)port);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri, make_string(url));
    client = (struct cxnet_System_Net_Http_HttpClient *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    default_headers =
        cxnet_System_Net_Http_HttpClient_DefaultRequestHeaders___get(client);
    cxnet_System_Net_Http_HttpHeaders_Add(default_headers,
        make_string("X-CX-Default"), make_string("from-client"));
    cxnet_System_Net_Http_HttpHeaders_Add(default_headers,
        make_string("X-CX-Override"), make_string("default"));
    request = (struct cxnet_System_Net_Http_HttpRequestMessage *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Http_HttpRequestMessage___typeinfo.Size);
    cxnet_System_Net_Http_HttpRequestMessage___constructor(request,
        cxnet_System_Net_Http_HttpMethod_Get___const_get(), uri);
    cxnet_System_Net_Http_HttpHeaders_Add(
        cxnet_System_Net_Http_HttpRequestMessage_Headers___const_get(request),
        make_string("X-CX-Override"), make_string("request"));
    task = cxnet_System_Net_Http_HttpClient_SendAsync(client, request);
    (void)cxnet_System_Net_Http_HttpHeaders_Remove(default_headers,
        make_string("X-CX-Default"));
    cxnet_System_Net_Http_HttpHeaders_Add(default_headers,
        make_string("X-CX-Default"), make_string("after-send"));
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) break;
        steps++;
    }
    stop_fixture(&fixture, thread);
    if (fixture.failed || !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task)) {
        fputs("default request headers were not sent or request headers did not take precedence\n", stderr);
        return 0;
    }
    response = cx_generic_640110dc1a526c1c94f059cac4ed8d246fe089fbe535a78bc1997c7adde4688b(task);
    cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
    cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    return 1;
}

static int run_cookie_request_case(
    struct cxnet_System_Net_Http_HttpClient *client,
    const char *path, const char *expected_body) {
    struct fixture fixture;
    test_thread thread;
    uint16_t port = 0u;
    char url[192];
    char request_line[160];
    struct cxcore_System_Uri *uri;
    struct cxcore_System_Threading_Tasks_Task *task;
    struct cxnet_System_Net_Http_HttpResponseMessage *response;
    uint32_t steps = 0u;
    (void)snprintf(request_line, sizeof(request_line), "GET %s HTTP/1.1", path);
    if (!start_fixture(&fixture, &port, &thread, 0,
            request_line, NULL, NULL)) {
        fputs("could not start cookie fixture\n", stderr);
        return 0;
    }
    (void)snprintf(url, sizeof(url), "http://localhost:%u%s",
        (unsigned)port, path);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri, make_string(url));
    task = cxnet_System_Net_Http_HttpClient_GetAsync__2(client, uri);
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) break;
        steps++;
    }
    stop_fixture(&fixture, thread);
    if (fixture.failed || !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task)) {
        fprintf(stderr, "cookie request did not complete for %s\n", path);
        return 0;
    }
    response = cx_generic_640110dc1a526c1c94f059cac4ed8d246fe089fbe535a78bc1997c7adde4688b(task);
    if (!string_equals(cxnet_System_Net_Http_HttpContent_ReadAsString(
            cxnet_System_Net_Http_HttpResponseMessage_Content___get(response)),
            expected_body)) {
        fprintf(stderr, "cookie behavior did not match for %s\n", path);
        return 0;
    }
    cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
    return 1;
}

static struct cxnet_System_Net_CookieParseResult parse_cookie_for_host(
    const char *uri_text, const char *cookie_text) {
    struct cxcore_System_Uri *uri =
        (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
            cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri, make_string(uri_text));
    return cxnet_System_Net_CookieContainer_ParseCookie(
        uri, make_string(cookie_text));
}

static int run_cookie_container_cases(void) {
    struct cxnet_System_Net_Http_HttpClient *client =
        (struct cxnet_System_Net_Http_HttpClient *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    struct cxnet_System_Net_Http_HttpClient *isolated_client =
        (struct cxnet_System_Net_Http_HttpClient *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    struct cxnet_System_Net_CookieContainer *isolated_cookies;
    struct cxcore_System_Uri *cookie_uri;
    struct cxcore_System_String *cookie_header;
    struct cxnet_System_Net_CookieDateParseResult parsed_date;
    struct cxnet_System_Net_CookieParseResult parsed_cookie;
    struct cxnet_System_Net_CookieContainer *canonical_cookies;
    struct cxcore_System_Uri *canonical_uri;
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    cxnet_System_Net_Http_HttpClient___constructor__2(isolated_client);
    isolated_cookies =
        cxnet_System_Net_Http_HttpClient_CookieContainer___get(isolated_client);
    cookie_uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(cookie_uri,
        make_string("http://localhost/cx-http-cookie-area/check"));
    parsed_date = cxnet_System_Net_CookieContainer_ParseCookieDate(
        make_string("Wed, 21 Oct 2015 07:28:00 GMT"));
    if (!parsed_date.Valid) {
        fputs("RFC 1123 cookie date parser rejected its legacy format\n", stderr);
        return 0;
    }
    parsed_date = cxnet_System_Net_CookieContainer_ParseCookieDate(
        make_string("Thu, 1-Jan-70 7:8:9 GMT"));
    if (!parsed_date.Valid) {
        fputs("cookie date parser rejected one-digit date/time components\n", stderr);
        return 0;
    }
    parsed_date = cxnet_System_Net_CookieContainer_ParseCookieDate(
        make_string("1 Jan 70 07:08:09 UTC"));
    if (!parsed_date.Valid) {
        fputs("cookie date parser rejected an alternate token order\n", stderr);
        return 0;
    }
    parsed_date = cxnet_System_Net_CookieContainer_ParseCookieDate(
        make_string("Thu, 1-Jan-70 07:888:09 GMT"));
    if (parsed_date.Valid) {
        fputs("cookie date parser accepted a three-digit minute\n", stderr);
        return 0;
    }
    parsed_date = cxnet_System_Net_CookieContainer_ParseCookieDate(
        make_string("Thu, 1-Jan-70 07:08:999 GMT"));
    if (parsed_date.Valid) {
        fputs("cookie date parser accepted a three-digit second\n", stderr);
        return 0;
    }
    parsed_cookie = parse_cookie_for_host("http://shop.example.com/",
        "bad=1; Domain=com");
    if (parsed_cookie.Valid) {
        fputs("cookie for the com public suffix was accepted\n", stderr);
        return 0;
    }
    parsed_cookie = parse_cookie_for_host("http://shop.example.co.uk/",
        "bad=1; Domain=co.uk");
    if (parsed_cookie.Valid) {
        fputs("cookie for the co.uk public suffix was accepted\n", stderr);
        return 0;
    }
    parsed_cookie = parse_cookie_for_host("http://blog.blogspot.com/",
        "bad=1; Domain=blogspot.com");
    if (parsed_cookie.Valid) {
        fputs("cookie for the blogspot.com private suffix was accepted\n", stderr);
        return 0;
    }
    parsed_cookie = parse_cookie_for_host("http://bar.foo.ck/",
        "bad=1; Domain=foo.ck");
    if (parsed_cookie.Valid) {
        fputs("cookie matching a wildcard public suffix was accepted\n", stderr);
        return 0;
    }
    parsed_cookie = parse_cookie_for_host("http://shop.www.ck/",
        "exception=allowed; Domain=www.ck");
    if (!parsed_cookie.Valid || parsed_cookie.Cookie->HostOnly) {
        fputs("public-suffix exception rule was not honored\n", stderr);
        return 0;
    }
    parsed_cookie = parse_cookie_for_host("http://sub.example.com/",
        "valid=allowed; Domain=example.com");
    if (!parsed_cookie.Valid || parsed_cookie.Cookie->HostOnly) {
        fputs("registrable parent cookie domain was rejected\n", stderr);
        return 0;
    }
    parsed_cookie = parse_cookie_for_host("http://com/",
        "exact=host-only; Domain=com");
    if (!parsed_cookie.Valid || !parsed_cookie.Cookie->HostOnly) {
        fputs("exact public-suffix host was not restricted to host-only scope\n", stderr);
        return 0;
    }
    parsed_cookie = parse_cookie_for_host("http://xn--bcher-kva.example/",
        "idna=matched; Domain=b\xC3\xBC" "cher.example");
    if (!parsed_cookie.Valid || parsed_cookie.Cookie->HostOnly ||
        !string_equals(parsed_cookie.Cookie->Domain,
            "xn--bcher-kva.example")) {
        fputs("Unicode cookie domain was not canonicalized to its A-label\n", stderr);
        return 0;
    }
    parsed_cookie = parse_cookie_for_host("http://example.com/",
        "bad=1; Domain=example.com:80");
    if (parsed_cookie.Valid) {
        fputs("cookie domain containing a port was accepted\n", stderr);
        return 0;
    }
    parsed_cookie = parse_cookie_for_host("http://example.com/",
        "bad=1; Domain=attacker@example.com");
    if (parsed_cookie.Valid) {
        fputs("cookie domain containing userinfo syntax was accepted\n", stderr);
        return 0;
    }
    parsed_cookie = parse_cookie_for_host("http://192.0.2.1/",
        "bad=1; Domain=0.2.1");
    if (parsed_cookie.Valid) {
        fputs("IPv4 cookie domain suffix matched an IP-literal host\n", stderr);
        return 0;
    }
    parsed_cookie = parse_cookie_for_host("http://[2001:db8::1]/",
        "bad=1; Domain=::1");
    if (parsed_cookie.Valid) {
        fputs("IPv6 cookie domain suffix matched an IP-literal host\n", stderr);
        return 0;
    }
    parsed_cookie = parse_cookie_for_host("http://192.0.2.1/",
        "exact=matched; Domain=192.0.2.1");
    if (!parsed_cookie.Valid || parsed_cookie.Cookie->HostOnly ||
        !string_equals(parsed_cookie.Cookie->Domain, "192.0.2.1")) {
        fputs("exact IPv4 cookie domain did not match its IP-literal host\n", stderr);
        return 0;
    }
    canonical_cookies =
        (struct cxnet_System_Net_CookieContainer *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_CookieContainer___typeinfo.Size);
    cxnet_System_Net_CookieContainer___constructor(canonical_cookies);
    canonical_uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(canonical_uri,
        make_string("http://xn--bcher-kva.example/"));
    cxnet_System_Net_CookieContainer_Add(canonical_cookies, canonical_uri,
        make_string("idna=matched; Domain=b\xC3\xBC" "cher.example"));
    cookie_header = cxnet_System_Net_CookieContainer_GetCookieHeader(
        canonical_cookies, canonical_uri);
    if (!string_equals(cookie_header, "idna=matched")) {
        fputs("IDNA cookie domain did not match the canonical request host\n", stderr);
        return 0;
    }
    canonical_uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(canonical_uri,
        make_string("http://[2001:db8::1]/"));
    cxnet_System_Net_CookieContainer_Add(canonical_cookies, canonical_uri,
        make_string("ipv6=matched; Domain=2001:0db8:0:0:0:0:0:1"));
    cookie_header = cxnet_System_Net_CookieContainer_GetCookieHeader(
        canonical_cookies, canonical_uri);
    if (!string_equals(cookie_header, "ipv6=matched")) {
        fputs("equivalent IPv6 cookie domain did not match the request host\n", stderr);
        return 0;
    }
    cxnet_System_Net_CookieContainer_Add(isolated_cookies, cookie_uri,
        make_string("rfc1123=old; Expires=Wed, 21 Oct 2015 07:28:00 GMT"));
    cxnet_System_Net_CookieContainer_Add(isolated_cookies, cookie_uri,
        make_string("rfc850=old; Expires=Wednesday, 21-Oct-15 07:28:00 GMT"));
    cxnet_System_Net_CookieContainer_Add(isolated_cookies, cookie_uri,
        make_string("asctime=old; Expires=Wed Oct 21 07:28:00 2015"));
    cookie_header = cxnet_System_Net_CookieContainer_GetCookieHeader(
        isolated_cookies, cookie_uri);
    if (cookie_header == NULL || cookie_header->_length != 0u) {
        fputs("expired cookie-date formats were retained in the cookie jar\n", stderr);
        return 0;
    }
    if (!run_cookie_request_case(client, "/cx-http-cookie-area/set", "set") ||
        !run_cookie_request_case(client, "/cx-http-cookie-area/check", "found") ||
        !run_cookie_request_case(client, "/cx-http-cookie-check", "missing") ||
        !run_cookie_request_case(client, "/cx-http-cookie-area/expired", "expired") ||
        !run_cookie_request_case(client, "/cx-http-cookie-area/past-check", "missing") ||
        !run_cookie_request_case(client, "/cx-http-cookie-area/delete", "deleted") ||
        !run_cookie_request_case(client, "/cx-http-cookie-area/check", "missing") ||
        !run_cookie_request_case(isolated_client,
            "/cx-http-cookie-area/check", "missing")) return 0;
    isolated_cookies =
        cxnet_System_Net_Http_HttpClient_CookieContainer___get(isolated_client);
    cxnet_System_Net_CookieContainer_Add(isolated_cookies, cookie_uri,
        make_string("session=blue; Path=/cx-http-cookie-area"));
    cookie_header = cxnet_System_Net_CookieContainer_GetCookieHeader(
        isolated_cookies, cookie_uri);
    if (!string_equals(cookie_header, "session=blue")) {
        fputs("expired legacy cookies were retained in the cookie jar\n", stderr);
        return 0;
    }
    cxnet_System_Net_Http_HttpClient_UseCookies___set(isolated_client, 0);
    if (!run_cookie_request_case(isolated_client,
            "/cx-http-cookie-area/check", "missing")) return 0;
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    cxnet_System_Net_Http_HttpClient_Dispose(isolated_client);
    return 1;
}

static int run_buffered_reader_case(int reader_kind) {
    struct fixture fixture;
    test_thread thread;
    uint16_t port = 0u;
    char url[160];
    const char *path = reader_kind == 0 ? "/cx-http-string" :
        (reader_kind == 1 ? "/cx-http-bytes" :
         (reader_kind == 3 ? "/cx-http-stream-large" :
          (reader_kind == 4 ? "/cx-http-stream-headers" :
           (reader_kind == 5 ? "/cx-http-stream-close-delimited" :
            (reader_kind == 6 ? "/cx-http-stream-large" :
             (reader_kind == 7 ? "/cx-http-stream" :
              (reader_kind == 8 ? "/cx-http-stream-cancel" :
               "/cx-http-stream")))))));
    struct cxcore_System_Uri *uri;
    struct cxnet_System_Net_Http_HttpClient *client;
    struct cxnet_System_Net_Http_CurlHttpMessageHandler *curl_handler = NULL;
    struct cxnet_System_Net_Http_HttpRequestMessage *request = NULL;
    struct cxnet_System_Net_Http_HttpResponseMessage *response = NULL;
    struct cxcore_System_Threading_Tasks_Task *task;
    struct cxcore_System_Collections_Array *bytes;
    struct cxcore_System_IO_Stream *stream;
    struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *read_task;
    struct cxcore_System_Threading_Tasks_CancellationTokenSource *read_cancellation_source;
    struct cxcore_System_Threading_Tasks_CancellationToken read_cancellation_token;
    uint32_t steps = 0u;
    char request_line[96];
    (void)snprintf(request_line, sizeof(request_line), "GET %s HTTP/1.1", path);
    if (!start_fixture(&fixture, &port, &thread, 0,
            request_line, NULL, NULL)) {
        fputs("could not start buffered HTTP reader fixture\n", stderr);
        return 0;
    }
    (void)snprintf(url, sizeof(url), "http://localhost:%u%s",
        (unsigned)port, path);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri, make_string(url));
    client = (struct cxnet_System_Net_Http_HttpClient *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    if (reader_kind == 3) {
        curl_handler = (struct cxnet_System_Net_Http_CurlHttpMessageHandler *)
            cxcore_System_Memory_Alloc(
                cxnet_System_Net_Http_CurlHttpMessageHandler___typeinfo.Size);
        cxnet_System_Net_Http_CurlHttpMessageHandler___constructor(curl_handler);
        cxnet_System_Net_Http_CurlHttpMessageHandler_MaxResponseContentBufferSize___set(
            curl_handler, 4u);
        cxnet_System_Net_Http_HttpClient___constructor__3(client,
            (struct cxnet_System_Net_Http_HttpMessageHandler *)curl_handler, 1);
    } else {
        cxnet_System_Net_Http_HttpClient___constructor__2(client);
    }
    if (reader_kind == 0) {
        task = cxnet_System_Net_Http_HttpClient_GetStringAsync__2(client, uri);
    } else if (reader_kind == 1) {
        task = cxnet_System_Net_Http_HttpClient_GetByteArrayAsync__2(client, uri);
    } else if (reader_kind == 4 || reader_kind == 6 || reader_kind == 7 ||
        reader_kind == 8) {
        request = (struct cxnet_System_Net_Http_HttpRequestMessage *)
            cxcore_System_Memory_Alloc(
                cxnet_System_Net_Http_HttpRequestMessage___typeinfo.Size);
        cxnet_System_Net_Http_HttpRequestMessage___constructor(request,
            cxnet_System_Net_Http_HttpMethod_Get___const_get(), uri);
        task = cxnet_System_Net_Http_HttpClient_SendAsync__4(client, request,
            cxnet_System_Net_Http_HttpCompletionOption_ResponseHeadersRead);
    } else {
        task = cxnet_System_Net_Http_HttpClient_GetStreamAsync__2(client, uri);
    }
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
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
    if (reader_kind < 2) stop_fixture(&fixture, thread);
    if (fixture.failed ||
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task)) {
        fputs("HttpClient buffered reader did not complete its local request\n", stderr);
        return 0;
    }
    if (request != NULL) cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
    if (reader_kind == 0) {
        if (!string_equals(cx_generic_61b4e22ba70d50cc885a0dac2d8bad266f92222a2650ca64de8edc4d369f0bd9(task), "pong")) {
            fputs("GetStringAsync returned the wrong content\n", stderr);
            return 0;
        }
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        return 1;
    }
    if (reader_kind == 1) {
        struct cxnet_System_Net_Http_ByteArrayContent *array_content;
        if (cx_generic_47e308080f2e4db04484c1e6ac349bad319be87946d6b240deb100ea368f522e(task) == NULL) {
            fputs("GetByteArrayAsync returned no content\n", stderr);
            return 0;
        }
        bytes = cx_generic_47e308080f2e4db04484c1e6ac349bad319be87946d6b240deb100ea368f522e(task);
        if (cxcore_System_Collections_Array_Length___const_get(bytes) != 4u) {
            fputs("GetByteArrayAsync returned the wrong content length\n", stderr);
            return 0;
        }
        array_content = (struct cxnet_System_Net_Http_ByteArrayContent *)
            cxcore_System_Memory_Alloc(
                cxnet_System_Net_Http_ByteArrayContent___typeinfo.Size);
        cxnet_System_Net_Http_ByteArrayContent___constructor(array_content, bytes);
        if (!string_equals(cxnet_System_Net_Http_HttpContent_ReadAsString(
                (struct cxnet_System_Net_Http_HttpContent *)array_content), "pong")) {
            fputs("GetByteArrayAsync returned the wrong bytes\n", stderr);
            return 0;
        }
        cxnet_System_Net_Http_ByteArrayContent_Dispose(array_content);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        return 1;
    }
    if (reader_kind == 4 || reader_kind == 6 || reader_kind == 7 ||
        reader_kind == 8) {
        response = cx_generic_640110dc1a526c1c94f059cac4ed8d246fe089fbe535a78bc1997c7adde4688b(task);
        if (response == NULL || fixture.body_sent) {
            fputs("ResponseHeadersRead waited for the streamed response body\n", stderr);
            return 0;
        }
    }
    if (reader_kind == 6 || reader_kind == 7 || reader_kind == 8) {
        struct cxnet_System_Net_Http_HttpContent *content =
            cxnet_System_Net_Http_HttpResponseMessage_Content___get(response);
        task = reader_kind == 7
            ? cxnet_System_Net_Http_HttpContent_ReadAsStringAsync(content)
            : cxnet_System_Net_Http_HttpContent_ReadAsByteArrayAsync(content);
        if (reader_kind == 8) {
            steps = 0u;
            while (!fixture.body_sent &&
                !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
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
            if (!fixture.body_sent ||
                cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task)) {
                fputs("async response read did not reach its pending-body state\n",
                    stderr);
                return 0;
            }
            cx_gc_collect();
            cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
            steps = 0u;
            while ((!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) ||
                    !fixture.client_closed) && steps < 300000u) {
                if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                    Sleep(1u);
#else
                    usleep(1000u);
#endif
                }
                steps++;
            }
            if (fixture.failed || !fixture.client_closed ||
                !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) ||
                !task->_isCanceled) {
                fprintf(stderr,
                    "async response disposal failed=%d closed=%d completed=%d canceled=%d\n",
                    fixture.failed, fixture.client_closed,
                    cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task),
                    task->_isCanceled);
                return 0;
            }
            stop_fixture(&fixture, thread);
            cxnet_System_Net_Http_HttpClient_Dispose(client);
            return 1;
        }
        if (cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task)) {
            fputs("async response content read completed before body arrival\n",
                stderr);
            return 0;
        }
        cx_gc_collect();
        steps = 0u;
        while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
            steps < 500000u) {
            if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                Sleep(1u);
#else
                usleep(1000u);
#endif
            }
            steps++;
        }
        if (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) ||
            task->_isFaulted || task->_isCanceled) {
            fputs("async response content read did not complete successfully\n",
                stderr);
            return 0;
        }
        if (reader_kind == 6) {
            uint32_t index;
            struct cxcore_System_Threading_Tasks_Task *repeatedTask;
            bytes = cx_generic_47e308080f2e4db04484c1e6ac349bad319be87946d6b240deb100ea368f522e(task);
            if (bytes == NULL ||
                cxcore_System_Collections_Array_Length___const_get(bytes) != 262144u) {
                fputs("ReadAsByteArrayAsync returned the wrong streamed body length\n",
                    stderr);
                return 0;
            }
            for (index = 0u; index < 262144u; index++) {
                uint8_t expected = (uint8_t)(index % 251u);
                uint8_t actual = *(uint8_t *)cx_array_at(bytes, index,
                    (cx_uint)sizeof(cx_byte));
                if (actual != expected) {
                    fputs("ReadAsByteArrayAsync returned incorrect streamed bytes\n",
                        stderr);
                    return 0;
                }
            }
            repeatedTask = cxnet_System_Net_Http_HttpContent_ReadAsByteArrayAsync(
                content);
            if (repeatedTask != task ||
                !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(
                    repeatedTask)) {
                fputs("ReadAsByteArrayAsync did not retain its buffered result\n",
                    stderr);
                return 0;
            }
        } else if (!string_equals(cx_generic_61b4e22ba70d50cc885a0dac2d8bad266f92222a2650ca64de8edc4d369f0bd9(task),
                "pong")) {
            fputs("ReadAsStringAsync returned incorrect streamed text\n", stderr);
            return 0;
        } else {
            struct cxcore_System_Threading_Tasks_Task *repeatedTask =
                cxnet_System_Net_Http_HttpContent_ReadAsStringAsync(content);
            if (repeatedTask != task ||
                !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(
                    repeatedTask)) {
                fputs("ReadAsStringAsync did not retain its buffered result\n",
                    stderr);
                return 0;
            }
        }
        cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
        stop_fixture(&fixture, thread);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        return 1;
    }
    if (reader_kind == 4) {
        stream = cxnet_System_Net_Http_CurlStreamingContent_ReadAsStream(
            (struct cxnet_System_Net_Http_CurlStreamingContent *)
            cxnet_System_Net_Http_HttpResponseMessage_Content___get(response));
    } else {
        stream = cx_generic_b7a000e5a1e82c2cc28bb7d06efab618c555997e8e29f1d0e05edd0237738424(task);
    }
    if (reader_kind == 3) {
        uint32_t total = 0u;
        if (stream == NULL) {
            fputs("GetStreamAsync returned no large response stream\n", stderr);
            return 0;
        }
        bytes = cx_array_with_type(cx_array_new(8192u, (cx_uint)sizeof(cx_byte)),
            "cx_generic_9c4b0dcf5365274a639812f507806a7475845f1a6e6b8c085532c41d4210ef4b");
        while (total < 262144u) {
            uint32_t copied;
            uint32_t index;
            read_task = cxcore_System_IO_Stream_ReadAsync__2(stream, bytes, 0u, 8192u);
            steps = 0u;
            while (!read_task->_isCompleted && steps < 200000u) {
                if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                    Sleep(1u);
#else
                    usleep(1000u);
#endif
                }
                steps++;
            }
            if (!read_task->_isCompleted || read_task->_isFaulted ||
                read_task->_isCanceled) {
                fputs("large HTTP body read did not complete successfully\n", stderr);
                return 0;
            }
            copied = cx_generic_32f22edd4c9ba40c64ed2658909ec8fb1d6e85d2c4276d46eebd01030eefa80f(
                read_task);
            if (copied == 0u) {
                fputs("large HTTP body ended before all bytes arrived\n", stderr);
                return 0;
            }
            for (index = 0u; index < copied; index++) {
                uint8_t expected = (uint8_t)((total + index) % 251u);
                uint8_t actual = *(uint8_t *)cx_array_at(bytes, index,
                    (cx_uint)sizeof(cx_byte));
                if (actual != expected) {
                    fputs("large HTTP body stream lost byte ordering\n", stderr);
                    return 0;
                }
            }
            total += copied;
        }
        read_task = cxcore_System_IO_Stream_ReadAsync__2(stream, bytes, 0u, 1u);
        steps = 0u;
        while (!read_task->_isCompleted && steps < 200000u) {
            if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                Sleep(1u);
#else
                usleep(1000u);
#endif
            }
            steps++;
        }
        if (!read_task->_isCompleted || read_task->_isFaulted ||
            read_task->_isCanceled ||
            cx_generic_32f22edd4c9ba40c64ed2658909ec8fb1d6e85d2c4276d46eebd01030eefa80f(
                read_task) != 0u) {
            fputs("large HTTP body stream did not report EOF\n", stderr);
            return 0;
        }
        cxnet_System_Net_Http_ResponseOwnedStream_Dispose(
            (struct cxnet_System_Net_Http_ResponseOwnedStream *)stream);
        stop_fixture(&fixture, thread);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        return 1;
    }
    if (stream == NULL || fixture.body_sent) {
        fputs("GetStreamAsync did not return before the response body arrived\n", stderr);
        return 0;
    }
    bytes = cx_array_with_type(cx_array_new(4u, (cx_uint)sizeof(cx_byte)),
        "cx_generic_9c4b0dcf5365274a639812f507806a7475845f1a6e6b8c085532c41d4210ef4b");
    read_cancellation_source =
        (struct cxcore_System_Threading_Tasks_CancellationTokenSource *)
        cxcore_System_Memory_Alloc(
            cxcore_System_Threading_Tasks_CancellationTokenSource___typeinfo.Size);
    cxcore_System_Threading_Tasks_CancellationTokenSource___constructor(
        read_cancellation_source);
    read_cancellation_token =
        cxcore_System_Threading_Tasks_CancellationTokenSource_Token___const_get(
            read_cancellation_source);
    if (reader_kind == 4) {
        read_task = cxnet_System_Net_Http_CurlResponseStream_ReadAsync(
            (struct cxnet_System_Net_Http_CurlResponseStream *)stream,
            bytes, 0u, 4u, read_cancellation_token);
    } else {
        read_task = cxnet_System_Net_Http_ResponseOwnedStream_ReadAsync(
            (struct cxnet_System_Net_Http_ResponseOwnedStream *)stream,
            bytes, 0u, 4u, read_cancellation_token);
    }
    cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(
        read_cancellation_source);
    steps = 0u;
    while (!read_task->_isCompleted && steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (!read_task->_isCompleted || !read_task->_isCanceled || fixture.body_sent) {
        fputs("GetStreamAsync did not cancel an individual pending read\n", stderr);
        return 0;
    }
    read_task = cxcore_System_IO_Stream_ReadAsync__2(stream, bytes, 0u, 4u);
    steps = 0u;
    while (!read_task->_isCompleted && steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (!read_task->_isCompleted || read_task->_isFaulted ||
        read_task->_isCanceled ||
        cx_generic_32f22edd4c9ba40c64ed2658909ec8fb1d6e85d2c4276d46eebd01030eefa80f(
            read_task) != 4u ||
        memcmp(cx_array_at(bytes, 0u, (cx_uint)sizeof(cx_byte)), "pong", 4u) != 0) {
        fputs("GetStreamAsync did not stream the expected body bytes\n", stderr);
        return 0;
    }
    read_task = cxcore_System_IO_Stream_ReadAsync__2(stream, bytes, 0u, 1u);
    steps = 0u;
    while (!read_task->_isCompleted && steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (!read_task->_isCompleted || read_task->_isFaulted ||
        read_task->_isCanceled ||
        cx_generic_32f22edd4c9ba40c64ed2658909ec8fb1d6e85d2c4276d46eebd01030eefa80f(
            read_task) != 0u) {
        fputs("GetStreamAsync did not report the body end cleanly\n", stderr);
        return 0;
    }
    if (reader_kind == 4) {
        cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
    } else {
        cxnet_System_Net_Http_ResponseOwnedStream_Dispose(
            (struct cxnet_System_Net_Http_ResponseOwnedStream *)stream);
    }
    stop_fixture(&fixture, thread);
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    return 1;
}

static int run_direct_curl_handler_send_case(int headersFirst) {
    struct fixture fixture;
    test_thread thread;
    uint16_t port = 0u;
    char url[160];
    struct cxcore_System_Uri *uri;
    struct cxnet_System_Net_Http_CurlHttpMessageHandler *handler;
    struct cxnet_System_Net_Http_HttpRequestMessage *request;
    struct cxnet_System_Net_Http_HttpResponseMessage *response;
    struct cxcore_System_Threading_Tasks_Task *task;
    struct cxcore_System_Collections_Array *body = NULL;
    struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *readTask = NULL;
    struct cxcore_System_Threading_Tasks_CancellationToken cancellationToken = {0};
    const char *path = headersFirst ? "/cx-http-stream" : "/cx-http-string";
    uint32_t steps = 0u;
    if (!start_fixture(&fixture, &port, &thread, 0,
            headersFirst ? "GET /cx-http-stream HTTP/1.1" :
                "GET /cx-http-string HTTP/1.1", NULL, NULL)) {
        fputs("could not start direct curl handler fixture\n", stderr);
        return 0;
    }
    (void)snprintf(url, sizeof(url), "http://localhost:%u%s",
        (unsigned)port, path);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri, make_string(url));
    handler = (struct cxnet_System_Net_Http_CurlHttpMessageHandler *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_CurlHttpMessageHandler___typeinfo.Size);
    cxnet_System_Net_Http_CurlHttpMessageHandler___constructor(handler);
    request = (struct cxnet_System_Net_Http_HttpRequestMessage *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpRequestMessage___typeinfo.Size);
    cxnet_System_Net_Http_HttpRequestMessage___constructor(request,
        cxnet_System_Net_Http_HttpMethod_Get___const_get(), uri);
    if (headersFirst) {
        cxnet_System_Net_Http_HttpRequestMessage_TransportResponseHeadersRead___set(
            request, 1);
    }
    task = cxnet_System_Net_Http_CurlHttpMessageHandler_SendAsync(
        handler, request, cancellationToken);
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
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
    if (fixture.failed || task == NULL ||
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) ||
        task->_isFaulted || task->_isCanceled) {
        fputs("direct CurlHttpMessageHandler.SendAsync did not complete successfully\n",
            stderr);
        return 0;
    }
    response = cx_generic_640110dc1a526c1c94f059cac4ed8d246fe089fbe535a78bc1997c7adde4688b(task);
    if (response == NULL ||
        cxnet_System_Net_Http_HttpResponseMessage_StatusCode___get(response) !=
            cxnet_System_Net_Http_HttpStatusCode_OK) {
        fputs(headersFirst
            ? "direct curl headers-first handler returned an incorrect response\n"
            : "direct curl handler returned an incorrect response\n", stderr);
        return 0;
    }
    if (headersFirst) {
        struct cxnet_System_Net_Http_CurlStreamingContent *content =
            (struct cxnet_System_Net_Http_CurlStreamingContent *)
            cxnet_System_Net_Http_HttpResponseMessage_Content___get(response);
        struct cxcore_System_IO_Stream *stream =
            cxnet_System_Net_Http_CurlStreamingContent_ReadAsStream(content);
        uint32_t readSteps = 0u;
        body = cx_array_with_type(cx_array_new(4u, (cx_uint)sizeof(cx_byte)),
            "cx_generic_9c4b0dcf5365274a639812f507806a7475845f1a6e6b8c085532c41d4210ef4b");
        readTask = cxnet_System_Net_Http_CurlResponseStream_ReadAsync(
            (struct cxnet_System_Net_Http_CurlResponseStream *)stream,
            body, 0u, 4u, cancellationToken);
        while (readTask != NULL && !readTask->_isCompleted &&
            readSteps < 200000u) {
            if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                Sleep(1u);
#else
                usleep(1000u);
#endif
            }
            readSteps++;
        }
        if (readTask == NULL || !readTask->_isCompleted ||
            readTask->_isFaulted || readTask->_isCanceled ||
            cx_generic_32f22edd4c9ba40c64ed2658909ec8fb1d6e85d2c4276d46eebd01030eefa80f(readTask) != 4u ||
            memcmp(cx_array_at(body, 0u, (cx_uint)sizeof(cx_byte)), "pong", 4u) != 0) {
            fputs("direct curl headers-first response stream returned the wrong body\n",
                stderr);
            return 0;
        }
    } else if (!string_equals(cxnet_System_Net_Http_HttpContent_ReadAsString(
            cxnet_System_Net_Http_HttpResponseMessage_Content___get(response)),
            "pong")) {
        fputs("direct curl handler returned an incorrect response body\n", stderr);
        return 0;
    }
    stop_fixture(&fixture, thread);
    cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
    cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
    cxnet_System_Net_Http_CurlHttpMessageHandler_Dispose(handler);
    return 1;
}

static int run_direct_curl_handler_cancellation_case(void) {
    struct fixture fixture;
    test_thread thread;
    uint16_t port = 0u;
    char url[160];
    struct cxcore_System_Uri *uri;
    struct cxnet_System_Net_Http_CurlHttpMessageHandler *handler;
    struct cxnet_System_Net_Http_HttpRequestMessage *request;
    struct cxcore_System_Threading_Tasks_CancellationTokenSource *source;
    struct cxcore_System_Threading_Tasks_CancellationToken cancellationToken;
    struct cxcore_System_Threading_Tasks_Task *task;
    cx_action cancelAction = {0};
    uint32_t steps = 0u;
    if (!start_fixture(&fixture, &port, &thread, 1,
            "GET /cx-http-cancel HTTP/1.1", NULL, NULL)) {
        fputs("could not start direct curl handler cancellation fixture\n", stderr);
        return 0;
    }
    (void)snprintf(url, sizeof(url), "http://localhost:%u/cx-http-cancel",
        (unsigned)port);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri, make_string(url));
    handler = (struct cxnet_System_Net_Http_CurlHttpMessageHandler *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_CurlHttpMessageHandler___typeinfo.Size);
    cxnet_System_Net_Http_CurlHttpMessageHandler___constructor(handler);
    request = (struct cxnet_System_Net_Http_HttpRequestMessage *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpRequestMessage___typeinfo.Size);
    cxnet_System_Net_Http_HttpRequestMessage___constructor(request,
        cxnet_System_Net_Http_HttpMethod_Get___const_get(), uri);
    source = (struct cxcore_System_Threading_Tasks_CancellationTokenSource *)
        cxcore_System_Memory_Alloc(
            cxcore_System_Threading_Tasks_CancellationTokenSource___typeinfo.Size);
    cxcore_System_Threading_Tasks_CancellationTokenSource___constructor(source);
    cancellationToken =
        cxcore_System_Threading_Tasks_CancellationTokenSource_Token___const_get(source);
    task = cxnet_System_Net_Http_CurlHttpMessageHandler_SendAsync(
        handler, request, cancellationToken);
    while (!fixture.request_received &&
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
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
    if (!fixture.request_received) {
        cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(source);
        cxnet_System_Net_Http_CurlHttpMessageHandler_Dispose(handler);
        while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
            steps < 400000u) {
            if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                Sleep(1u);
#else
                usleep(1000u);
#endif
            }
            steps++;
        }
        wake_fixture_accept(port);
        stop_fixture(&fixture, thread);
        fprintf(stderr,
            "direct curl handler did not reach the fixture before cancellation: failed=%d completed=%d canceled=%d faulted=%d\n",
            fixture.failed,
            cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task),
            task == NULL ? -1 : (int)task->_isCanceled,
            task == NULL ? -1 : (int)task->_isFaulted);
        cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
        return 0;
    }
    cancelAction.__function = cancel_token_on_scheduler;
    cancelAction.__environment = source;
    cancelAction.__target = (cx_ptr)source;
    cxcore_System_Threading_Tasks_TaskScheduler_PostAfter(50u, cancelAction);
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        steps < 400000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    stop_fixture(&fixture, thread);
    if (fixture.failed || !fixture.client_closed || task == NULL ||
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) ||
        !cxcore_System_Threading_Tasks_Task_IsCanceled___const_get(task) ||
        !cxnet_System_Net_Http_CurlHttpTransport_WasCanceled(task)) {
        fputs("direct curl handler did not cancel and close its in-flight request\n",
            stderr);
        return 0;
    }
    cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
    cxnet_System_Net_Http_CurlHttpMessageHandler_Dispose(handler);
    return 1;
}

static int run_direct_curl_handler_dispose_during_send_case(void) {
    struct fixture fixture;
    test_thread thread;
    uint16_t port = 0u;
    char url[160];
    struct cxcore_System_Uri *uri = NULL;
    struct cxnet_System_Net_Http_CurlHttpMessageHandler *handler = NULL;
    struct cxnet_System_Net_Http_HttpRequestMessage *request = NULL;
    struct cxnet_System_Net_Http_HttpRequestMessage * volatile rejectedRequest = NULL;
    struct cxnet_System_Net_Http_HttpResponseMessage * volatile response = NULL;
    struct cxcore_System_Threading_Tasks_Task *task = NULL;
    struct cxcore_System_Threading_Tasks_Task * volatile rejectedTask = NULL;
    struct cxcore_System_Threading_Tasks_CancellationToken cancellationToken = {0};
    volatile int disposedSendRejected = 0;
    volatile uint32_t steps = 0u;
    int fixtureStarted = 0;
    volatile int passed = 0;

    if (!start_fixture(&fixture, &port, &thread, 0,
            "GET /cx-http-string HTTP/1.1", NULL, NULL)) {
        fputs("could not start direct curl handler disposal fixture\n", stderr);
        return 0;
    }
    fixtureStarted = 1;
    fixture.hold_response = 1;
    (void)snprintf(url, sizeof(url), "http://localhost:%u/cx-http-string",
        (unsigned)port);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri, make_string(url));
    handler = (struct cxnet_System_Net_Http_CurlHttpMessageHandler *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_CurlHttpMessageHandler___typeinfo.Size);
    cxnet_System_Net_Http_CurlHttpMessageHandler___constructor(handler);
    request = (struct cxnet_System_Net_Http_HttpRequestMessage *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpRequestMessage___typeinfo.Size);
    cxnet_System_Net_Http_HttpRequestMessage___constructor(request,
        cxnet_System_Net_Http_HttpMethod_Get___const_get(), uri);
    task = cxnet_System_Net_Http_CurlHttpMessageHandler_SendAsync(
        handler, request, cancellationToken);
    while (!fixture.request_received &&
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
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
    if (!fixture.request_received || task == NULL ||
        cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) ||
        fixture.failed) {
        fputs("direct curl handler send was not held before disposal\n", stderr);
        goto cleanup;
    }

    cxnet_System_Net_Http_CurlHttpMessageHandler_Dispose(handler);
    rejectedRequest = (struct cxnet_System_Net_Http_HttpRequestMessage *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpRequestMessage___typeinfo.Size);
    cxnet_System_Net_Http_HttpRequestMessage___constructor(rejectedRequest,
        cxnet_System_Net_Http_HttpMethod_Get___const_get(), uri);
    {
        struct cx_exception_frame frame;
        struct cxcore_System_Exception * volatile exception = NULL;
        cx_exception_push(&frame);
        if (setjmp(frame.environment) == 0) {
            rejectedTask = cxnet_System_Net_Http_CurlHttpMessageHandler_SendAsync(
                handler, rejectedRequest, cancellationToken);
            cx_exception_pop(&frame);
        } else {
            exception = (struct cxcore_System_Exception *)
                cx_exception_current();
            cx_exception_pop(&frame);
            cx_exception_clear();
        }
        disposedSendRejected = rejectedTask == NULL && exception != NULL &&
            cxcore_System_Exception_Matches(exception,
                cxcore_System_ObjectDisposedException___typeinfo);
    }
    fixture.release_response = 1;
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        steps < 400000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    stop_fixture(&fixture, thread);
    fixtureStarted = 0;
    if (!disposedSendRejected || fixture.failed || task == NULL ||
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) ||
        task->_isFaulted || task->_isCanceled) {
        fputs("disposing the handler disrupted or admitted a send after disposal\n",
            stderr);
        goto cleanup;
    }
    response = cx_generic_640110dc1a526c1c94f059cac4ed8d246fe089fbe535a78bc1997c7adde4688b(task);
    if (response == NULL ||
        cxnet_System_Net_Http_HttpResponseMessage_StatusCode___get(response) !=
            cxnet_System_Net_Http_HttpStatusCode_OK ||
        !string_equals(cxnet_System_Net_Http_HttpContent_ReadAsString(
            cxnet_System_Net_Http_HttpResponseMessage_Content___get(response)),
            "pong")) {
        fputs("in-flight direct send did not survive handler disposal\n", stderr);
        goto cleanup;
    }
    passed = 1;

cleanup:
    if (fixtureStarted) {
        fixture.release_response = 1;
        stop_fixture(&fixture, thread);
    }
    if (response != NULL)
        cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
    if (rejectedRequest != NULL)
        cxnet_System_Net_Http_HttpRequestMessage_Dispose(rejectedRequest);
    if (request != NULL)
        cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
    if (handler != NULL)
        cxnet_System_Net_Http_CurlHttpMessageHandler_Dispose(handler);
    return passed;
}

static int run_direct_curl_handler_dispose_during_body_read_case(void) {
    struct fixture fixture;
    test_thread thread;
    uint16_t port = 0u;
    char url[160];
    struct cxcore_System_Uri *uri = NULL;
    struct cxnet_System_Net_Http_CurlHttpMessageHandler *handler = NULL;
    struct cxnet_System_Net_Http_HttpRequestMessage *request = NULL;
    struct cxnet_System_Net_Http_HttpResponseMessage *response = NULL;
    struct cxcore_System_Threading_Tasks_Task *sendTask = NULL;
    struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *readTask = NULL;
    struct cxcore_System_IO_Stream *stream = NULL;
    struct cxcore_System_Collections_Array *body = NULL;
    struct cxcore_System_Threading_Tasks_CancellationToken cancellationToken = {0};
    uint32_t steps = 0u;
    int fixtureStarted = 0;
    int passed = 0;

    if (!start_fixture(&fixture, &port, &thread, 0,
            "GET /cx-http-stream HTTP/1.1", NULL, NULL)) {
        fputs("could not start direct curl handler body-read disposal fixture\n",
            stderr);
        return 0;
    }
    fixtureStarted = 1;
    fixture.hold_stream_body = 1;
    (void)snprintf(url, sizeof(url), "http://localhost:%u/cx-http-stream",
        (unsigned)port);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri, make_string(url));
    handler = (struct cxnet_System_Net_Http_CurlHttpMessageHandler *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_CurlHttpMessageHandler___typeinfo.Size);
    cxnet_System_Net_Http_CurlHttpMessageHandler___constructor(handler);
    request = (struct cxnet_System_Net_Http_HttpRequestMessage *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpRequestMessage___typeinfo.Size);
    cxnet_System_Net_Http_HttpRequestMessage___constructor(request,
        cxnet_System_Net_Http_HttpMethod_Get___const_get(), uri);
    cxnet_System_Net_Http_HttpRequestMessage_TransportResponseHeadersRead___set(
        request, 1);
    sendTask = cxnet_System_Net_Http_CurlHttpMessageHandler_SendAsync(
        handler, request, cancellationToken);
    while ((!fixture.request_received ||
            !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(sendTask)) &&
        steps < 10000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (fixture.failed || !fixture.request_received || sendTask == NULL ||
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(sendTask) ||
        sendTask->_isFaulted || sendTask->_isCanceled) {
        fputs("direct handler did not return headers before the held body\n",
            stderr);
        goto cleanup;
    }
    response = cx_generic_640110dc1a526c1c94f059cac4ed8d246fe089fbe535a78bc1997c7adde4688b(
        sendTask);
    if (response == NULL ||
        cxnet_System_Net_Http_HttpResponseMessage_StatusCode___get(response) !=
            cxnet_System_Net_Http_HttpStatusCode_OK) {
        fputs("direct handler returned an invalid headers-first response\n",
            stderr);
        goto cleanup;
    }
    stream = cxnet_System_Net_Http_CurlStreamingContent_ReadAsStream(
        (struct cxnet_System_Net_Http_CurlStreamingContent *)
            cxnet_System_Net_Http_HttpResponseMessage_Content___get(response));
    body = cx_array_with_type(cx_array_new(4u, (cx_uint)sizeof(cx_byte)),
        "cx_generic_9c4b0dcf5365274a639812f507806a7475845f1a6e6b8c085532c41d4210ef4b");
    readTask = cxnet_System_Net_Http_CurlResponseStream_ReadAsync(
        (struct cxnet_System_Net_Http_CurlResponseStream *)stream,
        body, 0u, 4u, cancellationToken);
    if (readTask == NULL || readTask->_isCompleted) {
        fputs("held direct response body did not leave a read pending\n",
            stderr);
        goto cleanup;
    }
    cxnet_System_Net_Http_CurlHttpMessageHandler_Dispose(handler);
    fixture.release_stream_body = 1;
    steps = 0u;
    while ((!readTask->_isCompleted ||
            (!fixture.body_sent && !fixture.failed)) && steps < 10000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    stop_fixture(&fixture, thread);
    fixtureStarted = 0;
    if (fixture.failed || !fixture.body_sent || !readTask->_isCompleted ||
        readTask->_isFaulted || readTask->_isCanceled ||
        cx_generic_32f22edd4c9ba40c64ed2658909ec8fb1d6e85d2c4276d46eebd01030eefa80f(
            readTask) != 4u ||
        memcmp(cx_array_at(body, 0u, (cx_uint)sizeof(cx_byte)), "pong", 4u) != 0) {
        fputs("handler disposal disrupted the live response body read\n", stderr);
        goto cleanup;
    }
    passed = 1;

cleanup:
    if (fixtureStarted) {
        fixture.release_stream_body = 1;
        stop_fixture(&fixture, thread);
    }
    if (response != NULL)
        cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
    if (request != NULL)
        cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
    if (handler != NULL)
        cxnet_System_Net_Http_CurlHttpMessageHandler_Dispose(handler);
    return passed;
}

static int run_streaming_backpressure_case(void) {
    static const uint32_t body_length = 16777216u;
    struct fixture fixture;
    test_thread thread;
    uint16_t port = 0u;
    char url[160];
    struct cxcore_System_Uri *uri;
    struct cxnet_System_Net_Http_HttpClient *client;
    struct cxnet_System_Net_Http_HttpRequestMessage *request;
    struct cxcore_System_Threading_Tasks_Task *task;
    struct cxnet_System_Net_Http_HttpResponseMessage *response;
    struct cxcore_System_IO_Stream *stream;
    struct cxcore_System_Collections_Array *bytes;
    struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *read_task;
    struct cx_gc_root_handle *uri_root;
    struct cx_gc_root_handle *client_root;
    struct cx_gc_root_handle *request_root;
    struct cx_gc_root_handle *task_root;
    struct cx_gc_root_handle *response_root;
    struct cx_gc_root_handle *stream_root;
    struct cx_gc_root_handle *bytes_root;
    struct cx_gc_root_handle *read_task_root = NULL;
    uint32_t total = 0u;
    uint32_t steps = 0u;
    if (!start_fixture(&fixture, &port, &thread, 0,
            "GET /cx-http-stream-backpressure HTTP/1.1", NULL, NULL)) {
        fputs("could not start HTTP backpressure fixture\n", stderr);
        return 0;
    }
    (void)snprintf(url, sizeof(url), "http://localhost:%u/cx-http-stream-backpressure",
        (unsigned)port);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    uri_root = cx_gc_add_root(&uri, (cx_uint)sizeof(uri));
    cxcore_System_Uri___constructor(uri, make_string(url));
    client = (struct cxnet_System_Net_Http_HttpClient *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    client_root = cx_gc_add_root(&client, (cx_uint)sizeof(client));
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    request = (struct cxnet_System_Net_Http_HttpRequestMessage *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpRequestMessage___typeinfo.Size);
    request_root = cx_gc_add_root(&request, (cx_uint)sizeof(request));
    cxnet_System_Net_Http_HttpRequestMessage___constructor(request,
        cxnet_System_Net_Http_HttpMethod_Get___const_get(), uri);
    task = cxnet_System_Net_Http_HttpClient_SendAsync__4(client, request,
        cxnet_System_Net_Http_HttpCompletionOption_ResponseHeadersRead);
    task_root = cx_gc_add_root(&task, (cx_uint)sizeof(task));
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
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
    if (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) ||
        task->_isFaulted || task->_isCanceled) {
        fputs("backpressure response headers did not arrive\n", stderr);
        cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        stop_fixture(&fixture, thread);
        return 0;
    }
    response = cx_generic_640110dc1a526c1c94f059cac4ed8d246fe089fbe535a78bc1997c7adde4688b(task);
    response_root = cx_gc_add_root(&response, (cx_uint)sizeof(response));
    stream = response == NULL ? NULL :
        cxnet_System_Net_Http_CurlStreamingContent_ReadAsStream(
            (struct cxnet_System_Net_Http_CurlStreamingContent *)
            cxnet_System_Net_Http_HttpResponseMessage_Content___get(response));
    stream_root = cx_gc_add_root(&stream, (cx_uint)sizeof(stream));
    if (stream == NULL) {
        fputs("backpressure response returned no stream\n", stderr);
        if (response != NULL) {
            cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
        }
        cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        stop_fixture(&fixture, thread);
        return 0;
    }
    /* Let the transport run while its consumer is paused. */
    for (steps = 0u; steps < 400u; steps++) {
#ifdef _WIN32
        Sleep(1u);
#else
        usleep(1000u);
#endif
    }
    if (fixture.body_bytes_sent <= 16384u ||
        fixture.body_bytes_sent >= body_length || fixture.body_sent) {
        fprintf(stderr, "paused response did not apply backpressure (sent %lu bytes)\n",
            (unsigned long)fixture.body_bytes_sent);
        cxnet_System_Net_Http_CurlResponseStream_Dispose(
            (struct cxnet_System_Net_Http_CurlResponseStream *)stream);
        stop_fixture(&fixture, thread);
        cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
        cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        return 0;
    }
    bytes = cx_array_with_type(cx_array_new(8192u, (cx_uint)sizeof(cx_byte)),
        "cx_generic_9c4b0dcf5365274a639812f507806a7475845f1a6e6b8c085532c41d4210ef4b");
    bytes_root = cx_gc_add_root(&bytes, (cx_uint)sizeof(bytes));
    while (total < body_length) {
        uint32_t copied;
        uint32_t index;
        read_task = cxcore_System_IO_Stream_ReadAsync__2(stream, bytes, 0u, 8192u);
        if (read_task_root == NULL) {
            read_task_root = cx_gc_add_root(&read_task,
                (cx_uint)sizeof(read_task));
        }
        steps = 0u;
        while (!read_task->_isCompleted && steps < 200000u) {
            if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
                Sleep(1u);
#else
                usleep(1000u);
#endif
            }
            steps++;
        }
        if (!read_task->_isCompleted || read_task->_isFaulted || read_task->_isCanceled) {
            fprintf(stderr, "resumed backpressure stream read failed at %lu bytes\n",
                (unsigned long)total);
            if (read_task->_exception != NULL) {
                struct cxcore_System_String *message =
                    cxcore_System_Exception_Message___const_get(read_task->_exception);
                if (message != NULL) fprintf(stderr, "stream read exception: %.*s\n",
                    (int)message->_length, (char *)message->_data);
            }
            cxnet_System_Net_Http_CurlResponseStream_Dispose(
                (struct cxnet_System_Net_Http_CurlResponseStream *)stream);
            stop_fixture(&fixture, thread);
            cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
            cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
            cxnet_System_Net_Http_HttpClient_Dispose(client);
            return 0;
        }
        copied = cx_generic_32f22edd4c9ba40c64ed2658909ec8fb1d6e85d2c4276d46eebd01030eefa80f(read_task);
        if (copied == 0u || copied > body_length - total) {
            fputs("resumed backpressure stream ended at the wrong length\n", stderr);
            cxnet_System_Net_Http_CurlResponseStream_Dispose(
                (struct cxnet_System_Net_Http_CurlResponseStream *)stream);
            stop_fixture(&fixture, thread);
            cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
            cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
            cxnet_System_Net_Http_HttpClient_Dispose(client);
            return 0;
        }
        for (index = 0u; index < copied; index++) {
            uint8_t expected = (uint8_t)((total + index) % 251u);
            uint8_t actual = *(uint8_t *)cx_array_at(bytes, index,
                (cx_uint)sizeof(cx_byte));
            if (actual != expected) {
                fputs("resumed backpressure stream body bytes were corrupted\n", stderr);
                cxnet_System_Net_Http_CurlResponseStream_Dispose(
                    (struct cxnet_System_Net_Http_CurlResponseStream *)stream);
                stop_fixture(&fixture, thread);
                cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
                cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
                cxnet_System_Net_Http_HttpClient_Dispose(client);
                return 0;
            }
        }
        total += copied;
    }
    read_task = cxcore_System_IO_Stream_ReadAsync__2(stream, bytes, 0u, 1u);
    if (read_task_root == NULL) {
        read_task_root = cx_gc_add_root(&read_task, (cx_uint)sizeof(read_task));
    }
    steps = 0u;
    while (!read_task->_isCompleted && steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (!read_task->_isCompleted || read_task->_isFaulted || read_task->_isCanceled ||
        cx_generic_32f22edd4c9ba40c64ed2658909ec8fb1d6e85d2c4276d46eebd01030eefa80f(
            read_task) != 0u) {
        fputs("resumed backpressure stream did not reach EOF\n", stderr);
        cxnet_System_Net_Http_CurlResponseStream_Dispose(
            (struct cxnet_System_Net_Http_CurlResponseStream *)stream);
        stop_fixture(&fixture, thread);
        cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
        cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        return 0;
    }
    cxnet_System_Net_Http_CurlResponseStream_Dispose(
        (struct cxnet_System_Net_Http_CurlResponseStream *)stream);
    stop_fixture(&fixture, thread);
    cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
    cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    if (fixture.failed || fixture.body_bytes_sent != body_length || !fixture.body_sent) {
        fputs("backpressure fixture did not complete the full response\n", stderr);
        return 0;
    }
    cx_gc_remove_root(read_task_root);
    cx_gc_remove_root(bytes_root);
    cx_gc_remove_root(stream_root);
    cx_gc_remove_root(response_root);
    cx_gc_remove_root(task_root);
    cx_gc_remove_root(request_root);
    cx_gc_remove_root(client_root);
    cx_gc_remove_root(uri_root);
    return 1;
}

static int run_streaming_truncated_body_case(void) {
    struct fixture fixture;
    test_thread thread;
    uint16_t port = 0u;
    char url[160];
    struct cxcore_System_Uri *uri;
    struct cxnet_System_Net_Http_HttpClient *client;
    struct cxcore_System_Threading_Tasks_Task *task;
    struct cxcore_System_IO_Stream *stream;
    struct cxcore_System_Collections_Array *bytes;
    struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *read_task;
    uint32_t steps = 0u;
    if (!start_fixture(&fixture, &port, &thread, 0,
            "GET /cx-http-truncated-body HTTP/1.1", NULL, NULL)) {
        fputs("could not start truncated streaming body fixture\n", stderr);
        return 0;
    }
    (void)snprintf(url, sizeof(url), "http://localhost:%u/cx-http-truncated-body",
        (unsigned)port);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri, make_string(url));
    client = (struct cxnet_System_Net_Http_HttpClient *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    task = cxnet_System_Net_Http_HttpClient_GetStreamAsync__2(client, uri);
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
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
    if (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) ||
        task->_isFaulted || task->_isCanceled) {
        fputs("headers-first request did not complete before body framing failure\n", stderr);
        return 0;
    }
    stream = cx_generic_b7a000e5a1e82c2cc28bb7d06efab618c555997e8e29f1d0e05edd0237738424(task);
    bytes = cx_array_with_type(cx_array_new(16u, (cx_uint)sizeof(cx_byte)),
        "cx_generic_9c4b0dcf5365274a639812f507806a7475845f1a6e6b8c085532c41d4210ef4b");
    read_task = cxcore_System_IO_Stream_ReadAsync__2(stream, bytes, 0u, 10u);
    steps = 0u;
    while (!read_task->_isCompleted && steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (!read_task->_isCompleted || read_task->_isFaulted || read_task->_isCanceled ||
        cx_generic_32f22edd4c9ba40c64ed2658909ec8fb1d6e85d2c4276d46eebd01030eefa80f(
            read_task) != 4u ||
        memcmp(cx_array_at(bytes, 0u, (cx_uint)sizeof(cx_byte)), "part", 4u) != 0) {
        fputs("streaming truncated body lost bytes delivered before EOF\n", stderr);
        return 0;
    }
    read_task = cxcore_System_IO_Stream_ReadAsync__2(stream, bytes, 0u, 1u);
    steps = 0u;
    while (!read_task->_isCompleted && steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (!read_task->_isCompleted || !read_task->_isFaulted || read_task->_isCanceled) {
        fputs("streaming truncated body did not fault the next read\n", stderr);
        return 0;
    }
    cxnet_System_Net_Http_ResponseOwnedStream_Dispose(
        (struct cxnet_System_Net_Http_ResponseOwnedStream *)stream);
    stop_fixture(&fixture, thread);
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    return 1;
}

static int run_streaming_request_cancellation_case(void) {
    struct fixture fixture;
    test_thread thread;
    uint16_t port = 0u;
    char url[160];
    struct cxcore_System_Uri *uri;
    struct cxnet_System_Net_Http_HttpClient *client;
    struct cxnet_System_Net_Http_HttpRequestMessage *request;
    struct cxnet_System_Net_Http_HttpResponseMessage *response;
    struct cxnet_System_Net_Http_CurlStreamingContent *content;
    struct cxcore_System_IO_Stream *stream;
    struct cxcore_System_Collections_Array *bytes;
    struct cxcore_System_Threading_Tasks_CancellationTokenSource *source;
    struct cxcore_System_Threading_Tasks_CancellationToken token;
    struct cxcore_System_Threading_Tasks_Task *response_task;
    struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *read_task;
    uint32_t steps = 0u;

    if (!start_fixture(&fixture, &port, &thread, 0,
            "GET /cx-http-stream-cancel HTTP/1.1", NULL, NULL)) {
        fputs("could not start request-canceled response stream fixture\n", stderr);
        return 0;
    }
    (void)snprintf(url, sizeof(url), "http://localhost:%u/cx-http-stream-cancel",
        (unsigned)port);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri, make_string(url));
    client = (struct cxnet_System_Net_Http_HttpClient *)cxcore_System_Memory_Alloc(
        cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    source = (struct cxcore_System_Threading_Tasks_CancellationTokenSource *)
        cxcore_System_Memory_Alloc(
            cxcore_System_Threading_Tasks_CancellationTokenSource___typeinfo.Size);
    cxcore_System_Threading_Tasks_CancellationTokenSource___constructor(source);
    token = cxcore_System_Threading_Tasks_CancellationTokenSource_Token___const_get(source);
    request = (struct cxnet_System_Net_Http_HttpRequestMessage *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpRequestMessage___typeinfo.Size);
    cxnet_System_Net_Http_HttpRequestMessage___constructor(request,
        cxnet_System_Net_Http_HttpMethod_Get___const_get(), uri);
    response_task = cxnet_System_Net_Http_HttpClient_SendAsync__3(client, request,
        cxnet_System_Net_Http_HttpCompletionOption_ResponseHeadersRead, token);
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(response_task) &&
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
    if (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(response_task) ||
        response_task->_isFaulted || response_task->_isCanceled) {
        fputs("headers-first request did not return its live response\n", stderr);
        return 0;
    }
    response = cx_generic_640110dc1a526c1c94f059cac4ed8d246fe089fbe535a78bc1997c7adde4688b(
        response_task);
    content = (struct cxnet_System_Net_Http_CurlStreamingContent *)
        cxnet_System_Net_Http_HttpResponseMessage_Content___get(response);
    stream = cxnet_System_Net_Http_CurlStreamingContent_ReadAsStream(content);
    bytes = cx_array_with_type(cx_array_new(8u, (cx_uint)sizeof(cx_byte)),
        "cx_generic_9c4b0dcf5365274a639812f507806a7475845f1a6e6b8c085532c41d4210ef4b");
    read_task = cxcore_System_IO_Stream_ReadAsync__2(stream, bytes, 0u, 8u);
    steps = 0u;
    while (!read_task->_isCompleted && steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (!read_task->_isCompleted || read_task->_isFaulted || read_task->_isCanceled ||
        cx_generic_32f22edd4c9ba40c64ed2658909ec8fb1d6e85d2c4276d46eebd01030eefa80f(
            read_task) != 4u ||
        memcmp(cx_array_at(bytes, 0u, (cx_uint)sizeof(cx_byte)), "pong", 4u) != 0) {
        fputs("request-cancellation fixture did not deliver the body prefix\n", stderr);
        return 0;
    }
    read_task = cxcore_System_IO_Stream_ReadAsync__2(stream, bytes, 0u, 4u);
    cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(source);
    steps = 0u;
    while (!read_task->_isCompleted && steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (!read_task->_isCompleted || !read_task->_isCanceled) {
        fputs("request cancellation did not cancel the pending body read\n", stderr);
        return 0;
    }
    read_task = cxcore_System_IO_Stream_ReadAsync__2(stream, bytes, 0u, 4u);
    if (!read_task->_isCompleted || !read_task->_isCanceled) {
        fputs("a body read after request cancellation looked like clean EOF\n", stderr);
        return 0;
    }
    steps = 0u;
    while (!fixture.client_closed && steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    stop_fixture(&fixture, thread);
    if (fixture.failed || !fixture.client_closed || fixture.body_sent == 0) {
        fputs("request cancellation did not close the active body transfer\n", stderr);
        return 0;
    }
    cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
    cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    return 1;
}

static int run_headers_first_client_dispose_case(void) {
    struct fixture fixture;
    test_thread thread;
    uint16_t port = 0u;
    char url[160];
    struct cxcore_System_Uri *uri;
    struct cxnet_System_Net_Http_HttpClient *client;
    struct cxnet_System_Net_Http_HttpRequestMessage *request;
    struct cxnet_System_Net_Http_HttpResponseMessage *response = NULL;
    struct cxnet_System_Net_Http_CurlStreamingContent *content;
    struct cxcore_System_IO_Stream *stream;
    struct cxcore_System_Collections_Array *bytes;
    struct cxcore_System_Threading_Tasks_Task *response_task;
    struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *read_task;
    uint32_t steps = 0u;

    if (!start_fixture(&fixture, &port, &thread, 0,
            "GET /cx-http-stream-cancel HTTP/1.1", NULL, NULL)) {
        fputs("could not start headers-first client-disposal fixture\n", stderr);
        return 0;
    }
    (void)snprintf(url, sizeof(url), "http://localhost:%u/cx-http-stream-cancel",
        (unsigned)port);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri, make_string(url));
    client = (struct cxnet_System_Net_Http_HttpClient *)cxcore_System_Memory_Alloc(
        cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    request = (struct cxnet_System_Net_Http_HttpRequestMessage *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpRequestMessage___typeinfo.Size);
    cxnet_System_Net_Http_HttpRequestMessage___constructor(request,
        cxnet_System_Net_Http_HttpMethod_Get___const_get(), uri);
    response_task = cxnet_System_Net_Http_HttpClient_SendAsync__3(client, request,
        cxnet_System_Net_Http_HttpCompletionOption_ResponseHeadersRead,
        cxcore_System_Threading_Tasks_CancellationToken_None___const_get());
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(response_task) &&
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
    if (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(response_task) ||
        response_task->_isFaulted || response_task->_isCanceled) {
        fputs("headers-first client-disposal request did not return its response\n",
            stderr);
        goto cleanup;
    }
    response = cx_generic_640110dc1a526c1c94f059cac4ed8d246fe089fbe535a78bc1997c7adde4688b(
        response_task);
    if (response == NULL) {
        fputs("headers-first client-disposal response was null\n", stderr);
        goto cleanup;
    }
    content = (struct cxnet_System_Net_Http_CurlStreamingContent *)
        cxnet_System_Net_Http_HttpResponseMessage_Content___get(response);
    stream = cxnet_System_Net_Http_CurlStreamingContent_ReadAsStream(content);
    if (stream == NULL) {
        fputs("headers-first client-disposal response had no stream\n", stderr);
        goto cleanup;
    }
    bytes = cx_array_with_type(cx_array_new(8u, (cx_uint)sizeof(cx_byte)),
        "cx_generic_9c4b0dcf5365274a639812f507806a7475845f1a6e6b8c085532c41d4210ef4b");
    read_task = cxcore_System_IO_Stream_ReadAsync__2(stream, bytes, 0u, 4u);
    steps = 0u;
    while (!read_task->_isCompleted && steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (!read_task->_isCompleted || read_task->_isFaulted || read_task->_isCanceled ||
        cx_generic_32f22edd4c9ba40c64ed2658909ec8fb1d6e85d2c4276d46eebd01030eefa80f(
            read_task) != 4u ||
        memcmp(cx_array_at(bytes, 0u, (cx_uint)sizeof(cx_byte)), "pong", 4u) != 0) {
        fputs("client-disposal fixture did not deliver its body prefix\n", stderr);
        goto cleanup;
    }
    read_task = cxcore_System_IO_Stream_ReadAsync__2(stream, bytes, 0u, 4u);
    if (read_task->_isCompleted) {
        fputs("client-disposal fixture did not leave a body read pending\n", stderr);
        goto cleanup;
    }
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    steps = 0u;
    while ((!read_task->_isCompleted || !fixture.client_closed) && steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (fixture.failed || !fixture.client_closed || !read_task->_isCompleted ||
        !read_task->_isCanceled) {
        fprintf(stderr,
            "headers-first client disposal failed fixture=%d closed=%d completed=%d canceled=%d\n",
            fixture.failed, fixture.client_closed, read_task->_isCompleted,
            read_task->_isCanceled);
        goto cleanup;
    }
    cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
    cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    stop_fixture(&fixture, thread);
    return 1;

cleanup:
    if (response != NULL)
        cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
    cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    stop_fixture(&fixture, thread);
    return 0;
}

#ifdef _WIN32
static DWORD WINAPI curl_response_stream_dispose_worker(void *argument) {
#else
static void *curl_response_stream_dispose_worker(void *argument) {
#endif
    struct curl_response_stream_dispose_race *race =
        (struct curl_response_stream_dispose_race *)argument;
    struct cxnet_System_Net_Http_CurlResponseStream *stream = race->stream;
    struct cx_gc_root_handle *race_root = NULL;
    struct cx_gc_root_handle *stream_root = NULL;
    cx_bool attached = cx_gc_enter_thread();
    if (!attached) goto completed;
    race_root = cx_gc_add_root(&race, (cx_uint)sizeof(race));
    stream_root = cx_gc_add_root(&stream, (cx_uint)sizeof(stream));
    if (race_root == NULL || stream_root == NULL) {
        request_race_increment(&race->failed);
        goto cleanup;
    }
    request_race_increment(&race->ready);
    while (!request_race_load(&race->start)) {
#ifdef _WIN32
        Sleep(0u);
#else
        usleep(100u);
#endif
    }
    {
        struct cx_exception_frame frame;
        cx_exception_push(&frame);
        if (setjmp(frame.environment) == 0) {
            cxnet_System_Net_Http_CurlResponseStream_Dispose(stream);
            cx_exception_pop(&frame);
            request_race_store(&race->disposed, 1);
        } else {
            struct cxcore_System_Exception *exception =
                (struct cxcore_System_Exception *)cx_exception_current();
            const struct cxcore_System_String *message = exception == NULL
                ? NULL : cxcore_System_Exception_Message___const_get(exception);
            cx_exception_pop(&frame);
            cx_exception_clear();
            if (message != NULL) {
                fprintf(stderr, "curl response-stream dispose threw: %.*s\n",
                    (int)message->_length, (char *)message->_data);
            }
            request_race_increment(&race->failed);
        }
    }
cleanup:
    if (stream_root != NULL) cx_gc_remove_root(stream_root);
    if (race_root != NULL) cx_gc_remove_root(race_root);
    cx_gc_leave_thread(attached);
completed:
    request_race_increment(&race->completed);
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

static int start_curl_response_stream_dispose_worker(test_thread *thread,
    struct curl_response_stream_dispose_race *race) {
#ifdef _WIN32
    *thread = CreateThread(NULL, 0, curl_response_stream_dispose_worker,
        race, 0, NULL);
    return *thread != NULL;
#else
    return pthread_create(thread, NULL, curl_response_stream_dispose_worker,
        race) == 0;
#endif
}

static int run_curl_response_stream_dispose_read_race_case(void) {
    struct fixture fixture;
    test_thread server_thread;
    test_thread dispose_thread;
    uint16_t port = 0u;
    char url[160];
    struct cxcore_System_Uri *uri = NULL;
    struct cxnet_System_Net_Http_HttpClient *client = NULL;
    struct cxnet_System_Net_Http_HttpRequestMessage *request = NULL;
    struct cxnet_System_Net_Http_HttpResponseMessage *response = NULL;
    struct cxnet_System_Net_Http_CurlStreamingContent *content = NULL;
    struct cxnet_System_Net_Http_CurlResponseStream *stream = NULL;
    struct cxcore_System_Collections_Array *bytes = NULL;
    struct cxcore_System_Threading_Tasks_Task *response_task = NULL;
    struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *read_task = NULL;
    struct curl_response_stream_dispose_race race;
    struct cx_gc_root_handle *uri_root = NULL;
    struct cx_gc_root_handle *client_root = NULL;
    struct cx_gc_root_handle *request_root = NULL;
    struct cx_gc_root_handle *response_root = NULL;
    struct cx_gc_root_handle *content_root = NULL;
    struct cx_gc_root_handle *stream_root = NULL;
    struct cx_gc_root_handle *bytes_root = NULL;
    uint32_t steps = 0u;
    int server_started = 0;
    int dispose_started = 0;
    int result = 0;

    memset(&race, 0, sizeof(race));
    if (!start_fixture(&fixture, &port, &server_thread, 0,
            "GET /cx-http-stream-headers HTTP/1.1", NULL, NULL)) {
        fputs("could not start curl response-stream dispose fixture\n", stderr);
        return 0;
    }
    server_started = 1;
    fixture.hold_stream_body = 1;
    (void)snprintf(url, sizeof(url),
        "http://localhost:%u/cx-http-stream-headers", (unsigned)port);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    uri_root = cx_gc_add_root(&uri, (cx_uint)sizeof(uri));
    client = (struct cxnet_System_Net_Http_HttpClient *)cxcore_System_Memory_Alloc(
        cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    client_root = cx_gc_add_root(&client, (cx_uint)sizeof(client));
    cxcore_System_Uri___constructor(uri, make_string(url));
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    request = (struct cxnet_System_Net_Http_HttpRequestMessage *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpRequestMessage___typeinfo.Size);
    request_root = cx_gc_add_root(&request, (cx_uint)sizeof(request));
    cxnet_System_Net_Http_HttpRequestMessage___constructor(request,
        cxnet_System_Net_Http_HttpMethod_Get___const_get(), uri);
    response_task = cxnet_System_Net_Http_HttpClient_SendAsync__3(client,
        request, cxnet_System_Net_Http_HttpCompletionOption_ResponseHeadersRead,
        cxcore_System_Threading_Tasks_CancellationToken_None___const_get());
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(response_task) &&
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
    if (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(response_task) ||
        response_task->_isFaulted || response_task->_isCanceled) {
        fputs("curl response-stream fixture did not return headers\n", stderr);
        goto cleanup;
    }
    response = cx_generic_640110dc1a526c1c94f059cac4ed8d246fe089fbe535a78bc1997c7adde4688b(
        response_task);
    response_root = cx_gc_add_root(&response, (cx_uint)sizeof(response));
    content = (struct cxnet_System_Net_Http_CurlStreamingContent *)
        cxnet_System_Net_Http_HttpResponseMessage_Content___get(response);
    content_root = cx_gc_add_root(&content, (cx_uint)sizeof(content));
    stream = (struct cxnet_System_Net_Http_CurlResponseStream *)
        cxnet_System_Net_Http_CurlStreamingContent_ReadAsStream(content);
    stream_root = cx_gc_add_root(&stream, (cx_uint)sizeof(stream));
    bytes = cx_array_with_type(cx_array_new(4u, (cx_uint)sizeof(cx_byte)),
        "cx_generic_9c4b0dcf5365274a639812f507806a7475845f1a6e6b8c085532c41d4210ef4b");
    bytes_root = cx_gc_add_root(&bytes, (cx_uint)sizeof(bytes));
    read_task = cxnet_System_Net_Http_CurlResponseStream_ReadAsync(stream,
        bytes, 0u, 4u,
        cxcore_System_Threading_Tasks_CancellationToken_None___const_get());
    if (read_task->_isCompleted) {
        fputs("curl response-stream body read was not pending\n", stderr);
        goto cleanup;
    }
    race.stream = stream;
    if (!start_curl_response_stream_dispose_worker(&dispose_thread, &race)) {
        fputs("could not start curl response-stream dispose worker\n", stderr);
        goto cleanup;
    }
    dispose_started = 1;
    while (!request_race_load(&race.ready) &&
        !request_race_load(&race.completed) && steps < 300000u) {
#ifdef _WIN32
        Sleep(1u);
#else
        usleep(1000u);
#endif
        steps++;
    }
    if (!request_race_load(&race.ready)) {
        fputs("curl response-stream dispose worker did not reach its barrier\n",
            stderr);
        goto cleanup;
    }
    request_race_store(&race.start, 1);
    fixture.release_stream_body = 1;
    while ((!request_race_load(&race.completed) || !read_task->_isCompleted) &&
        steps < 500000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (dispose_started) {
        join_request_send_race_worker(dispose_thread);
        dispose_started = 0;
    }
    if (!request_race_load(&race.completed) ||
        request_race_load(&race.failed) != 0 || !race.disposed ||
        !read_task->_isCompleted ||
        (read_task->_isCanceled == 0 &&
            (read_task->_isFaulted ||
                cx_generic_32f22edd4c9ba40c64ed2658909ec8fb1d6e85d2c4276d46eebd01030eefa80f(
                    read_task) != 4u ||
                memcmp(cx_array_at(bytes, 0u, (cx_uint)sizeof(cx_byte)),
                    "pong", 4u) != 0)) ||
        cxnet_System_Net_Http_CurlResponseStream_CanRead___get(stream)) {
        fprintf(stderr,
            "curl response-stream dispose/read race failed: completed=%d failed=%d disposed=%d task=%d canceled=%d faulted=%d\n",
            request_race_load(&race.completed), request_race_load(&race.failed),
            race.disposed, read_task->_isCompleted, read_task->_isCanceled,
            read_task->_isFaulted);
        goto cleanup;
    }
    result = 1;
cleanup:
    fixture.release_stream_body = 1;
    request_race_store(&race.start, 1);
    if (dispose_started) join_request_send_race_worker(dispose_thread);
    if (server_started) {
        if (response != NULL)
            cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
        if (request != NULL)
            cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
        if (client != NULL) cxnet_System_Net_Http_HttpClient_Dispose(client);
        stop_fixture(&fixture, server_thread);
    }
    if (bytes_root != NULL) cx_gc_remove_root(bytes_root);
    if (stream_root != NULL) cx_gc_remove_root(stream_root);
    if (content_root != NULL) cx_gc_remove_root(content_root);
    if (response_root != NULL) cx_gc_remove_root(response_root);
    if (request_root != NULL) cx_gc_remove_root(request_root);
    if (client_root != NULL) cx_gc_remove_root(client_root);
    if (uri_root != NULL) cx_gc_remove_root(uri_root);
    return result;
}

static int run_response_read_scheduler_post_failure_case(void) {
    struct fixture fixture;
    test_thread server_thread;
    uint16_t port = 0u;
    char url[160];
    struct cxcore_System_Uri *uri;
    struct cxnet_System_Net_Http_HttpClient *client;
    struct cxnet_System_Net_Http_HttpRequestMessage *request;
    struct cxnet_System_Net_Http_HttpResponseMessage *response;
    struct cxnet_System_Net_Http_CurlStreamingContent *content;
    struct cxnet_System_Net_Http_CurlResponseStream *stream;
    struct cxcore_System_Collections_Array *bytes;
    struct cxcore_System_Threading_Tasks_CancellationTokenSource *request_source;
    struct cxcore_System_Threading_Tasks_CancellationTokenSource *read_source;
    struct cxcore_System_Threading_Tasks_CancellationToken request_token;
    struct cxcore_System_Threading_Tasks_CancellationToken read_token;
    struct cxcore_System_Threading_Tasks_Task *response_task;
    struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *read_task;
    uint32_t steps = 0u;

    if (!start_fixture(&fixture, &port, &server_thread, 0,
            "GET /cx-http-stream-cancel HTTP/1.1", NULL, NULL)) {
        fputs("could not start response-read cancellation fixture\n", stderr);
        return 0;
    }
    (void)snprintf(url, sizeof(url), "http://localhost:%u/cx-http-stream-cancel",
        (unsigned)port);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri, make_string(url));
    client = (struct cxnet_System_Net_Http_HttpClient *)cxcore_System_Memory_Alloc(
        cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    request_source = (struct cxcore_System_Threading_Tasks_CancellationTokenSource *)
        cxcore_System_Memory_Alloc(
            cxcore_System_Threading_Tasks_CancellationTokenSource___typeinfo.Size);
    cxcore_System_Threading_Tasks_CancellationTokenSource___constructor(request_source);
    request_token = cxcore_System_Threading_Tasks_CancellationTokenSource_Token___const_get(
        request_source);
    request = (struct cxnet_System_Net_Http_HttpRequestMessage *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpRequestMessage___typeinfo.Size);
    cxnet_System_Net_Http_HttpRequestMessage___constructor(request,
        cxnet_System_Net_Http_HttpMethod_Get___const_get(), uri);
    response_task = cxnet_System_Net_Http_HttpClient_SendAsync__3(client, request,
        cxnet_System_Net_Http_HttpCompletionOption_ResponseHeadersRead,
        request_token);
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(response_task) &&
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
    if (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(response_task) ||
        response_task->_isFaulted || response_task->_isCanceled) {
        fputs("response-read fixture did not return headers\n", stderr);
        stop_fixture(&fixture, server_thread);
        return 0;
    }
    response = cx_generic_640110dc1a526c1c94f059cac4ed8d246fe089fbe535a78bc1997c7adde4688b(
        response_task);
    content = (struct cxnet_System_Net_Http_CurlStreamingContent *)
        cxnet_System_Net_Http_HttpResponseMessage_Content___get(response);
    stream = (struct cxnet_System_Net_Http_CurlResponseStream *)
        cxnet_System_Net_Http_CurlStreamingContent_ReadAsStream(content);
    bytes = cx_array_with_type(cx_array_new(8u, (cx_uint)sizeof(cx_byte)),
        "cx_generic_9c4b0dcf5365274a639812f507806a7475845f1a6e6b8c085532c41d4210ef4b");
    read_task = cxnet_System_Net_Http_CurlResponseStream_ReadAsync(
        stream, bytes, 0u, 4u,
        cxcore_System_Threading_Tasks_CancellationToken_None___const_get());
    steps = 0u;
    while (!read_task->_isCompleted && steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (!read_task->_isCompleted || read_task->_isFaulted || read_task->_isCanceled ||
        cx_generic_32f22edd4c9ba40c64ed2658909ec8fb1d6e85d2c4276d46eebd01030eefa80f(
            read_task) != 4u ||
        memcmp(cx_array_at(bytes, 0u, (cx_uint)sizeof(cx_byte)), "pong", 4u) != 0) {
        fputs("response-read fixture did not deliver the initial body prefix\n", stderr);
        stop_fixture(&fixture, server_thread);
        return 0;
    }
    read_source = (struct cxcore_System_Threading_Tasks_CancellationTokenSource *)
        cxcore_System_Memory_Alloc(
            cxcore_System_Threading_Tasks_CancellationTokenSource___typeinfo.Size);
    cxcore_System_Threading_Tasks_CancellationTokenSource___constructor(read_source);
    read_token = cxcore_System_Threading_Tasks_CancellationTokenSource_Token___const_get(
        read_source);
    read_task = cxnet_System_Net_Http_CurlResponseStream_ReadAsync(
        stream, bytes, 0u, 4u, read_token);
    if (read_task->_isCompleted) {
        fputs("response body read did not remain pending for cancellation\n", stderr);
        stop_fixture(&fixture, server_thread);
        return 0;
    }
    cx_task_scheduler_test_fail_next_post();
    cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(read_source);
    if (!cx_task_scheduler_test_post_failure_was_consumed() ||
        !read_task->_isCompleted || !read_task->_isCanceled) {
        fputs("scheduler post rejection did not cancel the pending read inline\n", stderr);
        stop_fixture(&fixture, server_thread);
        return 0;
    }
    cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
    cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    stop_fixture(&fixture, server_thread);
    return 1;
}

static int run_response_read_cancellation_registration_failure_case(void) {
    struct fixture fixture;
    test_thread server_thread;
    uint16_t port = 0u;
    char url[160];
    struct cxcore_System_Uri *uri = NULL;
    struct cxnet_System_Net_Http_HttpClient *client = NULL;
    struct cxnet_System_Net_Http_HttpRequestMessage *request = NULL;
    struct cxnet_System_Net_Http_HttpResponseMessage *response = NULL;
    struct cxnet_System_Net_Http_CurlStreamingContent *content = NULL;
    struct cxnet_System_Net_Http_CurlResponseStream *stream = NULL;
    struct cxcore_System_Collections_Array *bytes = NULL;
    struct cxcore_System_Threading_Tasks_CancellationTokenSource *source = NULL;
    struct cxcore_System_Threading_Tasks_CancellationToken token;
    struct cxcore_System_Threading_Tasks_Task *response_task = NULL;
    struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *failed_read = NULL;
    struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *retry_read = NULL;
    uint32_t steps = 0u;
    int result = 0;

    if (!start_fixture(&fixture, &port, &server_thread, 0,
            "GET /cx-http-stream-headers HTTP/1.1", NULL, NULL)) {
        fputs("could not start response-read registration-failure fixture\n",
            stderr);
        return 0;
    }
    fixture.hold_stream_body = 1;
    (void)snprintf(url, sizeof(url),
        "http://localhost:%u/cx-http-stream-headers", (unsigned)port);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri, make_string(url));
    client = (struct cxnet_System_Net_Http_HttpClient *)cxcore_System_Memory_Alloc(
        cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    request = (struct cxnet_System_Net_Http_HttpRequestMessage *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpRequestMessage___typeinfo.Size);
    cxnet_System_Net_Http_HttpRequestMessage___constructor(request,
        cxnet_System_Net_Http_HttpMethod_Get___const_get(), uri);
    response_task = cxnet_System_Net_Http_HttpClient_SendAsync__3(client,
        request,
        cxnet_System_Net_Http_HttpCompletionOption_ResponseHeadersRead,
        cxcore_System_Threading_Tasks_CancellationToken_None___const_get());
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(
            response_task) && steps < 200000u) {
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
            response_task) || response_task->_isFaulted ||
        response_task->_isCanceled) {
        fputs("response-read registration fixture did not return headers\n",
            stderr);
        goto cleanup;
    }
    response = cx_generic_640110dc1a526c1c94f059cac4ed8d246fe089fbe535a78bc1997c7adde4688b(
        response_task);
    content = (struct cxnet_System_Net_Http_CurlStreamingContent *)
        cxnet_System_Net_Http_HttpResponseMessage_Content___get(response);
    stream = (struct cxnet_System_Net_Http_CurlResponseStream *)
        cxnet_System_Net_Http_CurlStreamingContent_ReadAsStream(content);
    bytes = cx_array_with_type(cx_array_new(4u, (cx_uint)sizeof(cx_byte)),
        "cx_generic_9c4b0dcf5365274a639812f507806a7475845f1a6e6b8c085532c41d4210ef4b");
    source = (struct cxcore_System_Threading_Tasks_CancellationTokenSource *)
        cxcore_System_Memory_Alloc(
            cxcore_System_Threading_Tasks_CancellationTokenSource___typeinfo.Size);
    cxcore_System_Threading_Tasks_CancellationTokenSource___constructor(source);
    token = cxcore_System_Threading_Tasks_CancellationTokenSource_Token___const_get(
        source);

    cxcore_System_Threading_Tasks_TaskRuntime_TestFailCancellationRegistrationAfter(
        1u);
    failed_read = cxnet_System_Net_Http_CurlResponseStream_ReadAsync(
        stream, bytes, 0u, 4u, token);
    cxcore_System_Threading_Tasks_TaskRuntime_TestFailCancellationRegistrationAfter(
        0u);
    if (!failed_read->_isCompleted || !failed_read->_isFaulted ||
        failed_read->_isCanceled || stream->_pendingRead != NULL ||
        source->_callbackCount != 0u) {
        fprintf(stderr,
            "response read cleanup failed: completed=%u faulted=%u canceled=%u pending=%p callbacks=%u\n",
            (unsigned)failed_read->_isCompleted,
            (unsigned)failed_read->_isFaulted,
            (unsigned)failed_read->_isCanceled,
            (void *)stream->_pendingRead,
            (unsigned)source->_callbackCount);
        goto cleanup;
    }

    retry_read = cxnet_System_Net_Http_CurlResponseStream_ReadAsync(
        stream, bytes, 0u, 4u,
        cxcore_System_Threading_Tasks_CancellationToken_None___const_get());
    if (retry_read->_isCompleted || stream->_pendingRead == NULL) {
        fputs("response stream remained unusable after registration failure\n",
            stderr);
        goto cleanup;
    }
    fixture.release_stream_body = 1;
    steps = 0u;
    while (!retry_read->_isCompleted && steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (!retry_read->_isCompleted || retry_read->_isFaulted ||
        retry_read->_isCanceled ||
        cx_generic_32f22edd4c9ba40c64ed2658909ec8fb1d6e85d2c4276d46eebd01030eefa80f(
            retry_read) != 4u ||
        memcmp(cx_array_at(bytes, 0u, (cx_uint)sizeof(cx_byte)),
            "pong", 4u) != 0) {
        fputs("response stream did not recover after registration failure\n",
            stderr);
        goto cleanup;
    }
    result = 1;

cleanup:
    cxcore_System_Threading_Tasks_TaskRuntime_TestFailCancellationRegistrationAfter(
        0u);
    fixture.release_stream_body = 1;
    if (response != NULL)
        cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
    if (request != NULL)
        cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
    if (client != NULL) cxnet_System_Net_Http_HttpClient_Dispose(client);
    stop_fixture(&fixture, server_thread);
    return result;
}

static int run_cancellation_client_dispose_overlap_case(void) {
    struct fixture fixture;
    test_thread thread;
    uint16_t port = 0u;
    char url[160];
    struct cxcore_System_Uri *uri;
    struct cxnet_System_Net_Http_HttpClient *client;
    struct cxcore_System_Threading_Tasks_CancellationTokenSource *source;
    struct cxcore_System_Threading_Tasks_CancellationToken token;
    struct cxcore_System_Threading_Tasks_Task *task;
    uint32_t steps = 0u;

    if (!start_fixture(&fixture, &port, &thread, 1,
            "GET /cx-http-cancel-dispose-overlap HTTP/1.1", NULL, NULL)) {
        fputs("could not start cancel/dispose overlap fixture\n", stderr);
        return 0;
    }
    (void)snprintf(url, sizeof(url),
        "http://localhost:%u/cx-http-cancel-dispose-overlap", (unsigned)port);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri, make_string(url));
    client = (struct cxnet_System_Net_Http_HttpClient *)cxcore_System_Memory_Alloc(
        cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    source = (struct cxcore_System_Threading_Tasks_CancellationTokenSource *)
        cxcore_System_Memory_Alloc(
            cxcore_System_Threading_Tasks_CancellationTokenSource___typeinfo.Size);
    cxcore_System_Threading_Tasks_CancellationTokenSource___constructor(source);
    token = cxcore_System_Threading_Tasks_CancellationTokenSource_Token___const_get(source);
    task = cxnet_System_Net_Http_HttpClient_GetAsync(client, uri, token);

    steps = 0u;
    while (!fixture.request_received && steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }

    /* Both owners can request cleanup while the native transfer is active. */
    cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(source);
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    cxcore_System_Threading_Tasks_CancellationTokenSource_Cancel(source);
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
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
    steps = 0u;
    while (!fixture.client_closed && steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    stop_fixture(&fixture, thread);
    if (fixture.failed || !fixture.request_received || !fixture.client_closed ||
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) ||
        !cxnet_System_Net_Http_CurlHttpTransport_WasCanceled(task)) {
        fprintf(stderr,
            "cancel/dispose overlap failed fixture=%d closed=%d completed=%d canceled=%d\n",
            fixture.failed, fixture.client_closed,
            cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task),
            cxnet_System_Net_Http_CurlHttpTransport_WasCanceled(task));
        return 0;
    }
    return 1;
}

static int run_rejected_request_framing_case(const char *header_name) {
    struct cxnet_System_Net_Http_HttpClient *client =
        (struct cxnet_System_Net_Http_HttpClient *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    struct cxnet_System_Net_Http_HttpRequestMessage *request =
        (struct cxnet_System_Net_Http_HttpRequestMessage *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpRequestMessage___typeinfo.Size);
    struct cxcore_System_Uri *uri = (struct cxcore_System_Uri *)
        cxcore_System_Memory_Alloc(cxcore_System_Uri___typeinfo.Size);
    struct cxcore_System_Threading_Tasks_Task *task;
    struct cxcore_System_Exception *exception;
    struct cxcore_System_String *message;
    uint32_t steps = 0u;

    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    cxcore_System_Uri___constructor(uri,
        make_string("http://127.0.0.1:1/no-network-request-expected"));
    cxnet_System_Net_Http_HttpRequestMessage___constructor(request,
        cxnet_System_Net_Http_HttpMethod_Get___const_get(),
        uri);
    if (!cxnet_System_Net_Http_HttpHeaders_TryAddWithoutValidation(
            cxnet_System_Net_Http_HttpRequestMessage_Headers___const_get(request),
            make_string(header_name), make_string("4"))) {
        fputs("could not construct request framing regression case\n", stderr);
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
    message = exception == NULL ? NULL :
        cxcore_System_Exception_Message___const_get(exception);
    if (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) ||
        !task->_isFaulted ||
        !string_equals(message,
            "Request Content-Length and Transfer-Encoding headers are managed by the HTTP transport.")) {
        fprintf(stderr, "%s framing was not rejected with a specific error\n", header_name);
        cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        return 0;
    }
    cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    return 1;
}

static int run_invalid_response_case(const char *path,
    const char *request_line, int headers_first) {
    struct fixture fixture;
    test_thread thread;
    uint16_t port = 0u;
    char url[160];
    struct cxcore_System_Uri *uri;
    struct cxnet_System_Net_Http_HttpClient *client;
    struct cxnet_System_Net_Http_HttpRequestMessage *request;
    struct cxcore_System_Threading_Tasks_Task *task;
    struct cxcore_System_Exception *exception;
    struct cxcore_System_String *message;
    uint32_t steps = 0u;

    if (!start_fixture(&fixture, &port, &thread, 0,
            request_line, NULL, NULL)) {
        fputs("could not start malformed response fixture\n", stderr);
        return 0;
    }
    (void)snprintf(url, sizeof(url), "http://localhost:%u%s",
        (unsigned)port, path);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri, make_string(url));
    request = (struct cxnet_System_Net_Http_HttpRequestMessage *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpRequestMessage___typeinfo.Size);
    cxnet_System_Net_Http_HttpRequestMessage___constructor(request,
        cxnet_System_Net_Http_HttpMethod_Get___const_get(), uri);
    client = (struct cxnet_System_Net_Http_HttpClient *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    if (headers_first) {
        task = cxnet_System_Net_Http_HttpClient_SendAsync__4(client, request,
            cxnet_System_Net_Http_HttpCompletionOption_ResponseHeadersRead);
    } else {
        task = cxnet_System_Net_Http_HttpClient_SendAsync(client, request);
    }
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) break;
        steps++;
    }
    stop_fixture(&fixture, thread);
    exception = task->_exception;
    message = exception == NULL ? NULL :
        cxcore_System_Exception_Message___const_get(exception);
    if (fixture.failed ||
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) ||
        !task->_isFaulted || message == NULL || message->_length == 0u) {
        fprintf(stderr,
            "malformed response was not rejected for %s: fixture=%d completed=%d faulted=%d message=%.*s\n",
            path,
            fixture.failed,
            cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task),
            task->_isFaulted,
            message == NULL ? 0 : (int)message->_length,
            message == NULL ? "" : (const char *)message->_data);
        cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        return 0;
    }
    if (strcmp(path, "/cx-http-101") == 0) {
        if (!cxcore_System_Exception_Matches(exception,
                cxnet_System_Net_Http_HttpRequestException___typeinfo)) {
            fputs("101 response fault was not an HttpRequestException\n", stderr);
            cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
            cxnet_System_Net_Http_HttpClient_Dispose(client);
            return 0;
        }
        struct cxcore_System_Nullable status =
            cxnet_System_Net_Http_HttpRequestException_StatusCode___const_get(
                (const struct cxnet_System_Net_Http_HttpRequestException *)exception);
        struct cxcore_System_Nullable status_value;
        if (!cxcore_System_Nullable_HasValue___const_get(&status)) {
            fputs("101 response fault did not retain an HTTP status\n", stderr);
            cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
            cxnet_System_Net_Http_HttpClient_Dispose(client);
            return 0;
        }
        cxcore_System_Nullable_Value___const_get(&status, &status_value);
        if (status_value._obj == NULL ||
            *(cxnet_System_Net_Http_HttpStatusCode *)status_value._obj !=
                cxnet_System_Net_Http_HttpStatusCode_SwitchingProtocols) {
            fprintf(stderr, "101 response fault retained an unexpected status\n");
            cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
            cxnet_System_Net_Http_HttpClient_Dispose(client);
            return 0;
        }
    }
    cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    return 1;
}

static int run_chunk_delimiter_mutation_cases(void) {
    const char *header_end = strstr(chunk_delimiter_mutation_response,
        "\r\n\r\n");
    size_t header_length;
    size_t body_length;
    size_t offset;
    if (header_end == NULL) return 0;
    header_length = (size_t)(header_end - chunk_delimiter_mutation_response) + 4u;
    body_length = sizeof(chunk_delimiter_mutation_response) - 1u - header_length;
    for (offset = 0u; offset < body_length; offset += 1u) {
        char path[80];
        char request_line[128];
        char value = chunk_delimiter_mutation_response[header_length + offset];
        if (value != '\r' && value != '\n') continue;
        (void)snprintf(path, sizeof(path),
            "/cx-http-chunk-delimiter-mutation-%zu", offset);
        (void)snprintf(request_line, sizeof(request_line), "GET %s HTTP/1.1",
            path);
        if (!run_invalid_response_case(path, request_line, 0)) return 0;
    }
    return 1;
}

static int run_chunk_truncation_cases(void) {
    const char *header_end = strstr(chunk_delimiter_mutation_response,
        "\r\n\r\n");
    size_t header_length;
    size_t body_length;
    size_t prefix;
    if (header_end == NULL) return 0;
    header_length = (size_t)(header_end - chunk_delimiter_mutation_response) + 4u;
    body_length = sizeof(chunk_delimiter_mutation_response) - 1u - header_length;
    for (prefix = 0u; prefix < body_length; prefix += 1u) {
        char path[80];
        char request_line[128];
        (void)snprintf(path, sizeof(path), "/cx-http-chunk-truncation-%zu",
            prefix);
        (void)snprintf(request_line, sizeof(request_line), "GET %s HTTP/1.1",
            path);
        if (!run_invalid_response_case(path, request_line, 0)) return 0;
    }
    return 1;
}

static int run_chunk_size_byte_cases(void) {
    unsigned int byte_value;
    for (byte_value = 0u; byte_value <= 255u; byte_value += 1u) {
        char path[80];
        char request_line[128];
        if ((byte_value >= (unsigned int)'0' &&
                byte_value <= (unsigned int)'9') ||
            (byte_value >= (unsigned int)'a' &&
                byte_value <= (unsigned int)'f') ||
            (byte_value >= (unsigned int)'A' &&
                byte_value <= (unsigned int)'F')) continue;
        (void)snprintf(path, sizeof(path), "/cx-http-chunk-size-byte-%u",
            byte_value);
        (void)snprintf(request_line, sizeof(request_line), "GET %s HTTP/1.1",
            path);
        if (!run_invalid_response_case(path, request_line, 0)) {
            fprintf(stderr, "invalid chunk-size byte 0x%02X was accepted\n",
                byte_value);
            return 0;
        }
    }
    return 1;
}

static int run_chunk_size_grammar_cases(void) {
    return run_invalid_response_case("/cx-http-signed-chunk-size",
            "GET /cx-http-signed-chunk-size HTTP/1.1", 0) &&
        run_invalid_response_case("/cx-http-prefixed-chunk-size",
            "GET /cx-http-prefixed-chunk-size HTTP/1.1", 0) &&
        run_invalid_response_case("/cx-http-non-hex-chunk-size",
            "GET /cx-http-non-hex-chunk-size HTTP/1.1", 0) &&
        run_invalid_response_case("/cx-http-leading-space-chunk-size",
            "GET /cx-http-leading-space-chunk-size HTTP/1.1", 0) &&
        run_invalid_response_case("/cx-http-lf-chunk-size",
            "GET /cx-http-lf-chunk-size HTTP/1.1", 0) &&
        run_invalid_response_case("/cx-http-missing-chunk-data-cr",
            "GET /cx-http-missing-chunk-data-cr HTTP/1.1", 0) &&
        run_invalid_response_case("/cx-http-malformed-chunk-data-terminator",
            "GET /cx-http-malformed-chunk-data-terminator HTTP/1.1", 0) &&
        run_invalid_response_case("/cx-http-malformed-zero-chunk-terminator",
            "GET /cx-http-malformed-zero-chunk-terminator HTTP/1.1", 0) &&
        run_invalid_response_case("/cx-http-truncated-chunk-data",
            "GET /cx-http-truncated-chunk-data HTTP/1.1", 0) &&
        run_invalid_response_case("/cx-http-malformed-trailer",
            "GET /cx-http-malformed-trailer HTTP/1.1", 0) &&
        run_invalid_response_case("/cx-http-chunk-trailer-del",
            "GET /cx-http-chunk-trailer-del HTTP/1.1", 0) &&
        run_invalid_response_case("/cx-http-chunk-trailer-control",
            "GET /cx-http-chunk-trailer-control HTTP/1.1", 0) &&
        run_invalid_response_case("/cx-http-chunk-trailer-obs-fold",
            "GET /cx-http-chunk-trailer-obs-fold HTTP/1.1", 0) &&
        run_invalid_response_case("/cx-http-chunk-extension-control",
            "GET /cx-http-chunk-extension-control HTTP/1.1", 0) &&
        run_invalid_response_case("/cx-http-chunk-extension-escaped-control",
            "GET /cx-http-chunk-extension-escaped-control HTTP/1.1", 0) &&
        run_invalid_response_case("/cx-http-truncated-chunk-trailer",
            "GET /cx-http-truncated-chunk-trailer HTTP/1.1", 0) &&
        run_chunk_size_byte_cases() &&
        run_chunk_delimiter_mutation_cases() &&
        run_chunk_truncation_cases();
}

static int run_segmented_chunk_case(void) {
    return run_method_case("GET /cx-http-segmented-chunked HTTP/1.1",
        "/cx-http-segmented-chunked",
        cxnet_System_Net_Http_HttpMethod_Get___const_get(), NULL, NULL);
}

static int run_request_reservation_case(void) {
    struct fixture fixture;
    test_thread thread;
    uint16_t port = 0u;
    char url[160];
    struct cxcore_System_Uri *uri;
    struct cxnet_System_Net_Http_HttpClient *client;
    struct cxnet_System_Net_Http_HttpRequestMessage *request;
    struct cxcore_System_Threading_Tasks_Task *first_task;
    struct cxcore_System_Threading_Tasks_Task *duplicate_task = NULL;
    struct cxnet_System_Net_Http_HttpResponseMessage *response = NULL;
    struct cx_gc_root_handle *uri_root;
    struct cx_gc_root_handle *client_root;
    struct cx_gc_root_handle *request_root;
    struct cx_gc_root_handle *first_task_root;
    struct cx_gc_root_handle * volatile duplicate_task_root = NULL;
    struct cx_gc_root_handle * volatile response_root = NULL;
    volatile uint32_t steps = 0u;
    volatile int duplicate_rejected = 0;
    volatile int method_rejected = 0;
    int uri_rejected = 0;
    volatile int result = 0;
    if (!start_fixture(&fixture, &port, &thread, 0,
            "GET /cx-http-request-reservation HTTP/1.1", NULL, NULL)) {
        fputs("could not start request-reservation fixture\n", stderr);
        return 0;
    }
    fixture.hold_response = 1;
    (void)snprintf(url, sizeof(url),
        "http://localhost:%u/cx-http-request-reservation", (unsigned)port);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    uri_root = cx_gc_add_root(&uri, (cx_uint)sizeof(uri));
    cxcore_System_Uri___constructor(uri, make_string(url));
    client = (struct cxnet_System_Net_Http_HttpClient *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    client_root = cx_gc_add_root(&client, (cx_uint)sizeof(client));
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    request = (struct cxnet_System_Net_Http_HttpRequestMessage *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpRequestMessage___typeinfo.Size);
    request_root = cx_gc_add_root(&request, (cx_uint)sizeof(request));
    cxnet_System_Net_Http_HttpRequestMessage___constructor(request,
        cxnet_System_Net_Http_HttpMethod_Get___const_get(), uri);
    first_task = cxnet_System_Net_Http_HttpClient_SendAsync(client, request);
    first_task_root = cx_gc_add_root(&first_task,
        (cx_uint)sizeof(first_task));
    while (!fixture.request_received &&
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(first_task) &&
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
    if (!fixture.request_received ||
        cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(first_task)) {
        fputs("request did not remain pending after server receipt\n", stderr);
        goto cleanup;
    }
    {
        struct cx_exception_frame frame;
        struct cxcore_System_Exception * volatile exception = NULL;
        cx_exception_push(&frame);
        if (setjmp(frame.environment) == 0) {
            duplicate_task = cxnet_System_Net_Http_HttpClient_SendAsync(
                client, request);
            cx_exception_pop(&frame);
        } else {
            exception = (struct cxcore_System_Exception *)
                cx_exception_current();
            cx_exception_pop(&frame);
            cx_exception_clear();
        }
        duplicate_rejected = exception != NULL &&
            cxcore_System_Exception_Matches(exception,
                cxcore_System_InvalidOperationException___typeinfo);
    }
    if (duplicate_task != NULL) {
        duplicate_task_root = cx_gc_add_root(&duplicate_task,
            (cx_uint)sizeof(duplicate_task));
    } else {
        duplicate_task_root = NULL;
    }
    {
        struct cx_exception_frame frame;
        struct cxcore_System_Exception * volatile exception = NULL;
        cx_exception_push(&frame);
        if (setjmp(frame.environment) == 0) {
            cxnet_System_Net_Http_HttpRequestMessage_Method___set(request,
                cxnet_System_Net_Http_HttpMethod_Get___const_get());
            cx_exception_pop(&frame);
        } else {
            exception = (struct cxcore_System_Exception *)
                cx_exception_current();
            cx_exception_pop(&frame);
            cx_exception_clear();
        }
        method_rejected = exception != NULL &&
            cxcore_System_Exception_Matches(exception,
                cxcore_System_InvalidOperationException___typeinfo);
    }
    {
        struct cx_exception_frame frame;
        struct cxcore_System_Exception * volatile exception = NULL;
        cx_exception_push(&frame);
        if (setjmp(frame.environment) == 0) {
            cxnet_System_Net_Http_HttpRequestMessage_RequestUri___set(request,
                uri);
            cx_exception_pop(&frame);
        } else {
            exception = (struct cxcore_System_Exception *)
                cx_exception_current();
            cx_exception_pop(&frame);
            cx_exception_clear();
        }
        uri_rejected = exception != NULL &&
            cxcore_System_Exception_Matches(exception,
                cxcore_System_InvalidOperationException___typeinfo);
    }
    if (!duplicate_rejected || !method_rejected || !uri_rejected ||
        cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(first_task) ||
        fixture.failed) {
        fputs("in-flight request reservation did not reject concurrent send and mutation\n",
            stderr);
        goto cleanup;
    }
    fixture.release_response = 1;
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(first_task) &&
        steps < 400000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(first_task) ||
        first_task->_isFaulted || first_task->_isCanceled) {
        fputs("reserved request did not complete after response release\n", stderr);
        goto cleanup;
    }
    response = cx_generic_640110dc1a526c1c94f059cac4ed8d246fe089fbe535a78bc1997c7adde4688b(
        first_task);
    response_root = cx_gc_add_root(&response, (cx_uint)sizeof(response));
    if (response == NULL ||
        !string_equals(cxnet_System_Net_Http_HttpContent_ReadAsString(
            cxnet_System_Net_Http_HttpResponseMessage_Content___get(response)),
            "pong")) {
        fputs("reserved request response was corrupted by rejected operations\n",
            stderr);
        goto cleanup;
    }
    while (request->_sendInProgress && steps < 450000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (request->_sendInProgress) {
        fputs("completed request did not release its send reservation\n", stderr);
        goto cleanup;
    }
    cxnet_System_Net_Http_HttpRequestMessage_Method___set(request,
        cxnet_System_Net_Http_HttpMethod_Get___const_get());
    cxnet_System_Net_Http_HttpRequestMessage_RequestUri___set(request, uri);
    result = 1;
cleanup:
    fixture.release_response = 1;
    if (response != NULL) cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
    cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    stop_fixture(&fixture, thread);
    if (duplicate_task_root != NULL) cx_gc_remove_root(duplicate_task_root);
    if (response_root != NULL) cx_gc_remove_root(response_root);
    cx_gc_remove_root(first_task_root);
    cx_gc_remove_root(request_root);
    cx_gc_remove_root(client_root);
    cx_gc_remove_root(uri_root);
    return result;
}

static int request_race_load(volatile int *value) {
#ifdef _WIN32
    return (int)InterlockedCompareExchange((volatile LONG *)value, 0, 0);
#else
    return __sync_val_compare_and_swap(value, 0, 0);
#endif
}

static int request_race_increment(volatile int *value) {
#ifdef _WIN32
    return (int)InterlockedIncrement((volatile LONG *)value);
#else
    return __sync_add_and_fetch(value, 1);
#endif
}

static void request_race_store(volatile int *value, int next) {
#ifdef _WIN32
    (void)InterlockedExchange((volatile LONG *)value, (LONG)next);
#else
    (void)__sync_lock_test_and_set(value, next);
#endif
}

static int response_content_set_attempt(
    struct cxnet_System_Net_Http_HttpResponseMessage *response,
    struct cxnet_System_Net_Http_HttpContent *content) {
    struct cx_exception_frame frame;
    struct cxcore_System_Exception * volatile exception = NULL;
    volatile int result = -1;
    cx_exception_push(&frame);
    if (setjmp(frame.environment) == 0) {
        cxnet_System_Net_Http_HttpResponseMessage_Content___set(response,
            content);
        cx_exception_pop(&frame);
        result = 1;
    } else {
        exception = (struct cxcore_System_Exception *)cx_exception_current();
        cx_exception_pop(&frame);
        cx_exception_clear();
        result = exception != NULL &&
            cxcore_System_Exception_Matches(exception,
                cxcore_System_ObjectDisposedException___typeinfo) ? 0 : -1;
    }
    return result;
}

static int multipart_read_attempt(struct multipart_serialization_race *race) {
    struct cx_exception_frame frame;
    struct cxcore_System_Exception * volatile exception = NULL;
    volatile int result = -1;
    cx_exception_push(&frame);
    if (setjmp(frame.environment) == 0) {
        race->serialized =
            cxnet_System_Net_Http_MultipartFormDataContent_ReadAsByteArray(
                race->content);
        cx_exception_pop(&frame);
        result = race->serialized == NULL ? -1 : 1;
    } else {
        exception = (struct cxcore_System_Exception *)cx_exception_current();
        cx_exception_pop(&frame);
        cx_exception_clear();
        result = exception != NULL &&
            cxcore_System_Exception_Matches(exception,
                cxcore_System_ObjectDisposedException___typeinfo) ? 0 : -1;
    }
    return result;
}

static void rest_response_failure_continuation(void *environment) {
    (void)environment;
}

static int run_rest_response_publication_failure_case(int byte_array) {
    struct cxnet_System_Net_Http_Rest_RestClient *client;
    struct cxnet_System_Net_Http_HttpResponseMessage *response;
    struct cxnet_System_Net_Http_StringContent *content;
    struct cxcore_System_Threading_Tasks_Task *pending;
    struct cxcore_System_Threading_Tasks_Task *completion;
    struct cx_exception_frame frame;
    struct cxcore_System_Exception * volatile exception = NULL;
    struct cx_gc_root_handle *client_root;
    struct cx_gc_root_handle *response_root;
    struct cx_gc_root_handle *content_root;
    struct cx_gc_root_handle *pending_root;
    struct cx_gc_root_handle *completion_root;
    volatile int completion_threw = 0;
    CX_FN_136FFEE706DF35B2 continuation = {0};

    client = (struct cxnet_System_Net_Http_Rest_RestClient *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_Rest_RestClient___typeinfo.Size);
    cxnet_System_Net_Http_Rest_RestClient___constructor(client);
    response = (struct cxnet_System_Net_Http_HttpResponseMessage *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpResponseMessage___typeinfo.Size);
    cxnet_System_Net_Http_HttpResponseMessage___constructor__2(response,
        cxnet_System_Net_Http_HttpStatusCode_OK);
    content = (struct cxnet_System_Net_Http_StringContent *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_StringContent___typeinfo.Size);
    cxnet_System_Net_Http_StringContent___constructor(content,
        make_string("response lifetime"));
    cxnet_System_Net_Http_HttpResponseMessage_Content___set(response,
        (struct cxnet_System_Net_Http_HttpContent *)content);

    pending = (struct cxcore_System_Threading_Tasks_Task *)
        cxcore_System_Memory_Alloc(cxcore_System_Threading_Tasks_Task___typeinfo.Size);
    cxcore_System_Threading_Tasks_Task___constructor__2(pending);
    pending->_isCompleted = 1;
    pending->_result = response;
    completion = (struct cxcore_System_Threading_Tasks_Task *)
        cxcore_System_Memory_Alloc(cxcore_System_Threading_Tasks_Task___typeinfo.Size);
    cxcore_System_Threading_Tasks_Task___constructor__2(completion);
    client_root = cx_gc_add_root(&client, (cx_uint)sizeof(client));
    response_root = cx_gc_add_root(&response, (cx_uint)sizeof(response));
    content_root = cx_gc_add_root(&content, (cx_uint)sizeof(content));
    pending_root = cx_gc_add_root(&pending, (cx_uint)sizeof(pending));
    completion_root = cx_gc_add_root(&completion, (cx_uint)sizeof(completion));
    continuation.__function = rest_response_failure_continuation;
    cxcore_System_Threading_Tasks_Task_OnCompleted(completion, continuation);

    cx_task_scheduler_test_fail_next_post();
    cx_exception_push(&frame);
    if (setjmp(frame.environment) == 0) {
        if (byte_array) {
            cxnet_System_Net_Http_Rest_RestClient_TestCompleteByteArrayResponse(
                client, pending, completion);
        } else {
            cxnet_System_Net_Http_Rest_RestClient_TestCompleteStringResponse(
                client, pending, completion);
        }
        cx_exception_pop(&frame);
    } else {
        exception = (struct cxcore_System_Exception *)cx_exception_current();
        cx_exception_pop(&frame);
        cx_exception_clear();
        completion_threw = exception != NULL;
    }

    if (!cx_task_scheduler_test_post_failure_was_consumed() ||
        !completion_threw || !completion->_isCompleted ||
        !response->_disposed) {
        fprintf(stderr,
            "REST %s publication failure assertions: injected=%d threw=%d completed=%d responseDisposed=%d\n",
            byte_array ? "byte-array" : "string",
            (int)cx_task_scheduler_test_post_failure_was_consumed(),
            (int)completion_threw, (int)completion->_isCompleted,
            (int)response->_disposed);
        cx_gc_remove_root(completion_root);
        cx_gc_remove_root(pending_root);
        cx_gc_remove_root(content_root);
        cx_gc_remove_root(response_root);
        cx_gc_remove_root(client_root);
        return 0;
    }
    cx_gc_remove_root(completion_root);
    cx_gc_remove_root(pending_root);
    cx_gc_remove_root(content_root);
    cx_gc_remove_root(response_root);
    cx_gc_remove_root(client_root);
    return 1;
}

static int run_rest_response_publication_failure_cases(void) {
    return run_rest_response_publication_failure_case(0) &&
        run_rest_response_publication_failure_case(1);
}

#ifdef _WIN32
static DWORD WINAPI multipart_serialization_worker(void *argument) {
#else
static void *multipart_serialization_worker(void *argument) {
#endif
    struct multipart_serialization_race *race =
        (struct multipart_serialization_race *)argument;
    struct cxnet_System_Net_Http_MultipartFormDataContent *content =
        race->content;
    struct cx_gc_root_handle *race_root = NULL;
    struct cx_gc_root_handle *content_root = NULL;
    cx_bool attached = cx_gc_enter_thread();
    if (!attached) {
        request_race_store(&race->failed, 1);
        goto completed;
    }
    race_root = cx_gc_add_root(&race, (cx_uint)sizeof(race));
    content_root = cx_gc_add_root(&content, (cx_uint)sizeof(content));
    request_race_store(&race->ready, 1);
    while (!request_race_load(&race->start)) {
#ifdef _WIN32
        Sleep(0u);
#else
        usleep(1000u);
#endif
    }
    race->read_status = multipart_read_attempt(race);
    if (race->read_status < 0) request_race_store(&race->failed, 1);
    cx_gc_remove_root(content_root);
    cx_gc_remove_root(race_root);
    cx_gc_leave_thread(attached);
completed:
    request_race_store(&race->completed, 1);
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

static int start_multipart_serialization_worker(
    test_thread *thread, struct multipart_serialization_race *race) {
#ifdef _WIN32
    *thread = CreateThread(NULL, 0, multipart_serialization_worker, race, 0, NULL);
    return *thread != NULL;
#else
    return pthread_create(thread, NULL, multipart_serialization_worker, race) == 0;
#endif
}

static void join_multipart_serialization_worker(test_thread thread) {
#ifdef _WIN32
    (void)WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
#else
    (void)pthread_join(thread, NULL);
#endif
}

#ifdef _WIN32
static DWORD WINAPI response_content_race_worker(void *argument) {
#else
static void *response_content_race_worker(void *argument) {
#endif
    struct response_content_race *race =
        (struct response_content_race *)argument;
    struct cxnet_System_Net_Http_HttpResponseMessage *response = race->response;
    struct cxnet_System_Net_Http_HttpContent *content = race->content;
    struct cx_gc_root_handle *race_root = NULL;
    struct cx_gc_root_handle *response_root = NULL;
    struct cx_gc_root_handle *content_root = NULL;
    cx_bool attached = cx_gc_enter_thread();
    int outcome;
    if (!attached) goto completed;
    race_root = cx_gc_add_root(&race, (cx_uint)sizeof(race));
    response_root = cx_gc_add_root(&response, (cx_uint)sizeof(response));
    content_root = cx_gc_add_root(&content, (cx_uint)sizeof(content));
    if (race_root == NULL || response_root == NULL || content_root == NULL) {
        request_race_store(&race->failed, 1);
        goto leave_thread;
    }
    request_race_store(&race->ready, 1);
    while (!request_race_load(&race->start)) {
#ifdef _WIN32
        Sleep(0u);
#else
        usleep(100u);
#endif
    }
    outcome = response_content_set_attempt(response, content);
    if (outcome == 1) {
        request_race_increment(&race->assigned);
    } else if (outcome == 0) {
        request_race_increment(&race->rejected);
    } else {
        request_race_increment(&race->failed);
    }
leave_thread:
    if (content_root != NULL) cx_gc_remove_root(content_root);
    if (response_root != NULL) cx_gc_remove_root(response_root);
    if (race_root != NULL) cx_gc_remove_root(race_root);
    cx_gc_leave_thread(attached);
completed:
    request_race_store(&race->completed, 1);
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

static int start_response_content_race_worker(test_thread *thread,
    struct response_content_race *race) {
#ifdef _WIN32
    *thread = CreateThread(NULL, 0, response_content_race_worker, race, 0, NULL);
    return *thread != NULL;
#else
    return pthread_create(thread, NULL, response_content_race_worker, race) == 0;
#endif
}

static int request_send_attempt(
    struct cxnet_System_Net_Http_HttpRequestMessage *request) {
    struct cx_exception_frame frame;
    struct cxcore_System_Exception * volatile exception = NULL;
    volatile int result = -1;
    cx_exception_push(&frame);
    if (setjmp(frame.environment) == 0) {
        cxnet_System_Net_Http_HttpRequestMessage_BeginSend(request, 100000u);
        cx_exception_pop(&frame);
        result = 1;
    } else {
        exception = (struct cxcore_System_Exception *)cx_exception_current();
        cx_exception_pop(&frame);
        cx_exception_clear();
        result = exception != NULL &&
            cxcore_System_Exception_Matches(exception,
                cxcore_System_InvalidOperationException___typeinfo) ? 0 : -1;
    }
    return result;
}

static int request_set_method_attempt(
    struct cxnet_System_Net_Http_HttpRequestMessage *request,
    struct cxnet_System_Net_Http_HttpMethod *method) {
    struct cx_exception_frame frame;
    struct cxcore_System_Exception * volatile exception = NULL;
    volatile int result = -1;
    cx_exception_push(&frame);
    if (setjmp(frame.environment) == 0) {
        cxnet_System_Net_Http_HttpRequestMessage_Method___set(request, method);
        cx_exception_pop(&frame);
        result = 1;
    } else {
        exception = (struct cxcore_System_Exception *)cx_exception_current();
        cx_exception_pop(&frame);
        cx_exception_clear();
        result = exception != NULL &&
            cxcore_System_Exception_Matches(exception,
                cxcore_System_InvalidOperationException___typeinfo) ? 0 : -1;
    }
    return result;
}

static int request_set_uri_attempt(
    struct cxnet_System_Net_Http_HttpRequestMessage *request,
    struct cxcore_System_Uri *uri) {
    struct cx_exception_frame frame;
    struct cxcore_System_Exception * volatile exception = NULL;
    volatile int result = -1;
    cx_exception_push(&frame);
    if (setjmp(frame.environment) == 0) {
        cxnet_System_Net_Http_HttpRequestMessage_RequestUri___set(request, uri);
        cx_exception_pop(&frame);
        result = 1;
    } else {
        exception = (struct cxcore_System_Exception *)cx_exception_current();
        cx_exception_pop(&frame);
        cx_exception_clear();
        result = exception != NULL &&
            cxcore_System_Exception_Matches(exception,
                cxcore_System_InvalidOperationException___typeinfo) ? 0 : -1;
    }
    return result;
}

#ifdef _WIN32
static DWORD WINAPI request_state_race_worker(void *argument) {
#else
static void *request_state_race_worker(void *argument) {
#endif
    struct request_state_race_worker *worker =
        (struct request_state_race_worker *)argument;
    struct request_state_race *race = worker->race;
    struct cxnet_System_Net_Http_HttpRequestMessage *request = race->request;
    struct cxcore_System_Uri *uri = race->uri;
    struct cxnet_System_Net_Http_HttpMethod *method = race->method;
    struct cx_gc_root_handle *race_root = NULL;
    struct cx_gc_root_handle *request_root = NULL;
    struct cx_gc_root_handle *uri_root = NULL;
    struct cx_gc_root_handle *method_root = NULL;
    cx_bool attached = cx_gc_enter_thread();
    int outcome = -1;
    if (!attached) goto completed;
    race_root = cx_gc_add_root(&race, (cx_uint)sizeof(race));
    request_root = cx_gc_add_root(&request, (cx_uint)sizeof(request));
    uri_root = cx_gc_add_root(&uri, (cx_uint)sizeof(uri));
    method_root = cx_gc_add_root(&method, (cx_uint)sizeof(method));
    if (race_root == NULL || request_root == NULL || uri_root == NULL ||
        method_root == NULL) {
        request_race_increment(&race->failed);
        goto cleanup;
    }
    request_race_increment(&race->ready);
    while (!request_race_load(&race->start)) {
#ifdef _WIN32
        Sleep(0u);
#else
        usleep(100u);
#endif
    }
    if (worker->operation == 0) {
        outcome = request_send_attempt(request);
        if (outcome == 1) request_race_increment(&race->send_started);
    } else if (worker->operation == 1) {
        outcome = request_set_method_attempt(request, method);
    } else {
        outcome = request_set_uri_attempt(request, uri);
    }
    if (outcome == 1 && worker->operation != 0) {
        request_race_increment(&race->mutation_succeeded);
    } else if (outcome == 0) {
        request_race_increment(&race->mutation_rejected);
    } else if (outcome < 0) {
        request_race_increment(&race->failed);
    }
cleanup:
    if (method_root != NULL) cx_gc_remove_root(method_root);
    if (uri_root != NULL) cx_gc_remove_root(uri_root);
    if (request_root != NULL) cx_gc_remove_root(request_root);
    if (race_root != NULL) cx_gc_remove_root(race_root);
    cx_gc_leave_thread(attached);
completed:
    request_race_increment(&race->completed);
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

static int start_request_state_race_worker(test_thread *thread,
    struct request_state_race_worker *worker) {
#ifdef _WIN32
    *thread = CreateThread(NULL, 0, request_state_race_worker, worker, 0, NULL);
    return *thread != NULL;
#else
    return pthread_create(thread, NULL, request_state_race_worker, worker) == 0;
#endif
}

static int content_read_stream_attempt(
    struct cxnet_System_Net_Http_ByteArrayContent *content,
    struct cxcore_System_Threading_Tasks_Task **task_out) {
    struct cx_exception_frame frame;
    struct cxcore_System_Exception * volatile exception = NULL;
    volatile int result = -1;
    *task_out = NULL;
    cx_exception_push(&frame);
    if (setjmp(frame.environment) == 0) {
        *task_out = cxnet_System_Net_Http_HttpContent_ReadAsStreamAsync(
            (struct cxnet_System_Net_Http_HttpContent *)content);
        cx_exception_pop(&frame);
        result = *task_out == NULL ? -1 : 1;
    } else {
        exception = (struct cxcore_System_Exception *)cx_exception_current();
        cx_exception_pop(&frame);
        cx_exception_clear();
        result = exception != NULL &&
            cxcore_System_Exception_Matches(exception,
                cxcore_System_ObjectDisposedException___typeinfo) ? 0 : -1;
    }
    return result;
}

static int content_dispose_attempt(
    struct cxnet_System_Net_Http_ByteArrayContent *content) {
    struct cx_exception_frame frame;
    cx_exception_push(&frame);
    if (setjmp(frame.environment) == 0) {
        cxnet_System_Net_Http_HttpContent_Dispose(
            (struct cxnet_System_Net_Http_HttpContent *)content);
        cx_exception_pop(&frame);
        return 1;
    }
    cx_exception_pop(&frame);
    cx_exception_clear();
    return -1;
}

#ifdef _WIN32
static DWORD WINAPI content_stream_read_race_worker(void *argument) {
#else
static void *content_stream_read_race_worker(void *argument) {
#endif
    struct content_stream_read_race_worker *worker =
        (struct content_stream_read_race_worker *)argument;
    struct content_stream_read_race *race = worker->race;
    struct cxnet_System_Net_Http_ByteArrayContent *content = race->content;
    struct cxcore_System_Threading_Tasks_Task *task = NULL;
    struct cx_gc_root_handle *race_root = NULL;
    struct cx_gc_root_handle *content_root = NULL;
    struct cx_gc_root_handle *task_root = NULL;
    cx_bool attached = cx_gc_enter_thread();
    if (!attached) goto completed;
    race_root = cx_gc_add_root(&race, (cx_uint)sizeof(race));
    content_root = cx_gc_add_root(&content, (cx_uint)sizeof(content));
    if (race_root == NULL || content_root == NULL) {
        request_race_increment(&race->failed);
        goto cleanup;
    }
    request_race_increment(&race->ready);
    while (!request_race_load(&race->start)) {
#ifdef _WIN32
        Sleep(0u);
#else
        usleep(100u);
#endif
    }
    if (worker->index == 2) {
        if (content_dispose_attempt(content) == 1) {
            request_race_store(&race->dispose_completed, 1);
        } else {
            request_race_increment(&race->failed);
        }
    } else {
        int outcome = content_read_stream_attempt(content, &task);
        if (outcome == 0) {
            request_race_store(&race->disposed_reads[worker->index], 1);
        } else if (outcome < 0) {
            request_race_increment(&race->failed);
        } else {
            race->tasks[worker->index] = task;
            task_root = cx_gc_add_root(&task, (cx_uint)sizeof(task));
            if (task_root == NULL) {
                request_race_increment(&race->failed);
            }
        }
    }
cleanup:
    if (task_root != NULL) cx_gc_remove_root(task_root);
    if (content_root != NULL) cx_gc_remove_root(content_root);
    if (race_root != NULL) cx_gc_remove_root(race_root);
    cx_gc_leave_thread(attached);
completed:
    request_race_increment(&race->completed);
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

static int start_content_stream_read_race_worker(test_thread *thread,
    struct content_stream_read_race_worker *worker) {
#ifdef _WIN32
    *thread = CreateThread(NULL, 0, content_stream_read_race_worker, worker,
        0, NULL);
    return *thread != NULL;
#else
    return pthread_create(thread, NULL, content_stream_read_race_worker,
        worker) == 0;
#endif
}

static int response_owned_stream_read_attempt(
    struct cxnet_System_Net_Http_ResponseOwnedStream *stream,
    struct cxcore_System_Collections_Array *bytes) {
    struct cx_exception_frame frame;
    struct cxcore_System_Exception * volatile exception = NULL;
    volatile int result = -1;
    cx_exception_push(&frame);
    if (setjmp(frame.environment) == 0) {
        result = cxnet_System_Net_Http_ResponseOwnedStream_Read(stream, bytes,
            0u, 4u);
        cx_exception_pop(&frame);
    } else {
        exception = (struct cxcore_System_Exception *)cx_exception_current();
        cx_exception_pop(&frame);
        cx_exception_clear();
        result = exception != NULL &&
            cxcore_System_Exception_Matches(exception,
                cxcore_System_ObjectDisposedException___typeinfo) ? -2 : -1;
    }
    return result;
}

static int response_owned_stream_dispose_attempt(
    struct cxnet_System_Net_Http_ResponseOwnedStream *stream) {
    struct cx_exception_frame frame;
    cx_exception_push(&frame);
    if (setjmp(frame.environment) == 0) {
        cxnet_System_Net_Http_ResponseOwnedStream_Dispose(stream);
        cx_exception_pop(&frame);
        return 1;
    }
    cx_exception_pop(&frame);
    cx_exception_clear();
    return -1;
}

#ifdef _WIN32
static DWORD WINAPI response_owned_stream_race_worker(void *argument) {
#else
static void *response_owned_stream_race_worker(void *argument) {
#endif
    struct response_owned_stream_race_worker *worker =
        (struct response_owned_stream_race_worker *)argument;
    struct response_owned_stream_race *race = worker->race;
    struct cxnet_System_Net_Http_ResponseOwnedStream *stream = race->stream;
    struct cxcore_System_Collections_Array *bytes = race->bytes;
    struct cx_gc_root_handle *race_root = NULL;
    struct cx_gc_root_handle *stream_root = NULL;
    struct cx_gc_root_handle *bytes_root = NULL;
    cx_bool attached = cx_gc_enter_thread();
    if (!attached) goto completed;
    race_root = cx_gc_add_root(&race, (cx_uint)sizeof(race));
    stream_root = cx_gc_add_root(&stream, (cx_uint)sizeof(stream));
    bytes_root = cx_gc_add_root(&bytes, (cx_uint)sizeof(bytes));
    if (race_root == NULL || stream_root == NULL || bytes_root == NULL) {
        request_race_increment(&race->failed);
        goto cleanup;
    }
    request_race_increment(&race->ready);
    while (!request_race_load(&race->start)) {
#ifdef _WIN32
        Sleep(0u);
#else
        usleep(100u);
#endif
    }
    if (worker->operation == 0) {
        int outcome = response_owned_stream_read_attempt(stream, bytes);
        if (outcome >= 0) {
            request_race_store(&race->read_status, outcome == 4 ? 1 : -1);
            if (outcome != 4 ||
                memcmp(cx_array_at(bytes, 0u, (cx_uint)sizeof(cx_byte)),
                    "pong", 4u) != 0) {
                request_race_increment(&race->failed);
            }
        } else if (outcome == -2) {
            request_race_store(&race->read_status, 0);
        } else {
            request_race_store(&race->read_status, -1);
            request_race_increment(&race->failed);
        }
    } else if (response_owned_stream_dispose_attempt(stream) == 1) {
        request_race_store(&race->dispose_completed, 1);
    } else {
        request_race_increment(&race->failed);
    }
cleanup:
    if (bytes_root != NULL) cx_gc_remove_root(bytes_root);
    if (stream_root != NULL) cx_gc_remove_root(stream_root);
    if (race_root != NULL) cx_gc_remove_root(race_root);
    cx_gc_leave_thread(attached);
completed:
    request_race_increment(&race->completed);
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

static int start_response_owned_stream_race_worker(test_thread *thread,
    struct response_owned_stream_race_worker *worker) {
#ifdef _WIN32
    *thread = CreateThread(NULL, 0, response_owned_stream_race_worker,
        worker, 0, NULL);
    return *thread != NULL;
#else
    return pthread_create(thread, NULL, response_owned_stream_race_worker,
        worker) == 0;
#endif
}

static int response_attach_send_lifetime_attempt(
    struct cxnet_System_Net_Http_HttpResponseMessage *response,
    struct cxnet_System_Net_Http_HttpResponseMessage *lifetime) {
    struct cx_iface_ref lifetime_ref;
    struct cx_exception_frame frame;
    lifetime_ref.vtable =
        cxnet_System_Net_Http_HttpResponseMessage___iface_cxcore_System_IDisposable___vtable;
    lifetime_ref.instance = lifetime;
    lifetime_ref.owner = CX_NULL;
    cx_exception_push(&frame);
    if (setjmp(frame.environment) == 0) {
        cxnet_System_Net_Http_HttpResponseMessage_SetSendLifetime(response,
            lifetime_ref);
        cx_exception_pop(&frame);
        return 1;
    }
    cx_exception_pop(&frame);
    cx_exception_clear();
    return -1;
}

static int response_dispose_attempt(
    struct cxnet_System_Net_Http_HttpResponseMessage *response) {
    struct cx_exception_frame frame;
    cx_exception_push(&frame);
    if (setjmp(frame.environment) == 0) {
        cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
        cx_exception_pop(&frame);
        return 1;
    }
    cx_exception_pop(&frame);
    cx_exception_clear();
    return -1;
}

#ifdef _WIN32
static DWORD WINAPI response_send_lifetime_race_worker(void *argument) {
#else
static void *response_send_lifetime_race_worker(void *argument) {
#endif
    struct response_send_lifetime_race_worker *worker =
        (struct response_send_lifetime_race_worker *)argument;
    struct response_send_lifetime_race *race = worker->race;
    struct cxnet_System_Net_Http_HttpResponseMessage *response =
        race->response;
    struct cxnet_System_Net_Http_HttpResponseMessage *lifetime =
        race->lifetime;
    struct cx_gc_root_handle *race_root = NULL;
    struct cx_gc_root_handle *response_root = NULL;
    struct cx_gc_root_handle *lifetime_root = NULL;
    cx_bool attached = cx_gc_enter_thread();
    if (!attached) goto completed;
    race_root = cx_gc_add_root(&race, (cx_uint)sizeof(race));
    response_root = cx_gc_add_root(&response, (cx_uint)sizeof(response));
    lifetime_root = cx_gc_add_root(&lifetime, (cx_uint)sizeof(lifetime));
    if (race_root == NULL || response_root == NULL || lifetime_root == NULL) {
        request_race_increment(&race->failed);
        goto cleanup;
    }
    request_race_increment(&race->ready);
    while (!request_race_load(&race->start)) {
#ifdef _WIN32
        Sleep(0u);
#else
        usleep(100u);
#endif
    }
    if (worker->operation == 0) {
        if (response_attach_send_lifetime_attempt(response, lifetime) != 1)
            request_race_increment(&race->failed);
    } else if (response_dispose_attempt(response) != 1) {
        request_race_increment(&race->failed);
    }
cleanup:
    if (lifetime_root != NULL) cx_gc_remove_root(lifetime_root);
    if (response_root != NULL) cx_gc_remove_root(response_root);
    if (race_root != NULL) cx_gc_remove_root(race_root);
    cx_gc_leave_thread(attached);
completed:
    request_race_increment(&race->completed);
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

static int start_response_send_lifetime_race_worker(test_thread *thread,
    struct response_send_lifetime_race_worker *worker) {
#ifdef _WIN32
    *thread = CreateThread(NULL, 0, response_send_lifetime_race_worker,
        worker, 0, NULL);
    return *thread != NULL;
#else
    return pthread_create(thread, NULL, response_send_lifetime_race_worker,
        worker) == 0;
#endif
}

#ifdef _WIN32
static DWORD WINAPI request_send_race_worker(void *argument) {
#else
static void *request_send_race_worker(void *argument) {
#endif
    struct request_send_race *race = (struct request_send_race *)argument;
    struct cxnet_System_Net_Http_HttpRequestMessage *request = race->request;
    struct cx_gc_root_handle *request_root;
    cx_bool attached = cx_gc_enter_thread();
    int outcome;
    if (!attached) {
        request_race_increment(&race->failed);
        request_race_increment(&race->completed);
#ifdef _WIN32
        return 0;
#else
        return NULL;
#endif
    }
    request_root = cx_gc_add_root(&request, (cx_uint)sizeof(request));
    if (request_root == NULL) {
        request_race_increment(&race->failed);
        if (request_root != NULL) cx_gc_remove_root(request_root);
        cx_gc_leave_thread(attached);
        request_race_increment(&race->completed);
#ifdef _WIN32
        return 0;
#else
        return NULL;
#endif
    }
    request_race_increment(&race->ready);
    while (!request_race_load(&race->start)) {
#ifdef _WIN32
        Sleep(0u);
#else
        usleep(100u);
#endif
    }
    outcome = request_send_attempt(request);
    if (outcome == 1) {
        request_race_increment(&race->winners);
    } else if (outcome == 0) {
        request_race_increment(&race->rejected);
    } else {
        request_race_increment(&race->failed);
    }
    cx_gc_remove_root(request_root);
    cx_gc_leave_thread(attached);
    request_race_increment(&race->completed);
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

static int start_request_send_race_worker(test_thread *thread,
    struct request_send_race *race) {
#ifdef _WIN32
    *thread = CreateThread(NULL, 0, request_send_race_worker, race, 0, NULL);
    return *thread != NULL;
#else
    return pthread_create(thread, NULL, request_send_race_worker, race) == 0;
#endif
}

static void join_request_send_race_worker(test_thread thread) {
#ifdef _WIN32
    (void)WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
#else
    (void)pthread_join(thread, NULL);
#endif
}

#ifdef _WIN32
static DWORD WINAPI header_mutation_worker(void *argument) {
#else
static void *header_mutation_worker(void *argument) {
#endif
    struct header_mutation_race *race =
        (struct header_mutation_race *)argument;
    struct cxnet_System_Net_Http_HttpHeaders *headers = race->headers;
    struct cx_gc_root_handle *race_root = NULL;
    struct cx_gc_root_handle *headers_root = NULL;
    cx_bool attached = cx_gc_enter_thread();
    if (!attached) {
        request_race_store(&race->failed, 1);
        request_race_store(&race->completed, 1);
#ifdef _WIN32
        return 0;
#else
        return NULL;
#endif
    }
    race_root = cx_gc_add_root(&race, (cx_uint)sizeof(race));
    headers_root = cx_gc_add_root(&headers, (cx_uint)sizeof(headers));
    if (race_root == NULL || headers_root == NULL) {
        request_race_store(&race->failed, 1);
        goto cleanup;
    }
    request_race_store(&race->ready, 1);
    while (!request_race_load(&race->start)) {
#ifdef _WIN32
        Sleep(0u);
#else
        usleep(100u);
#endif
    }
    if (!cxnet_System_Net_Http_HttpHeaders_Remove(headers, race->raceName)) {
        request_race_store(&race->failed, 1);
    }
    cxnet_System_Net_Http_HttpHeaders_Add(headers, race->raceName,
        race->replacementValue);
    cxnet_System_Net_Http_HttpHeaders_Add(headers, race->lateName,
        race->lateValue);
cleanup:
    if (headers_root != NULL) cx_gc_remove_root(headers_root);
    if (race_root != NULL) cx_gc_remove_root(race_root);
    cx_gc_leave_thread(attached);
    request_race_store(&race->completed, 1);
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

static int start_header_mutation_worker(test_thread *thread,
    struct header_mutation_race *race) {
#ifdef _WIN32
    *thread = CreateThread(NULL, 0, header_mutation_worker, race, 0, NULL);
    return *thread != NULL;
#else
    return pthread_create(thread, NULL, header_mutation_worker, race) == 0;
#endif
}

#ifdef _WIN32
static DWORD WINAPI client_setting_worker(void *argument) {
#else
static void *client_setting_worker(void *argument) {
#endif
    struct client_setting_race *race =
        (struct client_setting_race *)argument;
    struct cxnet_System_Net_Http_HttpClient *client = race->client;
    struct cx_gc_root_handle *race_root = NULL;
    struct cx_gc_root_handle *client_root = NULL;
    cx_bool attached = cx_gc_enter_thread();
    if (!attached) {
        request_race_store(&race->failed, 1);
        request_race_store(&race->completed, 1);
#ifdef _WIN32
        return 0;
#else
        return NULL;
#endif
    }
    race_root = cx_gc_add_root(&race, (cx_uint)sizeof(race));
    client_root = cx_gc_add_root(&client, (cx_uint)sizeof(client));
    if (race_root == NULL || client_root == NULL) {
        request_race_store(&race->failed, 1);
        goto cleanup;
    }
    request_race_store(&race->ready, 1);
    while (!request_race_load(&race->start)) {
#ifdef _WIN32
        Sleep(0u);
#else
        usleep(100u);
#endif
    }
    cxnet_System_Net_Http_HttpClient_TimeoutMilliseconds___set(client,
        race->newTimeoutMilliseconds);
    cxnet_System_Net_Http_HttpClient_MaxResponseContentBufferSize___set(client,
        race->newMaxResponseContentBufferSize);
    cxnet_System_Net_Http_HttpClient_UseCookies___set(client,
        (cx_bool)race->newUseCookies);
    cxnet_System_Net_Http_HttpClient_MaxConnectionsPerServer___set(client,
        race->newMaxConnectionsPerServer);
    cxnet_System_Net_Http_HttpClient_PooledConnectionIdleTimeoutMilliseconds___set(
        client, race->newPooledConnectionIdleTimeoutMilliseconds);
    cxnet_System_Net_Http_HttpClient_PooledConnectionLifetimeMilliseconds___set(
        client, race->newPooledConnectionLifetimeMilliseconds);
    cxnet_System_Net_Http_HttpClient_Proxy___set(client, race->newProxy);
    cxnet_System_Net_Http_HttpClient_ProxyBypassList___set(client,
        race->newProxyBypassList);
    cxnet_System_Net_Http_HttpClient_ProxyCredentials___set(client,
        race->newProxyCredentials);
    cxnet_System_Net_Http_HttpClient_BaseAddress___set(client,
        race->newBaseAddress);
cleanup:
    if (client_root != NULL) cx_gc_remove_root(client_root);
    if (race_root != NULL) cx_gc_remove_root(race_root);
    cx_gc_leave_thread(attached);
    request_race_store(&race->completed, 1);
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

static int start_client_setting_worker(test_thread *thread,
    struct client_setting_race *race) {
#ifdef _WIN32
    *thread = CreateThread(NULL, 0, client_setting_worker, race, 0, NULL);
    return *thread != NULL;
#else
    return pthread_create(thread, NULL, client_setting_worker, race) == 0;
#endif
}

static void client_dispose_on_scheduler(void *environment) {
    struct client_dispose_action *action =
        (struct client_dispose_action *)environment;
    action->started = 1;
    action->dispose_started = 1;
    cxnet_System_Net_Http_HttpClient_Dispose(action->client);
    action->dispose_started = 0;
    action->fired = 1;
}

static int run_queued_client_dispose_case(void) {
    struct limit_fixture fixture;
    struct client_dispose_action *dispose_action;
    cx_action scheduled_dispose = {0};
    test_thread thread;
    uint16_t port = 0u;
    char url[160];
    struct cxcore_System_Uri *uri;
    struct cxnet_System_Net_Http_HttpClient *client;
    struct cxcore_System_Threading_Tasks_Task *first;
    struct cxcore_System_Threading_Tasks_Task *second;
    uint32_t steps = 0u;
    uint64_t deadline;
    int fixture_started = 0;
    int passed = 0;
    if (!start_limit_fixture(&fixture, &port, &thread, 0, 1)) return 0;
    fixture_started = 1;
    (void)snprintf(url, sizeof(url), "http://127.0.0.1:%u/cx-http-limit",
        (unsigned)port);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri, make_string(url));
    client = (struct cxnet_System_Net_Http_HttpClient *)cxcore_System_Memory_Alloc(
        cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    cxnet_System_Net_Http_HttpClient_MaxConnectionsPerServer___set(client, 1u);
    first = cxnet_System_Net_Http_HttpClient_GetAsync__2(client, uri);
    deadline = test_monotonic_milliseconds() + 30000u;
    while (fixture.accepted_connections == 0 &&
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(first) &&
        test_monotonic_milliseconds() < deadline) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (fixture.failed || fixture.accepted_connections != 1 ||
        cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(first)) {
        fprintf(stderr,
            "first request did not occupy the connection slot failed=%d accepted=%d completed=%d canceled=%d faulted=%d steps=%u\n",
            fixture.failed, fixture.accepted_connections,
            cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(first),
            cxcore_System_Threading_Tasks_Task_IsCanceled___const_get(first),
            first->_isFaulted,
            steps);
        goto cleanup;
    }
    second = cxnet_System_Net_Http_HttpClient_GetAsync__2(client, uri);
    if (cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(second)) {
        fputs("queued request completed before client disposal\n", stderr);
        goto cleanup;
    }
    dispose_action = (struct client_dispose_action *)cxcore_System_Memory_Alloc(
        (cx_uint)sizeof(struct client_dispose_action));
    if (dispose_action == NULL) {
        fputs("could not allocate queued client-disposal action\n", stderr);
        goto cleanup;
    }
    cx_gc_store_reference((cx_ptr *)&dispose_action->client, (cx_ptr)client);
    dispose_action->started = 0;
    dispose_action->dispose_started = 0;
    dispose_action->fired = 0;
    dispose_action->failed = 0;
    scheduled_dispose.__function = client_dispose_on_scheduler;
    scheduled_dispose.__environment = dispose_action;
    scheduled_dispose.__target = (cx_ptr)dispose_action;
    cxcore_System_Threading_Tasks_TaskScheduler_PostAfter(50u,
        scheduled_dispose);
    while ((!dispose_action->fired ||
            !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(first) ||
            !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(second)) &&
        steps < 400000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    while (!fixture.client_closed && steps < 450000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    passed = dispose_action->fired && fixture.client_closed && !fixture.failed &&
        fixture.accepted_connections == 1 &&
        !fixture.accepted_before_first_closed &&
        cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(first) &&
        cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(second) &&
        cxnet_System_Net_Http_CurlHttpTransport_WasCanceled(first) &&
        cxnet_System_Net_Http_CurlHttpTransport_WasCanceled(second);
    if (!passed) {
        fprintf(stderr,
            "queued client disposal failed disposed=%d closed=%d failed=%d accepted=%d early=%d first=%d firstCanceled=%d second=%d secondCanceled=%d\n",
            dispose_action->fired, fixture.client_closed, fixture.failed,
            fixture.accepted_connections, fixture.accepted_before_first_closed,
            cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(first),
            cxnet_System_Net_Http_CurlHttpTransport_WasCanceled(first),
            cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(second),
            cxnet_System_Net_Http_CurlHttpTransport_WasCanceled(second));
    }
cleanup:
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    if (fixture_started) stop_limit_fixture(&fixture, thread);
    return passed;
}

#ifdef _WIN32
static DWORD WINAPI client_dispose_on_thread(void *environment) {
#else
static void *client_dispose_on_thread(void *environment) {
#endif
    struct client_dispose_action *action =
        (struct client_dispose_action *)environment;
    struct cxnet_System_Net_Http_HttpClient *client = NULL;
    struct cx_gc_root_handle *action_root = NULL;
    struct cx_gc_root_handle *client_root = NULL;
    request_race_store(&action->started, 1);
    cx_bool attached = cx_gc_enter_thread();
    if (!attached) {
        request_race_store(&action->failed, 1);
        request_race_store(&action->fired, 1);
        goto completed;
    }
    action_root = cx_gc_add_root(&action, (cx_uint)sizeof(action));
    if (action_root == NULL) goto leave_thread;
    client = action->client;
    client_root = cx_gc_add_root(&client, (cx_uint)sizeof(client));
    if (client_root != NULL) {
        request_race_store(&action->dispose_started, 1);
        cxnet_System_Net_Http_HttpClient_Dispose(client);
        request_race_store(&action->dispose_started, 0);
        cx_gc_remove_root(client_root);
    } else {
        request_race_store(&action->failed, 1);
    }
    cx_gc_remove_root(action_root);
leave_thread:
    cx_gc_leave_thread(attached);
completed:
    if (attached) request_race_store(&action->fired, 1);
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

static int run_concurrent_request_send_case(void) {
    struct request_send_race race;
    test_thread workers[2];
    int worker_started[2] = {0, 0};
    struct cxcore_System_Uri *uri = NULL;
    struct cxnet_System_Net_Http_HttpRequestMessage *request = NULL;
    struct cx_gc_root_handle *uri_root = NULL;
    struct cx_gc_root_handle *request_root = NULL;
    uint32_t steps = 0u;
    int result = 0;
    memset(&race, 0, sizeof(race));
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    uri_root = cx_gc_add_root(&uri, (cx_uint)sizeof(uri));
    cxcore_System_Uri___constructor(uri,
        make_string("http://localhost/cx-http-concurrent-request-send"));
    request = (struct cxnet_System_Net_Http_HttpRequestMessage *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpRequestMessage___typeinfo.Size);
    request_root = cx_gc_add_root(&request, (cx_uint)sizeof(request));
    cxnet_System_Net_Http_HttpRequestMessage___constructor(request,
        cxnet_System_Net_Http_HttpMethod_Get___const_get(), uri);
    race.request = request;
    if (!start_request_send_race_worker(&workers[0], &race)) {
        fputs("could not start first request-reservation worker\n", stderr);
        goto cleanup;
    }
    worker_started[0] = 1;
    if (!start_request_send_race_worker(&workers[1], &race)) {
        fputs("could not start second request-reservation worker\n", stderr);
        goto cleanup;
    }
    worker_started[1] = 1;
    while (request_race_load(&race.ready) < 2 && steps < 200000u) {
#ifdef _WIN32
        Sleep(1u);
#else
        usleep(1000u);
#endif
        steps++;
    }
    if (request_race_load(&race.ready) != 2) {
        fputs("request-send workers did not reach the barrier\n", stderr);
        goto cleanup;
    }
    request_race_store(&race.start, 1);
    while (request_race_load(&race.completed) < 2 && steps < 400000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (request_race_load(&race.completed) != 2 ||
        request_race_load(&race.winners) != 1 ||
        request_race_load(&race.rejected) != 1 ||
        request_race_load(&race.failed) != 0 || !request->_sendInProgress) {
        fprintf(stderr,
            "concurrent request reservation failed: completed=%d winners=%d rejected=%d failed=%d inProgress=%d\n",
            request_race_load(&race.completed),
            request_race_load(&race.winners),
            request_race_load(&race.rejected),
            request_race_load(&race.failed), request->_sendInProgress);
        goto cleanup;
    }
    cxnet_System_Net_Http_HttpRequestMessage_EndSend(request);
    if (request->_sendInProgress) {
        fputs("request EndSend did not release the winning reservation\n",
            stderr);
        goto cleanup;
    }
    cxnet_System_Net_Http_HttpRequestMessage_Method___set(request,
        cxnet_System_Net_Http_HttpMethod_Get___const_get());
    cxnet_System_Net_Http_HttpRequestMessage_RequestUri___set(request, uri);
    result = 1;
cleanup:
    for (int index = 0; index < 2; index++) {
        if (worker_started[index]) {
            join_request_send_race_worker(workers[index]);
            worker_started[index] = 0;
        }
    }
    if (request != NULL) cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
    if (request_root != NULL) cx_gc_remove_root(request_root);
    if (uri_root != NULL) cx_gc_remove_root(uri_root);
    return result;
}

static int run_concurrent_request_mutation_send_case(void) {
    struct request_state_race race;
    struct request_state_race_worker operations[3];
    test_thread workers[3];
    int worker_started[3] = {0, 0, 0};
    struct cxcore_System_Uri *initial_uri = NULL;
    struct cxcore_System_Uri *replacement_uri = NULL;
    struct cxnet_System_Net_Http_HttpMethod *replacement_method = NULL;
    struct cxnet_System_Net_Http_HttpRequestMessage *request = NULL;
    struct cx_gc_root_handle *initial_uri_root = NULL;
    struct cx_gc_root_handle *replacement_uri_root = NULL;
    struct cx_gc_root_handle *replacement_method_root = NULL;
    struct cx_gc_root_handle *request_root = NULL;
    uint32_t steps = 0u;
    int result = 0;
    memset(&race, 0, sizeof(race));
    initial_uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    initial_uri_root = cx_gc_add_root(&initial_uri, (cx_uint)sizeof(initial_uri));
    replacement_uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    replacement_uri_root = cx_gc_add_root(&replacement_uri,
        (cx_uint)sizeof(replacement_uri));
    cxcore_System_Uri___constructor(initial_uri,
        make_string("http://localhost/cx-http-request-mutation-original"));
    cxcore_System_Uri___constructor(replacement_uri,
        make_string("http://localhost/cx-http-request-mutation-replacement"));
    replacement_method = cxnet_System_Net_Http_HttpMethod_Post___const_get();
    replacement_method_root = cx_gc_add_root(&replacement_method,
        (cx_uint)sizeof(replacement_method));
    request = (struct cxnet_System_Net_Http_HttpRequestMessage *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpRequestMessage___typeinfo.Size);
    request_root = cx_gc_add_root(&request, (cx_uint)sizeof(request));
    cxnet_System_Net_Http_HttpRequestMessage___constructor(request,
        cxnet_System_Net_Http_HttpMethod_Get___const_get(), initial_uri);
    race.request = request;
    race.uri = replacement_uri;
    race.method = replacement_method;
    for (int index = 0; index < 3; index++) {
        operations[index].race = &race;
        operations[index].operation = index;
        if (!start_request_state_race_worker(&workers[index],
                &operations[index])) {
            fprintf(stderr,
                "could not start request mutation/send worker %d\n", index);
            goto cleanup;
        }
        worker_started[index] = 1;
    }
    while (request_race_load(&race.ready) < 3 &&
        request_race_load(&race.completed) == 0 && steps < 200000u) {
#ifdef _WIN32
        Sleep(1u);
#else
        usleep(1000u);
#endif
        steps++;
    }
    if (request_race_load(&race.ready) != 3) {
        fputs("request mutation/send workers did not reach the barrier\n",
            stderr);
        goto cleanup;
    }
    request_race_store(&race.start, 1);
    while (request_race_load(&race.completed) < 3 && steps < 400000u) {
#ifdef _WIN32
        Sleep(1u);
#else
        usleep(1000u);
#endif
        steps++;
    }
    if (request_race_load(&race.completed) != 3 ||
        request_race_load(&race.send_started) != 1 ||
        request_race_load(&race.mutation_succeeded) +
            request_race_load(&race.mutation_rejected) != 2 ||
        request_race_load(&race.failed) != 0 || !request->_sendInProgress) {
        fprintf(stderr,
            "concurrent request mutation/send failed: completed=%d send=%d mutationSucceeded=%d mutationRejected=%d failed=%d inProgress=%d\n",
            request_race_load(&race.completed),
            request_race_load(&race.send_started),
            request_race_load(&race.mutation_succeeded),
            request_race_load(&race.mutation_rejected),
            request_race_load(&race.failed), request->_sendInProgress);
        goto cleanup;
    }
    cxnet_System_Net_Http_HttpRequestMessage_EndSend(request);
    if (request->_sendInProgress ||
        request_set_method_attempt(request, replacement_method) != 1 ||
        request_set_uri_attempt(request, replacement_uri) != 1) {
        fputs("request mutation did not recover after concurrent send admission\n",
            stderr);
        goto cleanup;
    }
    result = 1;
cleanup:
    request_race_store(&race.start, 1);
    for (int index = 0; index < 3; index++) {
        if (worker_started[index]) {
            join_request_send_race_worker(workers[index]);
            worker_started[index] = 0;
        }
    }
    if (request != NULL) {
        if (request->_sendInProgress)
            cxnet_System_Net_Http_HttpRequestMessage_EndSend(request);
        cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
    }
    if (request_root != NULL) cx_gc_remove_root(request_root);
    if (replacement_method_root != NULL)
        cx_gc_remove_root(replacement_method_root);
    if (replacement_uri_root != NULL) cx_gc_remove_root(replacement_uri_root);
    if (initial_uri_root != NULL) cx_gc_remove_root(initial_uri_root);
    return result;
}

static int run_concurrent_content_stream_read_case(void) {
    struct content_stream_read_race race;
    struct content_stream_read_race_worker operations[2];
    test_thread workers[2];
    int worker_started[2] = {0, 0};
    struct cxcore_System_Collections_Array *bytes = cx_array_with_type(
        cx_array_new(4u, (cx_uint)sizeof(cx_byte)),
        "cx_generic_9c4b0dcf5365274a639812f507806a7475845f1a6e6b8c085532c41d4210ef4b");
    struct cxnet_System_Net_Http_ByteArrayContent *content =
        (struct cxnet_System_Net_Http_ByteArrayContent *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_ByteArrayContent___typeinfo.Size);
    struct cxcore_System_Threading_Tasks_Task *task = NULL;
    struct cxcore_System_IO_Stream *stream = NULL;
    struct cx_gc_root_handle *bytes_root = cx_gc_add_root(
        &bytes, (cx_uint)sizeof(bytes));
    struct cx_gc_root_handle *content_root = cx_gc_add_root(
        &content, (cx_uint)sizeof(content));
    struct cx_gc_root_handle *task_root = NULL;
    struct cx_gc_root_handle *stream_root = NULL;
    uint32_t steps = 0u;
    int result = 0;
    memset(&race, 0, sizeof(race));
    memcpy(cx_array_at(bytes, 0u, (cx_uint)sizeof(cx_byte)), "race", 4u);
    cxnet_System_Net_Http_ByteArrayContent___constructor(content, bytes);
    race.content = content;
    for (int index = 0; index < 2; index++) {
        operations[index].race = &race;
        operations[index].index = index;
        if (!start_content_stream_read_race_worker(&workers[index],
                &operations[index])) {
            fprintf(stderr, "could not start content-stream reader %d\n",
                index);
            goto cleanup;
        }
        worker_started[index] = 1;
    }
    while (request_race_load(&race.ready) < 2 &&
        request_race_load(&race.completed) == 0 && steps < 200000u) {
#ifdef _WIN32
        Sleep(1u);
#else
        usleep(1000u);
#endif
        steps++;
    }
    if (request_race_load(&race.ready) != 2) {
        fputs("content-stream readers did not reach the barrier\n", stderr);
        goto cleanup;
    }
    request_race_store(&race.start, 1);
    while (request_race_load(&race.completed) < 2 && steps < 400000u) {
#ifdef _WIN32
        Sleep(1u);
#else
        usleep(1000u);
#endif
        steps++;
    }
    if (request_race_load(&race.completed) != 2 ||
        request_race_load(&race.failed) != 0 || race.tasks[0] == NULL ||
        race.tasks[0] != race.tasks[1] || !race.tasks[0]->_isCompleted ||
        race.tasks[0]->_isFaulted || race.tasks[0]->_isCanceled) {
        fprintf(stderr,
            "concurrent ReadAsStreamAsync calls diverged: completed=%d failed=%d task0=%p task1=%p\n",
            request_race_load(&race.completed),
            request_race_load(&race.failed), (void *)race.tasks[0],
            (void *)race.tasks[1]);
        goto cleanup;
    }
    task = race.tasks[0];
    task_root = cx_gc_add_root(&task, (cx_uint)sizeof(task));
    stream = cx_generic_b7a000e5a1e82c2cc28bb7d06efab618c555997e8e29f1d0e05edd0237738424(
        task);
    stream_root = cx_gc_add_root(&stream, (cx_uint)sizeof(stream));
    if (task_root == NULL || stream_root == NULL || stream == NULL ||
        stream != cx_generic_b7a000e5a1e82c2cc28bb7d06efab618c555997e8e29f1d0e05edd0237738424(
            race.tasks[1])) {
        fputs("concurrent ReadAsStreamAsync calls did not share one stream\n",
            stderr);
        goto cleanup;
    }
    cxnet_System_Net_Http_HttpContent_Dispose(
        (struct cxnet_System_Net_Http_HttpContent *)content);
    if (cxcore_System_IO_MemoryStream_Read(
            (struct cxcore_System_IO_MemoryStream *)stream, bytes, 0u, 4u) != 4 ||
        memcmp(cx_array_at(bytes, 0u, (cx_uint)sizeof(cx_byte)), "race", 4u) != 0) {
        fputs("shared content stream changed after content disposal\n", stderr);
        goto cleanup;
    }
    result = 1;
cleanup:
    request_race_store(&race.start, 1);
    for (int index = 0; index < 2; index++) {
        if (worker_started[index]) {
            join_request_send_race_worker(workers[index]);
            worker_started[index] = 0;
        }
    }
    if (stream != NULL) {
        cxcore_System_IO_MemoryStream_Dispose(
            (struct cxcore_System_IO_MemoryStream *)stream);
    }
    if (content != NULL) {
        cxnet_System_Net_Http_HttpContent_Dispose(
            (struct cxnet_System_Net_Http_HttpContent *)content);
    }
    if (stream_root != NULL) cx_gc_remove_root(stream_root);
    if (task_root != NULL) cx_gc_remove_root(task_root);
    if (content_root != NULL) cx_gc_remove_root(content_root);
    if (bytes_root != NULL) cx_gc_remove_root(bytes_root);
    return result;
}

static int run_concurrent_content_stream_dispose_case(void) {
    struct content_stream_read_race race;
    struct content_stream_read_race_worker operations[3];
    test_thread workers[3];
    int worker_started[3] = {0, 0, 0};
    struct cxcore_System_Collections_Array *bytes = cx_array_with_type(
        cx_array_new(4u, (cx_uint)sizeof(cx_byte)),
        "cx_generic_9c4b0dcf5365274a639812f507806a7475845f1a6e6b8c085532c41d4210ef4b");
    struct cxnet_System_Net_Http_ByteArrayContent *content =
        (struct cxnet_System_Net_Http_ByteArrayContent *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_ByteArrayContent___typeinfo.Size);
    struct cxcore_System_Threading_Tasks_Task *tasks[2] = {NULL, NULL};
    struct cxcore_System_IO_Stream *streams[2] = {NULL, NULL};
    struct cx_gc_root_handle *bytes_root = cx_gc_add_root(
        &bytes, (cx_uint)sizeof(bytes));
    struct cx_gc_root_handle *content_root = cx_gc_add_root(
        &content, (cx_uint)sizeof(content));
    struct cx_gc_root_handle *task_roots[2] = {NULL, NULL};
    struct cx_gc_root_handle *stream_roots[2] = {NULL, NULL};
    uint32_t steps = 0u;
    int result = 0;
    memset(&race, 0, sizeof(race));
    memcpy(cx_array_at(bytes, 0u, (cx_uint)sizeof(cx_byte)), "race", 4u);
    cxnet_System_Net_Http_ByteArrayContent___constructor(content, bytes);
    race.content = content;
    for (int index = 0; index < 3; index++) {
        operations[index].race = &race;
        operations[index].index = index;
        if (!start_content_stream_read_race_worker(&workers[index],
                &operations[index])) {
            fprintf(stderr, "could not start content read/dispose worker %d\n",
                index);
            goto cleanup;
        }
        worker_started[index] = 1;
    }
    while (request_race_load(&race.ready) < 3 &&
        request_race_load(&race.completed) == 0 && steps < 200000u) {
#ifdef _WIN32
        Sleep(1u);
#else
        usleep(1000u);
#endif
        steps++;
    }
    if (request_race_load(&race.ready) != 3) {
        fputs("content read/dispose workers did not reach the barrier\n",
            stderr);
        goto cleanup;
    }
    request_race_store(&race.start, 1);
    while (request_race_load(&race.completed) < 3 && steps < 400000u) {
#ifdef _WIN32
        Sleep(1u);
#else
        usleep(1000u);
#endif
        steps++;
    }
    if (request_race_load(&race.completed) != 3 ||
        request_race_load(&race.failed) != 0 || !race.dispose_completed) {
        fprintf(stderr,
            "content read/dispose race failed: completed=%d failed=%d disposed=%d\n",
            request_race_load(&race.completed),
            request_race_load(&race.failed), race.dispose_completed);
        goto cleanup;
    }
    for (int index = 0; index < 2; index++) {
        tasks[index] = race.tasks[index];
        if (tasks[index] != NULL) {
            task_roots[index] = cx_gc_add_root(&tasks[index],
                (cx_uint)sizeof(tasks[index]));
            if (task_roots[index] == NULL || !tasks[index]->_isCompleted ||
                tasks[index]->_isCanceled ||
                (tasks[index]->_isFaulted &&
                    !cxcore_System_Exception_Matches(tasks[index]->_exception,
                        cxcore_System_ObjectDisposedException___typeinfo))) {
                fputs("content stream task had an invalid disposal-race outcome\n",
                    stderr);
                goto cleanup;
            }
            if (!tasks[index]->_isFaulted) {
                streams[index] = cx_generic_b7a000e5a1e82c2cc28bb7d06efab618c555997e8e29f1d0e05edd0237738424(
                    tasks[index]);
                stream_roots[index] = cx_gc_add_root(&streams[index],
                    (cx_uint)sizeof(streams[index]));
                if (streams[index] == NULL || stream_roots[index] == NULL ||
                    (index > 0 && tasks[index] == tasks[0] &&
                        streams[index] != streams[0]) ||
                    ((index == 0 || streams[index] != streams[0]) &&
                        (cxcore_System_IO_MemoryStream_Read(
                            (struct cxcore_System_IO_MemoryStream *)streams[index],
                            bytes, 0u, 4u) != 4 ||
                        memcmp(cx_array_at(bytes, 0u, (cx_uint)sizeof(cx_byte)),
                            "race", 4u) != 0))) {
                    fputs("successful content stream was not usable after disposal\n",
                        stderr);
                    goto cleanup;
                }
            }
        } else if (!race.disposed_reads[index]) {
            fputs("content read produced neither a task nor disposal rejection\n",
                stderr);
            goto cleanup;
        }
    }
    if (tasks[0] != NULL && tasks[1] != NULL && tasks[0] != tasks[1]) {
        fputs("concurrent content reads published different tasks\n", stderr);
        goto cleanup;
    }
    result = 1;
cleanup:
    request_race_store(&race.start, 1);
    for (int index = 0; index < 3; index++) {
        if (worker_started[index]) {
            join_request_send_race_worker(workers[index]);
            worker_started[index] = 0;
        }
    }
    for (int index = 0; index < 2; index++) {
        if (streams[index] != NULL &&
            (index == 0 || streams[index] != streams[0])) {
            cxcore_System_IO_MemoryStream_Dispose(
                (struct cxcore_System_IO_MemoryStream *)streams[index]);
        }
        if (stream_roots[index] != NULL) cx_gc_remove_root(stream_roots[index]);
        if (task_roots[index] != NULL) cx_gc_remove_root(task_roots[index]);
    }
    if (content != NULL) {
        cxnet_System_Net_Http_HttpContent_Dispose(
            (struct cxnet_System_Net_Http_HttpContent *)content);
    }
    if (content_root != NULL) cx_gc_remove_root(content_root);
    if (bytes_root != NULL) cx_gc_remove_root(bytes_root);
    return result;
}

static int run_response_owned_stream_dispose_race_case(void) {
    struct response_owned_stream_race race;
    struct response_owned_stream_race_worker operations[2];
    test_thread workers[2];
    int worker_started[2] = {0, 0};
    struct cxcore_System_Collections_Array *bytes = cx_array_with_type(
        cx_array_new(4u, (cx_uint)sizeof(cx_byte)),
        "cx_generic_9c4b0dcf5365274a639812f507806a7475845f1a6e6b8c085532c41d4210ef4b");
    struct cxcore_System_IO_MemoryStream *source =
        (struct cxcore_System_IO_MemoryStream *)cxcore_System_Memory_Alloc(
            cxcore_System_IO_MemoryStream___typeinfo.Size);
    struct cxnet_System_Net_Http_StreamContent *content =
        (struct cxnet_System_Net_Http_StreamContent *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_StreamContent___typeinfo.Size);
    struct cxnet_System_Net_Http_HttpResponseMessage *response =
        (struct cxnet_System_Net_Http_HttpResponseMessage *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpResponseMessage___typeinfo.Size);
    struct cxnet_System_Net_Http_ResponseOwnedStream *stream =
        (struct cxnet_System_Net_Http_ResponseOwnedStream *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_ResponseOwnedStream___typeinfo.Size);
    struct cx_gc_root_handle *bytes_root = cx_gc_add_root(
        &bytes, (cx_uint)sizeof(bytes));
    struct cx_gc_root_handle *source_root = cx_gc_add_root(
        &source, (cx_uint)sizeof(source));
    struct cx_gc_root_handle *content_root = cx_gc_add_root(
        &content, (cx_uint)sizeof(content));
    struct cx_gc_root_handle *response_root = cx_gc_add_root(
        &response, (cx_uint)sizeof(response));
    struct cx_gc_root_handle *stream_root = cx_gc_add_root(
        &stream, (cx_uint)sizeof(stream));
    uint32_t steps = 0u;
    int result = 0;
    memset(&race, 0, sizeof(race));
    race.read_status = -1;
    memcpy(cx_array_at(bytes, 0u, (cx_uint)sizeof(cx_byte)), "pong", 4u);
    cxcore_System_IO_MemoryStream___constructor__3(source, bytes);
    cxnet_System_Net_Http_StreamContent___constructor__2(content,
        (struct cxcore_System_IO_Stream *)source, 16u, 0);
    cxnet_System_Net_Http_HttpResponseMessage___constructor(response);
    cxnet_System_Net_Http_HttpResponseMessage_Content___set(response,
        (struct cxnet_System_Net_Http_HttpContent *)content);
    cxnet_System_Net_Http_ResponseOwnedStream___constructor(stream,
        cxnet_System_Net_Http_StreamContent_ReadAsStream(content), response);
    race.stream = stream;
    race.bytes = bytes;
    for (int index = 0; index < 2; index++) {
        operations[index].race = &race;
        operations[index].operation = index;
        if (!start_response_owned_stream_race_worker(&workers[index],
                &operations[index])) {
            fprintf(stderr, "could not start response-owned stream worker %d\n",
                index);
            goto cleanup;
        }
        worker_started[index] = 1;
    }
    while (request_race_load(&race.ready) < 2 &&
        request_race_load(&race.completed) == 0 && steps < 200000u) {
#ifdef _WIN32
        Sleep(1u);
#else
        usleep(1000u);
#endif
        steps++;
    }
    if (request_race_load(&race.ready) != 2) {
        fputs("response-owned stream workers did not reach the barrier\n",
            stderr);
        goto cleanup;
    }
    request_race_store(&race.start, 1);
    while (request_race_load(&race.completed) < 2 && steps < 400000u) {
#ifdef _WIN32
        Sleep(1u);
#else
        usleep(1000u);
#endif
        steps++;
    }
    if (request_race_load(&race.completed) != 2 ||
        request_race_load(&race.failed) != 0 || !race.dispose_completed ||
        (race.read_status != 0 && race.read_status != 1) ||
        cxnet_System_Net_Http_ResponseOwnedStream_CanRead___get(stream) ||
        response_owned_stream_read_attempt(stream, bytes) != -2) {
        fprintf(stderr,
            "response-owned stream disposal race failed: completed=%d failed=%d disposed=%d read=%d\n",
            request_race_load(&race.completed),
            request_race_load(&race.failed), race.dispose_completed,
            race.read_status);
        goto cleanup;
    }
    result = 1;
cleanup:
    request_race_store(&race.start, 1);
    for (int index = 0; index < 2; index++) {
        if (worker_started[index]) {
            join_request_send_race_worker(workers[index]);
            worker_started[index] = 0;
        }
    }
    if (stream != NULL)
        cxnet_System_Net_Http_ResponseOwnedStream_Dispose(stream);
    else if (response != NULL)
        cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
    if (content != NULL)
        cxnet_System_Net_Http_HttpContent_Dispose(
            (struct cxnet_System_Net_Http_HttpContent *)content);
    if (source != NULL) cxcore_System_IO_MemoryStream_Dispose(source);
    if (stream_root != NULL) cx_gc_remove_root(stream_root);
    if (response_root != NULL) cx_gc_remove_root(response_root);
    if (content_root != NULL) cx_gc_remove_root(content_root);
    if (source_root != NULL) cx_gc_remove_root(source_root);
    if (bytes_root != NULL) cx_gc_remove_root(bytes_root);
    return result;
}

static int run_multipart_serialization_dispose_race_case(void) {
    static const char array_type_name[] =
        "cx_generic_9c4b0dcf5365274a639812f507806a7475845f1a6e6b8c085532c41d4210ef4b";
    struct multipart_serialization_race race;
    test_thread worker;
    struct cxcore_System_Collections_Array *input = cx_array_with_type(
        cx_array_new(64u * 1024u, (cx_uint)sizeof(cx_byte)),
        array_type_name);
    struct cxnet_System_Net_Http_ByteArrayContent *part =
        (struct cxnet_System_Net_Http_ByteArrayContent *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_ByteArrayContent___typeinfo.Size);
    struct cxnet_System_Net_Http_MultipartFormDataContent *content =
        (struct cxnet_System_Net_Http_MultipartFormDataContent *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_MultipartFormDataContent___typeinfo.Size);
    struct cx_gc_root_handle *input_root = cx_gc_add_root(
        &input, (cx_uint)sizeof(input));
    struct cx_gc_root_handle *part_root = cx_gc_add_root(
        &part, (cx_uint)sizeof(part));
    struct cx_gc_root_handle *content_root = cx_gc_add_root(
        &content, (cx_uint)sizeof(content));
    uint32_t steps = 0u;
    memset(&race, 0, sizeof(race));
    race.content = content;
    race.read_status = -1;
    cxnet_System_Net_Http_ByteArrayContent___constructor(part, input);
    cxnet_System_Net_Http_MultipartFormDataContent___constructor(
        content, make_string("m26-race-boundary"));
    cxnet_System_Net_Http_MultipartFormDataContent_Add(content,
        (struct cxnet_System_Net_Http_HttpContent *)part,
        make_string("payload"));
    if (!start_multipart_serialization_worker(&worker, &race)) {
        fputs("could not start multipart serialization worker\n", stderr);
        goto failed;
    }
    while (!request_race_load(&race.ready) &&
        !request_race_load(&race.completed) && steps < 200000u) {
#ifdef _WIN32
        Sleep(1u);
#else
        usleep(1000u);
#endif
        steps++;
    }
    if (!request_race_load(&race.ready)) {
        fputs("multipart serialization worker did not reach its barrier\n",
            stderr);
        join_multipart_serialization_worker(worker);
        goto failed;
    }
    request_race_store(&race.start, 1);
#ifdef _WIN32
    Sleep(1u);
#else
    usleep(1000u);
#endif
    cxnet_System_Net_Http_MultipartFormDataContent_Dispose(content);
    request_race_store(&race.dispose_completed, 1);
    while (!request_race_load(&race.completed) && steps < 400000u) {
#ifdef _WIN32
        Sleep(1u);
#else
        usleep(1000u);
#endif
        steps++;
    }
    join_multipart_serialization_worker(worker);
    if (!request_race_load(&race.completed) ||
        request_race_load(&race.failed) || !race.dispose_completed ||
        (race.read_status != 0 && race.read_status != 1) ||
        (race.read_status == 1 &&
            cxcore_System_Collections_Array_Length___const_get(race.serialized) <
                cxcore_System_Collections_Array_Length___const_get(input)) ||
        multipart_read_attempt(&race) != 0) {
        fprintf(stderr,
            "multipart serialization/disposal race failed: completed=%d failed=%d disposed=%d read=%d\n",
            request_race_load(&race.completed),
            request_race_load(&race.failed), race.dispose_completed,
            race.read_status);
        goto failed;
    }
    cx_gc_remove_root(content_root);
    cx_gc_remove_root(part_root);
    cx_gc_remove_root(input_root);
    return 1;

failed:
    cxnet_System_Net_Http_MultipartFormDataContent_Dispose(content);
    cx_gc_remove_root(content_root);
    cx_gc_remove_root(part_root);
    cx_gc_remove_root(input_root);
    return 0;
}

static int run_multipart_dispose_failure_case(void) {
    static const char array_type_name[] =
        "cx_generic_9c4b0dcf5365274a639812f507806a7475845f1a6e6b8c085532c41d4210ef4b";
    struct cxcore_System_Collections_Array *first_bytes = cx_array_with_type(
        cx_array_new(0u, (cx_uint)sizeof(cx_byte)), array_type_name);
    struct cxcore_System_Collections_Array *second_bytes = cx_array_with_type(
        cx_array_new(0u, (cx_uint)sizeof(cx_byte)), array_type_name);
    struct cxnet_System_Net_Http_ByteArrayContent *first =
        (struct cxnet_System_Net_Http_ByteArrayContent *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_ByteArrayContent___typeinfo.Size);
    struct cxnet_System_Net_Http_ByteArrayContent *second =
        (struct cxnet_System_Net_Http_ByteArrayContent *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_ByteArrayContent___typeinfo.Size);
    struct cxnet_System_Net_Http_MultipartFormDataContent *multipart =
        (struct cxnet_System_Net_Http_MultipartFormDataContent *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_MultipartFormDataContent___typeinfo.Size);
    struct cx_gc_root_handle *first_bytes_root = cx_gc_add_root(
        &first_bytes, (cx_uint)sizeof(first_bytes));
    struct cx_gc_root_handle *second_bytes_root = cx_gc_add_root(
        &second_bytes, (cx_uint)sizeof(second_bytes));
    struct cx_gc_root_handle *first_root = cx_gc_add_root(
        &first, (cx_uint)sizeof(first));
    struct cx_gc_root_handle *second_root = cx_gc_add_root(
        &second, (cx_uint)sizeof(second));
    struct cx_gc_root_handle *multipart_root = cx_gc_add_root(
        &multipart, (cx_uint)sizeof(multipart));
    struct cxcore_System_Exception *exception = NULL;
    struct cxcore_System_String *first_name = make_string("first");
    struct cxcore_System_String *second_name = make_string("second");
    int first_failure_propagated = 0;
    int second_dispose_succeeded = 0;

    cxnet_System_Net_Http_ByteArrayContent___constructor(first, first_bytes);
    cxnet_System_Net_Http_ByteArrayContent___constructor(second, second_bytes);
    cxnet_System_Net_Http_MultipartFormDataContent___constructor(
        multipart, make_string("m26-dispose-failure-boundary"));
    cxnet_System_Net_Http_MultipartFormDataContent_Add(multipart,
        (struct cxnet_System_Net_Http_HttpContent *)first, first_name);
    cxnet_System_Net_Http_MultipartFormDataContent_Add(multipart,
        (struct cxnet_System_Net_Http_HttpContent *)second, second_name);
    cxnet_System_Net_Http_ByteArrayContent_TestInjectDisposeFailure();

    {
        struct cx_exception_frame frame;
        cx_exception_push(&frame);
        if (setjmp(frame.environment) == 0) {
            cxnet_System_Net_Http_MultipartFormDataContent_Dispose(multipart);
            cx_exception_pop(&frame);
        } else {
            exception = (struct cxcore_System_Exception *)cx_exception_current();
            cx_exception_pop(&frame);
            cx_exception_clear();
            first_failure_propagated = exception != NULL &&
                cxcore_System_Exception_Matches(exception,
                    cxcore_System_InvalidOperationException___typeinfo) &&
                string_equals(cxcore_System_Exception_Message___const_get(
                    exception),
                    "Injected redirected request-content disposal failure.");
        }
    }

    {
        struct cx_exception_frame frame;
        cx_exception_push(&frame);
        if (setjmp(frame.environment) == 0) {
            cxnet_System_Net_Http_MultipartFormDataContent_Dispose(multipart);
            cx_exception_pop(&frame);
            second_dispose_succeeded = 1;
        } else {
            cx_exception_pop(&frame);
            cx_exception_clear();
        }
    }

    if (!first_failure_propagated || !second_dispose_succeeded ||
        !cxnet_System_Net_Http_ByteArrayContent_TestDisposeFailureConsumed() ||
        !multipart->_partsDisposed || multipart->_parts != NULL ||
        cxcore_System_Threading_AtomicInt_Value___const_get(
            &first->__base._disposed) == 0 ||
        cxcore_System_Threading_AtomicInt_Value___const_get(
            &second->__base._disposed) == 0) {
        fprintf(stderr,
            "multipart disposal failure cleanup failed: propagated=%d repeated=%d injected=%d first=%d second=%d detached=%d\n",
            first_failure_propagated, second_dispose_succeeded,
            cxnet_System_Net_Http_ByteArrayContent_TestDisposeFailureConsumed(),
            cxcore_System_Threading_AtomicInt_Value___const_get(
                &first->__base._disposed),
            cxcore_System_Threading_AtomicInt_Value___const_get(
                &second->__base._disposed), multipart->_partsDisposed);
        cxnet_System_Net_Http_MultipartFormDataContent_Dispose(multipart);
        cx_gc_remove_root(multipart_root);
        cx_gc_remove_root(second_root);
        cx_gc_remove_root(first_root);
        cx_gc_remove_root(second_bytes_root);
        cx_gc_remove_root(first_bytes_root);
        return 0;
    }

    cx_gc_remove_root(multipart_root);
    cx_gc_remove_root(second_root);
    cx_gc_remove_root(first_root);
    cx_gc_remove_root(second_bytes_root);
    cx_gc_remove_root(first_bytes_root);
    return 1;
}

static int run_response_send_lifetime_dispose_race_case(void) {
    struct response_send_lifetime_race race;
    struct response_send_lifetime_race_worker operations[2];
    test_thread workers[2];
    int worker_started[2] = {0, 0};
    struct cxnet_System_Net_Http_HttpResponseMessage *response =
        (struct cxnet_System_Net_Http_HttpResponseMessage *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpResponseMessage___typeinfo.Size);
    struct cxnet_System_Net_Http_HttpResponseMessage *lifetime =
        (struct cxnet_System_Net_Http_HttpResponseMessage *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpResponseMessage___typeinfo.Size);
    struct cx_gc_root_handle *response_root = cx_gc_add_root(
        &response, (cx_uint)sizeof(response));
    struct cx_gc_root_handle *lifetime_root = cx_gc_add_root(
        &lifetime, (cx_uint)sizeof(lifetime));
    uint32_t steps = 0u;
    int result = 0;
    memset(&race, 0, sizeof(race));
    cxnet_System_Net_Http_HttpResponseMessage___constructor(response);
    cxnet_System_Net_Http_HttpResponseMessage___constructor(lifetime);
    race.response = response;
    race.lifetime = lifetime;
    for (int index = 0; index < 2; index++) {
        operations[index].race = &race;
        operations[index].operation = index;
        if (!start_response_send_lifetime_race_worker(&workers[index],
                &operations[index])) {
            fprintf(stderr, "could not start send-lifetime race worker %d\n",
                index);
            goto cleanup;
        }
        worker_started[index] = 1;
    }
    while (request_race_load(&race.ready) < 2 &&
        request_race_load(&race.completed) == 0 && steps < 200000u) {
#ifdef _WIN32
        Sleep(1u);
#else
        usleep(1000u);
#endif
        steps++;
    }
    if (request_race_load(&race.ready) != 2) {
        fputs("send-lifetime workers did not reach the barrier\n", stderr);
        goto cleanup;
    }
    request_race_store(&race.start, 1);
    while (request_race_load(&race.completed) < 2 && steps < 400000u) {
#ifdef _WIN32
        Sleep(1u);
#else
        usleep(1000u);
#endif
        steps++;
    }
    if (request_race_load(&race.completed) != 2 ||
        request_race_load(&race.failed) != 0 || !response->_disposed ||
        response->_sendLifetime.instance != CX_NULL || !lifetime->_disposed) {
        fprintf(stderr,
            "send-lifetime attach/dispose race failed: completed=%d failed=%d responseDisposed=%d lifetimeDetached=%d lifetimeDisposed=%d\n",
            request_race_load(&race.completed),
            request_race_load(&race.failed), response->_disposed,
            response->_sendLifetime.instance == CX_NULL, lifetime->_disposed);
        goto cleanup;
    }
    result = 1;
cleanup:
    request_race_store(&race.start, 1);
    for (int index = 0; index < 2; index++) {
        if (worker_started[index]) {
            join_request_send_race_worker(workers[index]);
            worker_started[index] = 0;
        }
    }
    if (response != NULL)
        cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
    if (lifetime != NULL)
        cxnet_System_Net_Http_HttpResponseMessage_Dispose(lifetime);
    if (lifetime_root != NULL) cx_gc_remove_root(lifetime_root);
    if (response_root != NULL) cx_gc_remove_root(response_root);
    return result;
}

static size_t count_header_occurrences(const char *headers, const char *name) {
    size_t count = 0u;
    size_t name_length = strlen(name);
    const char *cursor = headers;
    while ((cursor = strstr(cursor, name)) != NULL) {
        count++;
        cursor += name_length;
    }
    return count;
}

static int run_concurrent_header_mutation_send_case(void) {
    struct fixture fixture;
    struct header_mutation_race race;
    test_thread server_thread;
    test_thread mutation_thread;
    uint16_t port = 0u;
    char url[160];
    struct cxcore_System_Uri *uri = NULL;
    struct cxnet_System_Net_Http_HttpClient *client = NULL;
    struct cxnet_System_Net_Http_HttpRequestMessage *request = NULL;
    struct cxnet_System_Net_Http_HttpHeaders *headers = NULL;
    struct cxnet_System_Net_Http_HttpResponseMessage *response = NULL;
    struct cxcore_System_Threading_Tasks_Task *task = NULL;
    struct cx_gc_root_handle *uri_root = NULL;
    struct cx_gc_root_handle *client_root = NULL;
    struct cx_gc_root_handle *request_root = NULL;
    struct cx_gc_root_handle *headers_root = NULL;
    struct cx_gc_root_handle *task_root = NULL;
    struct cx_gc_root_handle *response_root = NULL;
    struct cx_gc_root_handle *race_root = NULL;
    uint32_t steps = 0u;
    int fixture_started = 0;
    int mutation_started = 0;
    int result = 0;
    memset(&race, 0, sizeof(race));
    race_root = cx_gc_add_root(&race, (cx_uint)sizeof(race));
    if (race_root == NULL) return 0;
    if (!start_fixture(&fixture, &port, &server_thread, 0,
            "GET /cx-http-header-mutation-race HTTP/1.1", NULL, NULL)) {
        fputs("could not start header-mutation fixture\n", stderr);
        cx_gc_remove_root(race_root);
        return 0;
    }
    fixture_started = 1;
    fixture.hold_response = 1;
    (void)snprintf(url, sizeof(url),
        "http://127.0.0.1:%u/cx-http-header-mutation-race", (unsigned)port);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    uri_root = cx_gc_add_root(&uri, (cx_uint)sizeof(uri));
    if (uri == NULL || uri_root == NULL) goto cleanup;
    cxcore_System_Uri___constructor(uri, make_string(url));
    client = (struct cxnet_System_Net_Http_HttpClient *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    client_root = cx_gc_add_root(&client, (cx_uint)sizeof(client));
    if (client == NULL || client_root == NULL) goto cleanup;
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    request = (struct cxnet_System_Net_Http_HttpRequestMessage *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpRequestMessage___typeinfo.Size);
    request_root = cx_gc_add_root(&request, (cx_uint)sizeof(request));
    if (request == NULL || request_root == NULL) goto cleanup;
    cxnet_System_Net_Http_HttpRequestMessage___constructor(request,
        cxnet_System_Net_Http_HttpMethod_Get___const_get(), uri);
    headers = cxnet_System_Net_Http_HttpRequestMessage_Headers___const_get(
        request);
    headers_root = cx_gc_add_root(&headers, (cx_uint)sizeof(headers));
    if (headers == NULL || headers_root == NULL) goto cleanup;
    race.headers = headers;
    race.raceName = make_string("X-CX-Race");
    race.initialValue = make_string("before");
    race.replacementValue = make_string("after");
    race.lateName = make_string("X-CX-Late");
    race.lateValue = make_string("late");
    race.stableName = make_string("X-CX-Stable");
    race.stableValue = make_string("fixed");
    cxnet_System_Net_Http_HttpHeaders_Add(headers, race.raceName,
        race.initialValue);
    cxnet_System_Net_Http_HttpHeaders_Add(headers, race.stableName,
        race.stableValue);
    if (!start_header_mutation_worker(&mutation_thread,
            &race)) {
        fputs("could not start header-mutation worker\n", stderr);
        goto cleanup;
    }
    mutation_started = 1;
    while (!request_race_load(&race.ready) &&
        !request_race_load(&race.completed) && steps < 200000u) {
#ifdef _WIN32
        Sleep(1u);
#else
        usleep(1000u);
#endif
        steps++;
    }
    if (!request_race_load(&race.ready)) {
        fputs("header-mutation worker did not reach the send barrier\n", stderr);
        goto cleanup;
    }
    request_race_store(&race.start, 1);
    task = cxnet_System_Net_Http_HttpClient_SendAsync(client, request);
    task_root = cx_gc_add_root(&task, (cx_uint)sizeof(task));
    if (task == NULL || task_root == NULL) {
        fputs("could not root header-mutation send task\n", stderr);
        goto cleanup;
    }
    while ((!request_race_load(&race.completed) || !fixture.request_received) &&
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        steps < 400000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (request_race_load(&race.failed) || !request_race_load(&race.completed) ||
        !fixture.request_received || fixture.failed ||
        cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task)) {
        fputs("header mutation/send did not reach a stable in-flight snapshot\n",
            stderr);
        goto cleanup;
    }
    {
        size_t before_count = count_header_occurrences(fixture.observed_headers,
            "X-CX-Race: before\r\n");
        size_t after_count = count_header_occurrences(fixture.observed_headers,
            "X-CX-Race: after\r\n");
        size_t race_count = count_header_occurrences(fixture.observed_headers,
            "X-CX-Race: ");
        size_t late_count = count_header_occurrences(fixture.observed_headers,
            "X-CX-Late: late\r\n");
        if (before_count > 1u || after_count > 1u || race_count > 1u ||
            late_count > 1u || (before_count + after_count != race_count) ||
            (late_count != 0u && (after_count != 1u || before_count != 0u)) ||
            strstr(fixture.observed_headers,
                "X-CX-Stable: fixed\r\n") == NULL) {
            fputs("wire headers were not a valid atomic snapshot of the mutation\n",
                stderr);
            goto cleanup;
        }
    }
    if (!string_equals(cxnet_System_Net_Http_HttpHeaders_GetFirstValue(headers,
            race.raceName), "after") ||
        !string_equals(cxnet_System_Net_Http_HttpHeaders_GetFirstValue(headers,
            race.lateName), "late")) {
        fputs("concurrent header mutation was not retained on the request\n",
            stderr);
        goto cleanup;
    }
    fixture.release_response = 1;
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        steps < 600000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) ||
        task->_isFaulted || task->_isCanceled) {
        fputs("header mutation/send request did not finish successfully\n", stderr);
        goto cleanup;
    }
    response = cx_generic_640110dc1a526c1c94f059cac4ed8d246fe089fbe535a78bc1997c7adde4688b(
        task);
    response_root = cx_gc_add_root(&response, (cx_uint)sizeof(response));
    if (response == NULL || response_root == NULL) goto cleanup;
    if (!string_equals(cxnet_System_Net_Http_HttpContent_ReadAsString(
            cxnet_System_Net_Http_HttpResponseMessage_Content___get(response)),
            "pong")) {
        fputs("header mutation/send response body did not match\n", stderr);
        goto cleanup;
    }
    result = 1;
cleanup:
    if (fixture_started) fixture.release_response = 1;
    if (mutation_started) join_request_send_race_worker(mutation_thread);
    if (response != NULL) cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
    if (client != NULL) cxnet_System_Net_Http_HttpClient_Dispose(client);
    if (request != NULL) cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
    if (fixture_started) stop_fixture(&fixture, server_thread);
    if (response_root != NULL) cx_gc_remove_root(response_root);
    if (task_root != NULL) cx_gc_remove_root(task_root);
    if (race_root != NULL) cx_gc_remove_root(race_root);
    if (headers_root != NULL) cx_gc_remove_root(headers_root);
    if (request_root != NULL) cx_gc_remove_root(request_root);
    if (client_root != NULL) cx_gc_remove_root(client_root);
    if (uri_root != NULL) cx_gc_remove_root(uri_root);
    return result;
}

static int run_concurrent_client_setting_send_case(void) {
    struct fixture fixture;
    struct client_setting_race race;
    test_thread server_thread;
    test_thread setting_thread;
    uint16_t port = 0u;
    char url[160];
    char updatedBaseUrl[160];
    struct cxcore_System_Uri *uri = NULL;
    struct cxcore_System_Uri *baseAddress = NULL;
    struct cxcore_System_Uri *cookieUri = NULL;
    struct cxcore_System_Uri *effectiveUri = NULL;
    struct cxcore_System_String *effectiveHost = NULL;
    struct cxcore_System_String *updatedBaseHost = NULL;
    struct cxnet_System_Net_Http_HttpClient *client = NULL;
    struct cxnet_System_Net_Http_HttpRequestMessage *request = NULL;
    struct cxnet_System_Net_Http_HttpResponseMessage *response = NULL;
    struct cxcore_System_Threading_Tasks_Task *task = NULL;
    struct cx_gc_root_handle *uri_root = NULL;
    struct cx_gc_root_handle *client_root = NULL;
    struct cx_gc_root_handle *request_root = NULL;
    struct cx_gc_root_handle *task_root = NULL;
    struct cx_gc_root_handle *response_root = NULL;
    struct cx_gc_root_handle *race_root = NULL;
    struct cx_gc_root_handle *base_address_root = NULL;
    struct cx_gc_root_handle *cookie_uri_root = NULL;
    uint32_t steps = 0u;
    cx_bool capturedUseCookies = 0;
    int fixture_started = 0;
    int setting_started = 0;
    int result = 0;
    memset(&race, 0, sizeof(race));
    race_root = cx_gc_add_root(&race, (cx_uint)sizeof(race));
    if (race_root == NULL) return 0;
    if (!start_fixture(&fixture, &port, &server_thread, 0,
            "GET /cx-http-cookie-area/check HTTP/1.1", NULL, NULL)) {
        fputs("could not start client-setting fixture\n", stderr);
        cx_gc_remove_root(race_root);
        return 0;
    }
    fixture_started = 1;
    fixture.hold_response = 1;
    (void)snprintf(url, sizeof(url), "http://localhost:%u/", (unsigned)port);
    (void)snprintf(updatedBaseUrl, sizeof(updatedBaseUrl),
        "http://127.0.0.1:%u/", (unsigned)port);
    baseAddress = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    base_address_root = cx_gc_add_root(&baseAddress, (cx_uint)sizeof(baseAddress));
    if (baseAddress == NULL || base_address_root == NULL) goto cleanup;
    cxcore_System_Uri___constructor(baseAddress, make_string(url));
    race.newBaseAddress = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    if (race.newBaseAddress == NULL) goto cleanup;
    cxcore_System_Uri___constructor(race.newBaseAddress,
        make_string(updatedBaseUrl));
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    uri_root = cx_gc_add_root(&uri, (cx_uint)sizeof(uri));
    if (uri == NULL || uri_root == NULL) goto cleanup;
    cxcore_System_Uri___constructor(uri,
        make_string("cx-http-cookie-area/check"));
    cookieUri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cookie_uri_root = cx_gc_add_root(&cookieUri, (cx_uint)sizeof(cookieUri));
    if (cookieUri == NULL || cookie_uri_root == NULL) goto cleanup;
    cxcore_System_Uri___constructor__2(cookieUri, baseAddress,
        make_string("cx-http-cookie-area/check"));
    client = (struct cxnet_System_Net_Http_HttpClient *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    client_root = cx_gc_add_root(&client, (cx_uint)sizeof(client));
    if (client == NULL || client_root == NULL) goto cleanup;
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    cxnet_System_Net_Http_HttpClient_BaseAddress___set(client, baseAddress);
    cxnet_System_Net_Http_HttpClient_TimeoutMilliseconds___set(client,
        30000u);
    cxnet_System_Net_Http_HttpClient_MaxResponseContentBufferSize___set(client,
        512u);
    cxnet_System_Net_Http_HttpClient_UseCookies___set(client, 0);
    cxnet_System_Net_Http_HttpClient_MaxConnectionsPerServer___set(client, 2u);
    cxnet_System_Net_Http_HttpClient_PooledConnectionIdleTimeoutMilliseconds___set(
        client, 2000u);
    cxnet_System_Net_Http_HttpClient_PooledConnectionLifetimeMilliseconds___set(
        client, 300000u);
    cxnet_System_Net_Http_HttpClient_Proxy___set(client, NULL);
    cxnet_System_Net_Http_HttpClient_ProxyBypassList___set(client, NULL);
    cxnet_System_Net_Http_HttpClient_ProxyCredentials___set(client, NULL);
    cxnet_System_Net_CookieContainer_Add(
        cxnet_System_Net_Http_HttpClient_CookieContainer___get(client), cookieUri,
        make_string("session=blue; Path=/cx-http-cookie-area"));
    request = (struct cxnet_System_Net_Http_HttpRequestMessage *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpRequestMessage___typeinfo.Size);
    request_root = cx_gc_add_root(&request, (cx_uint)sizeof(request));
    if (request == NULL || request_root == NULL) goto cleanup;
    cxnet_System_Net_Http_HttpRequestMessage___constructor(request,
        cxnet_System_Net_Http_HttpMethod_Get___const_get(), uri);
    race.client = client;
    race.newTimeoutMilliseconds = 45000u;
    race.newMaxResponseContentBufferSize = 1024u;
    race.newUseCookies = 1;
    race.newMaxConnectionsPerServer = 3u;
    race.newPooledConnectionIdleTimeoutMilliseconds = 4000u;
    race.newPooledConnectionLifetimeMilliseconds = 600000u;
    race.newProxy = make_string("");
    race.newProxyBypassList = make_string("localhost,127.0.0.1");
    race.newProxyCredentials = (struct cxnet_System_Net_Http_ProxyCredentials *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_ProxyCredentials___typeinfo.Size);
    if (race.newProxyCredentials == NULL) goto cleanup;
    cxnet_System_Net_Http_ProxyCredentials___constructor(
        race.newProxyCredentials, make_string("race-user"),
        make_string("race-password"));
    if (!start_client_setting_worker(&setting_thread, &race)) {
        fputs("could not start client-setting worker\n", stderr);
        goto cleanup;
    }
    setting_started = 1;
    while (!request_race_load(&race.ready) &&
        !request_race_load(&race.completed) && steps < 200000u) {
#ifdef _WIN32
        Sleep(1u);
#else
        usleep(1000u);
#endif
        steps++;
    }
    if (!request_race_load(&race.ready)) {
        fputs("client-setting worker did not reach the send barrier\n", stderr);
        goto cleanup;
    }
    request_race_store(&race.start, 1);
    task = cxnet_System_Net_Http_HttpClient_SendAsync(client, request);
    task_root = cx_gc_add_root(&task, (cx_uint)sizeof(task));
    if (task == NULL || task_root == NULL) {
        fputs("could not root client-setting send task\n", stderr);
        goto cleanup;
    }
    while ((!request_race_load(&race.completed) || !fixture.request_received) &&
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        steps < 400000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    {
        cx_uint capturedTimeout =
            cxnet_System_Net_Http_HttpRequestMessage_TransportTimeoutMilliseconds___get(
                request);
        cx_uint capturedBufferLimit =
            cxnet_System_Net_Http_HttpRequestMessage_TransportMaxResponseContentBufferSize___get(
                request);
        cx_uint capturedMaxConnections =
            cxnet_System_Net_Http_HttpRequestMessage_TransportMaxConnectionsPerServer___get(
                request);
        cx_uint capturedIdleTimeout =
            cxnet_System_Net_Http_HttpRequestMessage_TransportPooledConnectionIdleTimeoutMilliseconds___get(
                request);
        cx_uint capturedLifetime =
            cxnet_System_Net_Http_HttpRequestMessage_TransportPooledConnectionLifetimeMilliseconds___get(
                request);
        struct cxcore_System_String *capturedProxy =
            cxnet_System_Net_Http_HttpRequestMessage_TransportProxy___get(
                request);
        struct cxcore_System_String *capturedProxyBypassList =
            cxnet_System_Net_Http_HttpRequestMessage_TransportProxyBypassList___get(
                request);
        struct cxcore_System_String *capturedProxyUsername =
            cxnet_System_Net_Http_HttpRequestMessage_TransportProxyUsername___get(
                request);
        struct cxcore_System_String *capturedProxyPassword =
            cxnet_System_Net_Http_HttpRequestMessage_TransportProxyPassword___get(
                request);
        capturedUseCookies =
            cxnet_System_Net_Http_HttpRequestMessage_TransportUseCookies___get(
                request);
        effectiveUri =
            cxnet_System_Net_Http_HttpRequestMessage_RequestUri___get(request);
        effectiveHost = cxcore_System_Uri_Host___const_get(effectiveUri);
        updatedBaseHost = cxcore_System_Uri_Host___const_get(
            cxnet_System_Net_Http_HttpClient_BaseAddress___get(client));
        if (request_race_load(&race.failed) ||
            !request_race_load(&race.completed) || !fixture.request_received ||
            fixture.failed ||
            cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) ||
            !((capturedTimeout >= 29900u && capturedTimeout <= 30000u) ||
              (capturedTimeout >= 44900u && capturedTimeout <= 45000u)) ||
            (capturedBufferLimit != 512u && capturedBufferLimit != 1024u) ||
            (capturedMaxConnections != 2u && capturedMaxConnections != 3u) ||
            (capturedIdleTimeout != 2000u && capturedIdleTimeout != 4000u) ||
            (capturedLifetime != 300000u && capturedLifetime != 600000u) ||
            (capturedProxy != NULL && !string_equals(capturedProxy, "")) ||
            (capturedProxyBypassList != NULL &&
             !string_equals(capturedProxyBypassList, "localhost,127.0.0.1")) ||
            !((capturedProxyUsername == NULL &&
               capturedProxyPassword == NULL) ||
              (string_equals(capturedProxyUsername, "race-user") &&
               string_equals(capturedProxyPassword, "race-password"))) ||
            (capturedUseCookies != 0 && capturedUseCookies != 1) ||
            (!string_equals(effectiveHost, "localhost") &&
             !string_equals(effectiveHost, "127.0.0.1")) ||
            !string_equals(updatedBaseHost, "127.0.0.1") ||
            cxnet_System_Net_Http_HttpClient_TimeoutMilliseconds___get(client) !=
                45000u ||
            cxnet_System_Net_Http_HttpClient_MaxResponseContentBufferSize___get(
                client) != 1024u ||
            cxnet_System_Net_Http_HttpClient_MaxConnectionsPerServer___get(
                client) != 3u ||
            cxnet_System_Net_Http_HttpClient_PooledConnectionIdleTimeoutMilliseconds___get(
                client) != 4000u ||
            cxnet_System_Net_Http_HttpClient_PooledConnectionLifetimeMilliseconds___get(
                client) != 600000u ||
            !string_equals(cxnet_System_Net_Http_HttpClient_Proxy___get(client), "") ||
            !string_equals(cxnet_System_Net_Http_HttpClient_ProxyBypassList___get(
                client), "localhost,127.0.0.1") ||
            cxnet_System_Net_Http_ProxyCredentials_Username___get(
                cxnet_System_Net_Http_HttpClient_ProxyCredentials___get(client)) == NULL ||
            !string_equals(cxnet_System_Net_Http_ProxyCredentials_Username___get(
                cxnet_System_Net_Http_HttpClient_ProxyCredentials___get(client)),
                "race-user") ||
            !string_equals(cxnet_System_Net_Http_ProxyCredentials_Password___get(
                cxnet_System_Net_Http_HttpClient_ProxyCredentials___get(client)),
                "race-password") ||
            !cxnet_System_Net_Http_HttpClient_UseCookies___get(client)) {
            fprintf(stderr,
                "client settings/send snapshot failed: worker-failed=%d worker-completed=%d request-received=%d task-completed=%d captured-timeout=%u captured-buffer-limit=%u captured-max-connections=%u captured-idle-timeout=%u captured-lifetime=%u captured-use-cookies=%d client-timeout=%u client-buffer-limit=%u client-max-connections=%u client-idle-timeout=%u client-lifetime=%u client-use-cookies=%d\n",
                request_race_load(&race.failed),
                request_race_load(&race.completed), fixture.request_received,
                cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task),
                capturedTimeout, capturedBufferLimit, capturedMaxConnections,
                capturedIdleTimeout, capturedLifetime, (int)capturedUseCookies,
                cxnet_System_Net_Http_HttpClient_TimeoutMilliseconds___get(client),
                cxnet_System_Net_Http_HttpClient_MaxResponseContentBufferSize___get(
                    client),
                cxnet_System_Net_Http_HttpClient_MaxConnectionsPerServer___get(
                    client),
                cxnet_System_Net_Http_HttpClient_PooledConnectionIdleTimeoutMilliseconds___get(
                    client),
                cxnet_System_Net_Http_HttpClient_PooledConnectionLifetimeMilliseconds___get(
                    client),
                (int)cxnet_System_Net_Http_HttpClient_UseCookies___get(client));
            goto cleanup;
        }
    }
    fixture.release_response = 1;
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        steps < 600000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) ||
        task->_isFaulted || task->_isCanceled) {
        fputs("client settings/send response did not complete successfully\n",
            stderr);
        goto cleanup;
    }
    response = cx_generic_640110dc1a526c1c94f059cac4ed8d246fe089fbe535a78bc1997c7adde4688b(
        task);
    response_root = cx_gc_add_root(&response, (cx_uint)sizeof(response));
    if (response == NULL || response_root == NULL) goto cleanup;
    {
        cx_bool expectedCookie = capturedUseCookies &&
            string_equals(effectiveHost, "localhost");
        if (!string_equals(cxnet_System_Net_Http_HttpContent_ReadAsString(
                cxnet_System_Net_Http_HttpResponseMessage_Content___get(response)),
                expectedCookie ? "found" : "missing")) {
            fputs("client settings/send behavior did not match its snapshot\n",
            stderr);
            goto cleanup;
        }
    }
    result = 1;
cleanup:
    if (fixture_started) fixture.release_response = 1;
    if (setting_started) join_request_send_race_worker(setting_thread);
    if (response != NULL) cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
    if (client != NULL) cxnet_System_Net_Http_HttpClient_Dispose(client);
    if (request != NULL) cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
    if (fixture_started) stop_fixture(&fixture, server_thread);
    if (response_root != NULL) cx_gc_remove_root(response_root);
    if (task_root != NULL) cx_gc_remove_root(task_root);
    if (request_root != NULL) cx_gc_remove_root(request_root);
    if (client_root != NULL) cx_gc_remove_root(client_root);
    if (uri_root != NULL) cx_gc_remove_root(uri_root);
    if (cookie_uri_root != NULL) cx_gc_remove_root(cookie_uri_root);
    if (base_address_root != NULL) cx_gc_remove_root(base_address_root);
    if (race_root != NULL) cx_gc_remove_root(race_root);
    return result;
}

static int run_client_dispose_send_case(int crossThread) {
    struct fixture fixture;
    struct client_dispose_action *dispose_action = NULL;
    test_thread server_thread;
    cx_action callback = {0};
    uint16_t port = 0u;
    char url[160];
    struct cxcore_System_Uri *uri = NULL;
    struct cxnet_System_Net_Http_HttpClient *client = NULL;
    struct cxnet_System_Net_Http_HttpRequestMessage *request = NULL;
    struct cxcore_System_Threading_Tasks_Task *task = NULL;
    struct cx_gc_root_handle *uri_root = NULL;
    struct cx_gc_root_handle *client_root = NULL;
    struct cx_gc_root_handle *request_root = NULL;
    struct cx_gc_root_handle *task_root = NULL;
    struct cx_gc_root_handle *dispose_action_root = NULL;
    test_thread dispose_thread;
    uint32_t steps = 0u;
    uint64_t request_start;
    uint64_t deadline;
    int fixture_started = 0;
    int dispose_thread_started = 0;
    int result = 0;
    if (!start_fixture(&fixture, &port, &server_thread, 1,
            "GET /cx-http-concurrent-client-dispose HTTP/1.1", NULL, NULL)) {
        fputs("could not start concurrent client-dispose fixture\n", stderr);
        return 0;
    }
    fixture_started = 1;
    (void)snprintf(url, sizeof(url),
        "http://127.0.0.1:%u/cx-http-concurrent-client-dispose", (unsigned)port);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    uri_root = cx_gc_add_root(&uri, (cx_uint)sizeof(uri));
    cxcore_System_Uri___constructor(uri, make_string(url));
    client = (struct cxnet_System_Net_Http_HttpClient *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    client_root = cx_gc_add_root(&client, (cx_uint)sizeof(client));
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    request = (struct cxnet_System_Net_Http_HttpRequestMessage *)
        cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_HttpRequestMessage___typeinfo.Size);
    request_root = cx_gc_add_root(&request, (cx_uint)sizeof(request));
    cxnet_System_Net_Http_HttpRequestMessage___constructor(request,
        cxnet_System_Net_Http_HttpMethod_Get___const_get(), uri);
    task = cxnet_System_Net_Http_HttpClient_SendAsync(client, request);
    task_root = cx_gc_add_root(&task, (cx_uint)sizeof(task));
    request_start = test_monotonic_milliseconds();
    deadline = request_start + 30000u;
    while (!fixture.request_received &&
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        steps < 200000u && test_monotonic_milliseconds() < deadline) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (!fixture.request_received ||
        cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) ||
        fixture.failed || test_monotonic_milliseconds() > deadline) {
        fprintf(stderr,
            "request did not remain pending before concurrent client disposal: received=%d completed=%d canceled=%d faulted=%d fixtureFailed=%d elapsedMs=%llu steps=%u\n",
            fixture.request_received,
            cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task),
            cxcore_System_Threading_Tasks_Task_IsCanceled___const_get(task),
            task->_isFaulted, fixture.failed,
            (unsigned long long)(test_monotonic_milliseconds() - request_start),
            steps);
        goto cleanup;
    }
    deadline = test_monotonic_milliseconds() + 10000u;
    dispose_action = (struct client_dispose_action *)
        cxcore_System_Memory_Alloc(
            (cx_uint)sizeof(struct client_dispose_action));
    if (dispose_action == NULL) {
        fputs("could not allocate client-dispose action\n", stderr);
        goto cleanup;
    }
    dispose_action_root = cx_gc_add_root(&dispose_action,
        (cx_uint)sizeof(dispose_action));
    dispose_action->client = client;
    dispose_action->started = 0;
    dispose_action->dispose_started = 0;
    dispose_action->fired = 0;
    dispose_action->failed = 0;
    fixture.expect_client_close = 1;
    if (crossThread) {
#ifdef _WIN32
        dispose_thread = CreateThread(NULL, 0, client_dispose_on_thread,
            dispose_action, 0, NULL);
        if (dispose_thread == NULL) {
            fputs("could not start cross-thread client-dispose worker\n", stderr);
            goto cleanup;
        }
#else
        if (pthread_create(&dispose_thread, NULL, client_dispose_on_thread,
                dispose_action) != 0) {
            fputs("could not start cross-thread client-dispose worker\n", stderr);
            goto cleanup;
        }
#endif
        dispose_thread_started = 1;
    } else {
        callback.__function = client_dispose_on_scheduler;
        callback.__environment = dispose_action;
        callback.__target = (cx_ptr)dispose_action;
        cxcore_System_Threading_Tasks_TaskScheduler_PostAfter(1u, callback);
    }
    while ((!request_race_load(&dispose_action->fired) ||
            !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) ||
            !fixture.client_closed) && steps < 400000u &&
        test_monotonic_milliseconds() < deadline) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    while (request->_sendInProgress && steps < 450000u &&
        test_monotonic_milliseconds() < deadline) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) {
#ifdef _WIN32
            Sleep(1u);
#else
            usleep(1000u);
#endif
        }
        steps++;
    }
    if (!request_race_load(&dispose_action->fired) ||
        request_race_load(&dispose_action->failed) ||
        !cxcore_System_Threading_Tasks_Task_IsCanceled___const_get(task) ||
        !cxnet_System_Net_Http_CurlHttpTransport_WasCanceled(task) ||
        !fixture.client_closed || fixture.failed || request->_sendInProgress ||
        test_monotonic_milliseconds() > deadline) {
        fprintf(stderr,
            "client-dispose state: cross-thread=%d started=%d disposing=%d fired=%d failed=%d completed=%d canceled=%d transport-canceled=%d closed=%d fixture-failed=%d request-in-progress=%d\n",
            crossThread,
            request_race_load(&dispose_action->started),
            request_race_load(&dispose_action->dispose_started),
            request_race_load(&dispose_action->fired),
            request_race_load(&dispose_action->failed),
            cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task),
            cxcore_System_Threading_Tasks_Task_IsCanceled___const_get(task),
            cxnet_System_Net_Http_CurlHttpTransport_WasCanceled(task),
            fixture.client_closed,
            fixture.failed,
            request->_sendInProgress);
        fputs("concurrent client disposal did not cancel and release the active send\n",
            stderr);
        goto cleanup;
    }
    cxnet_System_Net_Http_HttpRequestMessage_Method___set(request,
        cxnet_System_Net_Http_HttpMethod_Get___const_get());
    result = 1;
cleanup:
    if (fixture_started) fixture.expect_client_close = 1;
    if (dispose_thread_started) {
#ifdef _WIN32
        (void)WaitForSingleObject(dispose_thread, INFINITE);
        CloseHandle(dispose_thread);
#else
        (void)pthread_join(dispose_thread, NULL);
#endif
    }
    if (client != NULL) cxnet_System_Net_Http_HttpClient_Dispose(client);
    if (fixture_started) {
        if (!fixture.request_received) {
            test_socket wake_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            struct sockaddr_in wake_address;
            static const char wake_request[] =
                "GET /cx-http-concurrent-client-dispose HTTP/1.1\r\n"
                "Host: localhost\r\nConnection: close\r\n\r\n";
            memset(&wake_address, 0, sizeof(wake_address));
            wake_address.sin_family = AF_INET;
            wake_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            wake_address.sin_port = htons(port);
            if (wake_socket != (test_socket)-1 &&
                connect(wake_socket, (struct sockaddr *)&wake_address,
                    (test_socklen)sizeof(wake_address)) == 0) {
#ifdef _WIN32
                (void)send(wake_socket, wake_request,
                    (int)(sizeof(wake_request) - 1u), 0);
                closesocket(wake_socket);
#else
                (void)send(wake_socket, wake_request,
                    sizeof(wake_request) - 1u, 0);
                close(wake_socket);
#endif
            } else if (wake_socket != (test_socket)-1) {
#ifdef _WIN32
                closesocket(wake_socket);
#else
                close(wake_socket);
#endif
            }
        }
        stop_fixture(&fixture, server_thread);
    }
    if (request != NULL) cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
    if (dispose_action_root != NULL) cx_gc_remove_root(dispose_action_root);
    if (task_root != NULL) cx_gc_remove_root(task_root);
    if (request_root != NULL) cx_gc_remove_root(request_root);
    if (client_root != NULL) cx_gc_remove_root(client_root);
    if (uri_root != NULL) cx_gc_remove_root(uri_root);
    return result;
}

static int run_concurrent_client_dispose_send_case(void) {
    return run_client_dispose_send_case(0);
}

static int run_cross_thread_client_dispose_send_case(void) {
    return run_client_dispose_send_case(1);
}

static int expect_multipart_add_argument_error(
    struct cxnet_System_Net_Http_MultipartFormDataContent *multipart,
    struct cxnet_System_Net_Http_HttpContent *content,
    struct cxcore_System_String *name,
    struct cxcore_System_String *file_name,
    const char *expected_parameter) {
    struct cx_exception_frame frame;
    struct cxcore_System_Exception *exception;
    const struct cxcore_System_String *actual_parameter;
    size_t expected_length = strlen(expected_parameter);
    int matches;

    cx_exception_push(&frame);
    if (setjmp(frame.environment) == 0) {
        if (file_name == NULL) {
            cxnet_System_Net_Http_MultipartFormDataContent_Add(
                multipart, content, name);
        } else {
            cxnet_System_Net_Http_MultipartFormDataContent_Add__2(
                multipart, content, name, file_name);
        }
        cx_exception_pop(&frame);
        return 0;
    }

    exception = (struct cxcore_System_Exception *)cx_exception_current();
    actual_parameter = cxcore_System_ArgumentException_ParamName___const_get(
        (const struct cxcore_System_ArgumentException *)exception);
    matches = cxcore_System_Exception_Matches(exception,
            cxcore_System_ArgumentException___typeinfo) &&
        actual_parameter != NULL && actual_parameter->_length == expected_length &&
        memcmp(actual_parameter->_data, expected_parameter, expected_length) == 0;
    cx_exception_pop(&frame);
    cx_exception_clear();
    return matches;
}

static int run_multipart_disposition_validation_case(void) {
    static const char *const invalid_values[] = {
        "\x01", "\t", "\x7f", "quote\"", "slash\\"
    };
    struct cxnet_System_Net_Http_MultipartFormDataContent *multipart =
        (struct cxnet_System_Net_Http_MultipartFormDataContent *)
            cxcore_System_Memory_Alloc(
                cxnet_System_Net_Http_MultipartFormDataContent___typeinfo.Size);
    struct cxnet_System_Net_Http_StringContent *string_content =
        (struct cxnet_System_Net_Http_StringContent *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Http_StringContent___typeinfo.Size);
    struct cxcore_System_String *boundary = NULL;
    struct cxcore_System_String *payload = NULL;
    struct cxcore_System_String *name = NULL;
    struct cxcore_System_String *file_name = NULL;
    struct cx_gc_root_handle *multipart_root = cx_gc_add_root(
        &multipart, (cx_uint)sizeof(multipart));
    struct cx_gc_root_handle *content_root = cx_gc_add_root(
        &string_content, (cx_uint)sizeof(string_content));
    struct cx_gc_root_handle *boundary_root = cx_gc_add_root(
        &boundary, (cx_uint)sizeof(boundary));
    struct cx_gc_root_handle *payload_root = cx_gc_add_root(
        &payload, (cx_uint)sizeof(payload));
    struct cx_gc_root_handle *name_root = cx_gc_add_root(
        &name, (cx_uint)sizeof(name));
    struct cx_gc_root_handle *file_name_root = cx_gc_add_root(
        &file_name, (cx_uint)sizeof(file_name));
    size_t index;
    int passed = 1;

    boundary = make_string("cxnet-validation-boundary");
    cxnet_System_Net_Http_MultipartFormDataContent___constructor(
        multipart, boundary);
    payload = make_string("payload");
    cxnet_System_Net_Http_StringContent___constructor(
        string_content, payload);
    name = NULL;
    if (!expect_multipart_add_argument_error(multipart,
            (struct cxnet_System_Net_Http_HttpContent *)string_content,
            name, NULL, "name")) {
        fputs("multipart accepted a null field name or reported the wrong parameter\n",
            stderr);
        passed = 0;
        goto cleanup;
    }

    name = make_string("");
    if (!expect_multipart_add_argument_error(multipart,
            (struct cxnet_System_Net_Http_HttpContent *)string_content,
            name, NULL, "name")) {
        fputs("multipart accepted an empty field name or reported the wrong parameter\n",
            stderr);
        passed = 0;
        goto cleanup;
    }
    name = make_string("field");
    for (index = 0u;
         index < sizeof(invalid_values) / sizeof(invalid_values[0]);
         index += 1u) {
        name = make_string(invalid_values[index]);
        if (!expect_multipart_add_argument_error(multipart,
                (struct cxnet_System_Net_Http_HttpContent *)string_content,
                name, NULL, "name")) {
            fprintf(stderr,
                "multipart accepted invalid field name at case %u or reported the wrong parameter\n",
                (unsigned)index);
            passed = 0;
            goto cleanup;
        }
    }

    name = make_string("field");
    for (index = 0u;
         index < sizeof(invalid_values) / sizeof(invalid_values[0]);
         index += 1u) {
        file_name = make_string(invalid_values[index]);
        if (!expect_multipart_add_argument_error(multipart,
                (struct cxnet_System_Net_Http_HttpContent *)string_content,
                name, file_name, "fileName")) {
            fprintf(stderr,
                "multipart accepted invalid filename at case %u or reported the wrong parameter\n",
                (unsigned)index);
            passed = 0;
            goto cleanup;
        }
    }

cleanup:
    cxnet_System_Net_Http_HttpContent_Dispose(
        (struct cxnet_System_Net_Http_HttpContent *)string_content);
    cxnet_System_Net_Http_MultipartFormDataContent_Dispose(multipart);
    cx_gc_remove_root(file_name_root);
    cx_gc_remove_root(name_root);
    cx_gc_remove_root(payload_root);
    cx_gc_remove_root(boundary_root);
    cx_gc_remove_root(content_root);
    cx_gc_remove_root(multipart_root);
    return passed;
}

int main(int argc, char **argv) {
    struct fixture fixture;
    test_thread thread;
    uint16_t port = 0u;
    char url[160];
    struct cxcore_System_Uri *uri;
    struct cxnet_System_Net_Http_HttpClient *client;
    struct cxcore_System_Threading_Tasks_Task *task;
    struct cxnet_System_Net_Http_HttpResponseMessage *response;
    struct cxnet_System_Net_Http_HttpContent *content;
    struct cxcore_System_Threading_Tasks_CancellationTokenSource *cancellationSource;
    struct cxcore_System_Threading_Tasks_CancellationToken cancellationToken;
    cx_action cancelAction = {0};
    uint32_t steps = 0u;
    static const uint32_t upload_cancel_delays_ms[] = {
        0u, 1u, 5u, 15u, 34u
    };

    if (argc == 2 && strcmp(argv[1], "--direct-curl-handler-send") == 0) {
        cx_gc_check_thread();
        return run_direct_curl_handler_send_case(0) ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1],
            "--ambiguous-framing-connection-recovery") == 0) {
        int mode;
        cx_gc_check_thread();
        for (mode = 2; mode <= 16; mode++) {
            if (!run_malformed_response_connection_recovery_case(mode))
                return 1;
        }
        return 0;
    }
    if (argc == 2 && strcmp(argv[1],
            "--truncated-stream-connection-recovery") == 0) {
        cx_gc_check_thread();
        return run_truncated_stream_connection_recovery_case() ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1],
            "--direct-curl-handler-headers-first") == 0) {
        cx_gc_check_thread();
        return run_direct_curl_handler_send_case(1) ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1],
            "--partial-response-header-cancel") == 0) {
        cx_gc_check_thread();
        return run_partial_response_header_cancellation_case() ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1],
            "--direct-curl-handler-cancellation") == 0) {
        cx_gc_check_thread();
        return run_direct_curl_handler_cancellation_case() ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1],
            "--direct-curl-handler-dispose-during-send") == 0) {
        cx_gc_check_thread();
        return run_direct_curl_handler_dispose_during_send_case() ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1],
            "--direct-curl-handler-dispose-during-body-read") == 0) {
        cx_gc_check_thread();
        return run_direct_curl_handler_dispose_during_body_read_case() ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1], "--redirect-cleanup-injection") == 0) {
        return run_redirect_cleanup_injected_failure_case() ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1],
            "--send-cancellation-registration-rollback") == 0) {
        return run_http_send_cancellation_registration_rollback_case() ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1],
            "--send-completion-registration-failure") == 0) {
        return run_http_send_completion_registration_failure_case() ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1],
            "--rest-result-continuation-registration-failure") == 0) {
        return run_rest_result_continuation_registration_failure_case()
            ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1],
            "--http-convenience-continuation-registration-failure") == 0) {
        return run_http_convenience_continuation_registration_failure_cases()
            ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1],
            "--send-handler-continuation-failure") == 0) {
        return run_http_send_handler_continuation_failure_case() ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1],
            "--content-buffer-continuation-registration-failure") == 0) {
        return run_http_content_buffer_continuation_registration_failure_case()
            ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1],
            "--stream-copy-continuation-registration-failure") == 0) {
        return run_stream_copy_continuation_registration_failure_case()
            ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1],
            "--curl-upload-continuation-registration-failure") == 0) {
        return run_curl_upload_continuation_registration_failure_case()
            ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1],
            "--upload-setup-failure-reservation-rollback") == 0) {
        return run_upload_setup_failure_reservation_rollback_case() ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1],
            "--upload-timeout-cancel-race") == 0) {
        cx_gc_check_thread();
        return run_upload_timeout_cancel_race_cases() ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1],
            "--upload-timeout-dispose-race") == 0) {
        cx_gc_check_thread();
        return run_upload_timeout_dispose_race_cases() ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1],
            "--queued-timeout-cancel-race") == 0) {
        cx_gc_check_thread();
        return run_queued_timeout_cancel_race_cases() ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1], "--queued-client-dispose") == 0) {
        cx_gc_check_thread();
        return run_queued_client_dispose_case() ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1],
            "--rest-response-publication-failure") == 0) {
        return run_rest_response_publication_failure_cases() ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1],
            "--request-content-replacement-disposal") == 0) {
        cx_gc_check_thread();
        return run_request_content_replacement_disposal_case() ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1],
            "--response-content-replacement-disposal-race") == 0) {
        cx_gc_check_thread();
        return run_response_content_replacement_disposal_race_case() ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1],
            "--concurrent-header-mutation-send") == 0) {
        cx_gc_check_thread();
        return run_concurrent_header_mutation_send_case() ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1],
            "--concurrent-client-setting-send") == 0) {
        cx_gc_check_thread();
        return run_concurrent_client_setting_send_case() ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1],
            "--concurrent-request-send-reservation") == 0) {
        cx_gc_check_thread();
        return run_concurrent_request_send_case() ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1],
            "--concurrent-request-mutation-send") == 0) {
        cx_gc_check_thread();
        return run_concurrent_request_mutation_send_case() ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1],
            "--concurrent-content-stream-read") == 0) {
        cx_gc_check_thread();
        return run_concurrent_content_stream_read_case() ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1],
            "--concurrent-content-stream-dispose") == 0) {
        cx_gc_check_thread();
        return run_concurrent_content_stream_dispose_case() ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1],
            "--response-owned-stream-dispose-race") == 0) {
        cx_gc_check_thread();
        return run_response_owned_stream_dispose_race_case() ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1],
            "--response-send-lifetime-dispose-race") == 0) {
        cx_gc_check_thread();
        return run_response_send_lifetime_dispose_race_case() ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1],
            "--response-read-scheduler-post-failure") == 0) {
        cx_gc_check_thread();
        return run_response_read_scheduler_post_failure_case() ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1],
            "--response-read-cancellation-registration-failure") == 0) {
        cx_gc_check_thread();
        return run_response_read_cancellation_registration_failure_case()
            ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1],
            "--curl-response-stream-dispose-read-race") == 0) {
        cx_gc_check_thread();
        return run_curl_response_stream_dispose_read_race_case() ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1],
            "--curl-upload-stream-dispose-read-race") == 0) {
        cx_gc_check_thread();
        return run_curl_upload_stream_dispose_read_race_case() ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1],
            "--multipart-serialization-dispose-race") == 0) {
        cx_gc_check_thread();
        return run_multipart_serialization_dispose_race_case() ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1], "--multipart-dispose-failure") == 0) {
        cx_gc_check_thread();
        return run_multipart_dispose_failure_case() ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1],
            "--multipart-disposition-validation") == 0) {
        cx_gc_check_thread();
        return run_multipart_disposition_validation_case() ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1],
            "--cross-thread-client-dispose-send") == 0) {
        cx_gc_check_thread();
        return run_cross_thread_client_dispose_send_case() ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1],
            "--headers-first-client-dispose") == 0) {
        cx_gc_check_thread();
        return run_headers_first_client_dispose_case() ? 0 : 1;
    }
    cx_gc_check_thread();
    if (argc == 2 && strcmp(argv[1], "--https-policy-only") == 0) {
        return run_optional_https_certificate_cases() ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1], "--chunk-size-grammar") == 0) {
        return run_chunk_size_grammar_cases() ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1], "--upload-cancel-after-headers") == 0) {
        return run_streaming_upload_header_cancel_cases() ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1], "--upload-response-wait-request-dispose") == 0) {
        cx_gc_check_thread();
        return run_upload_response_wait_request_disposal_case() ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1], "--request-reservation") == 0) {
        return run_request_reservation_case() ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1], "--segmented-chunked") == 0) {
        return run_segmented_chunk_case() ? 0 : 1;
    }
    if (!run_request_reservation_case()) return 1;
    if (!run_partial_response_header_cancellation_case()) return 1;
    if (!run_concurrent_request_send_case()) return 1;
    if (!run_concurrent_request_mutation_send_case()) return 1;
    if (!run_concurrent_content_stream_read_case()) return 1;
    if (!run_concurrent_content_stream_dispose_case()) return 1;
    if (!run_concurrent_header_mutation_send_case()) return 1;
    if (!run_concurrent_client_setting_send_case()) return 1;
    if (!run_response_content_replacement_disposal_race_case()) return 1;
    if (!run_response_read_scheduler_post_failure_case()) return 1;
    if (!run_response_read_cancellation_registration_failure_case()) return 1;
    if (!run_curl_response_stream_dispose_read_race_case()) return 1;
    if (!run_curl_upload_stream_dispose_read_race_case()) return 1;
    if (!run_multipart_serialization_dispose_race_case()) return 1;
    if (!run_multipart_dispose_failure_case()) return 1;
    if (!run_concurrent_client_dispose_send_case()) return 1;
    if (!run_cross_thread_client_dispose_send_case()) return 1;
    if (!run_rejected_request_framing_case("Content-Length")) return 1;
    if (!run_rejected_request_framing_case("Transfer-Encoding")) return 1;
    if (!run_invalid_response_case("/cx-http-malformed-status",
            "GET /cx-http-malformed-status HTTP/1.1", 1)) return 1;
    if (!run_invalid_response_case("/cx-http-101",
            "GET /cx-http-101 HTTP/1.1", 1)) return 1;
    if (!run_invalid_response_case("/cx-http-malformed-field-name",
            "GET /cx-http-malformed-field-name HTTP/1.1", 1)) return 1;
    if (!run_invalid_response_case("/cx-http-malformed-field-value",
            "GET /cx-http-malformed-field-value HTTP/1.1", 1)) return 1;
    if (!run_method_case("GET /cx-http-high-byte-text HTTP/1.1",
            "/cx-http-high-byte-text",
            cxnet_System_Net_Http_HttpMethod_Get___const_get(), NULL, NULL)) {
        return 1;
    }
    if (!run_http_utf8_supplementary_round_trip()) return 1;
    if (!run_string_charset_case("/cx-http-charset-latin1", "caf\xc3\xa9", 0) ||
        !run_string_charset_case("/cx-http-charset-ascii", "ascii", 0) ||
        !run_string_charset_case("/cx-http-charset-unsupported", NULL, 1) ||
        !run_string_charset_case("/cx-http-charset-duplicate", NULL, 1)) {
        return 1;
    }
    if (!run_invalid_response_case("/cx-http-malformed-trailer",
            "GET /cx-http-malformed-trailer HTTP/1.1", 0)) return 1;
    if (!run_invalid_response_case("/cx-http-chunk-trailer-del",
            "GET /cx-http-chunk-trailer-del HTTP/1.1", 0) ||
        !run_invalid_response_case("/cx-http-chunk-extension-control",
            "GET /cx-http-chunk-extension-control HTTP/1.1", 0) ||
        !run_invalid_response_case("/cx-http-chunk-extension-escaped-control",
            "GET /cx-http-chunk-extension-escaped-control HTTP/1.1", 0)) return 1;
    if (!run_invalid_response_case("/cx-http-malformed-chunk-extension",
            "GET /cx-http-malformed-chunk-extension HTTP/1.1", 0)) return 1;
    if (!run_invalid_response_case("/cx-http-truncated-chunk-data",
            "GET /cx-http-truncated-chunk-data HTTP/1.1", 0) ||
        !run_invalid_response_case("/cx-http-lf-chunk-size",
            "GET /cx-http-lf-chunk-size HTTP/1.1", 0) ||
        !run_invalid_response_case("/cx-http-missing-chunk-data-cr",
            "GET /cx-http-missing-chunk-data-cr HTTP/1.1", 0) ||
        !run_invalid_response_case("/cx-http-doubled-chunk-size-cr",
            "GET /cx-http-doubled-chunk-size-cr HTTP/1.1", 0) ||
        !run_invalid_response_case("/cx-http-doubled-chunk-data-cr",
            "GET /cx-http-doubled-chunk-data-cr HTTP/1.1", 0) ||
        !run_invalid_response_case("/cx-http-lf-chunk-trailer",
            "GET /cx-http-lf-chunk-trailer HTTP/1.1", 0)) return 1;
    if (!run_invalid_response_case("/cx-http-empty-chunk-size",
            "GET /cx-http-empty-chunk-size HTTP/1.1", 0) ||
        !run_invalid_response_case("/cx-http-signed-chunk-size",
            "GET /cx-http-signed-chunk-size HTTP/1.1", 0) ||
        !run_invalid_response_case("/cx-http-prefixed-chunk-size",
            "GET /cx-http-prefixed-chunk-size HTTP/1.1", 0) ||
        !run_invalid_response_case("/cx-http-overflowing-chunk-size",
            "GET /cx-http-overflowing-chunk-size HTTP/1.1", 0) ||
        !run_invalid_response_case("/cx-http-malformed-chunk-data-terminator",
            "GET /cx-http-malformed-chunk-data-terminator HTTP/1.1", 0) ||
        !run_invalid_response_case("/cx-http-malformed-chunk-extension-name",
            "GET /cx-http-malformed-chunk-extension-name HTTP/1.1", 0) ||
        !run_invalid_response_case("/cx-http-malformed-chunk-extension-value",
            "GET /cx-http-malformed-chunk-extension-value HTTP/1.1", 0) ||
        !run_invalid_response_case("/cx-http-malformed-chunk-extension-token",
            "GET /cx-http-malformed-chunk-extension-token HTTP/1.1", 0) ||
        !run_invalid_response_case("/cx-http-truncated-chunk-trailer",
            "GET /cx-http-truncated-chunk-trailer HTTP/1.1", 0)) return 1;
    if (!run_invalid_response_case("/cx-http-chunk-size-line-limit",
            "GET /cx-http-chunk-size-line-limit HTTP/1.1", 0)) return 1;
    if (!run_invalid_response_case("/cx-http-comma-length-conflict",
            "GET /cx-http-comma-length-conflict HTTP/1.1", 1)) return 1;
    if (!run_invalid_response_case("/cx-http-conflicting-length",
            "GET /cx-http-conflicting-length HTTP/1.1", 1)) return 1;
    if (!run_invalid_response_case("/cx-http-length-and-chunked",
            "GET /cx-http-length-and-chunked HTTP/1.1", 1)) return 1;
    if (!run_invalid_response_case(
            "/cx-http-conflicting-length-and-chunked",
            "GET /cx-http-conflicting-length-and-chunked HTTP/1.1", 1)) {
        return 1;
    }
    if (!run_invalid_response_case("/cx-http-repeated-chunked-encoding",
            "GET /cx-http-repeated-chunked-encoding HTTP/1.1", 1)) return 1;
    if (!run_invalid_response_case("/cx-http-comma-repeated-chunked-encoding",
            "GET /cx-http-comma-repeated-chunked-encoding HTTP/1.1", 1)) return 1;
    if (!run_invalid_response_case("/cx-http-205-invalid",
            "GET /cx-http-205-invalid HTTP/1.1", 1)) return 1;
    if (!run_invalid_response_case("/cx-http-205-chunked-invalid",
            "GET /cx-http-205-chunked-invalid HTTP/1.1", 0)) return 1;
    if (!run_invalid_response_case("/cx-http-204-length-invalid",
            "GET /cx-http-204-length-invalid HTTP/1.1", 1)) return 1;
    if (!run_invalid_response_case("/cx-http-204-transfer-invalid",
            "GET /cx-http-204-transfer-invalid HTTP/1.1", 1)) return 1;
    if (!run_streaming_upload_case()) return 1;
    if (!run_streaming_upload_cancellation_case(0, 0xffffffffu, 0) ||
        !run_streaming_upload_cancellation_case(1, 0xffffffffu, 0) ||
        !run_streaming_upload_cancellation_case(2, 0xffffffffu, 0)) return 1;
    if (!run_streaming_upload_header_cancel_cases()) return 1;
    for (size_t delay_index = 0u;
         delay_index < sizeof(upload_cancel_delays_ms) /
             sizeof(upload_cancel_delays_ms[0]);
         delay_index += 1u) {
        if (!run_streaming_upload_cancellation_case(1,
                upload_cancel_delays_ms[delay_index], 0)) return 1;
        if (!run_streaming_upload_cancellation_case(2,
                upload_cancel_delays_ms[delay_index], 0)) return 1;
    }
    if (!run_streaming_upload_timeout_case()) return 1;
    if (!run_upload_timeout_cancel_race_cases() ||
        !run_upload_timeout_dispose_race_cases()) return 1;
    if (!run_upload_response_wait_request_disposal_case()) return 1;
    if (!run_concurrent_streaming_upload_cancellation_case()) return 1;
    if (!run_cancellation_client_dispose_overlap_case()) return 1;
    if (!run_abandoned_response_recovery_case(0) ||
        !run_abandoned_response_recovery_case(1) ||
        !run_abandoned_response_recovery_case(2) ||
        !run_abandoned_response_recovery_case(3)) return 1;
    if (!run_redirect_cleanup_injected_failure_case()) return 1;
    if (!run_http_send_cancellation_registration_rollback_case()) return 1;
    if (!run_http_send_completion_registration_failure_case()) return 1;
    if (!run_http_send_handler_continuation_failure_case()) return 1;
    if (!run_http_content_buffer_continuation_registration_failure_case())
        return 1;
    if (!run_stream_copy_continuation_registration_failure_case()) return 1;
    if (!run_curl_upload_continuation_registration_failure_case()) return 1;
    if (!run_request_content_replacement_disposal_case()) return 1;
    if (!run_connection_reuse_case()) return 1;
    if (!run_connection_expiry_case(1) ||
        !run_connection_expiry_case(0)) return 1;
    if (!run_connection_limit_case()) return 1;
    if (!run_queued_timeout_case()) return 1;
    if (!run_queued_cancellation_case()) return 1;
    if (!run_queued_timeout_cancel_race_cases()) return 1;
    if (!run_queued_client_dispose_case()) return 1;
    if (!run_buffered_reader_case(0) ||
        !run_buffered_reader_case(1) ||
        !run_buffered_reader_case(2) ||
        !run_buffered_reader_case(3) ||
        !run_buffered_reader_case(4) ||
        !run_buffered_reader_case(5) ||
        !run_buffered_reader_case(6) ||
        !run_buffered_reader_case(7) ||
        !run_buffered_reader_case(8)) return 1;
    if (!run_streaming_backpressure_case()) return 1;
    if (!run_streaming_truncated_body_case()) return 1;
    if (!run_streaming_request_cancellation_case()) return 1;
    if (!run_headers_first_client_dispose_case()) return 1;
    if (!run_default_request_headers_case()) return 1;
    if (!run_cookie_container_cases()) return 1;
    if (!start_fixture(&fixture, &port, &thread, 0,
            "GET /cx-http-test HTTP/1.1", NULL, NULL)) {
        fputs("could not start local HTTP fixture\n", stderr);
        return 1;
    }
    (void)snprintf(url, sizeof(url), "http://localhost:%u/cx-http-test", (unsigned)port);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri, make_string(url));
    client = (struct cxnet_System_Net_Http_HttpClient *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    task = cxnet_System_Net_Http_HttpClient_GetAsync__2(client, uri);
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) break;
        steps++;
    }
    stop_fixture(&fixture, thread);
    if (fixture.failed || !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task)) {
        fputs("managed HttpClient did not complete its local request\n", stderr);
        return 1;
    }
    response = cx_generic_640110dc1a526c1c94f059cac4ed8d246fe089fbe535a78bc1997c7adde4688b(task);
    if (cxnet_System_Net_Http_HttpResponseMessage_StatusCode___get(response) !=
            cxnet_System_Net_Http_HttpStatusCode_OK ||
        !cxnet_System_Net_Http_HttpResponseMessage_IsSuccessStatusCode___get(response)) {
        fputs("managed HttpClient returned the wrong response status\n", stderr);
        return 1;
    }
    content = cxnet_System_Net_Http_HttpResponseMessage_Content___get(response);
    if (!string_equals(cxnet_System_Net_Http_HttpContent_ReadAsString(content), "pong")) {
        fputs("managed HttpClient returned the wrong response body\n", stderr);
        return 1;
    }
    if (!cxnet_System_Net_Http_HttpHeaders_Contains(
            cxnet_System_Net_Http_HttpResponseMessage_Headers___const_get(response),
            make_string("x-cx-test"))) {
        fputs("managed HttpClient did not preserve the response header\n", stderr);
        return 1;
    }
    if (!cxnet_System_Net_Http_HttpHeaders_Contains(
            cxnet_System_Net_Http_HttpContent_Headers___const_get(content),
            make_string("content-type"))) {
        fputs("managed HttpClient did not classify the response content header\n", stderr);
        return 1;
    }
    cxnet_System_Net_Http_HttpResponseMessage_Dispose(response);
    cxnet_System_Net_Http_HttpClient_Dispose(client);

    if (!run_empty_response_case("HEAD /cx-http-head HTTP/1.1",
            "/cx-http-head", cxnet_System_Net_Http_HttpMethod_Head___const_get(),
            cxnet_System_Net_Http_HttpStatusCode_OK, 1)) {
        return 1;
    }
    if (!run_empty_response_case("GET /cx-http-204 HTTP/1.1",
            "/cx-http-204", cxnet_System_Net_Http_HttpMethod_Get___const_get(),
            cxnet_System_Net_Http_HttpStatusCode_NoContent, 0)) {
        return 1;
    }
    if (!run_empty_response_case("GET /cx-http-205 HTTP/1.1",
            "/cx-http-205", cxnet_System_Net_Http_HttpMethod_Get___const_get(),
            cxnet_System_Net_Http_HttpStatusCode_ResetContent, 0)) {
        return 1;
    }
    if (!run_empty_response_case("GET /cx-http-205-chunked HTTP/1.1",
            "/cx-http-205-chunked",
            cxnet_System_Net_Http_HttpMethod_Get___const_get(),
            cxnet_System_Net_Http_HttpStatusCode_ResetContent, 0)) {
        return 1;
    }
    if (!run_empty_response_case("GET /cx-http-304 HTTP/1.1",
            "/cx-http-304", cxnet_System_Net_Http_HttpMethod_Get___const_get(),
            cxnet_System_Net_Http_HttpStatusCode_NotModified, 0)) {
        return 1;
    }
    if (!run_empty_response_case("OPTIONS /cx-http-options HTTP/1.1",
            "/cx-http-options", cxnet_System_Net_Http_HttpMethod_Options___const_get(),
            cxnet_System_Net_Http_HttpStatusCode_NoContent, 2)) {
        return 1;
    }
    if (!run_method_case("PUT /cx-http-put HTTP/1.1", "/cx-http-put",
            cxnet_System_Net_Http_HttpMethod_Put___const_get(), "payload",
            "Content-Type: text/plain; charset=utf-8")) {
        return 1;
    }
    if (!run_method_case("PATCH /cx-http-patch HTTP/1.1", "/cx-http-patch",
            cxnet_System_Net_Http_HttpMethod_Patch___const_get(), "payload",
            "Content-Type: text/plain; charset=utf-8")) {
        return 1;
    }
    if (!run_method_case("DELETE /cx-http-delete HTTP/1.1", "/cx-http-delete",
            cxnet_System_Net_Http_HttpMethod_Delete___const_get(), NULL, NULL)) {
        return 1;
    }
    if (!run_method_case("GET /cx%20path?q=a%20b HTTP/1.1",
            "/cx path?q=a b", cxnet_System_Net_Http_HttpMethod_Get___const_get(),
            NULL, NULL)) {
        return 1;
    }
    if (!run_method_case("GET /cx-http-chunked HTTP/1.1", "/cx-http-chunked",
            cxnet_System_Net_Http_HttpMethod_Get___const_get(), NULL, NULL)) {
        return 1;
    }
    if (!run_segmented_chunk_case()) return 1;
    if (!run_method_case("GET /cx-http-comma-length HTTP/1.1",
            "/cx-http-comma-length",
            cxnet_System_Net_Http_HttpMethod_Get___const_get(), NULL, NULL)) {
        return 1;
    }
    if (!run_method_case("GET /cx-http-close-delimited HTTP/1.1",
            "/cx-http-close-delimited",
            cxnet_System_Net_Http_HttpMethod_Get___const_get(), NULL, NULL)) {
        return 1;
    }
    if (!run_method_case("GET /cx-http-interim HTTP/1.1", "/cx-http-interim",
            cxnet_System_Net_Http_HttpMethod_Get___const_get(), NULL, NULL)) {
        return 1;
    }
    if (!run_early_upload_rejection_case(1) ||
        !run_early_upload_rejection_case(0)) return 1;
    if (!run_method_case("GET /cx-http-gzip HTTP/1.1", "/cx-http-gzip",
            cxnet_System_Net_Http_HttpMethod_Get___const_get(), NULL,
            "Accept-Encoding: gzip, deflate, br, zstd")) {
        return 1;
    }
    if (!run_method_case("GET /cx-http-gzip-stacked HTTP/1.1",
            "/cx-http-gzip-stacked",
            cxnet_System_Net_Http_HttpMethod_Get___const_get(), NULL,
            "Accept-Encoding: gzip, deflate, br, zstd")) {
        return 1;
    }
    if (!run_method_case("GET /cx-http-brotli HTTP/1.1", "/cx-http-brotli",
            cxnet_System_Net_Http_HttpMethod_Get___const_get(), NULL,
            "Accept-Encoding: gzip, deflate, br, zstd")) {
        return 1;
    }
    if (!run_method_case("GET /cx-http-zstd HTTP/1.1", "/cx-http-zstd",
            cxnet_System_Net_Http_HttpMethod_Get___const_get(), NULL,
            "Accept-Encoding: gzip, deflate, br, zstd")) {
        return 1;
    }

    if (!run_client_verb_case("POST /cx-http-post HTTP/1.1",
            "/cx-http-post", 0, "payload",
            "Content-Type: text/plain; charset=utf-8") ||
        !run_client_verb_case("PUT /cx-http-put HTTP/1.1",
            "/cx-http-put", 1, "payload",
            "Content-Type: text/plain; charset=utf-8") ||
        !run_client_verb_case("PATCH /cx-http-patch HTTP/1.1",
            "/cx-http-patch", 2, "payload",
            "Content-Type: text/plain; charset=utf-8") ||
        !run_client_verb_case("DELETE /cx-http-delete HTTP/1.1",
            "/cx-http-delete", 3, NULL, NULL)) {
        return 1;
    }
    if (!run_proxy_request_case()) return 1;

    if (!start_fixture(&fixture, &port, &thread, 0,
            "GET /cx-http-header-limit HTTP/1.1", NULL, NULL)) {
        fputs("could not start response-header-limit fixture\n", stderr);
        return 1;
    }
    (void)snprintf(url, sizeof(url), "http://localhost:%u/cx-http-header-limit", (unsigned)port);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri, make_string(url));
    client = (struct cxnet_System_Net_Http_HttpClient *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    task = cxnet_System_Net_Http_HttpClient_GetAsync__2(client, uri);
    steps = 0u;
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) break;
        steps++;
    }
    stop_fixture(&fixture, thread);
    {
        cx_bool completed = cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task);
        cx_bool exceeded = cxnet_System_Net_Http_CurlHttpTransport_ExceededResponseHeaderLimits(task);
        if (fixture.failed || !completed || !exceeded) {
            fprintf(stderr, "header limit evidence fixture=%d completed=%d exceeded=%d canceled=%d\n",
                fixture.failed, completed, exceeded,
                cxcore_System_Threading_Tasks_Task_IsCanceled___const_get(task));
            fputs("managed HttpClient did not enforce its response header limits\n", stderr);
            return 1;
        }
    }
    cxnet_System_Net_Http_HttpClient_Dispose(client);

    if (!start_fixture(&fixture, &port, &thread, 0,
            "GET /cx-http-header-bytes-limit HTTP/1.1", NULL, NULL)) {
        fputs("could not start response-header-byte-limit fixture\n", stderr);
        return 1;
    }
    (void)snprintf(url, sizeof(url), "http://localhost:%u/cx-http-header-bytes-limit", (unsigned)port);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri, make_string(url));
    client = (struct cxnet_System_Net_Http_HttpClient *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    task = cxnet_System_Net_Http_HttpClient_GetAsync__2(client, uri);
    steps = 0u;
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) break;
        steps++;
    }
    stop_fixture(&fixture, thread);
    {
        cx_bool completed = cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task);
        cx_bool exceeded = cxnet_System_Net_Http_CurlHttpTransport_ExceededResponseHeaderLimits(task);
        if (fixture.failed || !completed || !exceeded) {
            fprintf(stderr, "header byte limit evidence fixture=%d completed=%d exceeded=%d canceled=%d\n",
                fixture.failed, completed, exceeded,
                cxcore_System_Threading_Tasks_Task_IsCanceled___const_get(task));
            fputs("managed HttpClient did not enforce its response header byte limit\n", stderr);
            return 1;
        }
    }
    cxnet_System_Net_Http_HttpClient_Dispose(client);

    if (!start_fixture(&fixture, &port, &thread, 0,
            "GET /cx-http-truncated-body HTTP/1.1", NULL, NULL)) {
        fputs("could not start truncated-response fixture\n", stderr);
        return 1;
    }
    (void)snprintf(url, sizeof(url), "http://localhost:%u/cx-http-truncated-body", (unsigned)port);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri, make_string(url));
    client = (struct cxnet_System_Net_Http_HttpClient *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    task = cxnet_System_Net_Http_HttpClient_GetAsync__2(client, uri);
    steps = 0u;
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) break;
        steps++;
    }
    stop_fixture(&fixture, thread);
    {
        cx_bool completed = cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task);
        cx_bool failed = cxnet_System_Net_Http_CurlHttpTransport_FailedResponseTransfer(task);
        if (fixture.failed || !completed || !failed) {
            fprintf(stderr, "truncated response evidence fixture=%d completed=%d failed=%d\n",
                fixture.failed, completed, failed);
            fputs("managed HttpClient accepted a truncated fixed-length response\n", stderr);
            return 1;
        }
    }
    cxnet_System_Net_Http_HttpClient_Dispose(client);

    if (!start_fixture(&fixture, &port, &thread, 0,
            "GET /cx-http-truncated-chunk HTTP/1.1", NULL, NULL)) {
        fputs("could not start truncated-chunk fixture\n", stderr);
        return 1;
    }
    (void)snprintf(url, sizeof(url), "http://localhost:%u/cx-http-truncated-chunk", (unsigned)port);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri, make_string(url));
    client = (struct cxnet_System_Net_Http_HttpClient *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    task = cxnet_System_Net_Http_HttpClient_GetAsync__2(client, uri);
    steps = 0u;
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) break;
        steps++;
    }
    stop_fixture(&fixture, thread);
    {
        cx_bool completed = cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task);
        cx_bool failed = cxnet_System_Net_Http_CurlHttpTransport_FailedResponseTransfer(task);
        if (fixture.failed || !completed || !failed) {
            fprintf(stderr, "truncated chunk evidence fixture=%d completed=%d failed=%d\n",
                fixture.failed, completed, failed);
            fputs("managed HttpClient accepted an incomplete chunked response\n", stderr);
            return 1;
        }
    }
    cxnet_System_Net_Http_HttpClient_Dispose(client);

    if (!start_fixture(&fixture, &port, &thread, 0,
            "GET /cx-http-buffer-limit HTTP/1.1", NULL, NULL)) {
        fputs("could not start response-buffer-limit fixture\n", stderr);
        return 1;
    }
    (void)snprintf(url, sizeof(url), "http://localhost:%u/cx-http-buffer-limit", (unsigned)port);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri, make_string(url));
    {
        struct cxnet_System_Net_Http_CurlHttpMessageHandler *handler =
            (struct cxnet_System_Net_Http_CurlHttpMessageHandler *)
                cxcore_System_Memory_Alloc(
                    cxnet_System_Net_Http_CurlHttpMessageHandler___typeinfo.Size);
        cxnet_System_Net_Http_CurlHttpMessageHandler___constructor(handler);
        cxnet_System_Net_Http_CurlHttpMessageHandler_MaxResponseContentBufferSize___set(
            handler, 3u);
        client = (struct cxnet_System_Net_Http_HttpClient *)
            cxcore_System_Memory_Alloc(cxnet_System_Net_Http_HttpClient___typeinfo.Size);
        cxnet_System_Net_Http_HttpClient___constructor(client,
            (struct cxnet_System_Net_Http_HttpMessageHandler *)handler);
    }
    task = cxnet_System_Net_Http_HttpClient_GetAsync__2(client, uri);
    steps = 0u;
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) break;
        steps++;
    }
    stop_fixture(&fixture, thread);
    {
        cx_bool completed = cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task);
        cx_bool exceeded = cxnet_System_Net_Http_CurlHttpTransport_ExceededResponseContentBufferLimit(task);
        if (fixture.failed || !completed || !exceeded) {
            fprintf(stderr, "buffer limit evidence fixture=%d completed=%d exceeded=%d canceled=%d\n",
                fixture.failed, completed, exceeded,
                cxcore_System_Threading_Tasks_Task_IsCanceled___const_get(task));
            fputs("managed HttpClient did not enforce its response buffer limit\n", stderr);
            return 1;
        }
    }
    cxnet_System_Net_Http_HttpClient_Dispose(client);

    if (!start_fixture(&fixture, &port, &thread, 0,
            "GET /cx-http-gzip-expanded-limit HTTP/1.1",
            NULL, "Accept-Encoding: gzip, deflate, br, zstd")) {
        fputs("could not start decompressed-response-limit fixture\n", stderr);
        return 1;
    }
    (void)snprintf(url, sizeof(url), "http://localhost:%u/cx-http-gzip-expanded-limit", (unsigned)port);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri, make_string(url));
    {
        struct cxnet_System_Net_Http_CurlHttpMessageHandler *handler =
            (struct cxnet_System_Net_Http_CurlHttpMessageHandler *)
                cxcore_System_Memory_Alloc(
                    cxnet_System_Net_Http_CurlHttpMessageHandler___typeinfo.Size);
        cxnet_System_Net_Http_CurlHttpMessageHandler___constructor(handler);
        cxnet_System_Net_Http_CurlHttpMessageHandler_MaxResponseContentBufferSize___set(
            handler, 32u);
        client = (struct cxnet_System_Net_Http_HttpClient *)
            cxcore_System_Memory_Alloc(cxnet_System_Net_Http_HttpClient___typeinfo.Size);
        cxnet_System_Net_Http_HttpClient___constructor(client,
            (struct cxnet_System_Net_Http_HttpMessageHandler *)handler);
    }
    task = cxnet_System_Net_Http_HttpClient_GetAsync__2(client, uri);
    steps = 0u;
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) break;
        steps++;
    }
    stop_fixture(&fixture, thread);
    {
        cx_bool completed = cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task);
        cx_bool exceeded = cxnet_System_Net_Http_CurlHttpTransport_ExceededResponseContentBufferLimit(task);
        if (fixture.failed || !completed || !exceeded) {
            fprintf(stderr, "decompressed limit evidence fixture=%d completed=%d exceeded=%d\n",
                fixture.failed, completed, exceeded);
            fputs("managed HttpClient did not bound decompressed response bytes\n", stderr);
            return 1;
        }
    }
    cxnet_System_Net_Http_HttpClient_Dispose(client);

    if (!start_fixture(&fixture, &port, &thread, 1,
            "GET /cx-http-cancel HTTP/1.1", NULL, NULL)) {
        fputs("could not start cancellation fixture\n", stderr);
        return 1;
    }
    (void)snprintf(url, sizeof(url), "http://localhost:%u/cx-http-cancel", (unsigned)port);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri, make_string(url));
    client = (struct cxnet_System_Net_Http_HttpClient *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    cancellationSource = (struct cxcore_System_Threading_Tasks_CancellationTokenSource *)
        cxcore_System_Memory_Alloc(
            cxcore_System_Threading_Tasks_CancellationTokenSource___typeinfo.Size);
    cxcore_System_Threading_Tasks_CancellationTokenSource___constructor(cancellationSource);
    cancellationToken = cxcore_System_Threading_Tasks_CancellationTokenSource_Token___const_get(
        cancellationSource);
    cancelAction.__function = cancel_token_on_scheduler;
    cancelAction.__environment = cancellationSource;
    cancelAction.__target = (cx_ptr)cancellationSource;
    cxcore_System_Threading_Tasks_TaskScheduler_PostAfter(300u, cancelAction);
    task = cxnet_System_Net_Http_HttpClient_GetAsync(client, uri, cancellationToken);
    steps = 0u;
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) break;
        steps++;
    }
    stop_fixture(&fixture, thread);
    if (fixture.failed ||
        !cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) ||
        !cxnet_System_Net_Http_CurlHttpTransport_WasCanceled(task)) {
        fputs("managed HttpClient did not cancel its in-flight request\n", stderr);
        return 1;
    }
    cxnet_System_Net_Http_HttpClient_Dispose(client);

    if (!start_fixture(&fixture, &port, &thread, 1,
            "GET /cx-http-timeout HTTP/1.1", NULL, NULL)) {
        fputs("could not start timeout fixture\n", stderr);
        return 1;
    }
    (void)snprintf(url, sizeof(url), "http://localhost:%u/cx-http-timeout", (unsigned)port);
    uri = (struct cxcore_System_Uri *)cxcore_System_Memory_Alloc(
        cxcore_System_Uri___typeinfo.Size);
    cxcore_System_Uri___constructor(uri, make_string(url));
    client = (struct cxnet_System_Net_Http_HttpClient *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Http_HttpClient___typeinfo.Size);
    cxnet_System_Net_Http_HttpClient___constructor__2(client);
    cxnet_System_Net_Http_HttpClient_TimeoutMilliseconds___set(client, 250u);
    task = cxnet_System_Net_Http_HttpClient_GetAsync__2(client, uri);
    steps = 0u;
    while (!cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task) &&
        steps < 200000u) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) break;
        steps++;
    }
    stop_fixture(&fixture, thread);
    {
        cx_bool completed = cxcore_System_Threading_Tasks_Task_IsCompleted___const_get(task);
        cx_bool timedOut = cxnet_System_Net_Http_CurlHttpTransport_WasTimedOut(task);
        if (fixture.failed || !completed || !timedOut) {
            fprintf(stderr, "timeout evidence fixture=%d completed=%d canceled=%d timedOut=%d\n",
                fixture.failed, completed,
                cxnet_System_Net_Http_CurlHttpTransport_WasCanceled(task), timedOut);
        fputs("managed HttpClient did not report its configured timeout\n", stderr);
        return 1;
        }
    }
    cxnet_System_Net_Http_HttpClient_Dispose(client);
    if (!run_optional_https_certificate_cases()) return 1;
    puts("PASS managed HttpClient GET/POST/cancellation/timeout over CX scheduler and libcurl multi");
    return 0;
}

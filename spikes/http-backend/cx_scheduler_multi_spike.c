#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cxcore.h"
#include "cxnet_socket_runtime.h"

extern uint64_t cx_task_scheduler_post_after_cancellable(
    uint32_t milliseconds, cx_action continuation);
extern cx_bool cx_task_scheduler_cancel_timer(uint64_t registration_id);

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
typedef SOCKET fixture_socket;
typedef HANDLE fixture_thread;
typedef int fixture_socket_length;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int fixture_socket;
typedef pthread_t fixture_thread;
typedef socklen_t fixture_socket_length;
#endif

#define SPIKE_MAX_BODY 64u
#define SPIKE_MAX_SOCKETS 64u
#define SPIKE_TRANSFER_COUNT 16u
#define SPIKE_WATCHDOG_MS 7000u

struct request_state {
    int completed;
    CURLcode result;
    size_t body_length;
    char body[SPIKE_MAX_BODY];
};

struct fixture_server {
    fixture_socket listener;
    int failed;
};

struct transfer_state;

struct socket_registration {
    struct transfer_state *transfer;
    curl_socket_t socket;
    uint64_t registration_id;
    uint32_t ready_mask;
    uint32_t interest_mask;
    int active;
};

struct transfer_state {
    CURLM *multi;
    int running;
    int callback_error;
    uint32_t completed_count;
    int watchdog_fired;
    uint32_t resolver_start_count;
    uint64_t timer_id;
    uint64_t watchdog_id;
    uint32_t socket_callback_count;
    uint32_t readiness_callback_count;
    uint32_t timer_callback_count;
    uint32_t last_ready_mask;
    struct request_state requests[SPIKE_TRANSFER_COUNT];
    uint32_t registration_count;
    struct socket_registration *registrations[SPIKE_MAX_SOCKETS];
};

#ifdef _WIN32
static DWORD WINAPI fixture_server_thread(void *value) {
#else
static void *fixture_server_thread(void *value) {
#endif
    static const char response[] =
        "HTTP/1.1 200 OK\r\nContent-Length: 4\r\n"
        "Connection: close\r\n\r\npong";
    struct fixture_server *server = (struct fixture_server *)value;
    uint32_t request_index;
    for (request_index = 0u; request_index < SPIKE_TRANSFER_COUNT;
            ++request_index) {
        fixture_socket client = accept(server->listener, NULL, NULL);
        char request[2048];
        size_t request_length = 0u;
        size_t sent = 0u;
#ifdef _WIN32
        if (client == INVALID_SOCKET) server->failed = 1;
#else
        if (client < 0) server->failed = 1;
#endif
        while (!server->failed &&
            (request_length < 4u ||
                memcmp(request + request_length - 4u, "\r\n\r\n", 4u) != 0)) {
#ifdef _WIN32
            int amount = recv(client, request + request_length,
                (int)(sizeof(request) - request_length - 1u), 0);
#else
            ssize_t amount = recv(client, request + request_length,
                sizeof(request) - request_length - 1u, 0);
#endif
            if (amount <= 0 ||
                request_length + (size_t)amount >= sizeof(request)) {
                server->failed = 1;
                break;
            }
            request_length += (size_t)amount;
            request[request_length] = '\0';
        }
        while (!server->failed && sent < sizeof(response) - 1u) {
#ifdef _WIN32
            int amount = send(client, response + sent,
                (int)(sizeof(response) - 1u - sent), 0);
#else
            ssize_t amount = send(client, response + sent,
                sizeof(response) - 1u - sent, 0);
#endif
            if (amount <= 0) {
                server->failed = 1;
                break;
            }
            sent += (size_t)amount;
        }
#ifdef _WIN32
        if (!server->failed) {
            char discard[256];
            (void)shutdown(client, SD_SEND);
            while (recv(client, discard, (int)sizeof(discard), 0) > 0) { }
        }
        if (client != INVALID_SOCKET) closesocket(client);
#else
        if (!server->failed) {
            char discard[256];
            (void)shutdown(client, SHUT_WR);
            while (recv(client, discard, sizeof(discard), 0) > 0) { }
        }
        if (client >= 0) close(client);
#endif
        if (server->failed) break;
    }
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

static int start_fixture(struct fixture_server *server, uint16_t *port,
    fixture_thread *thread) {
    struct sockaddr_in address;
    fixture_socket_length length = (fixture_socket_length)sizeof(address);
#ifdef _WIN32
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return 0;
    server->listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server->listener == INVALID_SOCKET) return 0;
#else
    server->listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server->listener < 0) return 0;
#endif
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(server->listener, (struct sockaddr *)&address,
            (fixture_socket_length)sizeof(address)) != 0 ||
        listen(server->listener, (int)SPIKE_TRANSFER_COUNT) != 0 ||
        getsockname(server->listener, (struct sockaddr *)&address, &length) != 0) {
#ifdef _WIN32
        closesocket(server->listener);
        WSACleanup();
#else
        close(server->listener);
#endif
        return 0;
    }
    server->failed = 0;
    *port = ntohs(address.sin_port);
#ifdef _WIN32
    *thread = CreateThread(NULL, 0, fixture_server_thread, server, 0, NULL);
    return *thread != NULL;
#else
    return pthread_create(thread, NULL, fixture_server_thread, server) == 0;
#endif
}

static void stop_fixture(struct fixture_server *server, fixture_thread thread) {
#ifdef _WIN32
    (void)shutdown(server->listener, SD_BOTH);
    closesocket(server->listener);
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    WSACleanup();
#else
    (void)shutdown(server->listener, SHUT_RDWR);
    close(server->listener);
    pthread_join(thread, NULL);
#endif
}

static size_t receive_body(char *data, size_t size, size_t count, void *value) {
    struct request_state *request = (struct request_state *)value;
    size_t amount = size * count;
    if (amount > sizeof(request->body) - request->body_length) {
        return 0u;
    }
    memcpy(request->body + request->body_length, data, amount);
    request->body_length += amount;
    return amount;
}

static void collect_completions(struct transfer_state *transfer) {
    int remaining = 0;
    CURLMsg *message;
    while ((message = curl_multi_info_read(transfer->multi, &remaining)) != NULL) {
        if (message->msg == CURLMSG_DONE) {
            struct request_state *request = NULL;
            if (curl_easy_getinfo(message->easy_handle, CURLINFO_PRIVATE,
                    &request) != CURLE_OK || request == NULL ||
                request->completed) {
                transfer->callback_error = 1;
                continue;
            }
            request->result = message->data.result;
            request->completed = 1;
            transfer->completed_count += 1u;
        }
    }
}

static int resolver_started(void *resolver_state, void *reserved,
    void *user_value) {
    struct transfer_state *transfer = (struct transfer_state *)user_value;
    (void)resolver_state;
    (void)reserved;
    transfer->resolver_start_count += 1u;
    return 0;
}

static void drive_socket(struct socket_registration *registration,
    uint32_t ready_mask) {
    struct transfer_state *transfer = registration->transfer;
    int flags = 0;
    CURLMcode result;
    if (ready_mask & CXNET_SOCKET_READABLE) flags |= CURL_CSELECT_IN;
    if (ready_mask & CXNET_SOCKET_WRITABLE) flags |= CURL_CSELECT_OUT;
    if (ready_mask & CXNET_SOCKET_ERROR) flags |= CURL_CSELECT_ERR;
    transfer->readiness_callback_count += 1u;
    transfer->last_ready_mask = ready_mask;
    if (flags == 0) {
        transfer->callback_error = 1;
        return;
    }
    result = curl_multi_socket_action(transfer->multi, registration->socket,
        flags, &transfer->running);
    if (result != CURLM_OK) transfer->callback_error = 1;
    collect_completions(transfer);
}

static void socket_ready(void *environment) {
    struct socket_registration *registration =
        (struct socket_registration *)environment;
    uint32_t ready_mask = registration->ready_mask;
    registration->registration_id = 0u;
    if (!registration->active) return;
    drive_socket(registration, ready_mask);
}

static void timer_ready(void *environment) {
    struct transfer_state *transfer = (struct transfer_state *)environment;
    CURLMcode result;
    transfer->timer_id = 0u;
    transfer->timer_callback_count += 1u;
    result = curl_multi_socket_action(transfer->multi, CURL_SOCKET_TIMEOUT,
        0, &transfer->running);
    if (result != CURLM_OK) transfer->callback_error = 1;
    collect_completions(transfer);
}

static void watchdog_ready(void *environment) {
    struct transfer_state *transfer = (struct transfer_state *)environment;
    transfer->watchdog_id = 0u;
    transfer->watchdog_fired = 1;
}

static int replace_socket_registration(struct socket_registration *registration,
    uint32_t interest_mask) {
    struct transfer_state *transfer = registration->transfer;
    cx_action continuation = {0};
    if (registration->registration_id != 0u) {
        (void)cxnet_socket_cancel_wait(registration->registration_id);
        registration->registration_id = 0u;
    }
    registration->interest_mask = interest_mask;
    registration->ready_mask = 0u;
    registration->active = interest_mask != 0u;
    if (interest_mask == 0u) return 1;
    continuation.__function = socket_ready;
    continuation.__environment = registration;
    continuation.__target = (cx_ptr)registration;
    registration->registration_id = cxnet_socket_post_on_readiness_mask(
        (uintptr_t)registration->socket, interest_mask,
        &registration->ready_mask, continuation, (cx_action){0});
    if (registration->registration_id == 0u) transfer->callback_error = 1;
    return registration->registration_id != 0u;
}

static int curl_socket_event(CURL *easy, curl_socket_t socket, int what,
    void *user_value, void *socket_value) {
    struct transfer_state *transfer = (struct transfer_state *)user_value;
    struct socket_registration *registration =
        (struct socket_registration *)socket_value;
    uint32_t interest_mask = 0u;
    (void)easy;
    transfer->socket_callback_count += 1u;
    if (what == CURL_POLL_REMOVE) {
        if (registration != NULL) {
            (void)replace_socket_registration(registration, 0u);
            (void)curl_multi_assign(transfer->multi, socket, NULL);
        }
        return 0;
    }
    if (registration == NULL) {
        if (transfer->registration_count == SPIKE_MAX_SOCKETS) {
            transfer->callback_error = 1;
            return -1;
        }
        registration = (struct socket_registration *)cxcore_System_Memory_Alloc(
            (cx_uint)sizeof(*registration));
        if (registration == NULL) {
            transfer->callback_error = 1;
            return -1;
        }
        memset(registration, 0, sizeof(*registration));
        cx_gc_store_reference((cx_ptr *)&registration->transfer, (cx_ptr)transfer);
        registration->socket = socket;
        cx_gc_store_reference(
            (cx_ptr *)&transfer->registrations[transfer->registration_count],
            (cx_ptr)registration);
        transfer->registration_count += 1u;
        if (curl_multi_assign(transfer->multi, socket, registration) != CURLM_OK) {
            transfer->callback_error = 1;
            return -1;
        }
    }
    if (what == CURL_POLL_IN || what == CURL_POLL_INOUT) {
        interest_mask |= CXNET_SOCKET_READABLE;
    }
    if (what == CURL_POLL_OUT || what == CURL_POLL_INOUT) {
        interest_mask |= CXNET_SOCKET_WRITABLE;
    }
    return replace_socket_registration(registration, interest_mask) ? 0 : -1;
}

static int curl_timer_event(CURLM *multi, long timeout_ms, void *user_value) {
    struct transfer_state *transfer = (struct transfer_state *)user_value;
    cx_action continuation = {0};
    (void)multi;
    transfer->timer_callback_count += 1u;
    if (transfer->timer_id != 0u) {
        (void)cx_task_scheduler_cancel_timer(transfer->timer_id);
        transfer->timer_id = 0u;
    }
    if (timeout_ms < 0) return 0;
    continuation.__function = timer_ready;
    continuation.__environment = transfer;
    continuation.__target = (cx_ptr)transfer;
    transfer->timer_id = cx_task_scheduler_post_after_cancellable(
        (uint32_t)timeout_ms, continuation);
    if (transfer->timer_id == 0u) {
        transfer->callback_error = 1;
        return -1;
    }
    return 0;
}

static void cancel_pending_work(struct transfer_state *transfer) {
    if (transfer->timer_id != 0u) {
        (void)cx_task_scheduler_cancel_timer(transfer->timer_id);
        transfer->timer_id = 0u;
    }
    if (transfer->watchdog_id != 0u) {
        (void)cx_task_scheduler_cancel_timer(transfer->watchdog_id);
        transfer->watchdog_id = 0u;
    }
}

int main(void) {
    struct transfer_state *transfer;
    struct fixture_server server;
    fixture_thread thread;
    uint16_t port = 0u;
    CURL *easy[SPIKE_TRANSFER_COUNT] = {0};
    CURLMcode multi_result;
    int result = 1;
    int fixture_started = 0;
    int all_requests_passed;
    uint32_t request_index;
    const curl_version_info_data *version;

    cx_gc_check_thread();
    transfer = (struct transfer_state *)cxcore_System_Memory_Alloc(
        (cx_uint)sizeof(*transfer));
    if (transfer == NULL) return 1;
    memset(transfer, 0, sizeof(*transfer));
    version = curl_version_info(CURLVERSION_NOW);
    if (version == NULL || !(version->features & CURL_VERSION_ASYNCHDNS) ||
        !(version->features & CURL_VERSION_SSL)) {
        fputs("libcurl is missing asynchronous DNS or TLS support\n", stderr);
        return 1;
    }
    if (!start_fixture(&server, &port, &thread)) {
        fputs("could not start the loopback HTTP fixture\n", stderr);
        return 1;
    }
    fixture_started = 1;
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) goto cleanup_fixture;
    transfer->multi = curl_multi_init();
    if (transfer->multi == NULL) goto cleanup_curl;
    (void)curl_multi_setopt(transfer->multi, CURLMOPT_SOCKETFUNCTION, curl_socket_event);
    (void)curl_multi_setopt(transfer->multi, CURLMOPT_SOCKETDATA, transfer);
    (void)curl_multi_setopt(transfer->multi, CURLMOPT_TIMERFUNCTION, curl_timer_event);
    (void)curl_multi_setopt(transfer->multi, CURLMOPT_TIMERDATA, transfer);
    for (request_index = 0u; request_index < SPIKE_TRANSFER_COUNT;
            ++request_index) {
        char url[128];
        easy[request_index] = curl_easy_init();
        if (easy[request_index] == NULL) goto cleanup_curl;
        snprintf(url, sizeof(url), "http://localhost:%u/fixture/%u",
            (unsigned)port, (unsigned)request_index);
        (void)curl_easy_setopt(easy[request_index], CURLOPT_URL, url);
        (void)curl_easy_setopt(easy[request_index], CURLOPT_PROTOCOLS_STR, "http");
        (void)curl_easy_setopt(easy[request_index], CURLOPT_WRITEFUNCTION,
            receive_body);
        (void)curl_easy_setopt(easy[request_index], CURLOPT_WRITEDATA,
            &transfer->requests[request_index]);
        (void)curl_easy_setopt(easy[request_index], CURLOPT_PRIVATE,
            &transfer->requests[request_index]);
        (void)curl_easy_setopt(easy[request_index], CURLOPT_RESOLVER_START_FUNCTION,
            resolver_started);
        (void)curl_easy_setopt(easy[request_index], CURLOPT_RESOLVER_START_DATA,
            transfer);
        (void)curl_easy_setopt(easy[request_index], CURLOPT_DNS_CACHE_TIMEOUT, 0L);
        (void)curl_easy_setopt(easy[request_index], CURLOPT_CONNECTTIMEOUT_MS,
            3000L);
        (void)curl_easy_setopt(easy[request_index], CURLOPT_TIMEOUT_MS, 5000L);
        multi_result = curl_multi_add_handle(transfer->multi, easy[request_index]);
        if (multi_result != CURLM_OK) goto cleanup_curl;
    }
    {
        cx_action watchdog = {0};
        watchdog.__function = watchdog_ready;
        watchdog.__environment = transfer;
        watchdog.__target = (cx_ptr)transfer;
        transfer->watchdog_id = cx_task_scheduler_post_after_cancellable(
            SPIKE_WATCHDOG_MS, watchdog);
    }
    multi_result = curl_multi_socket_action(transfer->multi, CURL_SOCKET_TIMEOUT,
        0, &transfer->running);
    collect_completions(transfer);
    while (multi_result == CURLM_OK &&
        transfer->completed_count < SPIKE_TRANSFER_COUNT &&
        !transfer->callback_error && !transfer->watchdog_fired) {
        if (!cxcore_System_Threading_Tasks_TaskScheduler_RunOne()) break;
        collect_completions(transfer);
    }
    stop_fixture(&server, thread);
    fixture_started = 0;
    all_requests_passed = transfer->completed_count == SPIKE_TRANSFER_COUNT &&
        transfer->resolver_start_count == SPIKE_TRANSFER_COUNT &&
        !transfer->callback_error && !transfer->watchdog_fired &&
        !server.failed;
    if (all_requests_passed) {
        for (request_index = 0u; request_index < SPIKE_TRANSFER_COUNT;
                ++request_index) {
            struct request_state *request = &transfer->requests[request_index];
            if (!request->completed || request->result != CURLE_OK ||
                request->body_length != 4u ||
                memcmp(request->body, "pong", 4u) != 0) {
                all_requests_passed = 0;
                break;
            }
        }
    }
    if (all_requests_passed) result = 0;
    if (result != 0) {
        fprintf(stderr,
            "scheduler-integrated resolver-load transfer failed: completed=%u/%u; resolver-starts=%u/%u; callback=%d; watchdog=%d; server=%d; socket-callbacks=%u; ready-callbacks=%u; timers=%u; ready-mask=%u\n",
            transfer->completed_count, SPIKE_TRANSFER_COUNT,
            transfer->resolver_start_count, SPIKE_TRANSFER_COUNT,
            transfer->callback_error, transfer->watchdog_fired, server.failed,
            transfer->socket_callback_count, transfer->readiness_callback_count,
            transfer->timer_callback_count, transfer->last_ready_mask);
    }
cleanup_curl:
    cancel_pending_work(transfer);
    if (transfer->multi != NULL) {
        for (request_index = 0u; request_index < SPIKE_TRANSFER_COUNT;
                ++request_index) {
            if (easy[request_index] != NULL) {
                (void)curl_multi_remove_handle(transfer->multi,
                    easy[request_index]);
                curl_easy_cleanup(easy[request_index]);
            }
        }
    }
    if (transfer->multi != NULL) curl_multi_cleanup(transfer->multi);
    curl_global_cleanup();
cleanup_fixture:
    if (fixture_started) stop_fixture(&server, thread);
    if (result == 0) puts("CX scheduler/libcurl threaded resolver load pass");
    return result;
}

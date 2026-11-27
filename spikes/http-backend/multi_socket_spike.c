#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
typedef SOCKET spike_socket;
typedef HANDLE spike_thread;
typedef int spike_socket_length;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int spike_socket;
typedef pthread_t spike_thread;
typedef socklen_t spike_socket_length;
#endif

#define SPIKE_MAX_SOCKETS 32

struct socket_watch {
    curl_socket_t socket;
    int events;
};

struct spike_state {
    CURLM *multi;
    struct socket_watch watches[SPIKE_MAX_SOCKETS];
    size_t watch_count;
    long timeout_ms;
    size_t body_length;
    char body[64];
    int callback_error;
};

struct server_state {
    spike_socket listener;
    int failed;
};

static size_t receive_body(char *data, size_t size, size_t count, void *value) {
    struct spike_state *state = (struct spike_state *)value;
    size_t length = size * count;
    if (length > sizeof(state->body) - state->body_length) {
        state->callback_error = 1;
        return 0;
    }
    memcpy(state->body + state->body_length, data, length);
    state->body_length += length;
    return length;
}

static int find_watch(struct spike_state *state, curl_socket_t socket) {
    size_t index;
    for (index = 0; index < state->watch_count; ++index) {
        if (state->watches[index].socket == socket) return (int)index;
    }
    return -1;
}

static int socket_event(CURL *easy, curl_socket_t socket, int what,
    void *user_value, void *socket_value) {
    struct spike_state *state = (struct spike_state *)user_value;
    int index = find_watch(state, socket);
    int events = 0;
    (void)easy;
    (void)socket_value;
    if (what & CURL_POLL_IN) events |= 1;
    if (what & CURL_POLL_OUT) events |= 2;
    if (what == CURL_POLL_REMOVE) {
        if (index >= 0) {
            state->watches[index] = state->watches[state->watch_count - 1];
            state->watch_count -= 1;
        }
        return 0;
    }
    if (index < 0) {
        if (state->watch_count == SPIKE_MAX_SOCKETS) {
            state->callback_error = 1;
            return -1;
        }
        index = (int)state->watch_count++;
        state->watches[index].socket = socket;
    }
    state->watches[index].events = events;
    return 0;
}

static int timer_event(CURLM *multi, long timeout_ms, void *user_value) {
    struct spike_state *state = (struct spike_state *)user_value;
    (void)multi;
    state->timeout_ms = timeout_ms;
    return 0;
}

static int send_response(spike_socket socket) {
    static const char response[] =
        "HTTP/1.1 200 OK\r\nContent-Length: 4\r\n"
        "Connection: close\r\n\r\npong";
    size_t sent = 0;
    while (sent < sizeof(response) - 1) {
#ifdef _WIN32
        int result = send(socket, response + sent,
            (int)(sizeof(response) - 1 - sent), 0);
#else
        ssize_t result = send(socket, response + sent,
            sizeof(response) - 1 - sent, 0);
#endif
        if (result <= 0) return 0;
        sent += (size_t)result;
    }
    return 1;
}

static int receive_request_headers(spike_socket socket) {
    char headers[4096];
    size_t received = 0;
    while (received < sizeof(headers)) {
#ifdef _WIN32
        int result = recv(socket, headers + received,
            (int)(sizeof(headers) - received), 0);
#else
        ssize_t result = recv(socket, headers + received,
            sizeof(headers) - received, 0);
#endif
        if (result <= 0) return 0;
        received += (size_t)result;
        if (received >= 4 &&
            memcmp(headers + received - 4, "\r\n\r\n", 4) == 0) {
            return 1;
        }
    }
    return 0;
}

#ifdef _WIN32
static DWORD WINAPI server_thread(void *value) {
#else
static void *server_thread(void *value) {
#endif
    struct server_state *state = (struct server_state *)value;
    spike_socket client = accept(state->listener, NULL, NULL);
#ifdef _WIN32
    if (client == INVALID_SOCKET || !receive_request_headers(client) ||
        !send_response(client)) state->failed = 1;
    if (client != INVALID_SOCKET) closesocket(client);
    return 0;
#else
    if (client < 0 || !receive_request_headers(client) ||
        !send_response(client)) state->failed = 1;
    if (client >= 0) close(client);
    return NULL;
#endif
}

static int start_server(struct server_state *server, uint16_t *port,
    spike_thread *thread) {
    struct sockaddr_in address;
    spike_socket_length address_length = (spike_socket_length)sizeof(address);
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
            (spike_socket_length)sizeof(address)) != 0 ||
        listen(server->listener, 1) != 0 ||
        getsockname(server->listener, (struct sockaddr *)&address,
            &address_length) != 0) return 0;
    *port = ntohs(address.sin_port);
    server->failed = 0;
#ifdef _WIN32
    *thread = CreateThread(NULL, 0, server_thread, server, 0, NULL);
    return *thread != NULL;
#else
    return pthread_create(thread, NULL, server_thread, server) == 0;
#endif
}

static int wait_and_drive(struct spike_state *state, int *running) {
    size_t index;
    size_t watched_count = state->watch_count;
    long pending_timeout = state->timeout_ms;
    long timeout = pending_timeout < 0 ? 1000 : pending_timeout;
    int poll_result = 0;
    if (timeout > 1000) timeout = 1000;
    if (state->timeout_ms == 0) {
        state->timeout_ms = -1;
        return curl_multi_socket_action(state->multi, CURL_SOCKET_TIMEOUT,
            0, running) == CURLM_OK;
    }
#ifdef _WIN32
    {
        WSAPOLLFD descriptors[SPIKE_MAX_SOCKETS];
        for (index = 0; index < watched_count; ++index) {
            descriptors[index].fd = state->watches[index].socket;
            descriptors[index].events = 0;
            if (state->watches[index].events & 1) descriptors[index].events |= POLLRDNORM;
            if (state->watches[index].events & 2) descriptors[index].events |= POLLWRNORM;
            descriptors[index].revents = 0;
        }
        if (watched_count == 0) {
            Sleep((DWORD)timeout);
            poll_result = 0;
        } else {
            poll_result = WSAPoll(descriptors, (ULONG)watched_count, (int)timeout);
        }
        if (poll_result == SOCKET_ERROR) return 0;
        for (index = 0; index < watched_count; ++index) {
            int flags = 0;
            if (descriptors[index].revents & (POLLRDNORM | POLLIN | POLLHUP)) flags |= CURL_CSELECT_IN;
            if (descriptors[index].revents & (POLLWRNORM | POLLOUT)) flags |= CURL_CSELECT_OUT;
            if (descriptors[index].revents & (POLLERR | POLLNVAL)) flags |= CURL_CSELECT_ERR;
            if (flags != 0 && find_watch(state, descriptors[index].fd) >= 0 &&
                curl_multi_socket_action(state->multi,
                    descriptors[index].fd, flags, running) != CURLM_OK) return 0;
        }
    }
#else
    {
        struct pollfd descriptors[SPIKE_MAX_SOCKETS];
        for (index = 0; index < watched_count; ++index) {
            descriptors[index].fd = state->watches[index].socket;
            descriptors[index].events = 0;
            if (state->watches[index].events & 1) descriptors[index].events |= POLLIN;
            if (state->watches[index].events & 2) descriptors[index].events |= POLLOUT;
            descriptors[index].revents = 0;
        }
        poll_result = poll(descriptors, watched_count, (int)timeout);
        if (poll_result < 0) return 0;
        for (index = 0; index < watched_count; ++index) {
            int flags = 0;
            if (descriptors[index].revents & (POLLIN | POLLHUP)) flags |= CURL_CSELECT_IN;
            if (descriptors[index].revents & POLLOUT) flags |= CURL_CSELECT_OUT;
            if (descriptors[index].revents & (POLLERR | POLLNVAL)) flags |= CURL_CSELECT_ERR;
            if (flags != 0 && find_watch(state, descriptors[index].fd) >= 0 &&
                curl_multi_socket_action(state->multi,
                    descriptors[index].fd, flags, running) != CURLM_OK) return 0;
        }
    }
#endif
    if (poll_result == 0 && pending_timeout >= 0 && pending_timeout <= timeout) {
        state->timeout_ms = -1;
        return curl_multi_socket_action(state->multi, CURL_SOCKET_TIMEOUT,
            0, running) == CURLM_OK;
    }
    return 1;
}

static void stop_server(struct server_state *server, spike_thread thread) {
#ifdef _WIN32
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    closesocket(server->listener);
    WSACleanup();
#else
    pthread_join(thread, NULL);
    close(server->listener);
#endif
}

int main(void) {
    struct spike_state state;
    struct server_state server;
    spike_thread thread;
    uint16_t port = 0;
    CURL *easy = NULL;
    CURLM *multi = NULL;
    CURLMcode multi_result;
    CURLcode easy_result = CURLE_FAILED_INIT;
    int running = 0;
    int result = 1;
    int messages_left = 0;
    CURLMsg *message;
    const curl_version_info_data *version;

    memset(&state, 0, sizeof(state));
    state.timeout_ms = -1;
    version = curl_version_info(CURLVERSION_NOW);
    if (version == NULL || !(version->features & CURL_VERSION_ASYNCHDNS) ||
        !(version->features & CURL_VERSION_SSL)) {
        fputs("libcurl is missing asynchronous DNS or TLS support\n", stderr);
        return 1;
    }
    if (!start_server(&server, &port, &thread)) {
        fputs("could not start the loopback HTTP fixture\n", stderr);
        return 1;
    }
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) goto cleanup_server;
    multi = curl_multi_init();
    easy = curl_easy_init();
    if (multi == NULL || easy == NULL) goto cleanup_curl;
    state.multi = multi;
    curl_multi_setopt(multi, CURLMOPT_SOCKETFUNCTION, socket_event);
    curl_multi_setopt(multi, CURLMOPT_SOCKETDATA, &state);
    curl_multi_setopt(multi, CURLMOPT_TIMERFUNCTION, timer_event);
    curl_multi_setopt(multi, CURLMOPT_TIMERDATA, &state);
    {
        char url[128];
        snprintf(url, sizeof(url),
            "http://localhost:%u/fixture", (unsigned)port);
        curl_easy_setopt(easy, CURLOPT_URL, url);
        curl_easy_setopt(easy, CURLOPT_PROTOCOLS_STR, "http");
        curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, receive_body);
        curl_easy_setopt(easy, CURLOPT_WRITEDATA, &state);
        curl_easy_setopt(easy, CURLOPT_CONNECTTIMEOUT_MS, 3000L);
        curl_easy_setopt(easy, CURLOPT_TIMEOUT_MS, 5000L);
    }
    multi_result = curl_multi_add_handle(multi, easy);
    if (multi_result != CURLM_OK) goto cleanup_curl;
    multi_result = curl_multi_socket_action(multi, CURL_SOCKET_TIMEOUT, 0, &running);
    while (multi_result == CURLM_OK && running > 0 && !state.callback_error) {
        if (!wait_and_drive(&state, &running)) break;
        multi_result = CURLM_OK;
    }
    if (multi_result == CURLM_OK) {
        message = curl_multi_info_read(multi, &messages_left);
        if (message != NULL && message->msg == CURLMSG_DONE) {
            easy_result = message->data.result;
        }
    }
    if (easy_result == CURLE_OK && state.body_length == 4u &&
        memcmp(state.body, "pong", 4u) == 0 &&
        !state.callback_error) {
        result = 0;
    } else {
        fprintf(stderr, "multi socket transfer failed: %s; body bytes=%u\n",
            curl_easy_strerror(easy_result), (unsigned)state.body_length);
    }
cleanup_curl:
    if (multi != NULL && easy != NULL) {
        curl_multi_remove_handle(multi, easy);
        curl_easy_cleanup(easy);
    }
    if (multi != NULL) curl_multi_cleanup(multi);
    curl_global_cleanup();
cleanup_server:
    stop_server(&server, thread);
    if (result == 0) puts("curl multi socket spike pass");
    return result;
}

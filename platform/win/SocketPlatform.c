#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../common/SocketScheduler.internal.h"

static SRWLOCK cxnet_socket_startup_lock = SRWLOCK_INIT;
static long cxnet_socket_startup_references;
static int cxnet_socket_startup_error_code;

static int cxnet_platform_socket_startup_acquire(void) {
    int result = 1;
    AcquireSRWLockExclusive(&cxnet_socket_startup_lock);
    if (InterlockedCompareExchange(&cxnet_socket_startup_references, 0, 0) == 0) {
        WSADATA data;
        cxnet_socket_startup_error_code = WSAStartup(MAKEWORD(2, 2), &data);
        result = cxnet_socket_startup_error_code == 0;
    }
    if (result) {
        InterlockedIncrement(&cxnet_socket_startup_references);
    }
    ReleaseSRWLockExclusive(&cxnet_socket_startup_lock);
    return result;
}

static void cxnet_platform_socket_startup_release(void) {
    AcquireSRWLockExclusive(&cxnet_socket_startup_lock);
    if (cxnet_socket_startup_references > 0 &&
        InterlockedDecrement(&cxnet_socket_startup_references) == 0) {
        WSACleanup();
    }
    ReleaseSRWLockExclusive(&cxnet_socket_startup_lock);
}

static int cxnet_platform_socket_startup_error(void) {
    return cxnet_socket_startup_error_code == 0
        ? WSASYSNOTREADY : cxnet_socket_startup_error_code;
}

int cxnet_platform_resolve(const wchar_t *host,
    struct cxnet_resolved_address *addresses,
    uint32_t capacity, uint32_t *count) {
    ADDRINFOW hints;
    PADDRINFOW results = NULL;
    PADDRINFOW entry;
    uint32_t used = 0u;
    int status;
    if (host == NULL || addresses == NULL || count == NULL || capacity == 0u) {
        return WSAEINVAL;
    }
#ifdef CXNET_TESTING
    if (cxnet_test_resolver_try_resolve(host, addresses, capacity, count,
            &status)) return status;
#endif
    if (!cxnet_platform_socket_startup_acquire()) {
        return cxnet_platform_socket_startup_error();
    }
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    status = GetAddrInfoW(host, NULL, &hints, &results);
    if (status != 0) {
        cxnet_platform_socket_startup_release();
        return status;
    }
    for (entry = results; entry != NULL; entry = entry->ai_next) {
        struct cxnet_resolved_address candidate;
        uint32_t index;
        int duplicate = 0;
        memset(&candidate, 0, sizeof(candidate));
        if (entry->ai_family == AF_INET &&
            entry->ai_addrlen >= (int)sizeof(struct sockaddr_in)) {
            const struct sockaddr_in *address =
                (const struct sockaddr_in *)entry->ai_addr;
            candidate.address_family = 2u;
            candidate.word0 = ntohl(address->sin_addr.s_addr);
        } else if (entry->ai_family == AF_INET6 &&
            entry->ai_addrlen >= (int)sizeof(struct sockaddr_in6)) {
            const struct sockaddr_in6 *address =
                (const struct sockaddr_in6 *)entry->ai_addr;
            uint32_t words[4];
            memcpy(words, address->sin6_addr.u.Byte, sizeof(words));
            candidate.address_family = 23u;
            candidate.word0 = ntohl(words[0]);
            candidate.word1 = ntohl(words[1]);
            candidate.word2 = ntohl(words[2]);
            candidate.word3 = ntohl(words[3]);
            candidate.scope_id = address->sin6_scope_id;
        } else {
            continue;
        }
        for (index = 0u; index < used; ++index) {
            if (memcmp(&addresses[index], &candidate, sizeof(candidate)) == 0) {
                duplicate = 1;
                break;
            }
        }
        if (!duplicate) {
            if (used == capacity) {
                FreeAddrInfoW(results);
                cxnet_platform_socket_startup_release();
                return WSAEFAULT;
            }
            addresses[used++] = candidate;
        }
    }
    FreeAddrInfoW(results);
    cxnet_platform_socket_startup_release();
    if (used == 0u) return WSAHOST_NOT_FOUND;
    *count = used;
    return 0;
}

int cxnet_platform_socket_set_nonblocking(uintptr_t socket_handle) {
    u_long enabled = 1;
    if (socket_handle == UINTPTR_MAX) return WSAENOTSOCK;
    if (!cxnet_platform_socket_startup_acquire()) {
        return cxnet_platform_socket_startup_error();
    }
    if (ioctlsocket((SOCKET)socket_handle, FIONBIO, &enabled) != 0) {
        return WSAGetLastError();
    }
    return 0;
}

static int cxnet_platform_address_family(uint32_t address_family) {
    if (address_family == 2u) return AF_INET;
    if (address_family == 23u) return AF_INET6;
    return AF_UNSPEC;
}

static int cxnet_platform_make_address(
    uint32_t address_family,
    uint32_t word0, uint32_t word1, uint32_t word2, uint32_t word3,
    uint32_t scope_id, uint32_t port,
    struct sockaddr_storage *storage, int *length) {
    int native_family = cxnet_platform_address_family(address_family);
    if (port > 65535u) return WSAEINVAL;
    memset(storage, 0, sizeof(*storage));
    if (native_family == AF_INET) {
        struct sockaddr_in *address = (struct sockaddr_in *)storage;
        address->sin_family = AF_INET;
        address->sin_port = htons((u_short)port);
        address->sin_addr.s_addr = htonl(word0);
        *length = (int)sizeof(*address);
        return 0;
    }
    if (native_family == AF_INET6) {
        struct sockaddr_in6 *address = (struct sockaddr_in6 *)storage;
        uint32_t words[4] = {
            htonl(word0), htonl(word1), htonl(word2), htonl(word3)};
        address->sin6_family = AF_INET6;
        address->sin6_port = htons((u_short)port);
        address->sin6_scope_id = scope_id;
        memcpy(address->sin6_addr.u.Byte, words, sizeof(words));
        *length = (int)sizeof(*address);
        return 0;
    }
    return WSAEAFNOSUPPORT;
}

int cxnet_platform_socket_connect(
    uintptr_t socket_handle, uint32_t address_family,
    uint32_t word0, uint32_t word1, uint32_t word2, uint32_t word3,
    uint32_t scope_id, uint32_t port) {
    struct sockaddr_storage address;
    int length = 0;
    int error_code = cxnet_platform_make_address(address_family,
        word0, word1, word2, word3, scope_id, port, &address, &length);
    if (error_code != 0) return error_code;
    if (connect((SOCKET)socket_handle, (struct sockaddr *)&address, length) == 0) {
        return 0;
    }
    error_code = WSAGetLastError();
    if (error_code == WSAEWOULDBLOCK || error_code == WSAEINPROGRESS ||
        error_code == WSAEALREADY) return 1;
    return error_code;
}

int cxnet_platform_socket_finish_connect(uintptr_t socket_handle) {
    int error_code = 0;
    int length = (int)sizeof(error_code);
    if (getsockopt((SOCKET)socket_handle, SOL_SOCKET, SO_ERROR,
            (char *)&error_code, &length) == SOCKET_ERROR) return WSAGetLastError();
    return error_code;
}

int cxnet_platform_socket_connect_blocking(
    uintptr_t socket_handle, uint32_t address_family,
    uint32_t word0, uint32_t word1, uint32_t word2, uint32_t word3,
    uint32_t scope_id, uint32_t port) {
    int result = cxnet_platform_socket_connect(socket_handle, address_family,
        word0, word1, word2, word3, scope_id, port);
    if (result != 1) return result;
    for (;;) {
        WSAPOLLFD descriptor;
        descriptor.fd = (SOCKET)socket_handle;
        descriptor.events = POLLWRNORM;
        descriptor.revents = 0;
        result = WSAPoll(&descriptor, 1, -1);
        if (result == SOCKET_ERROR && WSAGetLastError() == WSAEINTR) continue;
        if (result == SOCKET_ERROR) return WSAGetLastError();
        return cxnet_platform_socket_finish_connect(socket_handle);
    }
}

int cxnet_platform_socket_accept(
    uintptr_t listener, uintptr_t *accepted_handle) {
    SOCKET accepted;
    u_long enabled = 1;
    if (accepted_handle == NULL) return WSAEINVAL;
    if (!cxnet_platform_socket_startup_acquire()) {
        return cxnet_platform_socket_startup_error();
    }
    accepted = accept((SOCKET)listener, NULL, NULL);
    if (accepted == INVALID_SOCKET) {
        int error_code = WSAGetLastError();
        cxnet_platform_socket_startup_release();
        if (error_code == WSAEWOULDBLOCK) return 1;
        return error_code;
    }
    if (ioctlsocket(accepted, FIONBIO, &enabled) != 0) {
        int error_code = WSAGetLastError();
        closesocket(accepted);
        cxnet_platform_socket_startup_release();
        return error_code;
    }
    *accepted_handle = (uintptr_t)accepted;
    return 0;
}

int cxnet_platform_socket_accept_blocking(
    uintptr_t listener, uintptr_t *accepted_handle) {
    for (;;) {
        int result = cxnet_platform_socket_accept(listener, accepted_handle);
        if (result != 1) return result;
        {
            WSAPOLLFD descriptor;
            descriptor.fd = (SOCKET)listener;
            descriptor.events = POLLRDNORM;
            descriptor.revents = 0;
            result = WSAPoll(&descriptor, 1, -1);
            if (result == SOCKET_ERROR && WSAGetLastError() == WSAEINTR) continue;
            if (result == SOCKET_ERROR) return WSAGetLastError();
        }
    }
}

int cxnet_platform_socket_get_address(
    uintptr_t handle, int remote, struct cxnet_socket_address *result) {
    struct sockaddr_storage address;
    int length = (int)sizeof(address);
    int status;
    if (result == NULL) return WSAEINVAL;
    memset(&address, 0, sizeof(address));
    status = remote
        ? getpeername((SOCKET)handle, (struct sockaddr *)&address, &length)
        : getsockname((SOCKET)handle, (struct sockaddr *)&address, &length);
    if (status == SOCKET_ERROR) return WSAGetLastError();
    memset(result, 0, sizeof(*result));
    if (address.ss_family == AF_INET) {
        const struct sockaddr_in *ipv4 = (const struct sockaddr_in *)&address;
        result->address_family = 2u;
        result->word0 = ntohl(ipv4->sin_addr.s_addr);
        result->port = ntohs(ipv4->sin_port);
        return 0;
    }
    if (address.ss_family == AF_INET6) {
        const struct sockaddr_in6 *ipv6 = (const struct sockaddr_in6 *)&address;
        uint32_t words[4];
        memcpy(words, ipv6->sin6_addr.u.Byte, sizeof(words));
        result->address_family = 23u;
        result->word0 = ntohl(words[0]);
        result->word1 = ntohl(words[1]);
        result->word2 = ntohl(words[2]);
        result->word3 = ntohl(words[3]);
        result->scope_id = ipv6->sin6_scope_id;
        result->port = ntohs(ipv6->sin6_port);
        return 0;
    }
    return WSAEAFNOSUPPORT;
}

int cxnet_platform_socket_create(uint32_t address_family, uintptr_t *socket_handle) {
    int native_family = cxnet_platform_address_family(address_family);
    SOCKET handle;
    u_long enabled = 1;
    if (native_family == AF_UNSPEC || socket_handle == NULL) return WSAEAFNOSUPPORT;
    if (!cxnet_platform_socket_startup_acquire()) {
        return cxnet_platform_socket_startup_error();
    }
    handle = WSASocket(native_family, SOCK_STREAM, IPPROTO_TCP, NULL, 0,
        WSA_FLAG_OVERLAPPED | WSA_FLAG_NO_HANDLE_INHERIT);
    if (handle == INVALID_SOCKET) {
        int error_code = WSAGetLastError();
        cxnet_platform_socket_startup_release();
        return error_code;
    }
    if (ioctlsocket(handle, FIONBIO, &enabled) != 0) {
        int error_code = WSAGetLastError();
        closesocket(handle);
        cxnet_platform_socket_startup_release();
        return error_code;
    }
    *socket_handle = (uintptr_t)handle;
    return 0;
}

int cxnet_platform_socket_bind(
    uintptr_t socket_handle, uint32_t address_family,
    uint32_t word0, uint32_t word1, uint32_t word2, uint32_t word3,
    uint32_t scope_id, uint32_t port) {
    int native_family = cxnet_platform_address_family(address_family);
    if (port > 65535u) return WSAEINVAL;
    if (native_family == AF_INET) {
        struct sockaddr_in address;
        memset(&address, 0, sizeof(address));
        address.sin_family = AF_INET;
        address.sin_port = htons((u_short)port);
        address.sin_addr.s_addr = htonl(word0);
        if (bind((SOCKET)socket_handle, (struct sockaddr *)&address,
                (int)sizeof(address)) == SOCKET_ERROR) return WSAGetLastError();
        return 0;
    }
    if (native_family == AF_INET6) {
        struct sockaddr_in6 address;
        uint32_t words[4];
        words[0] = htonl(word0);
        words[1] = htonl(word1);
        words[2] = htonl(word2);
        words[3] = htonl(word3);
        memset(&address, 0, sizeof(address));
        address.sin6_family = AF_INET6;
        address.sin6_port = htons((u_short)port);
        address.sin6_scope_id = scope_id;
        memcpy(address.sin6_addr.u.Byte, words, sizeof(words));
        if (bind((SOCKET)socket_handle, (struct sockaddr *)&address,
                (int)sizeof(address)) == SOCKET_ERROR) return WSAGetLastError();
        return 0;
    }
    return WSAEAFNOSUPPORT;
}

int cxnet_platform_socket_listen(uintptr_t socket_handle, int backlog) {
    if (backlog < 0) return WSAEINVAL;
    if (listen((SOCKET)socket_handle, backlog == 0 ? SOMAXCONN : backlog) == SOCKET_ERROR) {
        return WSAGetLastError();
    }
    return 0;
}

int cxnet_platform_socket_shutdown(uintptr_t socket_handle, int how) {
    if (how < 0 || how > 2) return WSAEINVAL;
    if (shutdown((SOCKET)socket_handle, how) == SOCKET_ERROR) return WSAGetLastError();
    return 0;
}

int cxnet_platform_socket_set_option(
    uintptr_t socket_handle, int option, int value) {
    int level = SOL_SOCKET;
    int name;
    int native_value = value;
    if (option == 1) { level = IPPROTO_TCP; name = TCP_NODELAY; }
    else if (option == 2) name = SO_KEEPALIVE;
    else if (option == 3) name = SO_RCVBUF;
    else if (option == 4) name = SO_SNDBUF;
    else if (option == 5) { level = IPPROTO_IPV6; name = IPV6_V6ONLY; native_value = !value; }
    else if (option == 6) name = SO_REUSEADDR;
    else return WSAEINVAL;
    if (setsockopt((SOCKET)socket_handle, level, name,
            (const char *)&native_value, (int)sizeof(native_value)) == SOCKET_ERROR) {
        return WSAGetLastError();
    }
    return 0;
}

int cxnet_platform_socket_get_option(
    uintptr_t socket_handle, int option, int *value) {
    int level = SOL_SOCKET;
    int name;
    int native_value = 0;
    int length = (int)sizeof(native_value);
    if (value == NULL) return WSAEINVAL;
    if (option == 1) { level = IPPROTO_TCP; name = TCP_NODELAY; }
    else if (option == 2) name = SO_KEEPALIVE;
    else if (option == 3) name = SO_RCVBUF;
    else if (option == 4) name = SO_SNDBUF;
    else if (option == 5) { level = IPPROTO_IPV6; name = IPV6_V6ONLY; }
    else if (option == 6) name = SO_REUSEADDR;
    else return WSAEINVAL;
    if (getsockopt((SOCKET)socket_handle, level, name,
            (char *)&native_value, &length) == SOCKET_ERROR) return WSAGetLastError();
    *value = option == 5 ? !native_value : native_value;
    return 0;
}

uint32_t cxnet_platform_socket_error_domain(void) {
    return 2u; /* Winsock */
}

int64_t cxnet_platform_socket_transfer(
    uintptr_t socket_handle,
    void *buffer,
    uint32_t count,
    int writable,
    int *native_error_code) {
    int request_count = count > INT_MAX ? INT_MAX : (int)count;
    int result;
    int error_code;
    if (InterlockedCompareExchange(&cxnet_socket_startup_references, 0, 0) == 0) {
        if (native_error_code != NULL) {
            *native_error_code = cxnet_platform_socket_startup_error();
        }
        return -1;
    }
    do {
        result = writable
            ? send((SOCKET)socket_handle, (const char *)buffer, request_count, 0)
            : recv((SOCKET)socket_handle, (char *)buffer, request_count, 0);
        error_code = result == SOCKET_ERROR ? WSAGetLastError() : 0;
    } while (result == SOCKET_ERROR && error_code == WSAEINTR);
    if (native_error_code != NULL) *native_error_code = error_code;
    if (result == SOCKET_ERROR && error_code == WSAEWOULDBLOCK) {
        return -2;
    }
    return result == SOCKET_ERROR ? -1 : result;
}

int64_t cxnet_platform_socket_transfer_blocking(
    uintptr_t socket_handle, void *buffer, uint32_t count,
    int writable, int *native_error_code) {
    for (;;) {
        int local_error = 0;
        int64_t result = cxnet_platform_socket_transfer(
            socket_handle, buffer, count, writable, &local_error);
        if (result != -2) {
            if (native_error_code != NULL) *native_error_code = local_error;
            return result;
        }
        {
            WSAPOLLFD descriptor;
            descriptor.fd = (SOCKET)socket_handle;
            descriptor.events = writable ? POLLWRNORM : POLLRDNORM;
            descriptor.revents = 0;
            result = WSAPoll(&descriptor, 1, -1);
            if (result == SOCKET_ERROR && WSAGetLastError() == WSAEINTR) continue;
            if (result == SOCKET_ERROR) {
                if (native_error_code != NULL) *native_error_code = WSAGetLastError();
                return -1;
            }
        }
    }
}

int cxnet_platform_socket_close(uintptr_t socket_handle) {
    if (socket_handle == UINTPTR_MAX) return 1;
    if (closesocket((SOCKET)socket_handle) != 0) return 0;
    cxnet_platform_socket_startup_release();
    return 1;
}

int cxnet_platform_socket_last_error(void) {
    return WSAGetLastError();
}

int cxnet_platform_wait_sockets(
    struct cx_scheduler_wait_entry *entries,
    size_t count,
    uint32_t timeout_milliseconds) {
    WSAPOLLFD *descriptors;
    size_t index;
    int timeout = timeout_milliseconds == UINT32_MAX
        ? -1
        : timeout_milliseconds > (uint32_t)INT32_MAX
            ? INT32_MAX
            : (int)timeout_milliseconds;
    int result;
    if (count == 0) {
        Sleep(timeout < 0 ? INFINITE : (DWORD)timeout);
        return 0;
    }
    if (count > ULONG_MAX || count > SIZE_MAX / sizeof(*descriptors)) {
        return -1;
    }
    descriptors = (WSAPOLLFD *)malloc(count * sizeof(*descriptors));
    if (descriptors == NULL) {
        return -1;
    }
    for (index = 0; index < count; ++index) {
        uint32_t interest_mask =
            ((struct cxnet_socket_wait *)entries[index].context)->interest_mask;
        descriptors[index].fd = (SOCKET)((struct cxnet_socket_wait *)entries[index].context)->handle;
        descriptors[index].events = 0;
        if ((interest_mask & CXNET_SOCKET_READABLE) != 0u) {
            descriptors[index].events |= POLLRDNORM;
        }
        if ((interest_mask & CXNET_SOCKET_WRITABLE) != 0u) {
            descriptors[index].events |= POLLWRNORM;
        }
        descriptors[index].revents = 0;
        entries[index].ready = 0;
    }
    result = WSAPoll(descriptors, (ULONG)count, timeout);
    if (result == SOCKET_ERROR) {
        int wait_error = WSAGetLastError();
        if (wait_error == WSAENOTSOCK) {
            int invalid_count = 0;
            for (index = 0; index < count; ++index) {
                int socket_type = 0;
                int socket_type_length = (int)sizeof(socket_type);
                SOCKET socket_handle = descriptors[index].fd;
                if (getsockopt(socket_handle, SOL_SOCKET, SO_TYPE,
                        (char *)&socket_type, &socket_type_length) ==
                    SOCKET_ERROR && WSAGetLastError() == WSAENOTSOCK) {
                    struct cxnet_socket_wait *wait =
                        (struct cxnet_socket_wait *)entries[index].context;
                    if (wait->ready_mask != NULL) {
                        *wait->ready_mask = CXNET_SOCKET_ERROR;
                    }
                    entries[index].ready = 1;
                    invalid_count++;
                }
            }
            if (invalid_count != 0) {
                free(descriptors);
                return invalid_count;
            }
        }
    }
    if (result > 0) {
        for (index = 0; index < count; ++index) {
            uint32_t interest_mask =
                ((struct cxnet_socket_wait *)entries[index].context)->interest_mask;
            struct cxnet_socket_wait *wait =
                (struct cxnet_socket_wait *)entries[index].context;
            uint32_t ready_mask = 0u;
            if ((interest_mask & CXNET_SOCKET_READABLE) != 0u &&
                (descriptors[index].revents & (POLLRDNORM | POLLIN)) != 0) {
                ready_mask |= CXNET_SOCKET_READABLE;
            }
            if ((interest_mask & CXNET_SOCKET_WRITABLE) != 0u &&
                (descriptors[index].revents & (POLLWRNORM | POLLOUT)) != 0) {
                ready_mask |= CXNET_SOCKET_WRITABLE;
            }
            if ((descriptors[index].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
                ready_mask |= CXNET_SOCKET_ERROR;
            }
            if (wait->ready_mask != NULL) *wait->ready_mask = ready_mask;
            entries[index].ready = ready_mask != 0u;
        }
    }
    free(descriptors);
    return result;
}

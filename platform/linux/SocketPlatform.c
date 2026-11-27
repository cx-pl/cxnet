#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <malloc.h>
#include "../common/SocketScheduler.internal.h"
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

int cxnet_platform_resolve(const char *host,
    struct cxnet_resolved_address *addresses,
    uint32_t capacity, uint32_t *count) {
    struct addrinfo hints;
    struct addrinfo *results = NULL;
    struct addrinfo *entry;
    uint32_t used = 0u;
    int status;
    if (host == NULL || addresses == NULL || count == NULL || capacity == 0u) {
        return EAI_BADFLAGS;
    }
#ifdef CXNET_TESTING
    if (cxnet_test_resolver_try_resolve(host, addresses, capacity, count,
            &status)) return status;
#endif
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    status = getaddrinfo(host, NULL, &hints, &results);
    if (status != 0) return status;
    for (entry = results; entry != NULL; entry = entry->ai_next) {
        struct cxnet_resolved_address candidate;
        uint32_t index;
        int duplicate = 0;
        memset(&candidate, 0, sizeof(candidate));
        if (entry->ai_family == AF_INET &&
            entry->ai_addrlen >= (socklen_t)sizeof(struct sockaddr_in)) {
            const struct sockaddr_in *address =
                (const struct sockaddr_in *)entry->ai_addr;
            candidate.address_family = 2u;
            candidate.word0 = ntohl(address->sin_addr.s_addr);
        } else if (entry->ai_family == AF_INET6 &&
            entry->ai_addrlen >= (socklen_t)sizeof(struct sockaddr_in6)) {
            const struct sockaddr_in6 *address =
                (const struct sockaddr_in6 *)entry->ai_addr;
            uint32_t words[4];
            memcpy(words, address->sin6_addr.s6_addr, sizeof(words));
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
                freeaddrinfo(results);
                return EAI_OVERFLOW;
            }
            addresses[used++] = candidate;
        }
    }
    freeaddrinfo(results);
    if (used == 0u) return EAI_NONAME;
    *count = used;
    return 0;
}

static int cxnet_platform_address_family(uint32_t address_family) {
    if (address_family == 2u) return AF_INET;
    if (address_family == 23u) return AF_INET6;
    return -1;
}

static int cxnet_platform_make_address(
    uint32_t address_family,
    uint32_t word0, uint32_t word1, uint32_t word2, uint32_t word3,
    uint32_t scope_id, uint32_t port,
    struct sockaddr_storage *storage, socklen_t *length) {
    int native_family = cxnet_platform_address_family(address_family);
    if (port > 65535u) return EINVAL;
    memset(storage, 0, sizeof(*storage));
    if (native_family == AF_INET) {
        struct sockaddr_in *address = (struct sockaddr_in *)storage;
        address->sin_family = AF_INET;
        address->sin_port = htons((uint16_t)port);
        address->sin_addr.s_addr = htonl(word0);
        *length = (socklen_t)sizeof(*address);
        return 0;
    }
    if (native_family == AF_INET6) {
        struct sockaddr_in6 *address = (struct sockaddr_in6 *)storage;
        uint32_t words[4] = {
            htonl(word0), htonl(word1), htonl(word2), htonl(word3)};
        address->sin6_family = AF_INET6;
        address->sin6_port = htons((uint16_t)port);
        address->sin6_scope_id = scope_id;
        memcpy(address->sin6_addr.s6_addr, words, sizeof(words));
        *length = (socklen_t)sizeof(*address);
        return 0;
    }
    return EAFNOSUPPORT;
}

int cxnet_platform_socket_connect(
    uintptr_t socket_handle, uint32_t address_family,
    uint32_t word0, uint32_t word1, uint32_t word2, uint32_t word3,
    uint32_t scope_id, uint32_t port) {
    struct sockaddr_storage address;
    socklen_t length = 0;
    int error_code = cxnet_platform_make_address(address_family,
        word0, word1, word2, word3, scope_id, port, &address, &length);
    if (error_code != 0) return error_code;
    if (connect((int)socket_handle, (struct sockaddr *)&address, length) == 0) {
        return 0;
    }
    error_code = errno;
    if (error_code == EINPROGRESS || error_code == EALREADY ||
        error_code == EAGAIN || error_code == EWOULDBLOCK) return 1;
    return error_code;
}

int cxnet_platform_socket_finish_connect(uintptr_t socket_handle) {
    int error_code = 0;
    socklen_t length = (socklen_t)sizeof(error_code);
    if (getsockopt((int)socket_handle, SOL_SOCKET, SO_ERROR,
            &error_code, &length) != 0) return errno;
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
        struct pollfd descriptor;
        descriptor.fd = (int)socket_handle;
        descriptor.events = POLLOUT;
        descriptor.revents = 0;
        result = poll(&descriptor, 1, -1);
        if (result < 0 && errno == EINTR) continue;
        if (result < 0) return errno;
        return cxnet_platform_socket_finish_connect(socket_handle);
    }
}

int cxnet_platform_socket_accept(
    uintptr_t listener, uintptr_t *accepted_handle) {
    int accepted;
    int flags;
    if (accepted_handle == NULL) return EINVAL;
    accepted = accept((int)listener, NULL, NULL);
    if (accepted < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 1;
        return errno;
    }
    flags = fcntl(accepted, F_GETFL, 0);
    if (flags < 0 || fcntl(accepted, F_SETFL, flags | O_NONBLOCK) < 0 ||
        fcntl(accepted, F_SETFD, FD_CLOEXEC) < 0) {
        int error_code = errno;
        close(accepted);
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
            struct pollfd descriptor;
            descriptor.fd = (int)listener;
            descriptor.events = POLLIN;
            descriptor.revents = 0;
            result = poll(&descriptor, 1, -1);
            if (result < 0 && errno == EINTR) continue;
            if (result < 0) return errno;
        }
    }
}

int cxnet_platform_socket_get_address(
    uintptr_t handle, int remote, struct cxnet_socket_address *result) {
    struct sockaddr_storage address;
    socklen_t length = (socklen_t)sizeof(address);
    int status;
    if (result == NULL) return EINVAL;
    memset(&address, 0, sizeof(address));
    status = remote
        ? getpeername((int)handle, (struct sockaddr *)&address, &length)
        : getsockname((int)handle, (struct sockaddr *)&address, &length);
    if (status != 0) return errno;
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
        memcpy(words, ipv6->sin6_addr.s6_addr, sizeof(words));
        result->address_family = 23u;
        result->word0 = ntohl(words[0]);
        result->word1 = ntohl(words[1]);
        result->word2 = ntohl(words[2]);
        result->word3 = ntohl(words[3]);
        result->scope_id = ipv6->sin6_scope_id;
        result->port = ntohs(ipv6->sin6_port);
        return 0;
    }
    return EAFNOSUPPORT;
}

int cxnet_platform_socket_create(uint32_t address_family, uintptr_t *socket_handle) {
    int native_family = cxnet_platform_address_family(address_family);
    int handle;
    int flags;
    if (native_family < 0 || socket_handle == NULL) return EAFNOSUPPORT;
    handle = socket(native_family, SOCK_STREAM, IPPROTO_TCP);
    if (handle < 0) return errno;
    flags = fcntl(handle, F_GETFL, 0);
    if (flags < 0 || fcntl(handle, F_SETFL, flags | O_NONBLOCK) < 0 ||
        fcntl(handle, F_SETFD, FD_CLOEXEC) < 0) {
        int error_code = errno;
        close(handle);
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
    if (native_family == AF_INET) {
        struct sockaddr_in address;
        if (port > 65535u) return EINVAL;
        memset(&address, 0, sizeof(address));
        address.sin_family = AF_INET;
        address.sin_port = htons((uint16_t)port);
        address.sin_addr.s_addr = htonl(word0);
        if (bind((int)socket_handle, (struct sockaddr *)&address,
                (socklen_t)sizeof(address)) != 0) return errno;
        return 0;
    }
    if (native_family == AF_INET6) {
        struct sockaddr_in6 address;
        uint32_t words[4];
        if (port > 65535u) return EINVAL;
        words[0] = htonl(word0);
        words[1] = htonl(word1);
        words[2] = htonl(word2);
        words[3] = htonl(word3);
        memset(&address, 0, sizeof(address));
        address.sin6_family = AF_INET6;
        address.sin6_port = htons((uint16_t)port);
        address.sin6_scope_id = scope_id;
        memcpy(address.sin6_addr.s6_addr, words, sizeof(words));
        if (bind((int)socket_handle, (struct sockaddr *)&address,
                (socklen_t)sizeof(address)) != 0) return errno;
        return 0;
    }
    return EAFNOSUPPORT;
}

int cxnet_platform_socket_listen(uintptr_t socket_handle, int backlog) {
    if (backlog < 0) return EINVAL;
    if (listen((int)socket_handle, backlog == 0 ? SOMAXCONN : backlog) != 0) {
        return errno;
    }
    return 0;
}

int cxnet_platform_socket_shutdown(uintptr_t socket_handle, int how) {
    int native_how;
    if (how == 0) native_how = SHUT_RD;
    else if (how == 1) native_how = SHUT_WR;
    else if (how == 2) native_how = SHUT_RDWR;
    else return EINVAL;
    if (shutdown((int)socket_handle, native_how) != 0) return errno;
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
    else return EINVAL;
    if (setsockopt((int)socket_handle, level, name, &native_value,
            (socklen_t)sizeof(native_value)) != 0) return errno;
    return 0;
}

int cxnet_platform_socket_get_option(
    uintptr_t socket_handle, int option, int *value) {
    int level = SOL_SOCKET;
    int name;
    int native_value = 0;
    socklen_t length = (socklen_t)sizeof(native_value);
    if (value == NULL) return EINVAL;
    if (option == 1) { level = IPPROTO_TCP; name = TCP_NODELAY; }
    else if (option == 2) name = SO_KEEPALIVE;
    else if (option == 3) name = SO_RCVBUF;
    else if (option == 4) name = SO_SNDBUF;
    else if (option == 5) { level = IPPROTO_IPV6; name = IPV6_V6ONLY; }
    else if (option == 6) name = SO_REUSEADDR;
    else return EINVAL;
    if (getsockopt((int)socket_handle, level, name, &native_value, &length) != 0) {
        return errno;
    }
    *value = option == 5 ? !native_value : native_value;
    return 0;
}

int cxnet_platform_socket_set_nonblocking(uintptr_t socket_handle) {
    if (socket_handle == UINTPTR_MAX) return EBADF;
    int flags = fcntl((int)socket_handle, F_GETFL, 0);
    if (flags < 0) return errno;
    if (fcntl((int)socket_handle, F_SETFL, flags | O_NONBLOCK) < 0) {
        return errno;
    }
    return 0;
}

uint32_t cxnet_platform_socket_error_domain(void) {
    return 1u; /* POSIX errno */
}

int64_t cxnet_platform_socket_transfer(
    uintptr_t socket_handle,
    void *buffer,
    uint32_t count,
    int writable,
    int *native_error_code) {
    ssize_t result;
    do {
        result = writable
            ? send((int)socket_handle, buffer, count, MSG_NOSIGNAL)
            : recv((int)socket_handle, buffer, count, 0);
    } while (result < 0 && errno == EINTR);
    if (native_error_code != NULL) {
        *native_error_code = result < 0 ? errno : 0;
    }
    if (result < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        return -2;
    }
    return (int64_t)result;
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
            struct pollfd descriptor;
            descriptor.fd = (int)socket_handle;
            descriptor.events = writable ? POLLOUT : POLLIN;
            descriptor.revents = 0;
            result = poll(&descriptor, 1, -1);
            if (result < 0 && errno == EINTR) continue;
            if (result < 0) {
                if (native_error_code != NULL) *native_error_code = errno;
                return -1;
            }
        }
    }
}

int cxnet_platform_socket_close(uintptr_t socket_handle) {
    if (socket_handle == UINTPTR_MAX) return 1;
    return close((int)socket_handle) == 0;
}

int cxnet_platform_socket_last_error(void) {
    return errno;
}

int cxnet_platform_wait_sockets(
    struct cx_scheduler_wait_entry *entries,
    size_t count,
    uint32_t timeout_milliseconds) {
    struct pollfd *descriptors;
    size_t index;
    int result;
    int timeout = timeout_milliseconds == UINT32_MAX
        ? -1
        : timeout_milliseconds > (uint32_t)INT32_MAX
            ? INT32_MAX
            : (int)timeout_milliseconds;
    if (count == 0) {
        return poll(NULL, 0, timeout);
    }
    if (count > SIZE_MAX / sizeof(*descriptors)) {
        return -1;
    }
    descriptors = (struct pollfd *)malloc(count * sizeof(*descriptors));
    if (descriptors == NULL) {
        return -1;
    }
    for (index = 0; index < count; ++index) {
        uint32_t interest_mask =
            ((struct cxnet_socket_wait *)entries[index].context)->interest_mask;
        descriptors[index].fd = (int)((struct cxnet_socket_wait *)entries[index].context)->handle;
        descriptors[index].events = 0;
        if ((interest_mask & CXNET_SOCKET_READABLE) != 0u) {
            descriptors[index].events |= POLLIN;
        }
        if ((interest_mask & CXNET_SOCKET_WRITABLE) != 0u) {
            descriptors[index].events |= POLLOUT;
        }
        descriptors[index].revents = 0;
        entries[index].ready = 0;
    }
    do {
        result = poll(descriptors, (nfds_t)count, timeout);
    } while (result < 0 && errno == EINTR);
    if (result > 0) {
        for (index = 0; index < count; ++index) {
            uint32_t interest_mask =
                ((struct cxnet_socket_wait *)entries[index].context)->interest_mask;
            struct cxnet_socket_wait *wait =
                (struct cxnet_socket_wait *)entries[index].context;
            uint32_t ready_mask = 0u;
            if ((interest_mask & CXNET_SOCKET_READABLE) != 0u &&
                (descriptors[index].revents & (POLLIN | POLLPRI)) != 0) {
                ready_mask |= CXNET_SOCKET_READABLE;
            }
            if ((interest_mask & CXNET_SOCKET_WRITABLE) != 0u &&
                (descriptors[index].revents & POLLOUT) != 0) {
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

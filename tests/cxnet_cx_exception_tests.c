#include "cxnet.internal.h"
#include <setjmp.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

extern void cxcore_System_String___constructor__2(
    struct cxcore_System_String *value, cx_uint length, cx_ptr data);

static int string_equals(
    const struct cxcore_System_String *value,
    const char *expected) {
    size_t length = strlen(expected);
    return value != NULL && value->_length == length && value->_data != NULL &&
        memcmp(value->_data, expected, length) == 0;
}

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

static int socket_error_maps_to(
    const char *domain, int native_code,
    cxnet_System_Net_Sockets_SocketError expected) {
    struct cxnet_System_Net_Sockets_SocketException *exception =
        (struct cxnet_System_Net_Sockets_SocketException *)cxcore_System_Memory_Alloc(
            cxnet_System_Net_Sockets_SocketException___typeinfo.Size);
    cxnet_System_Net_Sockets_SocketException___constructor__4(
        exception, make_string("socket fixture"), make_string(domain), native_code);
    return cxnet_System_Net_Sockets_SocketException_SocketErrorCode___const_get(
            exception) == expected &&
        cxcore_System_IO_IOException_NativeErrorCode___const_get(
            (struct cxcore_System_IO_IOException *)exception) == native_code &&
        string_equals(cxcore_System_IO_IOException_NativeErrorDomain___const_get(
            (struct cxcore_System_IO_IOException *)exception), domain);
}

int main(void) {
    struct cxnet_System_Net_Sockets_Socket socket = {0};
    struct cx_exception_frame frame;
    struct cxcore_System_Collections_Array buffer = {0};
    cx_byte bufferData[4] = {0};
    struct cxcore_System_Threading_Tasks_CancellationToken token =
        cxcore_System_Threading_Tasks_CancellationToken_None___const_get();

    cx_gc_check_thread();

    if (!socket_error_maps_to("POSIX errno", 111,
            cxnet_System_Net_Sockets_SocketError_ConnectionRefused) ||
        !socket_error_maps_to("POSIX errno", 104,
            cxnet_System_Net_Sockets_SocketError_ConnectionReset) ||
        !socket_error_maps_to("POSIX errno", 110,
            cxnet_System_Net_Sockets_SocketError_TimedOut) ||
        !socket_error_maps_to("Winsock", 10061,
            cxnet_System_Net_Sockets_SocketError_ConnectionRefused) ||
        !socket_error_maps_to("Winsock", 10054,
            cxnet_System_Net_Sockets_SocketError_ConnectionReset) ||
        !socket_error_maps_to("dns", -2,
            cxnet_System_Net_Sockets_SocketError_HostNotFound) ||
        !socket_error_maps_to("dns", -3,
            cxnet_System_Net_Sockets_SocketError_TryAgain)) {
        fputs("SocketException did not map or preserve a native error\n", stderr);
        return 9;
    }

    cx_exception_push(&frame);
    if (setjmp(frame.environment) == 0) {
        cxnet_System_Net_Sockets_Socket___constructor(
            &socket, (cx_ulong)UINT64_MAX);
        cx_exception_pop(&frame);
        return 1;
    }
    if (!cxcore_System_Exception_Matches(
            (const struct cxcore_System_Exception *)cx_exception_current(),
            cxnet_System_Net_Sockets_SocketException___typeinfo) ||
        !cxcore_System_Exception_Matches(
            (const struct cxcore_System_Exception *)cx_exception_current(),
            cxcore_System_IO_IOException___typeinfo)) {
        cx_exception_pop(&frame);
        cx_exception_clear();
        return 2;
    }
    {
        const struct cxcore_System_IO_IOException *exception =
            (const struct cxcore_System_IO_IOException *)cx_exception_current();
        const struct cxcore_System_String *domain =
            cxcore_System_IO_IOException_NativeErrorDomain___const_get(exception);
        if (cxcore_System_IO_IOException_NativeErrorCode___const_get(exception) == 0) {
            cx_exception_pop(&frame);
            cx_exception_clear();
            return 3;
        }
#ifdef _WIN32
        if (!string_equals(domain, "Winsock")) {
#else
        if (!string_equals(domain, "POSIX errno")) {
#endif
            cx_exception_pop(&frame);
            cx_exception_clear();
            return 4;
        }
    }
    cx_exception_pop(&frame);
    cx_exception_clear();

    // The failed constructor exits before it initializes Socket's mutex.
    // Initialize that field before reusing this synthetic instance below.
    cxcore_System_Threading_Mutex___constructor(&socket._lifecycleMutex);
    socket._disposed = CX_FALSE;
    cx_exception_push(&frame);
    if (setjmp(frame.environment) == 0) {
        (void)cxnet_System_Net_Sockets_Socket_ReadAsync(
            &socket, NULL, 0u, 0u);
        cx_exception_pop(&frame);
        return 5;
    }
    if (!cxcore_System_Exception_Matches(
            (const struct cxcore_System_Exception *)cx_exception_current(),
            cxcore_System_ArgumentNullException___typeinfo) ||
        !string_equals(
            cxcore_System_ArgumentException_ParamName___const_get(
                (const struct cxcore_System_ArgumentException *)cx_exception_current()),
            "buffer")) {
        cx_exception_pop(&frame);
        cx_exception_clear();
        return 6;
    }
    cx_exception_pop(&frame);
    cx_exception_clear();

    buffer._length = 4u;
    buffer._data = bufferData;
    cx_exception_push(&frame);
    if (setjmp(frame.environment) == 0) {
        (void)cxnet_System_Net_Sockets_Socket_ReadAsync__2(
            &socket, &buffer, 3u, 2u, token);
        cx_exception_pop(&frame);
        return 7;
    }
    if (!cxcore_System_Exception_Matches(
            (const struct cxcore_System_Exception *)cx_exception_current(),
            cxcore_System_ArgumentOutOfRangeException___typeinfo) ||
        !string_equals(
            cxcore_System_ArgumentException_ParamName___const_get(
                (const struct cxcore_System_ArgumentException *)cx_exception_current()),
            "count")) {
        cx_exception_pop(&frame);
        cx_exception_clear();
        return 8;
    }
    cx_exception_pop(&frame);
    cx_exception_clear();
    return 0;
}

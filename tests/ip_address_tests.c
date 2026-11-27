#include "cxnet.internal.h"
#include <stdio.h>
#include <string.h>

// The length/data String constructor is internal to cxcore's public C header,
// but this native integration test uses it to create parser inputs on the stack.
extern void cxcore_System_String___constructor__2(
    struct cxcore_System_String *value, cx_uint length, cx_ptr data);

static int string_equals(
    const struct cxcore_System_String *value,
    const char *expected) {
    size_t length = strlen(expected);
    return value != NULL && value->_length == length && value->_data != NULL &&
        memcmp(value->_data, expected, length) == 0;
}

static int round_trip(const char *input, const char *expected,
    cxnet_System_Net_AddressFamily family, cx_uint scope_id) {
    struct cxcore_System_String source = {0};
    struct cxnet_System_Net_IPAddress *address;
    struct cxcore_System_String *formatted;
    cxcore_System_String___constructor__2(
        &source, (cx_uint)strlen(input), (cx_ptr)input);
    address = cxnet_System_Net_IPAddress_Parse(&source);
    if (address == NULL) return 0;
    if (cxnet_System_Net_IPAddress_AddressFamily___const_get(address) != family ||
        cxnet_System_Net_IPAddress_ScopeId___const_get(address) != scope_id) {
        return 0;
    }
    formatted = cxnet_System_Net_IPAddress_ToString(address);
    return string_equals(formatted, expected);
}

int main(void) {
    struct cxcore_System_String bind_address_text = {0};
    struct cxnet_System_Net_IPAddress *bind_address;
    struct cxnet_System_Net_IPEndPoint *endpoint;
    struct cxnet_System_Net_IPEndPoint *connect_endpoint;
    struct cxnet_System_Net_Sockets_Socket *ipv4_socket;
    struct cxnet_System_Net_Sockets_Socket *ipv6_socket;
    struct cxnet_System_Net_Sockets_Socket *client_socket;
    struct cxnet_System_Net_Sockets_Socket *accepted_socket;
    struct cxnet_System_Net_Sockets_Socket *ipv6_listener;
    struct cxnet_System_Net_Sockets_Socket *ipv6_client;
    struct cxnet_System_Net_Sockets_Socket *ipv6_accepted;
    struct cxnet_System_Net_Sockets_Socket *dual_mode_client;
    struct cxnet_System_Net_Sockets_Socket *dual_mode_accepted;
    struct cxnet_System_Net_IPAddress *ipv6_loopback;
    struct cxnet_System_Net_IPAddress *ipv6_any;
    struct cxnet_System_Net_IPEndPoint *ipv6_endpoint;
    struct cxnet_System_Net_IPEndPoint *ipv6_connect_endpoint;
    struct cxnet_System_Net_IPEndPoint *dual_mode_connect_endpoint;
    struct cxcore_System_Collections_Array *send_buffer;
    struct cxcore_System_Collections_Array *receive_buffer;
    struct cxcore_System_Collections_Array *resolved_addresses;
    cx_gc_check_thread();
    if (!round_trip("127.0.0.1", "127.0.0.1",
            cxnet_System_Net_AddressFamily_InterNetwork, 0u)) {
        fputs("IPv4 parse/format failed\n", stderr);
        return 1;
    }
    if (!round_trip("2001:db8:0:0:0:0:2:1", "2001:db8::2:1",
            cxnet_System_Net_AddressFamily_InterNetworkV6, 0u)) {
        fputs("IPv6 zero compression failed\n", stderr);
        return 2;
    }
    if (!round_trip("::1", "::1",
            cxnet_System_Net_AddressFamily_InterNetworkV6, 0u)) {
        fputs("IPv6 loopback parse/format failed\n", stderr);
        return 3;
    }
    if (!round_trip("fe80::1%7", "fe80::1%7",
            cxnet_System_Net_AddressFamily_InterNetworkV6, 7u)) {
        fputs("IPv6 scope identifier round trip failed\n", stderr);
        return 4;
    }
    if (!round_trip("::ffff:192.0.2.1", "::ffff:192.0.2.1",
            cxnet_System_Net_AddressFamily_InterNetworkV6, 0u)) {
        fputs("IPv4-mapped IPv6 parse/format failed\n", stderr);
        return 5;
    }
    {
        struct cxcore_System_String first_text = {0};
        struct cxcore_System_String same_text = {0};
        struct cxcore_System_String other_scope_text = {0};
        struct cxnet_System_Net_IPAddress *first;
        struct cxnet_System_Net_IPAddress *same;
        struct cxnet_System_Net_IPAddress *other_scope;
        struct cxnet_System_Net_IPEndPoint *first_endpoint;
        struct cxnet_System_Net_IPEndPoint *same_endpoint;
        struct cxnet_System_Net_IPEndPoint *other_port_endpoint;
        cxcore_System_String___constructor__2(
            &first_text, 9u, (cx_ptr)"fe80::1%7");
        cxcore_System_String___constructor__2(
            &same_text, 9u, (cx_ptr)"fe80::1%7");
        cxcore_System_String___constructor__2(
            &other_scope_text, 9u, (cx_ptr)"fe80::1%8");
        first = cxnet_System_Net_IPAddress_Parse(&first_text);
        same = cxnet_System_Net_IPAddress_Parse(&same_text);
        other_scope = cxnet_System_Net_IPAddress_Parse(&other_scope_text);
        if (first == NULL || same == NULL || other_scope == NULL ||
            !cxnet_System_Net_IPAddress_Equals(first, same) ||
            cxnet_System_Net_IPAddress_Equals(first, other_scope) ||
            cxnet_System_Net_IPAddress_GetHashCode(first) !=
                cxnet_System_Net_IPAddress_GetHashCode(same)) {
            fputs("scoped IP address value equality failed\n", stderr);
            return 6;
        }
        first_endpoint = (struct cxnet_System_Net_IPEndPoint *)
            cxcore_System_Memory_Alloc(cxnet_System_Net_IPEndPoint___typeinfo.Size);
        same_endpoint = (struct cxnet_System_Net_IPEndPoint *)
            cxcore_System_Memory_Alloc(cxnet_System_Net_IPEndPoint___typeinfo.Size);
        other_port_endpoint = (struct cxnet_System_Net_IPEndPoint *)
            cxcore_System_Memory_Alloc(cxnet_System_Net_IPEndPoint___typeinfo.Size);
        cxnet_System_Net_IPEndPoint___constructor__2(first_endpoint, first, 443);
        cxnet_System_Net_IPEndPoint___constructor__2(same_endpoint, same, 443);
        cxnet_System_Net_IPEndPoint___constructor__2(
            other_port_endpoint, same, 444);
        if (!cxnet_System_Net_IPEndPoint_Equals(first_endpoint, same_endpoint) ||
            cxnet_System_Net_IPEndPoint_Equals(first_endpoint, other_port_endpoint) ||
            cxnet_System_Net_IPEndPoint_GetHashCode(first_endpoint) !=
                cxnet_System_Net_IPEndPoint_GetHashCode(same_endpoint)) {
            fputs("IP endpoint value equality failed\n", stderr);
            return 7;
        }
        cxnet_System_Net_IPEndPoint___constructor__2(
            other_port_endpoint, other_scope, 443);
        if (cxnet_System_Net_IPEndPoint_Equals(first_endpoint,
                other_port_endpoint)) {
            fputs("IP endpoint equality ignored its address scope\n", stderr);
            return 8;
        }
    }
    {
        struct cxcore_System_String numeric_host = {0};
        struct cxcore_System_String localhost_host = {0};
        cxcore_System_String___constructor__2(
            &numeric_host, 9u, (cx_ptr)"127.0.0.1");
        resolved_addresses = cxnet_System_Net_Dns_GetHostAddresses(&numeric_host);
        if (resolved_addresses == NULL || resolved_addresses->_length == 0u) {
            fputs("DNS resolver returned no numeric loopback address\n", stderr);
            return 11;
        }
        {
            uint32_t index;
            int found_loopback = 0;
            for (index = 0u; index < resolved_addresses->_length; ++index) {
                struct cxnet_System_Net_IPAddress *resolved =
                    *(struct cxnet_System_Net_IPAddress **)cx_array_at(
                        resolved_addresses, index, (cx_uint)sizeof(cx_ptr));
                struct cxcore_System_String *text =
                    cxnet_System_Net_IPAddress_ToString(resolved);
                if (string_equals(text, "127.0.0.1")) found_loopback = 1;
            }
            if (!found_loopback) {
                fputs("DNS resolver did not return the requested IPv4 address\n", stderr);
                return 12;
            }
        }
        cxcore_System_String___constructor__2(
            &localhost_host, 9u, (cx_ptr)"localhost");
        resolved_addresses = cxnet_System_Net_Dns_GetHostAddresses(&localhost_host);
        if (resolved_addresses == NULL || resolved_addresses->_length == 0u) {
            fputs("DNS resolver returned no localhost addresses\n", stderr);
            return 13;
        }
    }
    cxcore_System_String___constructor__2(
        &bind_address_text, 9u, (cx_ptr)"127.0.0.1");
    bind_address = (struct cxnet_System_Net_IPAddress *)cxcore_System_Memory_Alloc(
        cxnet_System_Net_IPAddress___typeinfo.Size);
    endpoint = (struct cxnet_System_Net_IPEndPoint *)cxcore_System_Memory_Alloc(
        cxnet_System_Net_IPEndPoint___typeinfo.Size);
    connect_endpoint = (struct cxnet_System_Net_IPEndPoint *)cxcore_System_Memory_Alloc(
        cxnet_System_Net_IPEndPoint___typeinfo.Size);
    ipv4_socket = (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
        cxnet_System_Net_Sockets_Socket___typeinfo.Size);
    ipv6_socket = (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
        cxnet_System_Net_Sockets_Socket___typeinfo.Size);
    client_socket = (struct cxnet_System_Net_Sockets_Socket *)cxcore_System_Memory_Alloc(
        cxnet_System_Net_Sockets_Socket___typeinfo.Size);
    accepted_socket = NULL;
    cxnet_System_Net_IPAddress___constructor__3(bind_address, &bind_address_text);
    cxnet_System_Net_IPEndPoint___constructor__2(
        endpoint, bind_address, 0);
    cxnet_System_Net_Sockets_Socket___constructor__2(ipv4_socket,
        cxnet_System_Net_AddressFamily_InterNetwork,
        cxnet_System_Net_Sockets_SocketType_Stream,
        cxnet_System_Net_Sockets_ProtocolType_Tcp);
    cxnet_System_Net_Sockets_Socket_ReuseAddress___set(ipv4_socket, CX_TRUE);
    if (!cxnet_System_Net_Sockets_Socket_ReuseAddress___get(ipv4_socket)) {
        fputs("IPv4 reuse-address option did not round trip\n", stderr);
        return 5;
    }
    cxnet_System_Net_Sockets_Socket_Bind(
        ipv4_socket, (struct cxnet_System_Net_EndPoint *)endpoint);
    if (!cxnet_System_Net_Sockets_Socket_IsBound___const_get(ipv4_socket)) {
        fputs("IPv4 socket did not enter the bound state\n", stderr);
        return 5;
    }
    cxnet_System_Net_Sockets_Socket_Listen__2(ipv4_socket, 8);
    if (!cxnet_System_Net_Sockets_Socket_IsListening___const_get(ipv4_socket)) {
        fputs("IPv4 socket did not enter the listening state\n", stderr);
        return 6;
    }
    {
        struct cxcore_System_Nullable local_endpoint =
            cxnet_System_Net_Sockets_Socket_LocalEndPoint___get(ipv4_socket);
        struct cxnet_System_Net_IPEndPoint *local_ip_endpoint =
            (struct cxnet_System_Net_IPEndPoint *)local_endpoint._obj;
        cx_ushort local_port;
        if (local_ip_endpoint == NULL ||
            cxnet_System_Net_IPEndPoint_AddressFamily___const_get(
                local_ip_endpoint) != cxnet_System_Net_AddressFamily_InterNetwork) {
            fputs("IPv4 ephemeral endpoint was not reported\n", stderr);
            return 7;
        }
        local_port = cxnet_System_Net_IPEndPoint_Port___const_get(local_ip_endpoint);
        if (local_port == 0u) {
            fputs("managed LocalEndPoint did not report its ephemeral port\n", stderr);
            return 8;
        }
        cxnet_System_Net_IPEndPoint___constructor__2(
            connect_endpoint, bind_address, (cx_int)local_port);
    }
    cxnet_System_Net_Sockets_Socket___constructor__2(client_socket,
        cxnet_System_Net_AddressFamily_InterNetwork,
        cxnet_System_Net_Sockets_SocketType_Stream,
        cxnet_System_Net_Sockets_ProtocolType_Tcp);
    cxnet_System_Net_Sockets_Socket_NoDelay___set(client_socket, CX_TRUE);
    cxnet_System_Net_Sockets_Socket_KeepAlive___set(client_socket, CX_TRUE);
    if (!cxnet_System_Net_Sockets_Socket_NoDelay___get(client_socket) ||
        !cxnet_System_Net_Sockets_Socket_KeepAlive___get(client_socket)) {
        fputs("TCP no-delay or keep-alive option did not round trip\n", stderr);
        return 8;
    }
    cxnet_System_Net_Sockets_Socket_Connect(
        client_socket, (struct cxnet_System_Net_EndPoint *)connect_endpoint);
    accepted_socket = (struct cxnet_System_Net_Sockets_Socket *)
        cxnet_System_Net_Sockets_Socket_Accept(ipv4_socket);
    if (accepted_socket == NULL ||
        !cxnet_System_Net_Sockets_Socket_Connected___const_get(accepted_socket)) {
        fputs("accepted TCP socket was not connected\n", stderr);
        return 8;
    }
    send_buffer = cx_array_new(8u, (cx_uint)sizeof(cx_byte));
    receive_buffer = cx_array_new(8u, (cx_uint)sizeof(cx_byte));
    memcpy(cx_array_at(send_buffer, 0u, (cx_uint)sizeof(cx_byte)), "ping", 4u);
    if (cxnet_System_Net_Sockets_Socket_Send(
            client_socket, send_buffer, 0u, 4u) != 4 ||
        cxnet_System_Net_Sockets_Socket_Receive(
            accepted_socket, receive_buffer, 0u, 4u) != 4 ||
        memcmp(cx_array_at(receive_buffer, 0u, (cx_uint)sizeof(cx_byte)),
            "ping", 4u) != 0) {
        fputs("synchronous TCP send/receive failed\n", stderr);
        return 9;
    }
    cxnet_System_Net_Sockets_Socket_Dispose(client_socket);
    cxnet_System_Net_Sockets_Socket_Dispose(accepted_socket);
    cxnet_System_Net_Sockets_Socket_Dispose(ipv4_socket);

    cxnet_System_Net_Sockets_Socket___constructor__2(ipv6_socket,
        cxnet_System_Net_AddressFamily_InterNetworkV6,
        cxnet_System_Net_Sockets_SocketType_Stream,
        cxnet_System_Net_Sockets_ProtocolType_Tcp);
    cxnet_System_Net_Sockets_Socket_DualMode___set(ipv6_socket, CX_TRUE);
    if (!cxnet_System_Net_Sockets_Socket_DualMode___get(ipv6_socket)) {
        fputs("IPv6 dual-mode option did not round trip\n", stderr);
        return 7;
    }
    cxnet_System_Net_Sockets_Socket_SendBufferSize___set(ipv6_socket, 8192);
    if (cxnet_System_Net_Sockets_Socket_SendBufferSize___get(ipv6_socket) < 8192 ||
        cxnet_System_Net_Sockets_Socket_ReceiveBufferSize___get(ipv6_socket) <= 0) {
        fputs("TCP buffer-size options did not round trip\n", stderr);
        return 10;
    }
    cxnet_System_Net_Sockets_Socket_Dispose(ipv6_socket);

    // Exercise actual IPv6 loopback traffic and IPv4-mapped connections through
    // a dual-mode listener, not only address parsing and option round trips.
    // Binding to port zero also checks endpoint queries and ephemeral-port
    // propagation into separate client sockets.
    {
        struct cxcore_System_String loopback_text = {0};
        struct cxcore_System_String any_text = {0};
        struct cxcore_System_Nullable local_endpoint;
        cx_ushort local_port;
        cxcore_System_String___constructor__2(&loopback_text, 3u,
            (cx_ptr)"::1");
        cxcore_System_String___constructor__2(&any_text, 2u,
            (cx_ptr)"::");
        ipv6_loopback = (struct cxnet_System_Net_IPAddress *)
            cxcore_System_Memory_Alloc(
                cxnet_System_Net_IPAddress___typeinfo.Size);
        cxnet_System_Net_IPAddress___constructor__3(ipv6_loopback,
            &loopback_text);
        ipv6_any = (struct cxnet_System_Net_IPAddress *)
            cxcore_System_Memory_Alloc(
                cxnet_System_Net_IPAddress___typeinfo.Size);
        cxnet_System_Net_IPAddress___constructor__3(ipv6_any, &any_text);
        ipv6_endpoint = (struct cxnet_System_Net_IPEndPoint *)
            cxcore_System_Memory_Alloc(
                cxnet_System_Net_IPEndPoint___typeinfo.Size);
        ipv6_connect_endpoint = (struct cxnet_System_Net_IPEndPoint *)
            cxcore_System_Memory_Alloc(
                cxnet_System_Net_IPEndPoint___typeinfo.Size);
        dual_mode_connect_endpoint = (struct cxnet_System_Net_IPEndPoint *)
            cxcore_System_Memory_Alloc(
                cxnet_System_Net_IPEndPoint___typeinfo.Size);
        ipv6_listener = (struct cxnet_System_Net_Sockets_Socket *)
            cxcore_System_Memory_Alloc(
                cxnet_System_Net_Sockets_Socket___typeinfo.Size);
        ipv6_client = (struct cxnet_System_Net_Sockets_Socket *)
            cxcore_System_Memory_Alloc(
                cxnet_System_Net_Sockets_Socket___typeinfo.Size);
        cxnet_System_Net_IPEndPoint___constructor__2(ipv6_endpoint,
            ipv6_any, 0);
        cxnet_System_Net_Sockets_Socket___constructor__2(ipv6_listener,
            cxnet_System_Net_AddressFamily_InterNetworkV6,
            cxnet_System_Net_Sockets_SocketType_Stream,
            cxnet_System_Net_Sockets_ProtocolType_Tcp);
        cxnet_System_Net_Sockets_Socket_DualMode___set(ipv6_listener, CX_TRUE);
        if (!cxnet_System_Net_Sockets_Socket_DualMode___get(ipv6_listener)) {
            fputs("IPv6 loopback listener did not enable dual mode\n", stderr);
            return 14;
        }
        cxnet_System_Net_Sockets_Socket_Bind(ipv6_listener,
            (struct cxnet_System_Net_EndPoint *)ipv6_endpoint);
        cxnet_System_Net_Sockets_Socket_Listen__2(ipv6_listener, 4);
        local_endpoint = cxnet_System_Net_Sockets_Socket_LocalEndPoint___get(
            ipv6_listener);
        if (local_endpoint._obj == NULL ||
            cxnet_System_Net_IPEndPoint_AddressFamily___const_get(
                (struct cxnet_System_Net_IPEndPoint *)local_endpoint._obj) !=
                cxnet_System_Net_AddressFamily_InterNetworkV6 ||
            (local_port = cxnet_System_Net_IPEndPoint_Port___const_get(
                (struct cxnet_System_Net_IPEndPoint *)local_endpoint._obj)) == 0u) {
            fputs("IPv6 loopback listener did not report its ephemeral endpoint\n",
                stderr);
            return 14;
        }
        cxnet_System_Net_IPEndPoint___constructor__2(ipv6_connect_endpoint,
            ipv6_loopback, (cx_int)local_port);
        cxnet_System_Net_Sockets_Socket___constructor__2(ipv6_client,
            cxnet_System_Net_AddressFamily_InterNetworkV6,
            cxnet_System_Net_Sockets_SocketType_Stream,
            cxnet_System_Net_Sockets_ProtocolType_Tcp);
        cxnet_System_Net_Sockets_Socket_Connect(ipv6_client,
            (struct cxnet_System_Net_EndPoint *)ipv6_connect_endpoint);
        ipv6_accepted = (struct cxnet_System_Net_Sockets_Socket *)
            cxnet_System_Net_Sockets_Socket_Accept(ipv6_listener);
        if (ipv6_accepted == NULL ||
            !cxnet_System_Net_Sockets_Socket_Connected___const_get(ipv6_client) ||
            !cxnet_System_Net_Sockets_Socket_Connected___const_get(ipv6_accepted) ||
            cxnet_System_Net_Sockets_Socket_Send(ipv6_client, send_buffer,
                0u, 4u) != 4 ||
            cxnet_System_Net_Sockets_Socket_Receive(ipv6_accepted,
                receive_buffer, 0u, 4u) != 4 ||
            memcmp(cx_array_at(receive_buffer, 0u,
                    (cx_uint)sizeof(cx_byte)), "ping", 4u) != 0) {
            fputs("IPv6 loopback TCP transfer failed\n", stderr);
            return 15;
        }
        {
            struct cx_generic_da8dc51a57f40bf840db285419f707191ac7e96fb939fa682bcb39a0a2207651 *read_task;
            memcpy(cx_array_at(send_buffer, 0u, (cx_uint)sizeof(cx_byte)),
                "short", 5u);
            read_task = cxnet_System_Net_Sockets_Socket_ReadAsync(
                ipv6_accepted, receive_buffer, 0u, 8u);
            if (read_task == NULL || read_task->_isCompleted ||
                cxnet_System_Net_Sockets_Socket_Send(ipv6_client,
                    send_buffer, 0u, 5u) != 5) {
                fputs("public Socket.ReadAsync did not remain pending for a short transfer\n",
                    stderr);
                return 16;
            }
            cxcore_System_Threading_Tasks_TaskScheduler_RunUntil(
                (struct cxcore_System_Object *)read_task);
            if (!read_task->_isCompleted || read_task->_isFaulted ||
                read_task->_isCanceled ||
                cx_generic_32f22edd4c9ba40c64ed2658909ec8fb1d6e85d2c4276d46eebd01030eefa80f(
                    read_task) != 5u ||
                memcmp(cx_array_at(receive_buffer, 0u,
                        (cx_uint)sizeof(cx_byte)), "short", 5u) != 0) {
                fputs("public Socket.ReadAsync did not return the short byte count and payload\n",
                    stderr);
                return 16;
            }
        }
        cxnet_System_Net_Sockets_Socket_Shutdown(ipv6_client,
            cxnet_System_Net_Sockets_SocketShutdown_Send);
        if (!cxnet_System_Net_Sockets_Socket_Connected___const_get(ipv6_client) ||
            !cxnet_System_Net_Sockets_Socket_Connected___const_get(ipv6_accepted) ||
            cxnet_System_Net_Sockets_Socket_Receive(ipv6_accepted,
                receive_buffer, 0u, 4u) != 0) {
            fputs("TCP send half-close did not deliver EOF while retaining the connection\n",
                stderr);
            return 17;
        }
        memcpy(cx_array_at(send_buffer, 0u, (cx_uint)sizeof(cx_byte)),
            "pong", 4u);
        if (cxnet_System_Net_Sockets_Socket_Send(ipv6_accepted, send_buffer,
                0u, 4u) != 4 ||
            cxnet_System_Net_Sockets_Socket_Receive(ipv6_client,
                receive_buffer, 0u, 4u) != 4 ||
            memcmp(cx_array_at(receive_buffer, 0u,
                    (cx_uint)sizeof(cx_byte)), "pong", 4u) != 0) {
            fputs("TCP reverse transfer failed after send half-close\n", stderr);
            return 17;
        }
        memcpy(cx_array_at(send_buffer, 0u, (cx_uint)sizeof(cx_byte)),
            "ping", 4u);
        cxnet_System_Net_Sockets_Socket_Dispose(ipv6_client);
        cxnet_System_Net_Sockets_Socket_Dispose(ipv6_accepted);

        // The same wildcard listener must also accept IPv4 through the
        // platform's IPv4-mapped IPv6 path when DualMode is enabled.
        cxnet_System_Net_IPEndPoint___constructor__2(
            dual_mode_connect_endpoint, bind_address, (cx_int)local_port);
        dual_mode_client = (struct cxnet_System_Net_Sockets_Socket *)
            cxcore_System_Memory_Alloc(
                cxnet_System_Net_Sockets_Socket___typeinfo.Size);
        cxnet_System_Net_Sockets_Socket___constructor__2(dual_mode_client,
            cxnet_System_Net_AddressFamily_InterNetwork,
            cxnet_System_Net_Sockets_SocketType_Stream,
            cxnet_System_Net_Sockets_ProtocolType_Tcp);
        cxnet_System_Net_Sockets_Socket_Connect(dual_mode_client,
            (struct cxnet_System_Net_EndPoint *)dual_mode_connect_endpoint);
        dual_mode_accepted = (struct cxnet_System_Net_Sockets_Socket *)
            cxnet_System_Net_Sockets_Socket_Accept(ipv6_listener);
        if (dual_mode_accepted == NULL ||
            !cxnet_System_Net_Sockets_Socket_Connected___const_get(
                dual_mode_client) ||
            !cxnet_System_Net_Sockets_Socket_Connected___const_get(
                dual_mode_accepted) ||
            cxnet_System_Net_Sockets_Socket_Send(dual_mode_client,
                send_buffer, 0u, 4u) != 4 ||
            cxnet_System_Net_Sockets_Socket_Receive(dual_mode_accepted,
                receive_buffer, 0u, 4u) != 4 ||
            memcmp(cx_array_at(receive_buffer, 0u,
                    (cx_uint)sizeof(cx_byte)), "ping", 4u) != 0) {
            fputs("IPv4 connection through the dual-mode listener failed\n",
                stderr);
            return 18;
        }
        cxnet_System_Net_Sockets_Socket_Dispose(dual_mode_client);
        cxnet_System_Net_Sockets_Socket_Dispose(dual_mode_accepted);
        cxnet_System_Net_Sockets_Socket_Dispose(ipv6_listener);
    }
    return 0;
}

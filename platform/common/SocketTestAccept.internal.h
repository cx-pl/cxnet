#ifndef __CXNET_SOCKET_TEST_ACCEPT_INTERNAL_H__
#define __CXNET_SOCKET_TEST_ACCEPT_INTERNAL_H__

#ifdef CXNET_TESTING
#if defined(_WIN32) && defined(CX_CXNET_BUILD)
#define CXNET_TEST_ACCEPT_API __declspec(dllexport)
#elif defined(_WIN32) && !defined(CX_STATIC_LINK)
#define CXNET_TEST_ACCEPT_API __declspec(dllimport)
#else
#define CXNET_TEST_ACCEPT_API
#endif

CXNET_TEST_ACCEPT_API void cxnet_socket_test_accept_after_native_hook_set(
    void (*hook)(void));
#endif

#endif

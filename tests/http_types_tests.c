#include "cxnet.internal.h"
#include <stdio.h>
#include <string.h>

extern void cxcore_System_String___constructor__2(
    struct cxcore_System_String *value, cx_uint length, cx_ptr data);

static int string_equals(const struct cxcore_System_String *value,
    const char *expected) {
    size_t length = strlen(expected);
    return value != NULL && value->_length == length &&
        (length == 0 || memcmp(value->_data, expected, length) == 0);
}

static int string_contains(const struct cxcore_System_String *value,
    const char *expected) {
    size_t length = strlen(expected);
    size_t index;
    if (value == NULL || value->_length < length) return 0;
    for (index = 0; index + length <= value->_length; index++) {
        if (memcmp((const char *)value->_data + index, expected, length) == 0) return 1;
    }
    return 0;
}

static struct cxcore_System_String *make_string(const char *text) {
    struct cxcore_System_String *value =
        (struct cxcore_System_String *)cxcore_System_Memory_Alloc(
            cxcore_System_String___typeinfo.Size);
    cxcore_System_String___constructor__2(
        value, (cx_uint)strlen(text), (cx_ptr)text);
    return value;
}

int main(void) {
    struct cxnet_System_Net_Http_HttpHeaders *headers;
    struct cxnet_System_Net_Http_HttpResponseMessage *response;
    struct cxnet_System_Net_Http_HttpRequestMessage *request;
    struct cxnet_System_Net_Http_HttpMethod *get;
    struct cxnet_System_Net_Http_StringContent *stringContent;
    struct cxnet_System_Net_Http_StreamContent *ownedStreamContent;
    struct cxnet_System_Net_Http_StreamContent *borrowedStreamContent;
    struct cxnet_System_Net_Http_MultipartFormDataContent *multipart;
    struct cxcore_System_IO_MemoryStream *ownedStream;
    struct cxcore_System_IO_MemoryStream *borrowedStream;
    struct cxcore_System_IO_MemoryStream *requestOwnedStream;
    struct cxnet_System_Net_Http_StreamContent *requestOwnedContent;
    struct cxcore_System_Uri *requestUri;
    struct cxcore_System_String *name;
    struct cxcore_System_String *value;
    struct cxcore_System_String *readContent;

    cx_gc_check_thread();
    headers = (struct cxnet_System_Net_Http_HttpHeaders *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Http_HttpHeaders___typeinfo.Size);
    cxnet_System_Net_Http_HttpHeaders___constructor(headers);
    name = make_string("X-Trace-Id");
    value = make_string("safe-value");
    cxnet_System_Net_Http_HttpHeaders_Add(headers, name, value);
    if (!cxnet_System_Net_Http_HttpHeaders_Contains(headers, make_string("x-trace-id")) ||
        !string_equals(cxnet_System_Net_Http_HttpHeaders_GetFirstValue(
            headers, make_string("X-TRACE-ID")), "safe-value")) {
        fputs("case-insensitive HTTP header lookup failed\n", stderr);
        return 1;
    }
    if (cxnet_System_Net_Http_HttpHeaders_TryAddWithoutValidation(
            headers, make_string("X-Bad"), make_string("one\r\nInjected: yes"))) {
        fputs("HTTP header line-break injection was accepted\n", stderr);
        return 1;
    }
    if (cxnet_System_Net_Http_HttpHeaders_TryAddWithoutValidation(
            headers, make_string("Bad Header"), make_string("value")) ||
        cxnet_System_Net_Http_HttpHeaders_TryAddWithoutValidation(
            headers, make_string("X-Bad\nInjected"), make_string("value")) ||
        cxnet_System_Net_Http_HttpHeaders_TryAddWithoutValidation(
            headers, make_string("X-Bad"), make_string("control\001value")) ||
        cxnet_System_Net_Http_HttpHeaders_TryAddWithoutValidation(
            headers, make_string("X-Bad"), make_string("delete\177value"))) {
        fputs("invalid HTTP header syntax or control byte was accepted\n", stderr);
        return 1;
    }
    get = cxnet_System_Net_Http_HttpMethod_Get___const_get();
    if (!string_equals(cxnet_System_Net_Http_HttpMethod_Method___const_get(get), "GET")) {
        fputs("standard HTTP method construction failed\n", stderr);
        return 1;
    }
    stringContent = (struct cxnet_System_Net_Http_StringContent *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Http_StringContent___typeinfo.Size);
    cxnet_System_Net_Http_StringContent___constructor(stringContent, make_string("hello content"));
    readContent = cxnet_System_Net_Http_HttpContent_ReadAsString(
        (struct cxnet_System_Net_Http_HttpContent *)stringContent);
    if (!string_equals(readContent, "hello content")) {
        fputs("StringContent UTF-8 round trip failed\n", stderr);
        return 1;
    }
    multipart = (struct cxnet_System_Net_Http_MultipartFormDataContent *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Http_MultipartFormDataContent___typeinfo.Size);
    cxnet_System_Net_Http_MultipartFormDataContent___constructor(
        multipart, make_string("test-boundary"));
    cxnet_System_Net_Http_MultipartFormDataContent_Add(
        multipart, (struct cxnet_System_Net_Http_HttpContent *)stringContent,
        make_string("field"));
    readContent = cxnet_System_Net_Http_HttpContent_ReadAsString(
        (struct cxnet_System_Net_Http_HttpContent *)multipart);
    if (readContent == NULL || readContent->_length < 20u ||
        !string_contains(readContent, "hello content")) {
        fputs("multipart buffered body did not contain its part\n", stderr);
        return 1;
    }
    ownedStream = (struct cxcore_System_IO_MemoryStream *)
        cxcore_System_Memory_Alloc(cxcore_System_IO_MemoryStream___typeinfo.Size);
    borrowedStream = (struct cxcore_System_IO_MemoryStream *)
        cxcore_System_Memory_Alloc(cxcore_System_IO_MemoryStream___typeinfo.Size);
    ownedStreamContent = (struct cxnet_System_Net_Http_StreamContent *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Http_StreamContent___typeinfo.Size);
    borrowedStreamContent = (struct cxnet_System_Net_Http_StreamContent *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Http_StreamContent___typeinfo.Size);
    cxcore_System_IO_MemoryStream___constructor(ownedStream);
    cxcore_System_IO_MemoryStream___constructor(borrowedStream);
    cxnet_System_Net_Http_StreamContent___constructor__2(ownedStreamContent,
        (struct cxcore_System_IO_Stream *)ownedStream, 1024u, CX_FALSE);
    cxnet_System_Net_Http_StreamContent___constructor__2(borrowedStreamContent,
        (struct cxcore_System_IO_Stream *)borrowedStream, 1024u, CX_TRUE);
    cxnet_System_Net_Http_StreamContent_Dispose(ownedStreamContent);
    cxnet_System_Net_Http_StreamContent_Dispose(borrowedStreamContent);
    if (cxcore_System_IO_MemoryStream_CanRead___get(ownedStream) != CX_FALSE ||
        cxcore_System_IO_MemoryStream_CanRead___get(borrowedStream) != CX_TRUE) {
        fputs("StreamContent did not respect owned/borrowed stream disposal\n", stderr);
        return 1;
    }
    cxcore_System_IO_MemoryStream_Dispose(borrowedStream);

    requestOwnedStream = (struct cxcore_System_IO_MemoryStream *)
        cxcore_System_Memory_Alloc(cxcore_System_IO_MemoryStream___typeinfo.Size);
    requestOwnedContent = (struct cxnet_System_Net_Http_StreamContent *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Http_StreamContent___typeinfo.Size);
    requestUri = (struct cxcore_System_Uri *)
        cxcore_System_Memory_Alloc(cxcore_System_Uri___typeinfo.Size);
    request = (struct cxnet_System_Net_Http_HttpRequestMessage *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Http_HttpRequestMessage___typeinfo.Size);
    cxcore_System_IO_MemoryStream___constructor(requestOwnedStream);
    cxnet_System_Net_Http_StreamContent___constructor__2(requestOwnedContent,
        (struct cxcore_System_IO_Stream *)requestOwnedStream, 1024u, CX_FALSE);
    cxcore_System_Uri___constructor(requestUri, make_string("http://example.test/"));
    cxnet_System_Net_Http_HttpRequestMessage___constructor(request, get, requestUri);
    cxnet_System_Net_Http_HttpRequestMessage_Content___set(request,
        (struct cxnet_System_Net_Http_HttpContent *)requestOwnedContent);
    cxnet_System_Net_Http_HttpRequestMessage_Dispose(request);
    if (cxcore_System_IO_MemoryStream_CanRead___get(requestOwnedStream) != CX_FALSE) {
        fputs("HttpRequestMessage.Dispose did not dispose its content stream\n", stderr);
        return 1;
    }

    response = (struct cxnet_System_Net_Http_HttpResponseMessage *)
        cxcore_System_Memory_Alloc(cxnet_System_Net_Http_HttpResponseMessage___typeinfo.Size);
    cxnet_System_Net_Http_HttpResponseMessage___constructor__2(
        response, cxnet_System_Net_Http_HttpStatusCode_Created);
    if (!cxnet_System_Net_Http_HttpResponseMessage_IsSuccessStatusCode___get(response)) {
        fputs("2xx response status was not marked successful\n", stderr);
        return 1;
    }
    cxnet_System_Net_Http_HttpResponseMessage_StatusCode___set(
        response, cxnet_System_Net_Http_HttpStatusCode_NotFound);
    if (cxnet_System_Net_Http_HttpResponseMessage_IsSuccessStatusCode___get(response)) {
        fputs("4xx response status was marked successful\n", stderr);
        return 1;
    }
    puts("PASS HTTP method, message status, safe headers, and stream ownership");
    return 0;
}

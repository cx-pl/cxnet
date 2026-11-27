#!/usr/bin/env python3
"""Run one generated REST contract against a local HTTP/1.1 endpoint."""

from __future__ import annotations

import argparse
import subprocess
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


EXPECTED_TARGET = "/wire/a%20b?q=x%20%26%20y"
EXPECTED_ROUTE_FRAGMENT = "/wire/fragment?q=x%20%26%20y"
EXPECTED_REPEATED_QUERY = "/wire/search?tag=first%20item&tag=x%26y"
EXPECTED_INTEGER_TAGS = "/wire/int-tags?value=1&value=42&value=-3"
EXPECTED_ULONG_TAGS = (
    "/wire/ulong-tags?value=0&value=18446744073709551615&value=42"
)
EXPECTED_NULLABLE_ULONG_TAGS = (
    "/wire/nullable-ulong-tags?value=0&value=18446744073709551615&value=42"
)
EXPECTED_ARRAY_TAGS = "/wire/array-tags?tag=first%20item&tag=x%26y"
EXPECTED_SCALAR_QUERY = (
    "/wire/scalar-query?enabled=True&ratio=-1.25&amount=0.125"
)
EXPECTED_CHAR_VALUES = (
    "/wire/char/%C3%A9?fallback=%21&allowed=A&allowed=%C3%A9"
)
EXPECTED_CONST_LIST_TAGS = (
    "/wire/const-list-tags?tag=first%20item&tag=x%26y"
)
EXPECTED_QUERY_MAP = (
    "/wire/query-map?tag%20name=first%20item&tag%20name=x%26y"
    "&mode=fast%20mode"
)
EXPECTED_NUMERIC_QUERY_MAP = (
    "/wire/query-map-numbers?page%20number=42&retry%20count=3"
)
EXPECTED_INT_COLLECTION_QUERY_MAP = (
    "/wire/query-map-int-collections?value%20set=7&value%20set=-3"
    "&value%20set=42&retry=-5"
)
EXPECTED_NULLABLE_SEQUENCES = (
    "/wire/nullable-sequences?tag=a%20b&tag=&tag=x%26y"
)
EXPECTED_NULLABLE_NUMERIC_SEQUENCES = (
    "/wire/nullable-numeric-sequences?value=7&value=-3"
)
EXPECTED_NULLABLE_NUMERIC_COLLECTIONS = (
    "/wire/nullable-numeric-collections?" +
    "value=7&value=-3&value=7&value=-3&value=7&value=-3&" +
    "value=7&value=-3&value=7&value=-3&value=7&value=-3&" +
    "value=7&value=-3&active=True&active=False"
)
EXPECTED_NULLABLE_QUERY_MAP = (
    "/wire/nullable-query-map?present=a%20b&empty=&tail=x%26y"
)
EXPECTED_NULLABLE_QUERY_MAP_COLLECTIONS = (
    "/wire/nullable-query-map-collections?tag%20name=first%20item"
    "&tag%20name=&tag%20name=x%26y"
)
EXPECTED_CONST_QUERY_MAP = (
    "/wire/const-query-map?tag%20name=first%20item&tag%20name=x%26y"
    "&mode=fast%20mode"
)
EXPECTED_LINKED_TAGS = "/wire/linked-tags?tag=first%20item&tag=x%26y"
EXPECTED_CONST_LINKED_TAGS = (
    "/wire/const-linked-tags?tag=first%20item&tag=x%26y"
)
EXPECTED_LINKED_MAP = (
    "/wire/linked-map?tag%20name=first%20item&tag%20name=x%26y"
)
EXPECTED_ARRAY_MAP = (
    "/wire/array-map?tag%20name=first%20item&tag%20name=x%26y"
)
EXPECTED_QUEUE_TAGS = "/wire/queue-tags?tag=first%20item&tag=x%26y"
EXPECTED_STACK_TAGS = "/wire/stack-tags?tag=x%26y&tag=first%20item"
EXPECTED_QUEUE_MAP = "/wire/queue-map?tag%20name=first%20item&tag%20name=x%26y"
EXPECTED_STACK_MAP = "/wire/stack-map?tag%20name=x%26y&tag%20name=first%20item"
STREAM_RELEASED = threading.Event()
REST_CONCURRENT_SLOW_STARTED = threading.Event()
REST_CONCURRENT_FAST_STARTED = threading.Event()
REST_CANCEL_STREAM_CLOSED = threading.Event()
REST_DISPOSE_STREAM_CLOSED = threading.Event()
STREAM_RESPONSE_BYTES = 32 * 1024


def multipart_labels_body(labels: list[str]) -> bytes:
    parts = []
    for label in labels:
        parts.append(
            b'--cxnet-boundary\r\n'
            b'Content-Disposition: form-data; name="label"\r\n'
            b'Content-Type: text/plain; charset=utf-8\r\n\r\n'
            + label.encode("utf-8")
            + b'\r\n'
        )
    parts.append(b'--cxnet-boundary--\r\n')
    return b"".join(parts)


def multipart_text_parts_body(parts: list[tuple[str, str]]) -> bytes:
    encoded_parts = []
    for name, value in parts:
        encoded_parts.append(
            b'--cxnet-boundary\r\n'
            + f'Content-Disposition: form-data; name="{name}"\r\n'.encode("ascii")
            + b'Content-Type: text/plain; charset=utf-8\r\n\r\n'
            + value.encode("utf-8")
            + b'\r\n'
        )
    encoded_parts.append(b'--cxnet-boundary--\r\n')
    return b"".join(encoded_parts)


class QuietThreadingHTTPServer(ThreadingHTTPServer):
    def handle_error(self, _request, _client_address):
        pass


def has_generated_headers(headers) -> bool:
    return (
        headers.get("Authorization") == "Bearer wire-token"
        and headers.get("X-Wire-Static") == "class"
    )


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def read_request_body(self) -> bytes:
        if self.headers.get("Transfer-Encoding", "").lower() == "chunked":
            body = bytearray()
            while True:
                size_line = self.rfile.readline().split(b";", 1)[0].strip()
                size = int(size_line, 16)
                if size == 0:
                    while self.rfile.readline() not in (b"\r\n", b"\n", b""):
                        pass
                    return bytes(body)
                body.extend(self.rfile.read(size))
                if self.rfile.read(2) != b"\r\n":
                    raise ValueError("malformed chunk terminator")
        content_length = int(self.headers.get("Content-Length", "0"))
        return self.rfile.read(content_length)

    def do_GET(self) -> None:  # noqa: N802
        generic_contract_routes = {
            "/generic-contract/wire%20generic%20dto": (
                200, "generic-dto", b'{"Name":"wire generic dto","Quantity":37}'),
            "/generic-contract-response/wrapped%20wire%20dto": (
                202, "generic-dto-response",
                b'{"Name":"wrapped wire dto","Quantity":41}'),
        }
        if self.path in generic_contract_routes:
            status, response_header, body = generic_contract_routes[self.path]
            self.send_response(status)
            self.send_header("X-CX-Wire", response_header)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        scalar_collection_routes = {
            "/wire/json-int-array-scalar": (200, "sequence", b"[3,1,4]"),
            "/wire/json-int-array-scalar-response": (202, "array", b"[3,1,4]"),
            "/wire/json-string-array-scalar": (
                200, "sequence", b'["wire A & B","a\\\"b"]'),
            "/wire/json-string-array-scalar-response": (
                201, "string-array", b'["wrapped","wire\\narray"]'),
            "/wire/json-string-list-scalar": (
                200, "sequence", b'["wire A & B","a\\\"b"]'),
            "/wire/json-string-list-scalar-response": (
                202, "string-list", b'["wrapped","wire\\nlist"]'),
            "/wire/json-string-linked-list": (
                200, "sequence", b'["wire A & B","a\\\"b"]'),
            "/wire/json-string-linked-list-response": (
                201, "linked-string-list", b'["wrapped","wire\\nlinked"]'),
            "/wire/json-string-queue": (
                200, "sequence", b'["wire A & B","a\\\"b"]'),
            "/wire/json-string-queue-response": (
                202, "queue-string", b'["wrapped","wire\\nqueue"]'),
            "/wire/json-string-stack": (
                200, "sequence", b'["wire A & B","a\\\"b"]'),
            "/wire/json-string-stack-response": (
                202, "stack-string", b'["wrapped","wire\\nstack"]'),
            "/wire/json-int-list": (200, "sequence", b"[3,1,4]"),
            "/wire/json-int-list-response": (202, "list", b"[3,1,4]"),
            "/wire/json-int-linked-list": (200, "sequence", b"[3,1,4]"),
            "/wire/json-int-linked-list-response": (201, "linked-list", b"[3,1,4]"),
            "/wire/json-int-queue": (200, "sequence", b"[3,1,4]"),
            "/wire/json-int-queue-response": (202, "queue", b"[3,1,4]"),
            "/wire/json-int-stack": (200, "sequence", b"[3,1,4]"),
            "/wire/json-int-stack-response": (202, "stack", b"[3,1,4]"),
        }
        if self.path in scalar_collection_routes and has_generated_headers(self.headers):
            status, response_header, body = scalar_collection_routes[self.path]
            self.send_response(status)
            self.send_header("X-CX-Wire", response_header)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path == "/wire/json-array" and
                has_generated_headers(self.headers)):
            body = (b'[{"Name":"wire-first","Quantity":4},'
                    b'{"Name":"wire-second","Quantity":9}]')
            self.send_response(200)
            self.send_header("X-CX-Wire", "generated")
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path == "/wire/json-array-response" and
                has_generated_headers(self.headers)):
            body = b'[{"Name":"wire-wrapped","Quantity":11}]'
            self.send_response(201)
            self.send_header("X-CX-Wire", "generated")
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path == "/wire/json-null-collection" and
                has_generated_headers(self.headers)):
            body = b"null"
            self.send_response(200)
            self.send_header("X-CX-Wire", "json-null-collection")
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path == "/wire/json-array-nullable" and
                has_generated_headers(self.headers)):
            body = (b'[{"Name":"wire-nullable-first","Quantity":31},null,'
                    b'{"Name":"wire-nullable-last","Quantity":33}]')
            self.send_response(200)
            self.send_header("X-CX-Wire", "nullable-array")
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path == "/wire/json-array-nullable-response" and
                has_generated_headers(self.headers)):
            body = (b'[{"Name":"wire-nullable-first","Quantity":31},null,'
                    b'{"Name":"wire-nullable-last","Quantity":33}]')
            self.send_response(202)
            self.send_header("X-CX-Wire", "nullable-array-response")
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path == "/wire/json-string-array-nullable" and
                has_generated_headers(self.headers)):
            body = b'["wire A & B",null,"wire\\nquote\\\""]'
            self.send_response(200)
            self.send_header("X-CX-Wire", "nullable-string-array")
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path == "/wire/json-string-array-nullable-response" and
                has_generated_headers(self.headers)):
            body = b'["wire wrapped",null,"wire\\narray"]'
            self.send_response(202)
            self.send_header("X-CX-Wire", "nullable-string-array-response")
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path == "/wire/json-int-array-nullable" and
                has_generated_headers(self.headers)):
            body = b'[31,null,-33]'
            self.send_response(200)
            self.send_header("X-CX-Wire", "nullable-int-array")
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path == "/wire/json-int-array-nullable-response" and
                has_generated_headers(self.headers)):
            body = b'[31,null,-33]'
            self.send_response(202)
            self.send_header("X-CX-Wire", "nullable-int-array-response")
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path == "/wire/json-bool-array-nullable" and
                has_generated_headers(self.headers)):
            body = b'[true,null,false]'
            self.send_response(200)
            self.send_header("X-CX-Wire", "nullable-bool-array")
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path == "/wire/json-bool-array-nullable-response" and
                has_generated_headers(self.headers)):
            body = b'[false,null,true]'
            self.send_response(202)
            self.send_header("X-CX-Wire", "nullable-bool-array-response")
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path == "/wire/json-decimal-array-nullable" and
                has_generated_headers(self.headers)):
            body = b'[12.5,null,-0.125]'
            self.send_response(200)
            self.send_header("X-CX-Wire", "nullable-decimal-array")
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path == "/wire/json-decimal-array-nullable-response" and
                has_generated_headers(self.headers)):
            body = b'[1.25e2,null,-1.25e-4]'
            self.send_response(202)
            self.send_header("X-CX-Wire", "nullable-decimal-array-response")
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path == "/wire/json-float-array-nullable" and
                has_generated_headers(self.headers)):
            body = b'[1.5,null,-0.25]'
            self.send_response(200)
            self.send_header("X-CX-Wire", "nullable-float-array")
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path == "/wire/json-float-array-nullable-response" and
                has_generated_headers(self.headers)):
            body = b'[1.25e2,null,-2.5e-1]'
            self.send_response(202)
            self.send_header("X-CX-Wire", "nullable-float-array-response")
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path == "/wire/json-double-array-nullable" and
                has_generated_headers(self.headers)):
            body = b'[1.5,null,-0.25]'
            self.send_response(200)
            self.send_header("X-CX-Wire", "nullable-double-array")
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path == "/wire/json-double-array-nullable-response" and
                has_generated_headers(self.headers)):
            body = b'[1.25e2,null,-2.5e-1]'
            self.send_response(202)
            self.send_header("X-CX-Wire", "nullable-double-array-response")
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path == "/wire/json-array-value" and
                has_generated_headers(self.headers)):
            body = (b'[{"Name":"wire-array-first","Quantity":14},'
                    b'{"Name":"wire-array-second","Quantity":18}]')
            self.send_response(200)
            self.send_header("X-CX-Wire", "generated")
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path == "/wire/json-array-value-response" and
                has_generated_headers(self.headers)):
            body = b'[{"Name":"wire-array-wrapped","Quantity":21}]'
            self.send_response(202)
            self.send_header("X-CX-Wire", "array")
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path == "/wire/json-object" and
                has_generated_headers(self.headers)):
            body = (b'{"alpha":{"Name":"wire-map-alpha","Quantity":25},'
                    b'"beta":{"Name":"wire-map-beta","Quantity":26}}')
            self.send_response(200)
            self.send_header("X-CX-Wire", "object")
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path == "/wire/json-object-response" and
                has_generated_headers(self.headers)):
            body = b'{"wrapped":{"Name":"wire-map-wrapped","Quantity":27}}'
            self.send_response(202)
            self.send_header("X-CX-Wire", "object")
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path == "/wire/json-object-nullable-response" and
                has_generated_headers(self.headers)):
            body = (b'{"present":{"Name":"wire-nullable-map",'
                    b'"Quantity":37},"missing":null}')
            self.send_response(202)
            self.send_header("X-CX-Wire", "nullable-object")
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path == "/wire/json-object" and
                has_generated_headers(self.headers)):
            body = (b'{"alpha":{"Name":"wire-map-alpha","Quantity":25},'
                    b'"beta":{"Name":"wire-map-beta","Quantity":26}}')
            self.send_response(200)
            self.send_header("X-CX-Wire", "object")
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path == "/wire/json-object-response" and
                has_generated_headers(self.headers)):
            body = b'{"wrapped":{"Name":"wire-map-wrapped","Quantity":27}}'
            self.send_response(202)
            self.send_header("X-CX-Wire", "object")
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path == "/wire/json-string-map" and
                has_generated_headers(self.headers)):
            body = b'{"label":"wire value","quote":"wire \\"quoted\\""}'
            self.send_response(200)
            self.send_header("X-CX-Wire", "scalar-map")
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path == "/wire/json-string-map-nullable" and
                has_generated_headers(self.headers)):
            body = b'{"present":"wire value","missing":null,"empty":""}'
            self.send_response(200)
            self.send_header("X-CX-Wire", "nullable-scalar-map")
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path == "/wire/json-string-map-nullable-response" and
                has_generated_headers(self.headers)):
            body = (b'{"present":"wire wrapped value",'
                    b'"missing":null,"empty":""}')
            self.send_response(202)
            self.send_header("X-CX-Wire", "nullable-scalar-map-response")
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path == "/wire/json-int-map-response" and
                has_generated_headers(self.headers)):
            body = b'{"positive":32,"negative":-8}'
            self.send_response(202)
            self.send_header("X-CX-Wire", "scalar-map")
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path == "/wire/json-map-list" and
                has_generated_headers(self.headers)):
            body = b'{"first":[3,1,4],"second":[5,9]}'
            self.send_response(200)
            self.send_header("X-CX-Wire", "collection-map")
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path == "/wire/json-map-list-response" and
                has_generated_headers(self.headers)):
            body = b'{"wrapped":[2,7,1],"second":[8,6]}'
            self.send_response(202)
            self.send_header("X-CX-Wire", "collection-map")
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path == "/wire/json-map-nullable-int-list" and
                has_generated_headers(self.headers)):
            body = b'{"items":[3,null,-4],"empty":[],"missing":null}'
            self.send_response(200)
            self.send_header("X-CX-Wire", "nullable-int-collection-map")
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path == "/wire/json-map-nullable-int-list-response" and
                has_generated_headers(self.headers)):
            body = b'{"items":[3,null,-4],"empty":[],"missing":null}'
            self.send_response(202)
            self.send_header("X-CX-Wire", "nullable-int-collection-map")
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path == "/wire/json-map-nullable-dto-list" and
                has_generated_headers(self.headers)):
            body = (b'{"items":[{"Name":"wire-nullable-first",'
                    b'"Quantity":31},null,{"Name":"wire-nullable-last",'
                    b'"Quantity":33}],"empty":[],"missing":null}')
            self.send_response(200)
            self.send_header("X-CX-Wire", "nullable-collection-map")
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path == "/wire/json-map-nullable-dto-list-response" and
                has_generated_headers(self.headers)):
            body = (b'{"wrapped":[{"Name":"wire-nullable-first",'
                    b'"Quantity":31},null,{"Name":"wire-nullable-last",'
                    b'"Quantity":33}],"empty":[],"missing":null}')
            self.send_response(202)
            self.send_header("X-CX-Wire", "nullable-collection-map")
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path == "/wire/json-map-nullable-dto-response" and
                has_generated_headers(self.headers)):
            body = b'{"present":{"Name":"wire-map-dto","Quantity":37},"missing":null}'
            self.send_response(202)
            self.send_header("X-Wire-Nullable", "dto-map-response")
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path == "/wire/json-nullable-int-map" and
                has_generated_headers(self.headers)):
            body = b'{"positive":32,"missing":null,"negative":-8}'
            self.send_response(200)
            self.send_header("X-CX-Wire", "nullable-scalar-map")
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path == "/wire/json-nullable-int-map-response" and
                has_generated_headers(self.headers)):
            body = b'{"present":11,"missing":null,"negative":-5}'
            self.send_response(202)
            self.send_header("X-CX-Wire", "nullable-scalar-map-response")
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path == "/wire/json-nullable-decimal-map" and
                has_generated_headers(self.headers)):
            body = b'{"positive":12.5,"missing":null,"negative":-0.125}'
            self.send_response(200)
            self.send_header("X-CX-Wire", "nullable-decimal-map")
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path == "/wire/json-nullable-decimal-map-response" and
                has_generated_headers(self.headers)):
            body = b'{"present":1.25e2,"missing":null,"negative":-1.25e-4}'
            self.send_response(202)
            self.send_header("X-CX-Wire", "nullable-decimal-map-response")
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path == "/wire/json-nullable-bool-map" and
                has_generated_headers(self.headers)):
            body = b'{"enabled":true,"missing":null,"disabled":false}'
            self.send_response(200)
            self.send_header("X-CX-Wire", "nullable-bool-map")
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path == "/wire/json-nullable-bool-map-response" and
                has_generated_headers(self.headers)):
            body = b'{"present":false,"missing":null,"enabled":true}'
            self.send_response(202)
            self.send_header("X-CX-Wire", "nullable-bool-map-response")
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path == "/wire/json-nullable-uuid-map" and
                has_generated_headers(self.headers)):
            body = b'{"first":"00112233-4455-6677-8899-aabbccddeeff","missing":null,"second":"10203040-5060-7080-90a0-b0c0d0e0f000"}'
            self.send_response(200)
            self.send_header("X-CX-Wire", "nullable-uuid-map")
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path == "/wire/json-nullable-uuid-map-response" and
                has_generated_headers(self.headers)):
            body = b'{"present":"00112233-4455-6677-8899-aabbccddeeff","missing":null,"other":"10203040-5060-7080-90a0-b0c0d0e0f000"}'
            self.send_response(202)
            self.send_header("X-CX-Wire", "nullable-uuid-map-response")
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path == "/wire/json-dto-stack-map" and
                has_generated_headers(self.headers)):
            body = b'{"items":[{"Name":"wire-stack-first","Quantity":41},{"Name":"wire-stack-last","Quantity":43}],"empty":[]}'
            self.send_response(200)
            self.send_header("X-CX-Wire", "dto-stack-map")
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path == "/wire/json-dto-stack-map-response" and
                has_generated_headers(self.headers)):
            body = b'{"wrapped":[{"Name":"wire-stack-wrapped-first","Quantity":51},{"Name":"wire-stack-wrapped-last","Quantity":53}],"empty":[]}'
            self.send_response(202)
            self.send_header("X-CX-Wire", "dto-stack-map-response")
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        dto_collection_map_routes = {
            "/wire/json-dto-array-map": (
                200, "dto-array-map",
                b'{"items":[{"Name":"wire-array-first","Quantity":61},{"Name":"wire-array-last","Quantity":63}],"empty":[]}'),
            "/wire/json-dto-array-map-response": (
                202, "dto-array-map-response",
                b'{"items":[{"Name":"wire-array-wrapped-first","Quantity":71},{"Name":"wire-array-wrapped-last","Quantity":73}],"empty":[]}'),
            "/wire/json-dto-linked-list-map": (
                200, "dto-linked-list-map",
                b'{"items":[{"Name":"wire-linked-first","Quantity":81},{"Name":"wire-linked-last","Quantity":83}],"empty":[]}'),
            "/wire/json-dto-linked-list-map-response": (
                202, "dto-linked-list-map-response",
                b'{"items":[{"Name":"wire-linked-wrapped-first","Quantity":91},{"Name":"wire-linked-wrapped-last","Quantity":93}],"empty":[]}'),
            "/wire/json-dto-queue-map": (
                200, "dto-queue-map",
                b'{"items":[{"Name":"wire-queue-first","Quantity":101},{"Name":"wire-queue-last","Quantity":103}],"empty":[]}'),
            "/wire/json-dto-queue-map-response": (
                202, "dto-queue-map-response",
                b'{"items":[{"Name":"wire-queue-wrapped-first","Quantity":111},{"Name":"wire-queue-wrapped-last","Quantity":113}],"empty":[]}'),
        }
        dto_collection_map = dto_collection_map_routes.get(self.path)
        if dto_collection_map is not None and has_generated_headers(self.headers):
            status, wire_name, body = dto_collection_map
            self.send_response(status)
            self.send_header("X-CX-Wire", wire_name)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path == "/wire/concurrent/slow" and
                has_generated_headers(self.headers)):
            REST_CONCURRENT_SLOW_STARTED.set()
            overlapped = REST_CONCURRENT_FAST_STARTED.wait(timeout=3)
            status = 200 if overlapped else 503
            body = (b"wire-concurrent-slow" if overlapped
                    else b"wire-concurrent-overlap-missed")
            self.send_response(status)
            self.send_header("Content-Type", "text/plain")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path == "/wire/concurrent/fast" and
                has_generated_headers(self.headers)):
            overlapped = REST_CONCURRENT_SLOW_STARTED.wait(timeout=3)
            REST_CONCURRENT_FAST_STARTED.set()
            status = 200 if overlapped else 503
            body = (b"wire-concurrent-fast" if overlapped
                    else b"wire-concurrent-overlap-missed")
            self.send_response(status)
            self.send_header("Content-Type", "text/plain")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path in ("/wire/no-content-response", "/wire/no-content-dto",
                          "/wire/no-content-stream-response")
                and has_generated_headers(self.headers)):
            self.send_response(204)
            self.send_header("X-Wire-NoContent", "preserved")
            self.send_header("Connection", "close")
            self.end_headers()
            return
        if self.path == "/wire/bytes" and has_generated_headers(self.headers):
            body = bytes(index % 251 for index in range(96 * 1024))
            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if self.path == "/wire/item-response" and has_generated_headers(self.headers):
            body = b'{"Name":"wire-item","Quantity":19}'
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("X-Wire-Item", "wrapped")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        nullable_dto_responses = {
            "/wire/nullable-item-null": (b" \t null\r\n", False),
            "/wire/nullable-item-value": (
                b'{"Name":"nullable-wire-item","Quantity":27}', False),
            "/wire/nullable-item-response-null": (b" \t null\r\n", True),
            "/wire/nullable-item-response-value": (
                b'{"Name":"nullable-wire-wrapped","Quantity":28}', True),
        }
        if (self.path in nullable_dto_responses and
                has_generated_headers(self.headers)):
            body, wrapped = nullable_dto_responses[self.path]
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            if wrapped:
                self.send_header("X-Wire-Nullable", "wrapped")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        scalar_responses = {
            "/wire/scalar-int": (200, b"-1042", "application/json"),
            "/wire/scalar-bool": (200, b"true", "application/json"),
            "/wire/scalar-long": (
                200, b"9223372036854775807", "application/json"),
            "/wire/scalar-double": (
                200, b"-12345.625", "application/json"),
            "/wire/scalar-byte": (
                200, b"255", "application/json"),
            "/wire/scalar-sbyte": (
                200, b"-128", "application/json"),
            "/wire/scalar-short": (
                200, b"-32768", "application/json"),
            "/wire/scalar-ushort": (
                200, b"65535", "application/json"),
            "/wire/scalar-uint": (
                200, b"4294967295", "application/json"),
            "/wire/scalar-ulong": (
                200, b"18446744073709551615", "application/json"),
            "/wire/scalar-float": (
                200, b"-1.25e+3", "application/json"),
            "/wire/scalar-ulong-response": (
                200, b"18446744073709551615", "application/json"),
            "/wire/scalar-byte-response": (
                200, b"255", "application/json"),
            "/wire/scalar-sbyte-response": (
                200, b"-128", "application/json"),
            "/wire/scalar-short-response": (
                200, b"-32768", "application/json"),
            "/wire/scalar-ushort-response": (
                200, b"65535", "application/json"),
            "/wire/scalar-uint-response": (
                200, b"4294967295", "application/json"),
            "/wire/scalar-float-response": (
                200, b"-1.25e+3", "application/json"),
            "/wire/scalar-response": (
                200, b"9223372036854775807", "application/json"),
            "/wire/scalar-bool-response": (
                200, b"false", "application/json"),
            "/wire/scalar-decimal-response": (
                200, b"1234567890.1234", "application/json"),
            "/wire/scalar-uuid": (
                200, b'"00112233-4455-6677-8899-aabbccddeeff"',
                "application/json"),
            "/wire/scalar-uuid-response": (
                200, b'"00112233-4455-6677-8899-aabbccddeeff"',
                "application/json"),
            "/wire/scalar-nullable-uuid-null": (
                200, b"null", "application/json"),
            "/wire/scalar-nullable-uuid-value": (
                200, b'"00112233-4455-6677-8899-aabbccddeeff"',
                "application/json"),
            "/wire/scalar-nullable-uuid-response-null": (
                200, b"null", "application/json"),
            "/wire/scalar-nullable-uuid-response-value": (
                200, b'"00112233-4455-6677-8899-aabbccddeeff"',
                "application/json"),
            "/wire/scalar-nullable-char-null": (
                200, b" \t null\r\n", "application/json"),
            "/wire/scalar-nullable-char-value": (
                200, '"💡"'.encode("utf-8"), "application/json"),
            "/wire/scalar-nullable-char-response-null": (
                200, b"null", "application/json"),
            "/wire/scalar-nullable-char-response-value": (
                200, '"💡"'.encode("utf-8"), "application/json"),
            "/wire/scalar-uuid-invalid": (
                200, b'"not-a-canonical-uuid"', "application/json"),
            "/wire/scalar-nullable-int-null": (
                200, b" \t null\r\n", "application/json"),
            "/wire/scalar-nullable-int-value": (
                200, b"73", "application/json"),
            "/wire/scalar-nullable-short-null": (
                200, b"null", "application/json"),
            "/wire/scalar-nullable-short-value": (
                200, b"-32768", "application/json"),
            "/wire/scalar-nullable-short-response-null": (
                200, b"null", "application/json"),
            "/wire/scalar-nullable-short-response-value": (
                200, b"32767", "application/json"),
            "/wire/scalar-nullable-ushort-null": (
                200, b"null", "application/json"),
            "/wire/scalar-nullable-ushort-value": (
                200, b"65535", "application/json"),
            "/wire/scalar-nullable-ushort-response-null": (
                200, b"null", "application/json"),
            "/wire/scalar-nullable-ushort-response-value": (
                200, b"65535", "application/json"),
            "/wire/scalar-nullable-uint-null": (
                200, b"null", "application/json"),
            "/wire/scalar-nullable-uint-value": (
                200, b"4294967295", "application/json"),
            "/wire/scalar-nullable-uint-response-null": (
                200, b"null", "application/json"),
            "/wire/scalar-nullable-uint-response-value": (
                200, b"4294967295", "application/json"),
            "/wire/scalar-nullable-byte-null": (
                200, b"null", "application/json"),
            "/wire/scalar-nullable-byte-value": (
                200, b"255", "application/json"),
            "/wire/scalar-nullable-sbyte-null": (
                200, b"null", "application/json"),
            "/wire/scalar-nullable-sbyte-value": (
                200, b"-128", "application/json"),
            "/wire/scalar-nullable-decimal-null": (
                200, b" \tnull\r\n", "application/json"),
            "/wire/scalar-nullable-decimal-value": (
                200, b"123.5", "application/json"),
            "/wire/scalar-nullable-float-null": (
                200, b" \t null\r\n", "application/json"),
            "/wire/scalar-nullable-float-value": (
                200, b"-0.125", "application/json"),
            "/wire/scalar-nullable-double-null": (
                200, b"null", "application/json"),
            "/wire/scalar-nullable-double-value": (
                200, b"-42.125", "application/json"),
            "/wire/scalar-nullable-float-response-null": (
                200, b"null", "application/json"),
            "/wire/scalar-nullable-float-response-value": (
                200, b"0.0", "application/json"),
            "/wire/scalar-nullable-double-response-null": (
                200, b"null", "application/json"),
            "/wire/scalar-nullable-double-response-value": (
                200, b"2.5", "application/json"),
            "/wire/scalar-nullable-bool-null": (
                200, b"null", "application/json"),
            "/wire/scalar-nullable-bool-value": (
                200, b"true", "application/json"),
            "/wire/scalar-nullable-int-response-null": (
                200, b"null", "application/json"),
            "/wire/scalar-nullable-int-response-value": (
                200, b"0", "application/json"),
            "/wire/scalar-nullable-byte-response-null": (
                200, b"null", "application/json"),
            "/wire/scalar-nullable-byte-response-value": (
                200, b"255", "application/json"),
            "/wire/scalar-nullable-sbyte-response-null": (
                200, b"null", "application/json"),
            "/wire/scalar-nullable-sbyte-response-value": (
                200, b"-128", "application/json"),
            "/wire/scalar-nullable-bool-response-null": (
                200, b"null", "application/json"),
            "/wire/scalar-nullable-bool-response-value": (
                200, b"false", "application/json"),
            "/wire/scalar-nullable-ulong-null": (
                200, b" \tnull\r\n", "application/json"),
            "/wire/scalar-nullable-ulong-value": (
                200, b"18446744073709551615", "application/json"),
            "/wire/scalar-nullable-ulong-response-null": (
                200, b"null", "application/json"),
            "/wire/scalar-nullable-ulong-response-value": (
                200, b"18446744073709551615", "application/json"),
            "/wire/scalar-nullable-long-null": (
                200, b"null", "application/json"),
            "/wire/scalar-nullable-long-value": (
                200, b"-9223372036854775808", "application/json"),
            "/wire/scalar-nullable-long-response-null": (
                200, b"null", "application/json"),
            "/wire/scalar-nullable-long-response-value": (
                200, b"-9223372036854775808", "application/json"),
            "/wire/scalar-nullable-response-null": (
                200, b"null", "application/json"),
            "/wire/scalar-nullable-response-value": (
                200, b"123.5", "application/json"),
            "/wire/scalar-nullable-error-response": (
                409, b"nullable scalar response failed", "text/plain"),
            "/wire/scalar-invalid": (
                200, b"not-a-number", "application/json"),
            "/wire/scalar-error-response": (
                409, b"scalar response failed", "text/plain"),
        }
        if self.path in scalar_responses and has_generated_headers(self.headers):
            status, body, content_type = scalar_responses[self.path]
            self.send_response(status)
            self.send_header("Content-Type", content_type)
            if self.path == "/wire/scalar-response":
                self.send_header("X-Wire-Scalar", "wrapped")
            elif self.path == "/wire/scalar-bool-response":
                self.send_header("X-Wire-Scalar", "boolean")
            elif self.path == "/wire/scalar-decimal-response":
                self.send_header("X-Wire-Scalar", "decimal")
            elif self.path == "/wire/scalar-ulong-response":
                self.send_header("X-Wire-Scalar", "ulong")
            elif self.path == "/wire/scalar-byte-response":
                self.send_header("X-Wire-Scalar", "byte")
            elif self.path == "/wire/scalar-sbyte-response":
                self.send_header("X-Wire-Scalar", "sbyte")
            elif self.path == "/wire/scalar-short-response":
                self.send_header("X-Wire-Scalar", "short")
            elif self.path == "/wire/scalar-ushort-response":
                self.send_header("X-Wire-Scalar", "ushort")
            elif self.path == "/wire/scalar-uint-response":
                self.send_header("X-Wire-Scalar", "uint")
            elif self.path == "/wire/scalar-float-response":
                self.send_header("X-Wire-Scalar", "float")
            elif self.path == "/wire/scalar-uuid-response":
                self.send_header("X-Wire-Scalar", "uuid")
            elif self.path == "/wire/scalar-nullable-uuid-response-null":
                self.send_header("X-Wire-Scalar", "uuid-null")
            elif self.path == "/wire/scalar-nullable-uuid-response-value":
                self.send_header("X-Wire-Scalar", "uuid-value")
            elif self.path == "/wire/scalar-nullable-char-response-null":
                self.send_header("X-Wire-Scalar", "char-null")
            elif self.path == "/wire/scalar-nullable-char-response-value":
                self.send_header("X-Wire-Scalar", "char-value")
            elif self.path == "/wire/scalar-nullable-int-response-null":
                self.send_header("X-Wire-Scalar", "int-null")
            elif self.path == "/wire/scalar-nullable-int-response-value":
                self.send_header("X-Wire-Scalar", "int-zero")
            elif self.path == "/wire/scalar-nullable-short-response-null":
                self.send_header("X-Wire-Scalar", "short-null")
            elif self.path == "/wire/scalar-nullable-short-response-value":
                self.send_header("X-Wire-Scalar", "short-max")
            elif self.path == "/wire/scalar-nullable-ushort-response-null":
                self.send_header("X-Wire-Scalar", "ushort-null")
            elif self.path == "/wire/scalar-nullable-ushort-response-value":
                self.send_header("X-Wire-Scalar", "ushort-max")
            elif self.path == "/wire/scalar-nullable-uint-response-null":
                self.send_header("X-Wire-Scalar", "uint-null")
            elif self.path == "/wire/scalar-nullable-uint-response-value":
                self.send_header("X-Wire-Scalar", "uint-max")
            elif self.path == "/wire/scalar-nullable-byte-response-null":
                self.send_header("X-Wire-Scalar", "byte-null")
            elif self.path == "/wire/scalar-nullable-byte-response-value":
                self.send_header("X-Wire-Scalar", "byte-max")
            elif self.path == "/wire/scalar-nullable-sbyte-response-null":
                self.send_header("X-Wire-Scalar", "sbyte-null")
            elif self.path == "/wire/scalar-nullable-sbyte-response-value":
                self.send_header("X-Wire-Scalar", "sbyte-min")
            elif self.path == "/wire/scalar-nullable-float-response-null":
                self.send_header("X-Wire-Scalar", "float-null")
            elif self.path == "/wire/scalar-nullable-float-response-value":
                self.send_header("X-Wire-Scalar", "float-zero")
            elif self.path == "/wire/scalar-nullable-double-response-null":
                self.send_header("X-Wire-Scalar", "double-null")
            elif self.path == "/wire/scalar-nullable-double-response-value":
                self.send_header("X-Wire-Scalar", "double-value")
            elif self.path == "/wire/scalar-nullable-bool-response-null":
                self.send_header("X-Wire-Scalar", "bool-null")
            elif self.path == "/wire/scalar-nullable-bool-response-value":
                self.send_header("X-Wire-Scalar", "bool-false")
            elif self.path == "/wire/scalar-nullable-ulong-response-null":
                self.send_header("X-Wire-Scalar", "ulong-null")
            elif self.path == "/wire/scalar-nullable-ulong-response-value":
                self.send_header("X-Wire-Scalar", "ulong-max")
            elif self.path == "/wire/scalar-nullable-long-response-null":
                self.send_header("X-Wire-Scalar", "long-null")
            elif self.path == "/wire/scalar-nullable-long-response-value":
                self.send_header("X-Wire-Scalar", "long-min")
            elif self.path == "/wire/scalar-nullable-response-null":
                self.send_header("X-Wire-Scalar", "nullable-null")
            elif self.path == "/wire/scalar-nullable-response-value":
                self.send_header("X-Wire-Scalar", "nullable-value")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        if (self.path in ("/wire/scalar-empty-response",
                          "/wire/scalar-nullable-empty-response") and
                has_generated_headers(self.headers)):
            self.send_response(204)
            self.send_header("X-Wire-Scalar", "empty")
            self.send_header("Connection", "close")
            self.end_headers()
            return
        if (self.path == "/wire/cancel-stream" and
                has_generated_headers(self.headers)):
            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Length", str(1024 * 1024))
            self.end_headers()
            try:
                self.wfile.write(b"pong")
                self.wfile.flush()
                time.sleep(0.25)
                chunk = b"x" * 4096
                remaining = 1024 * 1024 - 4
                while remaining > 0:
                    amount = min(remaining, len(chunk))
                    self.wfile.write(chunk[:amount])
                    self.wfile.flush()
                    remaining -= amount
                    time.sleep(0.005)
            except (BrokenPipeError, ConnectionResetError, OSError):
                REST_CANCEL_STREAM_CLOSED.set()
            return
        if (self.path == "/wire/dispose-stream" and
                has_generated_headers(self.headers)):
            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Length", str(1024 * 1024))
            self.end_headers()
            try:
                self.wfile.write(b"pong")
                self.wfile.flush()
                time.sleep(0.25)
                chunk = b"d" * 4096
                remaining = 1024 * 1024 - 4
                while remaining > 0:
                    amount = min(remaining, len(chunk))
                    self.wfile.write(chunk[:amount])
                    self.wfile.flush()
                    remaining -= amount
                    time.sleep(0.005)
            except (BrokenPipeError, ConnectionResetError, OSError):
                REST_DISPOSE_STREAM_CLOSED.set()
            return
        if self.path == "/wire/stream-response" and has_generated_headers(self.headers):
            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Length", str(STREAM_RESPONSE_BYTES))
            self.send_header("Connection", "close")
            self.end_headers()
            if not STREAM_RELEASED.wait(timeout=3):
                return
            for offset in range(0, STREAM_RESPONSE_BYTES, 4096):
                chunk = bytes(index % 251 for index in range(offset, offset + 4096))
                self.wfile.write(chunk)
                self.wfile.flush()
                time.sleep(0.001)
            return
        if self.path == "/wire/stream-release" and has_generated_headers(self.headers):
            STREAM_RELEASED.set()
            body = b"released"
            self.send_response(200)
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            return
        extra_headers = []
        response_type = "text/plain"
        if self.path == "/custom-dto":
            status, body = 200, b"wire-fetched"
            response_type = "application/x-cx-custom; charset=utf-8"
        elif (self.path == "/wire/overloaded?id=42" and
                has_generated_headers(self.headers)):
            status, body = 200, b"wire-overload-number"
        elif (self.path == "/wire/overloaded?name=wire%20name" and
                has_generated_headers(self.headers)):
            status, body = 200, b"wire-overload-text"
        elif self.path.startswith("/wire/overloaded?"):
            status, body = 400, b"wrong overload binding"
        elif (self.path == EXPECTED_TARGET and
                has_generated_headers(self.headers) and
                self.headers.get("X-Wire-Trace") == "trace-value"):
            status, body = 200, b"wire-ok"
        elif (self.path == EXPECTED_ROUTE_FRAGMENT and
              has_generated_headers(self.headers)):
            status, body = 200, b"wire-fragment-ok"
        elif self.path == EXPECTED_REPEATED_QUERY and has_generated_headers(self.headers):
            status, body = 200, b"wire-query-ok"
        elif self.path == EXPECTED_INTEGER_TAGS and has_generated_headers(self.headers):
            status, body = 200, b"wire-int-tags-ok"
        elif self.path == EXPECTED_ULONG_TAGS and has_generated_headers(self.headers):
            status, body = 200, b"wire-ulong-tags-ok"
        elif (self.path == EXPECTED_NULLABLE_ULONG_TAGS and
                has_generated_headers(self.headers)):
            status, body = 200, b"wire-nullable-ulong-tags-ok"
        elif self.path == EXPECTED_ARRAY_TAGS and has_generated_headers(self.headers):
            status, body = 200, b"wire-array-tags-ok"
        elif self.path == EXPECTED_SCALAR_QUERY and has_generated_headers(self.headers):
            status, body = 200, b"wire-scalar-query-ok"
        elif (self.path == EXPECTED_CHAR_VALUES and
                has_generated_headers(self.headers) and
                self.headers.get("X-Wire-Char") == "H"):
            status, body = 200, b'"\\u263a"'
            response_type = "application/json"
        elif self.path == EXPECTED_QUERY_MAP and has_generated_headers(self.headers):
            status, body = 200, b"wire-query-map-ok"
        elif (self.path == EXPECTED_NUMERIC_QUERY_MAP and
              has_generated_headers(self.headers)):
            status, body = 200, b"wire-numeric-query-map-ok"
        elif (self.path == EXPECTED_INT_COLLECTION_QUERY_MAP and
              has_generated_headers(self.headers)):
            status, body = 200, b"wire-int-collection-query-map-ok"
        elif (self.path == EXPECTED_NULLABLE_SEQUENCES and
              has_generated_headers(self.headers) and
              self.headers.get_all("X-Wire-Tag") == ["a b", "", "x&y"]):
            status, body = 200, b"wire-nullable-sequences-ok"
        elif (self.path == EXPECTED_NULLABLE_NUMERIC_SEQUENCES and
              has_generated_headers(self.headers) and
              self.headers.get_all("X-Wire-Amount") == ["0.125", "-2.5"]):
            status, body = 200, b"wire-nullable-numeric-sequences-ok"
        elif (self.path == EXPECTED_NULLABLE_NUMERIC_COLLECTIONS and
              has_generated_headers(self.headers) and
              all(self.headers.get_all("X-Numeric-Amount-" + suffix) ==
                  ["0.125", "-2.5"] for suffix in (
                      "List", "Array", "Linked", "Queue", "Stack",
                      "Enumerable", "Collection")) and
              self.headers.get_all("X-Numeric-Flag") == ["True", "False"]):
            status, body = 200, b"wire-nullable-numeric-collections-ok"
        elif (self.path == EXPECTED_NULLABLE_QUERY_MAP and
              has_generated_headers(self.headers)):
            status, body = 200, b"wire-nullable-query-map-ok"
        elif (self.path == EXPECTED_NULLABLE_QUERY_MAP_COLLECTIONS and
              has_generated_headers(self.headers)):
            status, body = 200, b"wire-nullable-query-map-collections-ok"
        elif (self.path == "/wire/nullable-query-map-collections" and
              has_generated_headers(self.headers)):
            status, body = 200, b"wire-nullable-query-map-omitted-ok"
        elif (self.path == "/wire/nullable-sequences" and
              has_generated_headers(self.headers) and
              self.headers.get_all("X-Wire-Tag") is None):
            status, body = 200, b"wire-nullable-sequences-omitted-ok"
        elif (self.path == EXPECTED_CONST_QUERY_MAP and
              has_generated_headers(self.headers)):
            status, body = 200, b"wire-const-query-map-ok"
        elif (self.path == EXPECTED_CONST_LIST_TAGS and
              has_generated_headers(self.headers)):
            status, body = 200, b"wire-const-list-tags-ok"
        elif self.path == EXPECTED_LINKED_TAGS and has_generated_headers(self.headers):
            status, body = 200, b"wire-linked-tags-ok"
        elif (self.path == EXPECTED_CONST_LINKED_TAGS and
              has_generated_headers(self.headers)):
            status, body = 200, b"wire-const-linked-tags-ok"
        elif self.path == EXPECTED_LINKED_MAP and has_generated_headers(self.headers):
            status, body = 200, b"wire-linked-map-ok"
        elif self.path == EXPECTED_ARRAY_MAP and has_generated_headers(self.headers):
            status, body = 200, b"wire-array-map-ok"
        elif self.path == EXPECTED_QUEUE_TAGS and has_generated_headers(self.headers):
            status, body = 200, b"wire-queue-tags-ok"
        elif self.path == EXPECTED_STACK_TAGS and has_generated_headers(self.headers):
            status, body = 200, b"wire-stack-tags-ok"
        elif self.path == EXPECTED_QUEUE_MAP and has_generated_headers(self.headers):
            status, body = 200, b"wire-queue-map-ok"
        elif self.path == EXPECTED_STACK_MAP and has_generated_headers(self.headers):
            status, body = 200, b"wire-stack-map-ok"
        elif (self.path == "/wire/header-values" and
                has_generated_headers(self.headers) and
                self.headers.get_all("X-Wire-Tag") == ["x&y", "first"]):
            status, body = 200, b"wire-header-values-stack-ok"
        elif (self.path == "/wire/header-values" and
                has_generated_headers(self.headers) and
                self.headers.get_all("X-Wire-Tag") == ["first", "x&y"]):
            status, body = 200, b"wire-header-values-ok"
        elif (self.path == "/wire/header-array-values" and
                has_generated_headers(self.headers) and
                self.headers.get_all("X-Wire-Tag") == ["first", "x&y"]):
            status, body = 200, b"wire-header-array-values-ok"
        elif (self.path == "/wire/optional" and
                has_generated_headers(self.headers) and
                self.headers.get("X-Wire-Optional") is None):
            status, body = 200, b"wire-optional-omitted"
        elif (self.path == "/wire/optional?q=" and
                has_generated_headers(self.headers) and
                self.headers.get("X-Wire-Optional") == ""):
            status, body = 200, b"wire-optional-empty"
        elif (self.path == "/wire/optional-scalars" and
                has_generated_headers(self.headers) and
                self.headers.get("X-Wire-Optional-Scalar") is None):
            status, body = 200, b"wire-optional-scalars-omitted"
        elif (self.path == "/wire/optional-scalars?count=7&amount=0.125" and
                has_generated_headers(self.headers) and
                self.headers.get("X-Wire-Optional-Scalar") == "42"):
            status, body = 200, b"wire-optional-scalars-present"
        elif (self.path == "/wire/optional-uuid" and
                has_generated_headers(self.headers) and
                self.headers.get("X-Wire-Optional-Uuid") is None):
            status, body = 200, b"wire-optional-uuid-omitted"
        elif (self.path ==
                "/wire/optional-uuid?id=00112233-4455-6677-8899-aabbccddeeff" and
                has_generated_headers(self.headers) and
                self.headers.get("X-Wire-Optional-Uuid") ==
                "00112233-4455-6677-8899-aabbccddeeff"):
            status, body = 200, b"wire-optional-uuid-present"
        elif (self.path ==
                "/wire/optional-uuid-sequence?id=00112233-4455-6677-8899-aabbccddeeff&id=10203040-5060-7080-90a0-b0c0d0e0f000" and
                has_generated_headers(self.headers)):
            status, body = 200, b"wire-optional-uuid-sequence"
        elif self.path == "/wire/error" and has_generated_headers(self.headers):
            status, body = 409, b"wire missing"
            extra_headers.append(("X-Wire-Error", "retained"))
        elif self.path == "/wire/dto-error" and has_generated_headers(self.headers):
            status, body = 400, b"x" * 5000
            extra_headers.append(("X-Wire-Api-Error", "retained"))
        elif self.path == "/wire/dto-malformed-error" and has_generated_headers(self.headers):
            status, body = 400, b"\xf0\x28\x8c\x28"
            response_type = "text/plain; charset=utf-8"
            extra_headers.append(("X-Wire-Malformed-Error", "retained"))
        elif (
            self.path == "/wire/dto-malformed-error-wrapper"
            and has_generated_headers(self.headers)
        ):
            status, body = 400, b"\xf0\x28\x8c\x28"
            response_type = "text/plain; charset=utf-8"
            extra_headers.append(("X-Wire-Malformed-Error", "retained"))
        elif self.path == "/wire/stream-malformed-error" and has_generated_headers(self.headers):
            status, body = 400, b"\xf0\x28\x8c\x28"
            response_type = "text/plain; charset=utf-8"
            extra_headers.append(("X-Wire-Malformed-Error", "retained"))
        elif (
            self.path == "/wire/stream-malformed-error-wrapper"
            and has_generated_headers(self.headers)
        ):
            status, body = 400, b"\xf0\x28\x8c\x28"
            response_type = "text/plain; charset=utf-8"
            extra_headers.append(("X-Wire-Malformed-Error", "retained"))
        else:
            status, body = 400, b"bad route"
        self.send_response(status)
        self.send_header("X-CX-Wire", "generated")
        self.send_header("Content-Type", response_type)
        for name, value in extra_headers:
            self.send_header(name, value)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self) -> None:  # noqa: N802
        request_body = self.read_request_body()
        expected_body = b'{"Name":"gear & lamp","Quantity":4}'
        expected_multipart = (
            b'--cxnet-boundary\r\n'
            b'Content-Disposition: form-data; name="label"\r\n'
            b'Content-Type: text/plain; charset=utf-8\r\n\r\n'
            b'A & B\r\n--cxnet-boundary--\r\n'
        )
        expected_multipart_queue = multipart_labels_body(["A & B", "second"])
        expected_multipart_stack = multipart_labels_body(["second", "A & B"])
        expected_multipart_repeated = multipart_labels_body(["A & B", "second"])
        expected_multipart_scalars = multipart_text_parts_body(
            [("count", "7"), ("active", "False")])
        expected_multipart_scalar_sequence = multipart_text_parts_body(
            [("quantity", "5"), ("quantity", "-2")])
        expected_multipart_chars = multipart_text_parts_body(
            [("marker", "A"), ("marker", "é")])
        expected_multipart_optional_empty = b"--cxnet-boundary--\r\n"
        expected_multipart_optional_blank = multipart_text_parts_body(
            [("note", "")])
        expected_multipart_optional_value = multipart_text_parts_body(
            [("count", "7"), ("amount", "0.125"), ("note", "A & B"),
             ("id", "00112233-4455-6677-8899-aabbccddeeff")])
        expected_multipart_nullable_sequence = multipart_text_parts_body(
            [("role", "admin user"), ("role", ""), ("role", "a&b")])
        expected_multipart_nullable_numeric_sequence = multipart_text_parts_body(
            [("amount", "0.125"), ("amount", "-2.5")])
        expected_multipart_content = (
            b'--cxnet-boundary\r\n'
            b'Content-Disposition: form-data; name="file"; filename="a.txt"\r\n'
            b'Content-Type: application/custom\r\n\r\nraw\r\n'
            b'--cxnet-boundary--\r\n'
        )
        if (self.path == "/custom-dto" and
                self.headers.get_content_type() == "application/x-cx-custom" and
                request_body == b"wire-option|custom request|17"):
            status, body = 200, b"wire-saved"
            response_type = "application/x-cx-custom; charset=utf-8"
        elif (self.path == "/custom-annotated-dto" and
                self.headers.get_content_type() == "application/x-cx-rest-item" and
                request_body == b"wire-annotated-request:wire-option"):
            status, body = 200, b"wire-annotated-response"
            response_type = "application/x-cx-rest-item; charset=utf-8"
        elif (self.path == "/wire/empty-body" and
                has_generated_headers(self.headers) and
                self.headers.get_content_type() == "text/plain" and
                self.headers.get_content_charset() == "utf-8" and
                self.headers.get("Content-Length") == "0" and
                self.headers.get("Transfer-Encoding") is None and
                request_body == b""):
            status, body = 200, b"wire-empty-body-ok"
            response_type = "text/plain"
        elif (self.path == "/wire/binary-body" and
                has_generated_headers(self.headers) and
                self.headers.get("Content-Type") is None and
                request_body == b"\x00\x01\x7f\x80\xff"):
            status, body = 200, b"wire-binary-body-ok"
            response_type = "text/plain"
        elif (self.path == "/wire/optional-binary-body" and
                has_generated_headers(self.headers) and
                self.headers.get("Content-Type") is None and
                request_body == b"\x00\x01\x7f\x80\xff"):
            status, body = 200, b"wire-optional-binary-body-ok"
            response_type = "text/plain"
        elif (self.path == "/wire/optional-body" and
                has_generated_headers(self.headers) and
                self.headers.get("Content-Type") is None and
                request_body == b""):
            status, body = 200, b"wire-null-body-ok"
            response_type = "text/plain"
        elif (self.path == "/wire/optional-body" and
                has_generated_headers(self.headers) and
                self.headers.get_content_type() == "text/plain" and
                self.headers.get_content_charset() == "utf-8" and
                request_body == b""):
            status, body = 200, b"wire-empty-optional-body-ok"
            response_type = "text/plain"
        elif (self.path == "/wire/optional-dto-body" and
                has_generated_headers(self.headers) and
                self.headers.get("Content-Type") is None and
                request_body == b""):
            status, body = 200, b"wire-null-dto-body-ok"
            response_type = "text/plain"
        elif (self.path == "/wire/optional-dto-body" and
                has_generated_headers(self.headers) and
                self.headers.get_content_type() == "application/json" and
                request_body == b'{"Name":"optional dto","Quantity":8}'):
            status, body = 200, b"wire-optional-dto-body-ok"
            response_type = "text/plain"
        elif self.path == "/wire/binary-body":
            print(f"binary body mismatch: headers={dict(self.headers.items())!r} "
                  f"body={request_body!r}", file=sys.stderr)
            status, body = 400, b"bad binary request"
            response_type = "text/plain"
        elif (self.path == "/wire/multipart" and
                has_generated_headers(self.headers) and
                self.headers.get_content_type() == "multipart/form-data" and
                self.headers.get_param("boundary") == "cxnet-boundary" and
                request_body == expected_multipart):
            status, body = 200, b"wire-multipart-ok"
            response_type = "text/plain"
        elif (self.path == "/wire/multipart-queue" and
                has_generated_headers(self.headers) and
                self.headers.get_content_type() == "multipart/form-data" and
                self.headers.get_param("boundary") == "cxnet-boundary" and
                request_body == expected_multipart_queue):
            status, body = 200, b"wire-multipart-queue-ok"
            response_type = "text/plain"
        elif (self.path == "/wire/multipart-stack" and
                has_generated_headers(self.headers) and
                self.headers.get_content_type() == "multipart/form-data" and
                self.headers.get_param("boundary") == "cxnet-boundary" and
                request_body == expected_multipart_stack):
            status, body = 200, b"wire-multipart-stack-ok"
            response_type = "text/plain"
        elif (self.path == "/wire/multipart-const-stack" and
                has_generated_headers(self.headers) and
                self.headers.get_content_type() == "multipart/form-data" and
                self.headers.get_param("boundary") == "cxnet-boundary" and
                request_body == expected_multipart_stack):
            status, body = 200, b"wire-multipart-const-stack-ok"
            response_type = "text/plain"
        elif (self.path == "/wire/multipart-const-array" and
                has_generated_headers(self.headers) and
                self.headers.get_content_type() == "multipart/form-data" and
                self.headers.get_param("boundary") == "cxnet-boundary" and
                request_body == expected_multipart_repeated):
            status, body = 200, b"wire-multipart-const-array-ok"
            response_type = "text/plain"
        elif (self.path in ("/wire/multipart-enumerable", "/wire/multipart-array",
                            "/wire/multipart-list", "/wire/multipart-collection",
                            "/wire/multipart-const-list") and
                has_generated_headers(self.headers) and
                self.headers.get_content_type() == "multipart/form-data" and
                self.headers.get_param("boundary") == "cxnet-boundary" and
                request_body == expected_multipart_repeated):
            status, body = 200, ("wire-multipart-" +
                                 self.path[len("/wire/multipart-"):] +
                                 "-ok").encode("ascii")
            response_type = "text/plain"
        elif (self.path == "/wire/multipart-content" and
                has_generated_headers(self.headers) and
                self.headers.get_content_type() == "multipart/form-data" and
                self.headers.get_param("boundary") == "cxnet-boundary" and
                request_body == expected_multipart_content):
            status, body = 200, b"wire-multipart-content-ok"
            response_type = "text/plain"
        elif (self.path == "/wire/multipart-scalars" and
                has_generated_headers(self.headers) and
                self.headers.get_content_type() == "multipart/form-data" and
                self.headers.get_param("boundary") == "cxnet-boundary" and
                request_body == expected_multipart_scalars):
            status, body = 200, b"wire-multipart-scalars-ok"
            response_type = "text/plain"
        elif (self.path == "/wire/multipart-scalar-sequence" and
                has_generated_headers(self.headers) and
                self.headers.get_content_type() == "multipart/form-data" and
                self.headers.get_param("boundary") == "cxnet-boundary" and
                request_body == expected_multipart_scalar_sequence):
            status, body = 200, b"wire-multipart-scalar-sequence-ok"
            response_type = "text/plain"
        elif (self.path == "/wire/multipart-chars" and
                has_generated_headers(self.headers) and
                self.headers.get_content_type() == "multipart/form-data" and
                self.headers.get_param("boundary") == "cxnet-boundary" and
                request_body == expected_multipart_chars):
            status, body = 200, b"wire-multipart-chars-ok"
            response_type = "text/plain"
        elif (self.path == "/wire/multipart-optional" and
                has_generated_headers(self.headers) and
                self.headers.get_content_type() == "multipart/form-data" and
                self.headers.get_param("boundary") == "cxnet-boundary"):
            if request_body == expected_multipart_optional_empty:
                body = b"wire-multipart-optional-empty-ok"
                status = 200
            elif request_body == expected_multipart_optional_blank:
                body = b"wire-multipart-optional-blank-ok"
                status = 200
            elif request_body == expected_multipart_optional_value:
                body = b"wire-multipart-optional-value-ok"
                status = 200
            else:
                print(
                    "optional multipart mismatch: "
                    f"headers={dict(self.headers.items())!r} "
                    f"body={request_body!r}",
                    file=sys.stderr,
                )
                body = b"bad request"
                status = 400
            response_type = "text/plain"
        elif (self.path == "/wire/multipart-nullable-sequence" and
                has_generated_headers(self.headers) and
                self.headers.get_content_type() == "multipart/form-data" and
                self.headers.get_param("boundary") == "cxnet-boundary"):
            if request_body == expected_multipart_nullable_sequence:
                body = b"wire-multipart-nullable-sequence-ok"
                status = 200
            elif request_body == b"--cxnet-boundary--\r\n":
                body = b"wire-multipart-nullable-sequence-omitted-ok"
                status = 200
            else:
                body = b"bad request"
                status = 400
            response_type = "text/plain"
        elif (self.path == "/wire/multipart-nullable-numeric-sequence" and
                has_generated_headers(self.headers) and
                self.headers.get_content_type() == "multipart/form-data" and
                self.headers.get_param("boundary") == "cxnet-boundary" and
                request_body == expected_multipart_nullable_numeric_sequence):
            status, body = 200, b"wire-multipart-nullable-numeric-sequence-ok"
            response_type = "text/plain"
        elif (self.path == "/wire/form" and
                has_generated_headers(self.headers) and
                self.headers.get_content_type() == "application/x-www-form-urlencoded" and
                request_body == b"user+name=A+B&active=True"):
            status, body = 200, b"wire-form-ok"
            response_type = "text/plain"
        elif (self.path == "/wire/form-numbers" and
                has_generated_headers(self.headers) and
                self.headers.get_content_type() == "application/x-www-form-urlencoded" and
                request_body == b"quantity=5&quantity=-2&active=False"):
            status, body = 200, b"wire-form-numbers-ok"
            response_type = "text/plain"
        elif (self.path == "/wire/form-chars" and
                has_generated_headers(self.headers) and
                self.headers.get_content_type() == "application/x-www-form-urlencoded" and
                request_body == "marker=A&marker=%C3%A9".encode("ascii")):
            status, body = 200, b"wire-form-chars-ok"
            response_type = "text/plain"
        elif (self.path == "/wire/form-optional-scalars" and
                has_generated_headers(self.headers) and
                self.headers.get_content_type() == "application/x-www-form-urlencoded" and
                request_body == b""):
            status, body = 200, b"wire-form-optional-scalars-omitted"
            response_type = "text/plain"
        elif (self.path == "/wire/form-optional-scalars" and
                has_generated_headers(self.headers) and
                self.headers.get_content_type() == "application/x-www-form-urlencoded" and
                request_body == b"count=7&amount=0.125&active=True&id=00112233-4455-6677-8899-aabbccddeeff"):
            status, body = 200, b"wire-form-optional-scalars-present"
            response_type = "text/plain"
        elif (self.path == "/wire/form-nullable-sequence" and
                has_generated_headers(self.headers) and
                self.headers.get_content_type() == "application/x-www-form-urlencoded" and
                request_body == b"role=admin+user&role=&role=a%26b"):
            status, body = 200, b"wire-form-nullable-sequence-ok"
            response_type = "text/plain"
        elif (self.path == "/wire/form-nullable-sequence" and
                has_generated_headers(self.headers) and
                self.headers.get_content_type() == "application/x-www-form-urlencoded" and
                request_body == b""):
            status, body = 200, b"wire-form-nullable-sequence-omitted-ok"
            response_type = "text/plain"
        elif (self.path == "/wire/form-nullable-numeric-sequence" and
                has_generated_headers(self.headers) and
                self.headers.get_content_type() == "application/x-www-form-urlencoded" and
                request_body == b"quantity=5&quantity=-2"):
            status, body = 200, b"wire-form-nullable-numeric-sequence-ok"
            response_type = "text/plain"
        elif (self.path == "/wire/form-nullable-decimal-sequence" and
                has_generated_headers(self.headers) and
                self.headers.get_content_type() == "application/x-www-form-urlencoded" and
                request_body == b"amount=0.125&amount=-2.5"):
            status, body = 200, b"wire-form-nullable-decimal-sequence-ok"
            response_type = "text/plain"
        elif (self.path == "/wire/form-nullable-bool-sequence" and
                has_generated_headers(self.headers) and
                self.headers.get_content_type() == "application/x-www-form-urlencoded" and
                request_body == b"active=False&active=True"):
            status, body = 200, b"wire-form-nullable-bool-sequence-ok"
            response_type = "text/plain"
        elif (self.path == "/wire/form-array" and
                has_generated_headers(self.headers) and
                self.headers.get_content_type() == "application/x-www-form-urlencoded" and
                request_body == b"quantity=5&quantity=-2&active=False"):
            status, body = 200, b"wire-form-array-ok"
            response_type = "text/plain"
        elif (self.path == "/wire/form-roles" and
                has_generated_headers(self.headers) and
                self.headers.get_content_type() == "application/x-www-form-urlencoded" and
                request_body == b"role=admin+user&role=a%26b"):
            status, body = 200, b"wire-form-roles-ok"
            response_type = "text/plain"
        elif (self.path == "/wire/form-roles-const-list" and
                has_generated_headers(self.headers) and
                self.headers.get_content_type() == "application/x-www-form-urlencoded" and
                request_body == b"role=admin+user&role=a%26b"):
            status, body = 200, b"wire-form-roles-const-list-ok"
            response_type = "text/plain"
        elif (self.path == "/wire/form-roles-queue" and
                has_generated_headers(self.headers) and
                self.headers.get_content_type() == "application/x-www-form-urlencoded" and
                request_body == b"role=admin+user&role=a%26b"):
            status, body = 200, b"wire-form-roles-ok"
            response_type = "text/plain"
        elif (self.path == "/wire/form-roles-const-queue" and
                has_generated_headers(self.headers) and
                self.headers.get_content_type() == "application/x-www-form-urlencoded" and
                request_body == b"role=admin+user&role=a%26b"):
            status, body = 200, b"wire-form-roles-const-queue-ok"
            response_type = "text/plain"
        elif (self.path == "/wire/form-roles-stack" and
                has_generated_headers(self.headers) and
                self.headers.get_content_type() == "application/x-www-form-urlencoded" and
                request_body == b"role=a%26b&role=admin+user"):
            status, body = 200, b"wire-form-roles-stack-ok"
            response_type = "text/plain"
        elif (self.path == "/wire/stream" and
                has_generated_headers(self.headers) and
                self.headers.get_content_type() == "application/octet-stream" and
                request_body == bytes(index % 251 for index in range(256 * 1024))):
            status, body = 200, b"wire-stream-ok"
            response_type = "text/plain"
        elif (self.path == EXPECTED_TARGET and
                has_generated_headers(self.headers) and
                self.headers.get_content_type() == "application/json" and
                request_body == expected_body):
            status = 200
            body = b'{"Name":"saved","Quantity":5}'
            response_type = "application/json"
        else:
            print(
                f"unmatched POST {self.path}: "
                f"headers={dict(self.headers.items())!r} body={request_body!r}",
                file=sys.stderr,
            )
            status, body = 400, b"bad request"
            response_type = "text/plain"
        self.send_response(status)
        self.send_header("X-CX-Wire", "generated")
        self.send_header("Content-Type", response_type)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(body)

    def _do_verb(self, expected: str) -> None:
        valid = (
            self.path == f"/wire/verbs/{expected.lower()}"
            and has_generated_headers(self.headers)
        )
        status = 200 if valid else 400
        body = expected.encode("ascii") if valid and expected != "HEAD" else b""
        self.send_response(status)
        self.send_header("X-CX-Wire", "generated")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Connection", "close")
        self.end_headers()
        if body:
            self.wfile.write(body)

    def do_PUT(self) -> None:  # noqa: N802
        self._do_verb("PUT")

    def do_PATCH(self) -> None:  # noqa: N802
        self._do_verb("PATCH")

    def do_DELETE(self) -> None:  # noqa: N802
        self._do_verb("DELETE")

    def do_HEAD(self) -> None:  # noqa: N802
        self._do_verb("HEAD")

    def do_OPTIONS(self) -> None:  # noqa: N802
        self._do_verb("OPTIONS")

    def log_message(self, _format: str, *_args: object) -> None:
        pass


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("executable")
    args = parser.parse_args()

    STREAM_RELEASED.clear()
    REST_CONCURRENT_SLOW_STARTED.clear()
    REST_CONCURRENT_FAST_STARTED.clear()
    REST_CANCEL_STREAM_CLOSED.clear()
    REST_DISPOSE_STREAM_CLOSED.clear()
    server = QuietThreadingHTTPServer(("127.0.0.1", 0), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        address = f"http://127.0.0.1:{server.server_port}/"
        result = subprocess.run([args.executable, address], check=False)
        if result.returncode != 0:
            print(f"generated REST wire fixture exited {result.returncode}", file=sys.stderr)
            return result.returncode or 1
        if not REST_CANCEL_STREAM_CLOSED.wait(timeout=2):
            print("generated REST cancellation did not close the body connection",
                  file=sys.stderr)
            return 1
        if not REST_DISPOSE_STREAM_CLOSED.wait(timeout=2):
            print("disposing a generated REST stream did not close the body connection",
                  file=sys.stderr)
            return 1
        return 0
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)


if __name__ == "__main__":
    raise SystemExit(main())

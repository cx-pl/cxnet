#!/usr/bin/env python3
"""Exercise nullable unsigned REST map responses against loopback HTTP."""

from __future__ import annotations

from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import subprocess
import sys
from threading import Thread


UNSIGNED_BODY = (
    b'{"zero":0,"maximum":18446744073709551615,"missing":null}'
)
COLLECTION_TYPES = {
    "numbers": "list",
    "numbers-array": "array",
    "numbers-linked-list": "linked-list",
    "numbers-queue": "queue",
    "numbers-stack": "stack",
}
NULLABLE_ELEMENT_COLLECTION_TYPES = {
    "nullable-elements-list": "list",
    "nullable-elements-array": "array",
    "nullable-elements-linked-list": "linked-list",
    "nullable-elements-queue": "queue",
    "nullable-elements-stack": "stack",
}
DIRECT_COLLECTION_BODY = b'{"numbers":[3,1,4],"empty":[]}'
WRAPPED_COLLECTION_BODY = b'{"numbers":[2,7,1],"empty":[]}'
DIRECT_NULLABLE_ELEMENT_BODY = b'{"numbers":[5,null,8],"empty":[]}'
WRAPPED_NULLABLE_ELEMENT_BODY = b'{"numbers":[1,null,3],"empty":[]}'
DIRECT_NULLABLE_LIST_BODY = (
    b'{"numbers":[5,8,13],"empty":[],"missing":null}'
)
WRAPPED_NULLABLE_LIST_BODY = (
    b'{"numbers":[1,2,3],"empty":[],"missing":null}'
)
DIRECT_NULLABLE_DTO_MAP_BODY = (
    b'{"item":{"Name":"map item","Quantity":11},"missing":null}'
)
WRAPPED_NULLABLE_DTO_MAP_BODY = (
    b'{"item":{"Name":"map item","Quantity":23},"missing":null}'
)


class Handler(BaseHTTPRequestHandler):
    def do_GET(self) -> None:
        self.server.request_count += 1
        collection_route = self.path.removeprefix("/")
        wrapped = collection_route.endswith("-response")
        if wrapped:
            collection_route = collection_route.removesuffix("-response")
        if collection_route in COLLECTION_TYPES:
            status = 202 if wrapped else 200
            fixture_header = (
                f"{COLLECTION_TYPES[collection_route]}-map" if wrapped else None
            )
            body = (
                WRAPPED_COLLECTION_BODY if wrapped else DIRECT_COLLECTION_BODY
            )
        elif collection_route == "nullable-numbers":
            status = 202 if wrapped else 200
            fixture_header = "nullable-list-map" if wrapped else None
            body = (
                WRAPPED_NULLABLE_LIST_BODY
                if wrapped else DIRECT_NULLABLE_LIST_BODY
            )
        elif collection_route in NULLABLE_ELEMENT_COLLECTION_TYPES:
            collection_type = NULLABLE_ELEMENT_COLLECTION_TYPES[
                collection_route
            ]
            status = 202 if wrapped else 200
            fixture_header = (
                f"nullable-{collection_type}-elements-map" if wrapped else None
            )
            body = (
                WRAPPED_NULLABLE_ELEMENT_BODY
                if wrapped else DIRECT_NULLABLE_ELEMENT_BODY
            )
        elif self.path == "/unsigned-map":
            status = 200
            fixture_header = None
            body = UNSIGNED_BODY
        elif self.path == "/unsigned-map-response":
            status = 202
            fixture_header = "unsigned-map"
            body = UNSIGNED_BODY
        elif self.path == "/nullable-dto-map":
            status = 200
            fixture_header = None
            body = DIRECT_NULLABLE_DTO_MAP_BODY
        elif self.path == "/nullable-dto-map-response":
            status = 202
            fixture_header = "nullable-dto-map"
            body = WRAPPED_NULLABLE_DTO_MAP_BODY
        else:
            self.send_error(404)
            return

        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        if fixture_header is not None:
            self.send_header("X-Fixture", fixture_header)
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, format: str, *args: object) -> None:
        pass


class CountingHTTPServer(ThreadingHTTPServer):
    def __init__(self, address, handler):
        super().__init__(address, handler)
        self.request_count = 0


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: run_wire.py <RestMapCollectionProbe>", file=sys.stderr)
        return 2

    executable = Path(sys.argv[1]).resolve(strict=True)
    server = CountingHTTPServer(("127.0.0.1", 0), Handler)
    thread = Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        base_address = f"http://127.0.0.1:{server.server_port}/"
        result = subprocess.run(
            [str(executable), base_address],
            cwd=executable.parent,
            check=False,
            timeout=30,
        )
        if result.returncode != 0:
            return result.returncode
        if server.request_count != 26:
            print(
                f"expected twenty-six REST requests, received {server.request_count}",
                file=sys.stderr,
            )
            return 1
        print("collection and nullable REST map checks passed")
        return 0
    except subprocess.TimeoutExpired:
        print("REST map probe timed out after 30 seconds", file=sys.stderr)
        return 1
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)


if __name__ == "__main__":
    raise SystemExit(main())

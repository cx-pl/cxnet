#!/usr/bin/env python3
"""Run the generated REST CRUD executable against a local HTTP/1.1 server."""

from __future__ import annotations

import argparse
import json
import subprocess
import threading
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path


class ProductsServer(HTTPServer):
    def __init__(self, address: tuple[str, int]):
        super().__init__(address, ProductsHandler)
        self.events: list[tuple[str, str, bytes]] = []
        self.errors: list[str] = []


class ProductsHandler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def _body(self) -> bytes:
        length = int(self.headers.get("Content-Length", "0"))
        return self.rfile.read(length)

    def _respond(self, status: int, body: bytes = b"") -> None:
        self.send_response(status)
        if body:
            self.send_header("Content-Type", "application/json")
        if status != 204:
            self.send_header("Content-Length", str(len(body)))
        self.send_header("Connection", "close")
        self.end_headers()
        if body:
            self.wfile.write(body)
        self.close_connection = True

    def _record(self, method: str, body: bytes) -> None:
        self.server.events.append((method, self.path, body))

    def do_POST(self) -> None:
        body = self._body()
        self._record("POST", body)
        try:
            product = json.loads(body)
            valid = (
                self.path == "/products"
                and self.headers.get_content_type() == "application/json"
                and product == {"Id": 0, "Name": "desk lamp"}
            )
        except (UnicodeDecodeError, json.JSONDecodeError, TypeError):
            valid = False
        if not valid:
            self.server.errors.append(f"invalid POST /products: {body!r}")
            self._respond(400)
            return
        self._respond(200, b'{"Id":41,"Name":"desk lamp"}')

    def do_GET(self) -> None:
        body = self._body()
        self._record("GET", body)
        if self.path == "/products" and not body:
            self._respond(200, b'[{"Id":41,"Name":"desk lamp"}]')
            return
        if self.path == "/products/uuid" and not body:
            self._respond(200, b'"00112233-4455-6677-8899-aabbccddeeff"')
            return
        if self.path == "/products/by-id/00112233-4455-6677-8899-aabbccddeeff" and not body:
            self._respond(200)
            return
        if self.path != "/products/41" or body:
            self.server.errors.append(f"invalid GET {self.path}: {body!r}")
            self._respond(400)
            return
        self._respond(200, b'{"Id":41,"Name":"desk lamp"}')

    def do_PUT(self) -> None:
        body = self._body()
        self._record("PUT", body)
        try:
            product = json.loads(body)
            valid = (
                self.path == "/products/41"
                and self.headers.get_content_type() == "application/json"
                and product == {"Id": 41, "Name": "reading lamp"}
            )
        except (UnicodeDecodeError, json.JSONDecodeError, TypeError):
            valid = False
        if not valid:
            self.server.errors.append(f"invalid PUT {self.path}: {body!r}")
            self._respond(400)
            return
        self._respond(200, b'{"Id":41,"Name":"reading lamp"}')

    def do_DELETE(self) -> None:
        body = self._body()
        self._record("DELETE", body)
        if self.path != "/products/41" or body:
            self.server.errors.append(f"invalid DELETE {self.path}: {body!r}")
            self._respond(400)
            return
        self._respond(204)

    def log_message(self, format: str, *args: object) -> None:
        pass


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", type=Path)
    args = parser.parse_args()
    executable = args.executable.resolve()
    if not executable.is_file():
        parser.error(f"executable not found: {executable}")

    server = ProductsServer(("127.0.0.1", 0))
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        address = f"http://127.0.0.1:{server.server_port}/"
        result = subprocess.run(
            [str(executable), address],
            check=False,
            capture_output=True,
            text=True,
            timeout=90,
        )
        if result.returncode != 0:
            raise RuntimeError(
                f"REST example exited {result.returncode}\n"
                f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}"
            )
        expected_methods = [
            "POST", "GET", "GET", "GET", "GET", "PUT", "DELETE"
        ]
        observed_methods = [event[0] for event in server.events]
        if server.errors or observed_methods != expected_methods:
            raise RuntimeError(
                f"wire fixture failed: errors={server.errors!r}, "
                f"events={server.events!r}, stdout={result.stdout!r}, "
                f"stderr={result.stderr!r}"
            )
        print("REST CRUD HTTP wire fixture pass")
        return 0
    finally:
        server.shutdown()
        thread.join(timeout=5)
        server.server_close()


if __name__ == "__main__":
    raise SystemExit(main())

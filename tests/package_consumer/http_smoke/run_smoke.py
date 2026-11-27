#!/usr/bin/env python3
"""Run the packaged HttpClient against local HTTP and HTTPS responses."""

from __future__ import annotations

from http.server import BaseHTTPRequestHandler, HTTPServer
import os
from pathlib import Path
import ssl
import subprocess
import sys
from threading import Thread


BODY = b"m26-package-http-ok"


class Handler(BaseHTTPRequestHandler):
    def do_GET(self) -> None:
        self.server.request_count += 1
        if self.path != "/package-smoke":
            self.send_error(404)
            return
        self.send_response(200)
        self.send_header("Content-Length", str(len(BODY)))
        self.end_headers()
        self.wfile.write(BODY)

    def log_message(self, format: str, *args: object) -> None:
        pass


class CountingHTTPServer(HTTPServer):
    def __init__(self, address, handler):
        super().__init__(address, handler)
        self.request_count = 0

    def handle_error(self, _request, _client_address):
        pass


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: run_smoke.py <M26PackageHttpSmoke>", file=sys.stderr)
        return 2

    executable = Path(sys.argv[1]).resolve(strict=True)
    environment = os.environ.copy()
    environment.pop("LD_LIBRARY_PATH", None)
    fixture_dir_candidates = (
        Path(__file__).resolve().parent / "fixtures" / "https",
        Path(__file__).resolve().parents[2] / "fixtures" / "https",
    )
    fixture_dir = next(
        (path for path in fixture_dir_candidates if path.is_dir()), None)
    if fixture_dir is None:
        print("HTTPS fixture certificates are missing", file=sys.stderr)
        return 2
    ca_file = fixture_dir / "test-ca.pem"
    server = CountingHTTPServer(("127.0.0.1", 0), Handler)
    https_server = CountingHTTPServer(("127.0.0.1", 0), Handler)
    tls_context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    tls_context.load_cert_chain(
        fixture_dir / "localhost.pem", fixture_dir / "localhost-key.pem")
    https_server.socket = tls_context.wrap_socket(
        https_server.socket, server_side=True)
    threads = [
        Thread(target=instance.serve_forever, daemon=True)
        for instance in (server, https_server)
    ]
    for thread in threads:
        thread.start()
    try:
        cases = (
            [str(executable),
             f"http://127.0.0.1:{server.server_port}/package-smoke",
             BODY.decode("ascii"), ""],
            [str(executable),
             f"https://localhost:{https_server.server_port}/package-smoke",
             BODY.decode("ascii"), ""],
            [str(executable),
             f"https://localhost:{https_server.server_port}/package-smoke",
             BODY.decode("ascii"), str(ca_file)],
            [str(executable),
             f"https://127.0.0.1:{https_server.server_port}/package-smoke",
             BODY.decode("ascii"), str(ca_file),
             "expect-tls-failure"],
        )
        for command in cases:
            result = subprocess.run(
                command,
                cwd=executable.parent,
                env=environment,
                check=False,
            )
            if result.returncode != 0:
                return result.returncode
        if server.request_count != 1 or https_server.request_count != 2:
            print(
                "HTTPS failure cases unexpectedly reached the local server: "
                f"HTTP requests={server.request_count}, "
                f"HTTPS requests={https_server.request_count}",
                file=sys.stderr,
            )
            return 6
        return 0
    finally:
        for instance in (https_server, server):
            instance.shutdown()
        for thread in threads:
            thread.join()
        for instance in (https_server, server):
            instance.server_close()


if __name__ == "__main__":
    raise SystemExit(main())

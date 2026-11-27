#!/usr/bin/env python3
"""Run the managed HTTP e2e binary with a local HTTPS certificate fixture."""

from __future__ import annotations

import os
from pathlib import Path
import subprocess
import sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import ssl
import threading
import time


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def do_GET(self) -> None:  # noqa: N802 - BaseHTTPRequestHandler API
        if self.path == "/downgrade":
            time.sleep(0.12)
            self.send_response(302)
            self.send_header("Location", self.server.redirect_target)
            self.send_header("Content-Length", "0")
            self.send_header("Connection", "close")
            self.end_headers()
            self.close_connection = True
            return
        body = b"pong"
        self.send_response(200)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Connection", "close")
        self.end_headers()
        try:
            self.wfile.write(body)
        except (ConnectionError, OSError):
            # Expected when the client rejects the certificate or hostname.
            pass
        self.close_connection = True

    def log_message(self, _format: str, *_args: object) -> None:
        pass


class QuietThreadingHTTPServer(ThreadingHTTPServer):
    daemon_threads = True

    def handle_error(self, _request: object, _client_address: object) -> None:
        # TLS verification rejection closes the socket before an HTTP request.
        pass


def main() -> int:
    if len(sys.argv) < 2:
        print("usage: https_fixture.py <managed-http-e2e-executable> [arguments...]",
            file=sys.stderr)
        return 2

    fixture_dir = Path(__file__).resolve().parent / "fixtures" / "https"
    servers: list[QuietThreadingHTTPServer] = []
    threads: list[threading.Thread] = []

    def start_server(cert_name: str, key_name: str) -> QuietThreadingHTTPServer:
        server = QuietThreadingHTTPServer(("127.0.0.1", 0), Handler)
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.load_cert_chain(
            certfile=fixture_dir / cert_name,
            keyfile=fixture_dir / key_name,
        )
        server.socket = context.wrap_socket(server.socket, server_side=True)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        servers.append(server)
        threads.append(thread)
        return server

    server = start_server("localhost.pem", "localhost-key.pem")
    expired_server = start_server(
        "expired-localhost.pem", "expired-localhost-key.pem")

    redirect_target = QuietThreadingHTTPServer(("127.0.0.1", 0), Handler)
    redirect_thread = threading.Thread(target=redirect_target.serve_forever, daemon=True)
    redirect_thread.start()
    server.redirect_target = f"http://127.0.0.1:{redirect_target.server_address[1]}/final"
    servers.append(redirect_target)
    threads.append(redirect_thread)

    port = server.server_address[1]
    environment = os.environ.copy()
    environment.update(
        {
            "CXNET_TEST_HTTPS_URL": f"https://localhost:{port}/tls",
            "CXNET_TEST_HTTPS_WRONG_HOST_URL": f"https://127.0.0.1:{port}/tls",
            "CXNET_TEST_HTTPS_EXPIRED_URL":
                f"https://localhost:{expired_server.server_address[1]}/tls",
            "CXNET_TEST_HTTPS_CA_FILE": str(fixture_dir / "test-ca.pem"),
            "CXNET_TEST_HTTPS_UNTRUSTED_CA_FILE": str(fixture_dir / "untrusted-ca.pem"),
            "CXNET_TEST_HTTPS_DOWNGRADE_URL": f"https://localhost:{server.server_address[1]}/downgrade",
            "CXNET_TEST_HTTPS_DOWNGRADE_CA_FILE": str(fixture_dir / "test-ca.pem"),
        }
    )
    try:
        return subprocess.run([sys.argv[1], *sys.argv[2:]], env=environment,
            check=False).returncode
    finally:
        for active_server in servers:
            active_server.shutdown()
            active_server.server_close()
        for active_thread in threads:
            active_thread.join(timeout=5)


if __name__ == "__main__":
    raise SystemExit(main())

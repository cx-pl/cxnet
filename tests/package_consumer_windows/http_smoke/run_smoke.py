#!/usr/bin/env python3
"""Run a Windows package-only HttpClient against a local HTTP response."""

from __future__ import annotations

from http.server import BaseHTTPRequestHandler, HTTPServer
import os
from pathlib import Path
import ssl
import subprocess
import sys
from threading import Thread


BODY = b"m26-windows-package-http-ok"


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

    def handle_error(self, _request, _client_address) -> None:
        pass


def run_case(command: list[str], executable: Path, environment: dict[str, str]) -> int:
    result = subprocess.run(
        command,
        cwd=executable.parent,
        env=environment,
        check=False,
    )
    return result.returncode


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: run_smoke.py <M26WindowsPackageHttpSmoke.exe>", file=sys.stderr)
        return 2

    executable = Path(sys.argv[1]).resolve(strict=True)
    if sys.platform == "win32":
        windows_root = os.environ.get(
            "SystemRoot", os.environ.get("WINDIR", r"C:\Windows"))
        environment = {
            "PATH": str(Path(windows_root) / "System32") + ";" + windows_root,
            "SystemRoot": windows_root,
            "WINDIR": windows_root,
        }
    else:
        environment = {"PATH": "/usr/bin:/bin", "LD_LIBRARY_PATH": ""}
    server = CountingHTTPServer(("127.0.0.1", 0), Handler)
    fixture_dir = Path(__file__).resolve().parents[2] / "fixtures" / "https"
    ca_file = fixture_dir / "test-ca.pem"
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
             BODY.decode("ascii")],
            [str(executable),
             f"https://localhost:{https_server.server_port}/package-smoke",
             BODY.decode("ascii"), str(ca_file)],
            [str(executable),
             f"https://127.0.0.1:{https_server.server_port}/package-smoke",
             BODY.decode("ascii"), str(ca_file), "expect-tls-failure"],
        )
        for command in cases:
            result = run_case(command, executable, environment)
            if result != 0:
                return result
        if server.request_count != 1 or https_server.request_count != 1:
            print(
                "unexpected local request count: "
                f"HTTP={server.request_count}, HTTPS={https_server.request_count}",
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

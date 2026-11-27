#!/usr/bin/env python3
"""Exercise the restored REST and CXDATA packages over loopback HTTP."""

from __future__ import annotations

from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import subprocess
import sys
from threading import Thread


BODY = b'{"Name":"package-rest-json-ok","Count":27}'


class Handler(BaseHTTPRequestHandler):
    def do_GET(self) -> None:
        self.server.request_count += 1
        if self.path != "/package-json":
            self.send_error(404)
            return
        self.send_response(200)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(BODY)))
        self.end_headers()
        self.wfile.write(BODY)

    def log_message(self, format: str, *args: object) -> None:
        pass


class CountingHTTPServer(ThreadingHTTPServer):
    def __init__(self, address, handler):
        super().__init__(address, handler)
        self.request_count = 0


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: run_smoke.py <M26PackageRestJsonSmoke>", file=sys.stderr)
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
        )
        if result.returncode != 0:
            return result.returncode
        if server.request_count != 1:
            print(
                f"expected one REST request, received {server.request_count}",
                file=sys.stderr,
            )
            return 1
        return 0
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)


if __name__ == "__main__":
    raise SystemExit(main())

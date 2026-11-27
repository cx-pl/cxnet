#!/usr/bin/env python3
"""Run the CX HTTP download executable against a delayed local response."""

from __future__ import annotations

import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import subprocess
import tempfile
import threading
import time


BODY = bytes(index % 251 for index in range(262144))


class DownloadFixture(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def do_GET(self) -> None:
        if self.path != "/payload":
            self.send_error(404)
            return
        self.send_response(200)
        self.send_header("Content-Length", str(len(BODY)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.close_connection = True
        time.sleep(0.2)
        for offset in range(0, len(BODY), 8192):
            self.wfile.write(BODY[offset : offset + 8192])
            self.wfile.flush()
            time.sleep(0.001)

    def log_message(self, _format: str, *_args: object) -> None:
        pass


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)

    server = ThreadingHTTPServer(("127.0.0.1", 0), DownloadFixture)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        with tempfile.TemporaryDirectory(prefix="cx-http-download-") as directory:
            destination = Path(directory) / "download.bin"
            url = f"http://127.0.0.1:{server.server_port}/payload"
            result = subprocess.run(
                [str(executable), url, str(destination)],
                check=False,
                capture_output=True,
                text=True,
                timeout=60,
            )
            if result.returncode != 0:
                raise RuntimeError(
                    f"download exited {result.returncode}:\n"
                    f"{result.stdout}{result.stderr}"
                )
            actual = destination.read_bytes()
            if actual != BODY:
                raise RuntimeError(
                    f"download mismatch: expected {len(BODY)} bytes, "
                    f"received {len(actual)}"
                )
            print(f"HTTP download fixture passed ({len(actual)} bytes)")
            if result.stdout.strip():
                print(result.stdout.strip())
        return 0
    finally:
        server.shutdown()
        thread.join()
        server.server_close()


if __name__ == "__main__":
    raise SystemExit(main())

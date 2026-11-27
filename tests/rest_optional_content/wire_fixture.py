from __future__ import annotations

import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import subprocess
import sys
import threading


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def do_POST(self) -> None:  # noqa: N802
        length_header = self.headers.get("Content-Length")
        length = int(length_header or "0")
        body = self.rfile.read(length)
        common = (
            self.path == "/optional-content"
            and self.headers.get("Authorization") == "Bearer optional-token"
            and self.headers.get("X-Optional-Content") == "present"
            and self.headers.get("Content-Type") is None
            and self.headers.get("Transfer-Encoding") is None
        )
        omitted = common and length_header == "0" and body == b""
        populated = (
            common
            and length_header == "3"
            and body == b"\x00\x7f\xff"
        )
        status = 200 if omitted or populated else 400
        if status != 200:
            print(f"unexpected request: headers={dict(self.headers.items())!r} body={body!r}",
                  file=sys.stderr)
        payload = b"ok" if status == 200 else b"bad request"
        self.send_response(status)
        self.send_header("Content-Length", str(len(payload)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(payload)

    def log_message(self, _format: str, *_args: object) -> None:
        pass


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("executable")
    args = parser.parse_args()
    server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        url = f"http://127.0.0.1:{server.server_port}/"
        result = subprocess.run([args.executable, url], check=False)
        if result.returncode != 0:
            print(f"nullable HttpContent fixture exited {result.returncode}",
                  file=sys.stderr)
            return result.returncode or 1
        return 0
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)


if __name__ == "__main__":
    raise SystemExit(main())

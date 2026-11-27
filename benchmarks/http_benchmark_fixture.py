#!/usr/bin/env python3
"""Run the opt-in managed HTTP benchmark against a deterministic loopback server."""

import argparse
import os
import platform
import subprocess
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


class Fixture(ThreadingHTTPServer):
    daemon_threads = True
    allow_reuse_address = True

    def __init__(self, address, handler):
        super().__init__(address, handler)
        self.count_lock = threading.Lock()
        self.connection_count = 0
        self.requests = 0
        self.body_bytes = 1024 * 1024

    def get_request(self):
        request, address = super().get_request()
        with self.count_lock:
            self.connection_count += 1
        return request, address


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def setup(self):
        super().setup()

    def do_GET(self):
        with self.server.count_lock:
            self.server.requests += 1
        if self.path == "/body":
            try:
                size = self.server.body_bytes
            except ValueError:
                self.send_error(400)
                return
            if size < 1 or size > 64 * 1024 * 1024:
                self.send_error(400)
                return
            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Length", str(size))
            self.end_headers()
            try:
                offset = 0
                while offset < size:
                    length = min(65536, size - offset)
                    self.wfile.write(bytes(((offset + i) * 31 + 17) & 255
                                           for i in range(length)))
                    offset += length
            except (BrokenPipeError, ConnectionResetError):
                pass
            return
        if self.path == "/cancel":
            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Transfer-Encoding", "chunked")
            self.end_headers()
            try:
                self.wfile.write(b"4\r\npong\r\n")
                self.wfile.flush()
                while True:
                    time.sleep(0.05)
                    self.wfile.write(b"4\r\nping\r\n")
                    self.wfile.flush()
            except (BrokenPipeError, ConnectionResetError, OSError):
                pass
            return
        self.send_error(404)

    def log_message(self, _format, *_args):
        pass

    def finish(self):
        try:
            super().finish()
        except (BrokenPipeError, ConnectionResetError, OSError):
            pass

    def handle(self):
        try:
            super().handle()
        except (ConnectionAbortedError, ConnectionResetError, OSError):
            pass


def main():
    parser = argparse.ArgumentParser()
    modes = parser.add_subparsers(dest="mode", required=True)
    run = modes.add_parser("run", help="run the benchmark with a local fixture")
    run.add_argument("executable")
    run.add_argument("requests", nargs="?", default="20")
    run.add_argument("samples", nargs="?", default="5")
    run.add_argument("body_bytes", nargs="?", default="1048576")
    serve = modes.add_parser("serve", help="host the fixture for a remote benchmark")
    serve.add_argument("--host", default="127.0.0.1")
    serve.add_argument("--port", type=int, default=0)
    serve.add_argument("--body-bytes", type=int, default=1024 * 1024)
    argv = sys.argv[1:]
    # Keep the original `fixture.py <executable> [requests samples bytes]` form.
    if not argv or argv[0] not in ("run", "serve"):
        argv.insert(0, "run")
    args = parser.parse_args(argv)
    if args.mode == "serve":
        if args.port < 0 or args.port > 65535:
            parser.error("--port must be between 0 and 65535")
        if args.body_bytes < 1 or args.body_bytes > 64 * 1024 * 1024:
            parser.error("--body-bytes must be between 1 and 67108864")
        return run_server(args.host, args.port, args.body_bytes)

    server = Fixture(("127.0.0.1", 0), Handler)
    server.body_bytes = int(args.body_bytes)
    worker = threading.Thread(target=server.serve_forever, daemon=True)
    worker.start()
    env = os.environ.copy()
    env["CXNET_HTTP_BENCH_BASE_URL"] = "http://127.0.0.1:%d" % server.server_port
    env["CXNET_HTTP_BENCH_BODY_BYTES"] = args.body_bytes
    result = subprocess.run([args.executable, args.requests, args.samples,
                             args.body_bytes], env=env, check=False)
    server.shutdown()
    server.server_close()
    worker.join(timeout=2)
    with server.count_lock:
        connections = server.connection_count
        requests = server.requests
    reused = max(0, requests - connections)
    print("fixture_requests=%d fixture_connections=%d pooled_reuses=%d" %
          (requests, connections, reused), file=sys.stderr)
    return result.returncode


def run_server(host, port, body_bytes):
    """Serve deterministic benchmark routes until interrupted."""
    server = Fixture((host, port), Handler)
    server.body_bytes = body_bytes
    bound_host, bound_port = server.server_address[:2]
    print("server_host=%s server_os=%s listening_on=%s:%d body_bytes=%d" %
          (platform.node(), platform.platform(), bound_host, bound_port,
           body_bytes), flush=True)
    worker = threading.Thread(target=server.serve_forever, daemon=True)
    worker.start()
    try:
        worker.join()
    except KeyboardInterrupt:
        server.shutdown()
        worker.join(timeout=5)
    finally:
        if worker.is_alive():
            server.shutdown()
            worker.join(timeout=5)
        server.server_close()
        with server.count_lock:
            connections = server.connection_count
            requests = server.requests
        reused = max(0, requests - connections)
        print("fixture_requests=%d fixture_connections=%d pooled_reuses=%d" %
              (requests, connections, reused), file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

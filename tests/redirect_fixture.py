#!/usr/bin/env python3
"""Run managed HttpClient redirect checks against two local HTTP origins."""

from __future__ import annotations

import base64
import os
import select
import socket
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import ssl
import subprocess
import sys
import threading
import time


class TargetHandler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def do_GET(self) -> None:  # noqa: N802 - BaseHTTPRequestHandler API
        if self.path == "/slow-cancel":
            time.sleep(0.5)
            try:
                self._write(b"cancelled-response")
            except ConnectionError:
                pass
            return
        if self.path == "/cookie-target":
            body = b"cookie-ok" if "redirectcookie=ok" in self.headers.get("Cookie", "") else b"cookie-missing"
            self._write(body)
            return
        cookies = self.headers.get("Cookie", "")
        clean = (not self.headers.get("Authorization") and
                 "redirect-secret" not in cookies and
                 "default-secret" not in cookies)
        body = b"clean" if clean else b"leaked"
        self._write(body)

    def _write(self, body: bytes) -> None:
        self.send_response(200)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(body)
        self.close_connection = True

    def log_message(self, _format: str, *_args: object) -> None:
        pass


class OriginHandler(TargetHandler):
    def _read_fixed_body(self) -> bytes:
        length = int(self.headers.get("Content-Length", "0"))
        return self.rfile.read(length)

    def do_POST(self) -> None:  # noqa: N802 - BaseHTTPRequestHandler API
        if self.path == "/precanceled-upload":
            self.server.precanceled_upload_seen.set()
            self._write(b"unexpected-upload")

    def do_GET(self) -> None:  # noqa: N802 - BaseHTTPRequestHandler API
        if self.path == "/scheduler-shutdown":
            self.server.scheduler_shutdown_started.set()
            try:
                self.send_response(200)
                self.send_header("Content-Length", "1048576")
                self.send_header("Connection", "close")
                self.end_headers()
                self.wfile.write(b"held")
                self.wfile.flush()
                self.connection.settimeout(5.0)
                self.server.scheduler_shutdown_closed = (
                    self.connection.recv(1) == b""
                )
            except (ConnectionError, OSError):
                self.server.scheduler_shutdown_closed = True
            finally:
                self.server.scheduler_shutdown_observed.set()
            self.close_connection = True
        elif self.path == "/slow-cancel":
            time.sleep(0.5)
            try:
                self._write(b"cancelled-response")
            except ConnectionError:
                pass
        elif self.path in ("/concurrent-cancel/one", "/concurrent-cancel/two",
                           "/concurrent-cancel-dispose/one",
                           "/concurrent-cancel-dispose/two"):
            if self.path.startswith("/concurrent-cancel-dispose/"):
                self.server.concurrent_dispose_started_paths.add(self.path)
            else:
                self.server.concurrent_cancel_started_paths.add(self.path)
                if len(self.server.concurrent_cancel_started_paths) == 2:
                    self.server.concurrent_cancel_started.set()
            self.send_response(200)
            self.send_header("Content-Length", "1048576")
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(b"pong")
            self.wfile.flush()
            self.connection.settimeout(5.0)
            try:
                closed = self.connection.recv(1) == b""
            except (ConnectionError, OSError):
                closed = True
            if self.path.startswith("/concurrent-cancel-dispose/"):
                self.server.concurrent_dispose_closed_paths[self.path] = closed
                if len(self.server.concurrent_dispose_closed_paths) == 2:
                    self.server.concurrent_dispose_observed.set()
            else:
                self.server.concurrent_cancel_closed_paths[self.path] = closed
            if len(self.server.concurrent_cancel_closed_paths) == 2:
                self.server.concurrent_cancel_observed.set()
            self.close_connection = True
        elif self.path == "/stream-cancel":
            self.server.stream_cancel_started.set()
            self.send_response(200)
            self.send_header("Content-Length", "1048576")
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(b"pong")
            self.wfile.flush()
            self.connection.settimeout(5.0)
            try:
                try:
                    received = self.connection.recv(1)
                    self.server.stream_cancel_closed = received == b""
                    self.server.stream_cancel_result = repr(received)
                except ConnectionError as error:
                    # Cancellation may close with a reset instead of an orderly FIN.
                    self.server.stream_cancel_closed = True
                    self.server.stream_cancel_result = (
                        type(error).__name__ + ": " + str(error))
                except OSError as error:
                    self.server.stream_cancel_closed = False
                    self.server.stream_cancel_result = (
                        type(error).__name__ + ": " + str(error))
            finally:
                self.server.stream_cancel_observed.set()
            self.close_connection = True
        elif self.path == "/cross-origin":
            self.send_response(302)
            self.send_header("Location", self.server.cross_origin_url)
            self.send_header("Content-Length", "0")
            self.send_header("Connection", "close")
            self.end_headers()
            self.close_connection = True
        elif self.path == "/ipv6-alias":
            self._redirect(self.server.ipv6_same_origin_url)
        elif self.path == "/ipv6-same-origin":
            body = (b"ipv6-same-origin" if
                    self.headers.get("Authorization") ==
                    "Bearer ipv6-alias-secret" else b"ipv6-credentials-stripped")
            self._write(body)
        elif self.path == "/cookie-redirect":
            self.send_response(302)
            self.send_header("Location", "/cookie-target")
            self.send_header("Set-Cookie", "redirectcookie=ok; Path=/; Max-Age=60")
            self.send_header("Content-Length", "0")
            self.send_header("Connection", "close")
            self.end_headers()
            self.close_connection = True
        elif self.path == "/cookie-target":
            body = b"cookie-ok" if "redirectcookie=ok" in self.headers.get("Cookie", "") else b"cookie-missing"
            self._write(body)
        elif self.path == "/loop":
            self.send_response(302)
            self.send_header("Location", "/loop")
            self.send_header("Content-Length", "0")
            self.send_header("Connection", "close")
            self.end_headers()
            self.close_connection = True
        elif self.path == "/relative":
            self.send_response(302)
            self.send_header("Location", "/final")
            self.send_header("Content-Length", "0")
            self.send_header("Connection", "close")
            self.end_headers()
            self.close_connection = True
        elif self.path == "/policy-snapshot-allow":
            time.sleep(0.12)
            self._redirect("/final")
        elif self.path == "/policy-snapshot-max-one":
            time.sleep(0.12)
            self._redirect("/policy-snapshot-max-two")
        elif self.path == "/policy-snapshot-max-two":
            self._redirect("/final")
        elif self.path == "/policy-race-allow":
            time.sleep(0.12)
            self._redirect("/final")
        elif self.path == "/policy-race-max-one":
            time.sleep(0.12)
            self._redirect("/policy-race-max-two")
        elif self.path == "/policy-race-max-two":
            self._redirect("/final")
        elif self.path == "/slow-redirect":
            self.send_response(302)
            self.send_header("Location", "/slow-final")
            self.send_header("Content-Length", "0")
            self.send_header("Connection", "close")
            self.end_headers()
            self.close_connection = True
        elif self.path == "/slow-final":
            time.sleep(0.5)
            self._write(b"too-late")
        elif self.path == "/timeout-1":
            time.sleep(0.12)
            self._redirect("/timeout-2")
        elif self.path == "/timeout-2":
            time.sleep(0.25)
            self._redirect("/timeout-final")
        elif self.path == "/timeout-final":
            self._write(b"too-late")
        elif self.path == "/final":
            self._write(b"redirect-ok")
        elif self.path == "/echo":
            body = b"GET:"
            if self.headers.get("Content-Type"):
                body += b"length-header-present"
            self._write(body)
        else:
            self._write(b"redirect-ok")

    def _redirect(self, location: str) -> None:
        self.send_response(302)
        self.send_header("Location", location)
        self.send_header("Content-Length", "0")
        self.send_header("Connection", "close")
        self.end_headers()
        self.close_connection = True

    def do_POST(self) -> None:  # noqa: N802 - BaseHTTPRequestHandler API
        if self.headers.get("Transfer-Encoding", "").lower() == "chunked":
            chunks = []
            while True:
                line = self.rfile.readline()
                size = int(line.strip().split(b";", 1)[0], 16)
                if size == 0:
                    while self.rfile.readline() not in (b"\r\n", b"\n", b""):
                        pass
                    break
                chunk = self.rfile.read(size)
                if self.rfile.read(2) != b"\r\n":
                    raise ValueError("malformed request chunk")
                chunks.append(chunk)
            body = b"".join(chunks)
        else:
            length = int(self.headers.get("Content-Length", "0"))
            body = self.rfile.read(length)
        if self.path == "/echo-post":
            self._write(b"POST:" + body)
            return
        if self.path == "/post-307":
            status, location = 307, "/echo-post"
        elif self.path == "/post-303":
            status, location = 303, "/echo"
        else:
            status, location = 302, "/echo"
        self.send_response(status)
        self.send_header("Location", location)
        self.send_header("Content-Length", "0")
        self.send_header("Connection", "close")
        self.end_headers()
        self.close_connection = True

    def do_PUT(self) -> None:  # noqa: N802 - BaseHTTPRequestHandler API
        if self.path == "/echo-put":
            self._write(b"PUT:" + self._read_fixed_body())
            return
        self._write(b"unexpected-put")

    def do_PATCH(self) -> None:  # noqa: N802 - BaseHTTPRequestHandler API
        if self.path == "/echo-patch":
            self._write(b"PATCH:" + self._read_fixed_body())
            return
        self._write(b"unexpected-patch")

    def do_DELETE(self) -> None:  # noqa: N802 - BaseHTTPRequestHandler API
        if self.path == "/delete-echo":
            self._write(b"DELETE:empty")
            return
        self._write(b"unexpected-delete")

    def do_HEAD(self) -> None:  # noqa: N802 - BaseHTTPRequestHandler API
        if self.path == "/head-echo":
            self.send_response(200)
            self.send_header("Content-Type", "text/plain")
            self.send_header("Content-Length", "4")
            self.send_header("X-Head-Route", "passed")
            self.send_header("Connection", "close")
            self.end_headers()
            self.close_connection = True
            return
        self.send_error(404)


class QuietServer(ThreadingHTTPServer):
    daemon_threads = True


class QuietIPv6Server(QuietServer):
    address_family = socket.AF_INET6


class QuietTlsServer(QuietServer):
    def handle_error(self, _request: object, _client_address: object) -> None:
        # Certificate rejection closes TLS before an HTTP request is parsed.
        pass


class TlsHandler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def do_GET(self) -> None:  # noqa: N802 - BaseHTTPRequestHandler API
        body = b"pong"
        self.send_response(200)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(body)
        self.close_connection = True

    def log_message(self, _format: str, *_args: object) -> None:
        pass


class ProxyHandler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def do_GET(self) -> None:  # noqa: N802 - BaseHTTPRequestHandler API
        if "proxy-auth-" not in self.path:
            self._write(b"proxy-ok")
            return
        if self.path.endswith("/proxy-auth-redirect"):
            self._write_redirect("http://cxnet-proxy-auth-target.invalid/resource")
            return
        expected = "Basic " + base64.b64encode(b"proxy-user:proxy-secret").decode("ascii")
        if self.headers.get("Proxy-Authorization") != expected:
            self.send_response(407)
            self.send_header("Proxy-Authenticate", 'Basic realm="cxnet-test"')
            self.send_header("Content-Length", "0")
            self.send_header("Connection", "close")
            self.end_headers()
            self.close_connection = True
            return
        if self.headers.get("Authorization"):
            self._write(b"proxy-authorization-leaked")
            return
        self._write(b"proxy-auth-ok")

    def do_CONNECT(self) -> None:  # noqa: N802 - BaseHTTPRequestHandler API
        expected = f"localhost:{self.server.https_origin_port}"
        if self.path != expected:
            self.send_error(502)
            return
        upstream = socket.create_connection(
            ("127.0.0.1", self.server.https_origin_port), timeout=5.0
        )
        try:
            self.send_response(200, "Connection Established")
            self.end_headers()
            self.wfile.flush()
            sockets = [self.connection, upstream]
            while True:
                readable, _, exceptional = select.select(sockets, [], sockets, 10.0)
                if exceptional or not readable:
                    break
                for source in readable:
                    data = source.recv(65536)
                    if not data:
                        return
                    destination = upstream if source is self.connection else self.connection
                    destination.sendall(data)
        except (OSError, ssl.SSLError):
            pass
        finally:
            upstream.close()

    def _write(self, body: bytes) -> None:
        self.send_response(200)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(body)
        self.close_connection = True

    def _write_redirect(self, location: str) -> None:
        self.send_response(302)
        self.send_header("Location", location)
        self.send_header("Content-Length", "0")
        self.send_header("Connection", "close")
        self.end_headers()
        self.close_connection = True

    def log_message(self, _format: str, *_args: object) -> None:
        pass


def main() -> int:
    flags = set(sys.argv[1:-1])
    supported_flags = {
        "--require-stream-cancel",
        "--require-concurrent-cancel",
        "--require-concurrent-dispose",
        "--require-precanceled-upload",
        "--require-scheduler-shutdown",
    }
    executable = sys.argv[-1] if len(sys.argv) >= 2 and flags <= supported_flags else None
    require_stream_cancel = "--require-stream-cancel" in flags
    require_concurrent_cancel = "--require-concurrent-cancel" in flags
    require_concurrent_dispose = "--require-concurrent-dispose" in flags
    require_precanceled_upload = "--require-precanceled-upload" in flags
    require_scheduler_shutdown = "--require-scheduler-shutdown" in flags
    if executable is None:
        print("usage: redirect_fixture.py [--require-stream-cancel] "
            "[--require-concurrent-cancel] [--require-concurrent-dispose] "
            "[--require-precanceled-upload] [--require-scheduler-shutdown] "
            "<managed-http-executable>", file=sys.stderr)
        return 2

    target = QuietServer(("127.0.0.1", 0), TargetHandler)
    target_thread = threading.Thread(target=target.serve_forever, daemon=True)
    target_thread.start()
    origin = QuietServer(("127.0.0.1", 0), OriginHandler)
    origin.cross_origin_url = f"http://127.0.0.1:{target.server_address[1]}/credentials"
    origin.stream_cancel_closed = False
    origin.stream_cancel_result = "not observed"
    origin.stream_cancel_started = threading.Event()
    origin.stream_cancel_observed = threading.Event()
    origin.concurrent_cancel_started_paths = set()
    origin.concurrent_cancel_closed_paths = {}
    origin.concurrent_cancel_started = threading.Event()
    origin.concurrent_cancel_observed = threading.Event()
    origin.concurrent_dispose_started_paths = set()
    origin.concurrent_dispose_closed_paths = {}
    origin.concurrent_dispose_observed = threading.Event()
    origin.precanceled_upload_seen = threading.Event()
    origin.scheduler_shutdown_started = threading.Event()
    origin.scheduler_shutdown_observed = threading.Event()
    origin.scheduler_shutdown_closed = False
    origin_thread = threading.Thread(target=origin.serve_forever, daemon=True)
    origin_thread.start()
    ipv6_origin = QuietIPv6Server(("::1", 0), OriginHandler)
    ipv6_origin.ipv6_same_origin_url = (
        f"http://[::1]:{ipv6_origin.server_address[1]}/ipv6-same-origin"
    )
    ipv6_origin_thread = threading.Thread(
        target=ipv6_origin.serve_forever, daemon=True
    )
    ipv6_origin_thread.start()
    proxy = QuietServer(("127.0.0.1", 0), ProxyHandler)
    proxy_thread = threading.Thread(target=proxy.serve_forever, daemon=True)
    proxy_thread.start()
    https_server = QuietTlsServer(("127.0.0.1", 0), TlsHandler)
    https_directory = Path(__file__).resolve().parent / "fixtures" / "https"
    tls_context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    tls_context.load_cert_chain(
        certfile=https_directory / "localhost.pem",
        keyfile=https_directory / "localhost-key.pem",
    )
    https_server.socket = tls_context.wrap_socket(https_server.socket, server_side=True)
    https_thread = threading.Thread(target=https_server.serve_forever, daemon=True)
    https_thread.start()
    https_proxy = QuietTlsServer(("127.0.0.1", 0), ProxyHandler)
    https_proxy.https_origin_port = https_server.server_address[1]
    proxy_tls_context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    proxy_tls_context.load_cert_chain(
        certfile=https_directory / "localhost.pem",
        keyfile=https_directory / "localhost-key.pem",
    )
    https_proxy.socket = proxy_tls_context.wrap_socket(
        https_proxy.socket, server_side=True
    )
    https_proxy_thread = threading.Thread(target=https_proxy.serve_forever, daemon=True)
    https_proxy_thread.start()
    environment = os.environ.copy()
    environment["NO_PROXY"] = "127.0.0.1,localhost,::1,[::1]"
    environment["no_proxy"] = environment["NO_PROXY"]
    environment["CXNET_TEST_REDIRECT_BASE_URL"] = (
        f"http://127.0.0.1:{origin.server_address[1]}"
    )
    environment["CXNET_TEST_IPV6_REDIRECT_URL"] = (
        f"http://[0:0:0:0:0:0:0:1]:{ipv6_origin.server_address[1]}/ipv6-alias"
    )
    environment["CXNET_TEST_HTTP_PROXY_URL"] = (
        f"http://127.0.0.1:{proxy.server_address[1]}"
    )
    environment["CXNET_TEST_HTTPS_PROXY_URL"] = (
        f"https://localhost:{https_proxy.server_address[1]}"
    )
    environment["CXNET_TEST_HTTPS_PROXY_WRONG_HOST_URL"] = (
        f"https://127.0.0.1:{https_proxy.server_address[1]}"
    )
    environment["CXNET_TEST_HTTPS_URL"] = (
        f"https://localhost:{https_server.server_address[1]}/tls"
    )
    environment["CXNET_TEST_HTTPS_WRONG_HOST_URL"] = (
        f"https://127.0.0.1:{https_server.server_address[1]}/tls"
    )
    environment["CXNET_TEST_HTTPS_CA_FILE"] = str(https_directory / "test-ca.pem")
    try:
        arguments = [executable]
        if require_scheduler_shutdown:
            arguments.append("--scheduler-shutdown")
        result = subprocess.run(arguments, env=environment, check=False)
        if require_scheduler_shutdown and result.returncode == 0:
            if not origin.scheduler_shutdown_started.wait(timeout=2.0):
                print("scheduler shutdown fixture did not receive the request",
                    file=sys.stderr)
                return 1
            origin.scheduler_shutdown_observed.wait(timeout=6.0)
            if not origin.scheduler_shutdown_closed:
                print("scheduler shutdown did not close the active HTTP connection",
                    file=sys.stderr)
                return 1
        if require_stream_cancel and result.returncode == 0:
            origin.stream_cancel_started.wait(timeout=2.0)
            origin.stream_cancel_observed.wait(timeout=6.0)
        if (require_stream_cancel and result.returncode == 0 and
                not origin.stream_cancel_closed):
            print("headers-first cancellation did not close the streamed connection",
                file=sys.stderr)
            print(f"request started: {origin.stream_cancel_started.is_set()}",
                file=sys.stderr)
            print(f"server observation: {origin.stream_cancel_result}", file=sys.stderr)
            return 1
        if require_concurrent_cancel and result.returncode == 0:
            origin.concurrent_cancel_started.wait(timeout=3.0)
            origin.concurrent_cancel_observed.wait(timeout=6.0)
            expected = {"/concurrent-cancel/one", "/concurrent-cancel/two"}
            if (origin.concurrent_cancel_started_paths != expected or
                    origin.concurrent_cancel_closed_paths != {path: True for path in expected}):
                print("concurrent cancellation did not close both streamed connections",
                    file=sys.stderr)
                print(f"started: {sorted(origin.concurrent_cancel_started_paths)}",
                    file=sys.stderr)
                print(f"closed: {origin.concurrent_cancel_closed_paths}", file=sys.stderr)
                return 1
        if require_concurrent_dispose and result.returncode == 0:
            origin.concurrent_dispose_observed.wait(timeout=6.0)
            expected = {
                "/concurrent-cancel-dispose/one",
                "/concurrent-cancel-dispose/two",
            }
            if origin.concurrent_dispose_closed_paths != {
                path: True for path in expected
            }:
                print("cancel/dispose overlap did not close both concurrent streams",
                    file=sys.stderr)
                print(f"closed: {origin.concurrent_dispose_closed_paths}",
                    file=sys.stderr)
                return 1
        if require_precanceled_upload and result.returncode == 0:
            if origin.precanceled_upload_seen.wait(timeout=0.25):
                print("pre-canceled upload reached the loopback server",
                    file=sys.stderr)
                return 1
        return result.returncode
    finally:
        for server in (origin, ipv6_origin, target, proxy, https_server, https_proxy):
            server.shutdown()
            server.server_close()
        for thread in (origin_thread, ipv6_origin_thread, target_thread,
                proxy_thread, https_thread, https_proxy_thread):
            thread.join(timeout=5)


if __name__ == "__main__":
    raise SystemExit(main())

from __future__ import annotations

import socket
import subprocess
import sys


def recv_headers(connection: socket.socket) -> tuple[bytes, bytes]:
    data = bytearray()
    while b"\r\n\r\n" not in data:
        chunk = connection.recv(4096)
        if not chunk:
            raise RuntimeError("connection closed before request headers")
        data.extend(chunk)
    split = data.index(b"\r\n\r\n") + 4
    return bytes(data[:split]), bytes(data[split:])


def main() -> int:
    if len(sys.argv) != 2:
        return 2

    listener = socket.socket()
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listener.bind(("127.0.0.1", 0))
    listener.listen(2)
    listener.settimeout(10)
    port = listener.getsockname()[1]
    process = subprocess.Popen(
        [sys.argv[1], "--source-failure",
         f"http://127.0.0.1:{port}/upload",
         f"http://127.0.0.1:{port}/recovery"],
        stdin=subprocess.PIPE,
        text=True,
    )
    upload = None
    recovery = None
    try:
        upload, _ = listener.accept()
        upload.settimeout(10)
        headers, body = recv_headers(upload)
        if not headers.startswith(b"POST /upload HTTP/1.1\r\n"):
            raise RuntimeError(f"unexpected upload request: {headers!r}")
        while b"M26!" not in body:
            chunk = upload.recv(4096)
            if not chunk:
                raise RuntimeError("upload closed before the expected body prefix")
            body += chunk

        assert process.stdin is not None
        process.stdin.write("\n")
        process.stdin.flush()
        process.stdin.close()

        try:
            while upload.recv(4096):
                pass
        except (ConnectionResetError, OSError):
            pass
        upload.close()
        upload = None

        recovery, _ = listener.accept()
        recovery.settimeout(10)
        recovery_headers, _ = recv_headers(recovery)
        if not recovery_headers.startswith(b"GET /recovery HTTP/1.1\r\n"):
            raise RuntimeError(f"unexpected recovery request: {recovery_headers!r}")
        response = b"recovered"
        recovery.sendall(
            b"HTTP/1.1 200 OK\r\n"
            b"Content-Length: 9\r\n"
            b"Connection: close\r\n\r\n" + response
        )
        result = process.wait(timeout=15)
        if result != 0:
            print(f"upload source failure fixture exited with {result}")
            return result
        return 0
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
        print(f"upload source failure fixture did not recover cleanly: {error}")
        return 1
    finally:
        if process.poll() is None:
            process.kill()
            process.wait()
        if upload is not None:
            upload.close()
        if recovery is not None:
            recovery.close()
        listener.close()


if __name__ == "__main__":
    raise SystemExit(main())

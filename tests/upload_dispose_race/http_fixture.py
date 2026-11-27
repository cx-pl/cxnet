from __future__ import annotations

import socket
import subprocess
import sys


def main() -> int:
    if len(sys.argv) != 2:
        return 2

    listener = socket.socket()
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listener.bind(("127.0.0.1", 0))
    listener.listen(1)
    listener.settimeout(10)
    address = f"http://127.0.0.1:{listener.getsockname()[1]}/upload"
    process = subprocess.Popen(
        [sys.argv[1], address], stdin=subprocess.PIPE, text=True
    )
    connection = None
    try:
        connection, _ = listener.accept()
        process.stdin.write("\n")
        process.stdin.flush()
        process.stdin.close()
        result = process.wait(timeout=10)
        if result != 0:
            print(f"CX upload race fixture exited with {result}")
            return result

        connection.settimeout(5)
        while True:
            try:
                if connection.recv(4096) == b"":
                    return 0
            except ConnectionResetError:
                return 0
    except (OSError, subprocess.TimeoutExpired) as error:
        print(f"upload disposal fixture did not close cleanly: {error}")
        return 1
    finally:
        if process.poll() is None:
            process.kill()
            process.wait()
        if connection is not None:
            connection.close()
        listener.close()


if __name__ == "__main__":
    raise SystemExit(main())

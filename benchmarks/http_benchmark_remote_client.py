#!/usr/bin/env python3
"""Collect repeatable raw results for a split-host CX HTTP benchmark."""

import argparse
import csv
import hashlib
import io
import os
import platform
import re
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path
from urllib.parse import urlsplit


def positive_int(value):
    try:
        parsed = int(value)
    except ValueError as exc:
        raise argparse.ArgumentTypeError("must be an integer") from exc
    if parsed < 1:
        raise argparse.ArgumentTypeError("must be positive")
    return parsed


def parse_result(stdout, stderr, returncode, args):
    problems = []
    try:
        rows = list(csv.DictReader(io.StringIO(stdout)))
    except csv.Error as exc:
        rows = []
        problems.append(f"CSV parse failed: {exc}")

    if len(rows) != args.samples:
        problems.append(f"expected {args.samples} CSV samples, got {len(rows)}")
    for index, row in enumerate(rows):
        if row.get("sample") != str(index + 1):
            problems.append(f"sample {index} has an unexpected index")
        if row.get("requests") != str(args.requests):
            problems.append(f"sample {index} has an unexpected request count")
        expected_sample_bytes = args.requests * args.body_bytes
        if row.get("body_bytes") != str(expected_sample_bytes):
            problems.append(f"sample {index} has an unexpected body size")

    cancel = re.search(r"^cancel_latency_ns=(\d+),prefix_bytes=(\d+)$",
                       stderr, re.MULTILINE)
    if cancel is None:
        problems.append("cancellation latency or four-byte prefix is missing")
        cancel_latency_us = None
    else:
        cancel_latency_us = int(cancel.group(1)) / 1000.0
        if int(cancel.group(2)) != 4:
            problems.append("cancellation benchmark did not receive four bytes")

    heap = re.search(r"^post_dispose_heap_bytes=(\d+)$", stderr, re.MULTILINE)
    if heap is None:
        problems.append("post-disposal heap measurement is missing")

    if returncode != 0:
        problems.append(f"benchmark exited with status {returncode}")
    return rows, cancel_latency_us, (heap.group(1) if heap else None), problems


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path,
                        help="native cxnet_http_benchmarks executable")
    parser.add_argument("--base-url", required=True,
                        help="reachable split-fixture URL, e.g. http://host:8080")
    parser.add_argument("--output-dir", required=True, type=Path,
                        help="new or empty directory for raw output and summary")
    parser.add_argument("--build-label", default="unspecified",
                        help="compiler, target, link mode, and build configuration")
    parser.add_argument("--runs", type=positive_int, default=5)
    parser.add_argument("--requests", type=positive_int, default=10)
    parser.add_argument("--samples", type=positive_int, default=3)
    parser.add_argument("--body-bytes", type=positive_int, default=262144)
    parser.add_argument("--timeout-seconds", type=positive_int, default=600)
    args = parser.parse_args()

    executable = args.executable.resolve()
    if not executable.is_file():
        parser.error(f"benchmark executable does not exist: {executable}")
    if args.body_bytes > 64 * 1024 * 1024:
        parser.error("--body-bytes must not exceed 67108864")
    parsed_url = urlsplit(args.base_url)
    if parsed_url.scheme != "http" or not parsed_url.hostname or parsed_url.port is None:
        parser.error("--base-url must be an http URL with an explicit host and port")
    if parsed_url.username or parsed_url.password or parsed_url.query or parsed_url.fragment:
        parser.error("--base-url must not contain credentials, query, or fragment")

    output_dir = args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    if any(output_dir.iterdir()):
        parser.error(f"output directory must be empty: {output_dir}")

    executable_hash = hashlib.sha256(executable.read_bytes()).hexdigest()
    environment = os.environ.copy()
    environment["CXNET_HTTP_BENCH_BASE_URL"] = args.base_url.rstrip("/")
    environment["CXNET_HTTP_BENCH_BODY_BYTES"] = str(args.body_bytes)
    results = []

    for number in range(1, args.runs + 1):
        command = [str(executable), str(args.requests), str(args.samples),
                   str(args.body_bytes)]
        try:
            completed = subprocess.run(
                command, env=environment, text=True, encoding="utf-8",
                errors="replace", capture_output=True,
                timeout=args.timeout_seconds, check=False)
            stdout = completed.stdout
            stderr = completed.stderr
            returncode = completed.returncode
        except subprocess.TimeoutExpired as exc:
            stdout = exc.stdout or ""
            stderr = exc.stderr or ""
            if isinstance(stdout, bytes):
                stdout = stdout.decode("utf-8", errors="replace")
            if isinstance(stderr, bytes):
                stderr = stderr.decode("utf-8", errors="replace")
            stderr += f"\nremote runner timeout after {args.timeout_seconds} seconds\n"
            returncode = 124

        stem = f"run-{number:02d}"
        (output_dir / f"{stem}.csv").write_text(stdout, encoding="utf-8")
        (output_dir / f"{stem}.stderr.log").write_text(stderr, encoding="utf-8")
        rows, cancellation_us, heap_bytes, problems = parse_result(
            stdout, stderr, returncode, args)
        results.append((stem, rows, cancellation_us, heap_bytes, problems))
        print(f"{stem}: " + ("PASS" if not problems else "FAIL: " + "; ".join(problems)),
              flush=True)

    host = parsed_url.hostname
    try:
        host_label = f"{host}:{parsed_url.port}"
    except ValueError:
        host_label = host
    lines = [
        "# Split-host HTTP benchmark client results",
        "",
        f"- Collected (UTC): {datetime.now(timezone.utc).isoformat()}",
        f"- Client host: `{platform.node()}`",
        f"- Client OS: `{platform.platform()}`",
        f"- Python: `{platform.python_version()}`",
        f"- Fixture endpoint: `{host_label}`",
        f"- Build label: `{args.build_label}`",
        f"- Executable: `{executable.name}`",
        f"- Executable SHA-256: `{executable_hash}`",
        f"- Per-run request/sample/body settings: {args.requests}/{args.samples}/{args.body_bytes}",
        "",
        "| Run | Throughput samples (MiB/s) | Cancellation (µs) | Post-disposal heap (bytes) | Result |",
        "| --- | --- | ---: | ---: | --- |",
    ]
    for stem, rows, cancellation_us, heap_bytes, problems in results:
        rates = ", ".join(row.get("body_mib_per_second", "?") for row in rows)
        cancel_text = f"{cancellation_us:.3f}" if cancellation_us is not None else "missing"
        heap_text = heap_bytes or "missing"
        result_text = "PASS" if not problems else "FAIL"
        lines.append(f"| {stem} | {rates} | {cancel_text} | {heap_text} | {result_text} |")

    lines.extend([
        "",
        f"Expected fixture accounting for {args.runs} runs: "
        f"{args.runs * (args.requests * args.samples + 3)} requests, "
        f"{args.runs * 2} connections, "
        f"{args.runs * (args.requests * args.samples + 1)} inferred pooled reuses. "
        "Compare this with the fixture server's final accounting line.",
        "",
        "Raw CSV and stderr for every invocation are saved beside this report.",
    ])
    all_problems = [problem for result in results for problem in result[4]]
    if all_problems:
        lines.extend(["", "## Validation failures", ""])
        lines.extend(f"- {problem}" for problem in all_problems)
    (output_dir / "summary.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"summary: {output_dir / 'summary.md'}")
    return 1 if all_problems else 0


if __name__ == "__main__":
    raise SystemExit(main())

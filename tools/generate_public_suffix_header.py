#!/usr/bin/env python3
"""Generate the compact C rule tables used by CookieContainer."""

from __future__ import annotations

import re
import sys
from pathlib import Path


def c_string(value: str) -> str:
    encoded = value.encode("utf-8")
    pieces = []
    for byte in encoded:
        if 0x20 <= byte <= 0x7E and byte not in (ord('"'), ord("\\")):
            pieces.append(chr(byte))
        elif byte == ord('"'):
            pieces.append(r'\"')
        elif byte == ord("\\"):
            pieces.append(r"\\")
        else:
            pieces.append(f"\\{byte:03o}")
    return '"' + "".join(pieces) + '"'


def emit_array(name: str, rules: list[str]) -> list[str]:
    output = [f"static const char *const {name}[] = {{"]
    output.extend(f"    {c_string(rule)}," for rule in rules)
    output.append("};")
    output.append(f"#define {name.upper()}_COUNT {len(rules)}u")
    return output


def main() -> None:
    if len(sys.argv) != 3:
        raise SystemExit("usage: generate_public_suffix_header.py INPUT.dat OUTPUT.h")
    source = Path(sys.argv[1])
    target = Path(sys.argv[2])
    text = source.read_text(encoding="utf-8")
    version_match = re.search(r"^// VERSION: (.+)$", text, re.MULTILINE)
    commit_match = re.search(r"^// COMMIT: (.+)$", text, re.MULTILINE)
    if not version_match or not commit_match:
        raise SystemExit("input does not contain the upstream PSL version and commit")

    exact: set[str] = set()
    wildcard: set[str] = set()
    exception: set[str] = set()
    for raw_line in text.splitlines():
        rule = raw_line.strip().lower()
        if not rule or rule.startswith("//"):
            continue
        if rule.startswith("!"):
            exception.add(rule[1:])
        elif rule.startswith("*."):
            wildcard.add(rule[2:])
        else:
            exact.add(rule)

    def ordered(values: set[str]) -> list[str]:
        return sorted(values, key=lambda value: value.encode("utf-8"))

    lines = [
        "/* Generated from public_suffix_list.dat; do not edit by hand.",
        " * Source: https://publicsuffix.org/list/public_suffix_list.dat",
        f" * VERSION: {version_match.group(1)}",
        f" * COMMIT: {commit_match.group(1)}",
        " * The source list is distributed under the Mozilla Public License 2.0.",
        " */",
        "#ifndef CXNET_PUBLIC_SUFFIX_RULES_H",
        "#define CXNET_PUBLIC_SUFFIX_RULES_H",
        "",
    ]
    lines += emit_array("cxnet_psl_exact", ordered(exact))
    lines.append("")
    lines += emit_array("cxnet_psl_wildcard", ordered(wildcard))
    lines.append("")
    lines += emit_array("cxnet_psl_exception", ordered(exception))
    lines += ["", "#endif", ""]
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text("\n".join(lines), encoding="utf-8", newline="\n")


if __name__ == "__main__":
    main()

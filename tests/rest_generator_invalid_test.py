"""Compile invalid REST fixtures and verify their source diagnostics."""

from pathlib import Path
import re
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[2]
COMPILER = ROOT / "cxc/src/CxCompiler/bin/Release/net10.0/CxCompiler.dll"
INVALID_PROJECT = ROOT / (
    "cxnet/tests/rest_generator_invalid/invalid-rest-contract.cxproj"
)
OPEN_GENERIC_PROJECT = ROOT / (
    "cxnet/tests/rest_generator_open_generic_invalid/"
    "open-generic-rest-contract.cxproj"
)


def compile_fixture(project: Path) -> str:
    result = subprocess.run(
        [
            "dotnet",
            str(COMPILER),
            str(project),
            "--emit-only",
            "--configuration",
            "Release",
        ],
        cwd=ROOT,
        capture_output=True,
        text=True,
        timeout=180,
        check=False,
    )
    output = result.stdout + result.stderr
    if result.returncode == 0:
        raise AssertionError(f"{project.name} unexpectedly compiled successfully:\n{output}")
    return output


def verify_invalid_contract() -> None:
    output = compile_fixture(INVALID_PROJECT)
    expected_counts = {
        "CXREST001": 3,
        "CXREST010": 4,
        "CXREST014": 7,
        "CXREST019": 1,
        "CXREST020": 1,
        "CXREST022": 7,
    }
    found_counts = {
        code: len(re.findall(rf"\b{code}\b", output))
        for code in expected_counts
    }
    if found_counts != expected_counts:
        raise AssertionError(
            f"REST diagnostic counts differ: expected {expected_counts}, "
            f"found {found_counts}:\n{output}"
        )
    located_diagnostics = re.findall(
        r"Program\.cx\(\d+,\d+\): CXREST\d{3}:", output
    )
    if len(located_diagnostics) != sum(expected_counts.values()):
        raise AssertionError(f"REST diagnostics lack source locations:\n{output}")
    if "generated output was not accepted" not in output:
        raise AssertionError(f"generator failure was not reported:\n{output}")


def verify_open_generic_request() -> None:
    output = compile_fixture(OPEN_GENERIC_PROJECT)
    expected = re.compile(
        r"Program\.cx\(\d+,\d+\): error: Cannot resolve generic argument 'T'"
    )
    if expected.search(output) is None:
        raise AssertionError(f"missing source diagnostic for open generic contract:\n{output}")


def main() -> int:
    try:
        verify_invalid_contract()
        print("invalid REST contract diagnostics: PASS")
        verify_open_generic_request()
        print("open generic REST contract diagnostic: PASS")
    except (AssertionError, subprocess.SubprocessError) as error:
        print(error, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

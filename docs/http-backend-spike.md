# HTTP backend spike: libcurl 8.23.0

## Pinned source and build matrix

The selected candidate is libcurl 8.23.0 from the official source archive. The
archive SHA-256 and signing-key fingerprint are in
[`libcurl-8.23.0.lock`](libcurl-8.23.0.lock). GPG verification succeeded against
Daniel Stenberg's key with fingerprint
`27ED EAF2 2F3A BCEB 50DB 9A12 5CC9 08FD B71E 12C2`. Upstream identifies the
release as dated 2026-10-14 and lists the archive and its signature on the
[release archive page](https://curl.se/download/) and [verification guide](https://curl.se/docs/verify.html).
HTTP content decoding also uses static zlib 1.3.2, pinned by
[`zlib-1.3.2.lock`](zlib-1.3.2.lock); the official archive hash was verified
before building the Linux x64 and Windows x86/x64 variants. Brotli 1.2.1 and
zstd 1.5.7 are pinned by [`brotli-1.2.1.lock`](brotli-1.2.1.lock) and
[`zstd-1.5.7.lock`](zstd-1.5.7.lock); their source archive SHA-256 values were
verified before building the same three targets.

All builds used static libcurl, disabled the command-line tool and tests, and
disabled unused protocols. The required features were present in each CMake
configuration:

| Target | TLS provider | Resolver | Compression | Result |
|---|---|---|---|---|
| Linux x64, GCC 11.4 | OpenSSL 3.0.2 | threaded resolver | static zlib 1.3.2, Brotli 1.2.1, zstd 1.5.7 | Configure and static library build passed; CMake reported `AsynchDNS`, `SSL`, `IPv6`, `threadsafe`, `libz`, `brotli`, and `zstd` |
| Windows x64, MSVC 19.51 | Schannel | threaded resolver | static zlib 1.3.2, Brotli 1.2.1, zstd 1.5.7 | Configure and static library build passed; CMake reported `AsynchDNS`, `SSL`, `IPv6`, `threadsafe`, `libz`, `brotli`, and `zstd` |
| Windows x86, MSVC 19.51 | Schannel | threaded resolver | static zlib 1.3.2, Brotli 1.2.1, zstd 1.5.7 | Configure and static library build passed; CMake reported `AsynchDNS`, `SSL`, `IPv6`, `threadsafe`, `libz`, `brotli`, and `zstd` |

The [libcurl feature reference](https://curl.se/libcurl/c/curl_version_info.html)
defines `AsynchDNS` as asynchronous name lookup support that reduces blocking
with the multi interface. The [source build guide](https://curl.se/docs/install.html)
documents CMake builds and resolver choices. Linux uses the system CA bundle
and OpenSSL; Windows uses Schannel and the Windows certificate store. Before
each request, the handler rejects libcurl builds without
`CURL_VERSION_ASYNCHDNS`, `CURL_VERSION_SSL`, or either HTTP/HTTPS protocol;
it also requires `CURL_VERSION_LIBZ`, `CURL_VERSION_BROTLI`, and
`CURL_VERSION_ZSTD` when automatic decompression is enabled.

The source archive was verified locally with GPG and hashed locally to populate
the lock. The libcurl upstream license and copyright notices must be included
with any redistributed binary or source package. No curl executable is used.

The static curl, zlib, Brotli decoder/common, and zstd libraries, headers, and
license notices are included
in `src/binary/{win-x86,win-x64,linux-x64}` and selected by
[`src/cxnet.cxproj`](../src/cxnet.cxproj). `cxpm build` creates the package,
and restore places each target's native files below `bin/<rid>`. Packing,
publishing to a fresh local feed, and restoring all three RIDs have been
exercised. `package.dependencies` maps the source-only `cxcore` reference
to the published `cxcore` package. A fresh local feed restored both packages
transitively, and shared consumer modules compiled and linked against the
restored package assets on Windows x86/x64 and Linux x64. A fresh consumer that
constructs and disposes `HttpClient` was then staged with only its generated
module and loaded successfully on Windows x86/x64 and Linux x64. Windows uses
`LoadLibraryEx` restricted to the module directory and System32; Linux uses
`RTLD_NOW` with an empty `LD_LIBRARY_PATH`. The static curl and zlib assets
introduce no extra shared libraries. Windows still requires the installed VC
runtime, while Linux requires the matching system OpenSSL `libssl` and
`libcrypto` libraries; OpenSSL 3.0.2 through 4.0.3 have passed package checks.
Package-only build and isolated loading pass on pristine Debian 12 (glibc 2.36),
Debian 13.7 (glibc 2.41), Ubuntu 22.04 (glibc 2.35), Ubuntu 24.04, and Rocky
Linux 9.9 (glibc 2.34), using system OpenSSL 3; other pristine OS image/install
checks remain open. Exact compiler, package, and loader evidence is in
[`tests/package_consumer/README.md`](../tests/package_consumer/README.md).

The load check is reproducible by building the consumer against a fresh local
feed as above, copying only `m26consumer.dll` or `libm26consumer.so` into an
empty staging directory, and running the helper with a Python interpreter that
matches the module architecture:

```text
python cxnet/tests/package_runtime_load.py <staging-directory>/m26consumer.dll
python cxnet/tests/package_runtime_load.py <staging-directory>/libm26consumer.so
```

The helper starts a child with a restricted search path. On Windows it uses
`LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32`; on Linux it
clears `LD_LIBRARY_PATH` and loads with immediate symbol resolution.

The HTTPS fixture is `tests/https_fixture.py` with test-only certificates in
`tests/fixtures/https/`. It tests local-CA success, rejection under default
trust, wrong-host rejection, expired-certificate rejection, and rejection of an
unrelated CA. On Schannel the
fixture opts into best-effort handling only for unavailable revocation status;
peer-chain and hostname checks stay enabled. These cases pass on Linux x64 and
Windows x86/x64. The fixture private keys must not be used outside tests.

Reproduce the package flow by publishing `cxcore` before `cxnet`, since the
packed `cxnet` manifest depends on `cxcore@^0.1.0`:

```powershell
cxpm build --project cxcore/src/cxcore.cxproj
cxpm publish --project cxcore/src/cxcore.cxproj --feed .build/local-cx-feed
cxpm build --project cxnet/src/cxnet.cxproj
cxpm publish --project cxnet/src/cxnet.cxproj --feed .build/local-cx-feed
```

Create a consumer manifest with the local feed and `cxnet@^0.1.0` as a
dependency, then run `cxpm restore --target <rid>` and compile it with
`cxc --compile --cxcore-dir cxcore`. The consumer restores `cxcore` transitively.
For Linux x64, use CMake 3.31 or newer. `ldd -r` on the resulting shared
consumer resolves the bundled curl/zlib symbols and the host OpenSSL 3 libraries
in the Linux test environment.

## Multi socket action prototype

`spikes/http-backend/multi_socket_spike.c` uses libcurl's socket and timer
callbacks, `curl_multi_socket_action`, and a local TCP HTTP fixture. The URL
uses `localhost`, so the transfer exercises the configured resolver without
public DNS or internet access. The fixture verifies a streamed response body
and the build checks TLS plus asynchronous DNS feature bits. The prototype
builds and passes CTest on Linux x64 and Windows x86/x64 using the pinned
static libraries.

This prototype uses a small native `poll`/`WSAPoll` loop to isolate libcurl's
socket-action API. A second test, `cx_scheduler_multi_spike.c`, runs the same
local response through `cxnet_socket_post_on_readiness_mask`, its read/write/
error result mask, and cancelable CX scheduler timers. It uses
`curl_multi_socket_action` only from scheduler callbacks and returns the body
to the test after libcurl completes. It also runs 16 concurrent localhost
transfers with per-transfer DNS caching disabled and requires libcurl's resolver
start callback for every transfer. The transfers use one multi handle and are
served by a local listener, so the resolver-load test needs no public DNS or
internet access. This confirms that the threaded resolver starts distinct
lookups while the scheduler drives concurrent transfers. Five consecutive runs
pass on Windows x86/x64 and Linux x64 in static/shared mode; Linux also passes
strict GCC and Clang 14 builds. Earlier repeated single-transfer runs pass 10
times on Linux x64, 20 times on Windows x64, and 10 times on Windows x86 with
the pinned static curl builds. The spike still does not test HTTPS certificates,
streamed uploads, or broad cancellation races. A separate `cxnet_managed_http_e2e` target
tests the parameterless managed `HttpClient` against local fixtures; it verifies
GET/POST routing, POST body/content type, response status/body/header mapping,
active cancellation, configured timeout, and the configurable response buffer
limit on Linux x64 and Windows x86/x64. The limit regression sends a four-byte
body through a handler configured for three bytes and checks that the request
faults instead of returning an empty successful response. The handler defaults
to 64 MiB and accepts limits from 1 byte to 64 MiB. Response parsing faults
explicitly at 128 headers or 16 KiB of aggregate header bytes; 129-header and
17,000-byte fixtures verify both limits on Linux x64 and Windows x86/x64.
Additional fixtures verify PUT/PATCH upload bodies and content type, DELETE,
URI path/query escaping, truncated fixed-length and chunked response rejection,
successful chunked decoding with a trailer, close-delimited body decoding,
interim `100 Continue` followed by a final response, gzip decoding, and removal of stale
wire `Content-Encoding`/`Content-Length` fields after decompression. A stacked
double-gzip response with `Content-Encoding: gzip ,  gzip` verifies comma-token
and optional-whitespace handling; the decoded body and header cleanup pass in all
six Windows x86/x64 and Linux x64 static/shared cells. A highly
compressed 64-byte body exceeds a configured 32-byte limit after decoding and
faults, proving the bound applies to expanded bytes. These checks pass on Linux
x64 and Windows x86/x64. They also verify that HEAD does
not wait for the representation body despite its `Content-Length` header and that
a 204 response completes with empty content. `HeadAsync` and
`OptionsAsync` are exercised through the client convenience API.
`GetStringAsync`, `GetByteArrayAsync`, and `GetStreamAsync` are covered against
loopback requests. The string and byte helpers buffer before completion;
`GetStreamAsync` returns a response-owned live stream once headers arrive. A
delayed-body fixture verifies the stream task completes before body EOF and then
reads the payload and EOF on Linux x64 and Windows x86/x64. The curl callback
uses a bounded 64 KiB ring buffer with native pause/resume backpressure. A paced
HTTP/1.1 256 KiB loopback body crosses the ring capacity and verifies ordering/EOF
on Linux x64 and Windows x86/x64. This exposed a one-shot readiness rearm gap; the
curl bridge now rearms unchanged socket interest after each action unless the body
is paused. A truncated fixed-length response preserves its received prefix and
faults the next read on those targets. Conflicting duplicate response
`Content-Length` fields fault with the framing error in all six Windows x86/x64
and Linux x64 static/shared cells. `MaxResponseContentBufferSize` is a buffered
completion limit; a 256 KiB headers-first response still succeeds with a 4-byte
setting on Linux x64 and Windows x86/x64, with memory bounded by the 64 KiB queue.
A close-delimited headers-first stream fixture sends no `Content-Length`, delays
the `pong` payload, and closes the connection to signal EOF. It verifies
`GetStreamAsync` completes before the payload arrives, canceling a pending read
leaves the stream usable, the next read returns `pong`, and the final read observes
EOF. This case passes in all six Windows x86/x64 and Linux x64 static/shared
cells, and in strict Linux GCC and Clang 14 static/shared builds.
A fast 16 MiB headers-first fixture constrains the server send buffer and pauses
the consumer until the sender stalls. It then resumes the consumer, verifies all
patterned bytes through EOF, and completes in all six Windows x86/x64 and Linux
x64 static/shared cells. This found per-transfer socket-registration exhaustion
across libcurl remove/re-add events; detached registration records are now reused,
and the bridge waits for enough ring capacity before unpausing the paused chunk.
The full 12-test `cxnet` CTest suite passes in all six Windows x86/x64 and Linux
x64 static/shared cells.
Per-read cancellation is covered: canceling a pending read leaves the response
stream usable, verified on Linux x64 and Windows x86/x64. Request cancellation
after headers and a received body prefix cancels a pending read, keeps later
reads canceled rather than reporting EOF, and closes the transport. The managed
HTTP e2e passes three consecutive runs on Windows x64, Windows x86, and Linux
x64. The public API smoke fixture covers the same headers-first cancellation
through shared modules and passes three consecutive runs in all six Windows
x86/x64 and Linux x64 static/shared cells. Broader cancellation races remain open.
The public smoke now also starts two concurrent headers-first reads, cancels one
caller token, disposes their common `HttpClient`, then cancels the peer token
before scheduler dispatch. Both reads cancel and the server observes both
connections close. The original cancel/dispose ordering passed three consecutive
runs in all six static/shared cells; this peer-token-after-disposal ordering
passes in all six cells. Additional cancellation-order and upload races remain
open. The managed HTTP e2e also now runs a concurrent streaming-upload race:
one 8 MiB caller-token upload is canceled after partial transfer while a second
upload has sent its body and waits for a held response. The first transport
closes; releasing the peer response completes the second request. This passes
three consecutive runs in all six Windows x86/x64 and Linux x64 static/shared
cells, plus strict GCC and Clang 14.0.6 Linux x64 static/shared builds. Other
upload/cancellation-order races remain open. A second ordering fixture sends a
64 KiB `StreamContent` body completely and holds the response, then cancels the
caller token, disposes the client before scheduler dispatch, and cancels the
token again. It forces GC while the fully uploaded request is waiting for its
response and verifies that the task remains pending before cancellation. It
then verifies a canceled task and server-observed connection closure when both
owners request cleanup. This case passes in all six Windows x86/x64 and Linux
x64 static/shared cells, and in strict GCC and Clang 14.0.6 Linux x64
static/shared builds.

The managed HTTP e2e also covers cancellation while response headers are still
incomplete. A loopback fixture sends a status line, content length, and partial
header, then waits for the client to close. With `ResponseHeadersRead`, the
request task remains pending across forced GC; caller cancellation then cancels
the task, closes the connection, and invokes its completion callback exactly
once. The focused CTest passes in current-source Windows x86/x64 MSVC Release
static and shared builds, plus strict Linux x64 GCC 11.4 and Clang 14.0 static
and shared builds. Other cancellation orderings and native callback races
remain open.

## Remaining backend gates

The libcurl scheduler-integrated spike validates concurrent local HTTP
transfers, readiness replacement, timer cancellation, and 16 resolver-start
callbacks with the CX scheduler owning `curl_multi_socket_action` callbacks.
This exercises libcurl's asynchronous resolver under concurrent transfer load;
it does not measure resolver latency or exercise public-DNS servers. Streamed
uploads and cancellation during every native callback race remain open. Delayed
and paced download fixtures pass on Linux x64 and Windows x86/x64.
Restored-package compilation/linking passes on all supported RIDs.
Production code must detach
readiness registrations before libcurl closes or reuses a descriptor and marshal
all managed completion onto the CX scheduler owner thread.

The scheduler-integrated fixture can be reproduced after building the pinned
curl archives described above. It adds `cxcore` and `cxnet` as CMake subprojects;
`cxcore` tests are disabled, and `cxnet` tests are enabled when
`BUILD_TESTING=ON`:

```powershell
cmake -S cxnet/spikes/http-backend -B .build/m26-http-deps/spike-cx-win64 -A x64 `
  -DCURL_BUILD_DIR=$PWD/.build/m26-http-deps/build-win64 `
  -DZLIB_DIR=$PWD/.build/m26-http-deps/zlib-install-win64/lib/cmake/zlib `
  -DCX_BUILD_STATIC=ON -DBUILD_TESTING=ON
cmake --build .build/m26-http-deps/spike-cx-win64 --config Release --target cxnet_cx_scheduler_multi_spike
ctest --test-dir .build/m26-http-deps/spike-cx-win64 -C Release `
  -R cxnet_cx_scheduler_multi_spike --repeat until-fail:20 --output-on-failure
```

For the managed adapter fixture, replace the target and CTest name with
`cxnet_managed_http_e2e`:

```powershell
cmake --build .build/m26-http-deps/spike-cx-win64 --config Release --target cxnet_managed_http_e2e
ctest --test-dir .build/m26-http-deps/spike-cx-win64 -C Release `
  -R cxnet_managed_http_e2e --output-on-failure
```

Use `-A Win32`, `build-win32`, and the `zlib-install-win32` prefix for x86. On
Linux, configure with `CURL_BUILD_DIR` pointing to `build-linux` and
`ZLIB_DIR` pointing to `zlib-install-linux/lib/cmake/zlib`; then build the same
target and run CTest with `--repeat until-fail:10`.

## Reproduction

Download all four source archives in their lock files and verify their locked
SHA-256 values. Verify the curl `.asc` signature using its listed fingerprint.
Build and install zlib, Brotli, and zstd for each target before configuring
curl. Brotli uses these static-library switches:

```text
BUILD_SHARED_LIBS=OFF
BROTLI_BUILD_TOOLS=OFF
BROTLI_DISABLE_TESTS=ON
```

Build zstd with:

```text
ZSTD_BUILD_SHARED=OFF
ZSTD_BUILD_STATIC=ON
ZSTD_BUILD_PROGRAMS=OFF
ZSTD_BUILD_TESTS=OFF
```

Build zlib with:

```text
ZLIB_BUILD_TESTING=OFF
ZLIB_BUILD_SHARED=OFF
ZLIB_BUILD_STATIC=ON
ZLIB_INSTALL=ON
```

Use a separate install prefix for Linux, Windows x64, and Windows x86. Configure
curl from the extracted source with CMake 3.31 or newer, `CURL_ZLIB=ON`,
`CURL_BROTLI=ON`, and `CURL_ZSTD=ON`, supplying each dependency's include
directory and static libraries through `BROTLI_INCLUDE_DIR`,
`BROTLIDEC_LIBRARY`, `BROTLICOMMON_LIBRARY`, `ZSTD_INCLUDE_DIR`, and
`ZSTD_LIBRARY`. Set `ZSTD_USE_STATIC_LIBS=ON`.
`ZLIB_DIR=<prefix>/lib/cmake/zlib`, `ZLIB_INCLUDE_DIR=<prefix>/include`,
`ZLIB_LIBRARY_RELEASE=<prefix>/lib/libz.a` on Linux or `zs.lib` on Windows, and
`CMAKE_PREFIX_PATH=<prefix>`. The remaining core curl switches are:

```text
BUILD_CURL_EXE=OFF
BUILD_SHARED_LIBS=OFF
BUILD_STATIC_LIBS=ON
BUILD_TESTING=OFF
CURL_DISABLE_INSTALL=ON
ENABLE_THREADED_RESOLVER=ON
ENABLE_ARES=OFF
USE_NGHTTP2=OFF
```

Select `CURL_USE_OPENSSL=ON` on Linux and `CURL_USE_SCHANNEL=ON` on Windows.
Configure the Linux curl build with `CFLAGS=-DOPENSSL_NO_ENGINE`; the CX HTTP
API does not expose curl's OpenSSL Engine support, and this keeps the pinned
OpenSSL 3-built archive linkable with OpenSSL 4, which removed those symbols.
Build the `libcurl_static` target in Release mode. CMake's feature report must
include `AsynchDNS`, `SSL`, `IPv6`, `libz`, `brotli`, and `zstd`; inspect
`curl_config.h` for `USE_RESOLV_THREADED` on Linux. Include curl, zlib, Brotli,
and zstd license notices in redistributed packages. The current local
source builds reside under the ignored `.build/m26-http-deps` directory. The
redistributable static libraries, headers, and license notices are copied into
`cxnet/src/binary/<rid>/` and included by the cxnet package manifest. On Linux,
configure both zlib and curl with `CMAKE_POSITION_INDEPENDENT_CODE=ON`; the
static curl archive is linked into shared CX consumer modules, which requires
PIC objects. The package-consumer link check caught and verified this
requirement.

Configure the socket-action fixture with the corresponding libcurl CMake build
directory, then build and run CTest:

```powershell
cmake -S cxnet/spikes/http-backend -B .build/m26-http-deps/spike-win64 -A x64 `
  -DCURL_BUILD_DIR=$PWD/.build/m26-http-deps/build-win64 `
  -DZLIB_DIR=$PWD/.build/m26-http-deps/zlib-install-win64/lib/cmake/zlib
cmake --build .build/m26-http-deps/spike-win64 --config Release
ctest --test-dir .build/m26-http-deps/spike-win64 -C Release --output-on-failure
```

On Linux, use the WSL Ubuntu environment and the same CMake 3.31+ binary used
for `cxnet`; pass both the Linux curl build directory as `CURL_BUILD_DIR` and
the `zlib-install-linux/lib/cmake/zlib` directory as `ZLIB_DIR`. Windows x86
uses `-A Win32`, the `build-win32` directory, and the `zlib-install-win32`
prefix. The socket-action fixture passes in all three tested cells with the
local `localhost` HTTP exchange. Its loopback server reads the complete request
headers before replying; Windows resets the socket if the fixture closes with
the client's request still unread. The full `cxnet` CTest suite passes in all
six Windows x86/x64 and Linux x64 static/shared cells, and the standalone
Windows x64 socket-action fixture passes five consecutive runs.

## Shared managed HTTP end-to-end coverage (2026-11-17)

The spike now builds and registers the comprehensive managed HTTP fixture,
redirect fixture, HTTPS fixture, and public API smoke fixture for shared as well
as static `cxnet` builds. Windows test DLLs export generated C symbols in
`CXNET_TESTING` builds so the C fixtures can inspect test-only transport
diagnostics while linking to the shared module. The public smoke fixture waits
for the server-side cancellation observation, and treats either orderly EOF or
connection reset as transport closure.

The managed HTTP, redirect, HTTPS, and public smoke CTests pass in all three
shared targets: Windows x86, Windows x64, and Linux x64. The public smoke now
starts two headers-first requests on one client and reads a body prefix from
both. It cancels one pending read while checking the other remains pending,
then cancels the second; subsequent reads remain canceled and the server must
observe both connection closures. This concurrent case passes three
consecutive times in all six Windows x86/x64 and Linux x64 static/shared
targets (18 runs total). Rebuild with
`cxnet_managed_http_e2e`, `cxnet_managed_http_redirect_e2e`, and
`cxnet_managed_http_public_smoke`, then run the same four-test selection with
`ctest --output-on-failure`. The HTTPS test is registered separately and runs
the managed HTTP executable under the local certificate fixture. The public
smoke also verifies HTTPS CONNECT tunneling through a local TLS proxy with an
explicit CA bundle, rejects the same proxy when its CA is not trusted, and
rejects a trusted proxy certificate with a hostname mismatch; these cases pass
in all six Windows x86/x64 and Linux x64 static/shared cells.

The managed HTTP e2e also disposes `HttpClient` after a headers-first response
has delivered a prefix while a second body read is pending. The pending read is
canceled and the server observes connection closure; this passes in all six
Windows x86/x64 and Linux x64 static/shared cells.

## HTTPS proxy trust fixture (2026-11-18)

`CurlHttpMessageHandler.ProxyCaCertificateFile` supplies a CA bundle specifically
for TLS to an HTTPS proxy; the destination `CaCertificateFile` remains separate.
The public smoke routes an HTTPS request through a TLS-wrapped local CONNECT
proxy and checks success with the fixture CA, failure with default proxy trust,
and failure when the trusted proxy certificate's hostname does not match. All
three cases pass in all six Windows x86/x64 and Linux x64 static/shared cells.
On Schannel,
`AllowUnknownCertificateRevocation` enables best-effort handling for proxy TLS
as well as destination TLS while preserving chain and hostname verification.

## Managed HTTP loopback benchmark (2026-11-17)

M26 closure note (2026-11-27): the user asked to skip the separate-physical-host
repeat because they do not have a second machine. The workstation loopback
baselines remain recorded below; no independent-host measurement is claimed.

An opt-in benchmark measures warmed `HttpClient.GetStreamAsync` requests against
a deterministic local HTTP/1.1 server. It reports time until response headers,
time until the first body byte, end-to-end body throughput, managed allocation
count/bytes, GC count/collected bytes/pause time, and heap bytes before and after
forced collection. It also measures cancellation latency while a body read is
pending. The fixture reports accepted connections and inferred persistent
connection reuses. Enable it with `-DCXNET_BUILD_HTTP_BENCHMARKS=ON`, build
`cxnet_http_benchmarks`, then run:

```sh
python3 cxnet/benchmarks/http_benchmark_fixture.py \
  .build/m26-http-benchmark-linux/cxnet_http_benchmarks 10 3 262144
```

Arguments are request count per sample, sample count, and response body bytes.
Each cell below ran three samples of 10 requests and 256 KiB bodies. Linux x64
used GCC 11.4 and the default CMake configuration (`CMAKE_BUILD_TYPE` unset);
Windows x86/x64 used MSVC Release. All used the repository's pinned curl and
zlib.

The fixture can also run separately from the benchmark process, including on a
different physical host. On the server machine, start it with an explicit
reachable bind address and port:

```sh
python3 cxnet/benchmarks/http_benchmark_fixture.py serve \
  --host 0.0.0.0 --port 8080 --body-bytes 262144 \
  >m26-http-benchmark-server.log 2>&1
```

Keep the server process running. In another server-side terminal, use
`tail -f m26-http-benchmark-server.log` to confirm startup and watch request
accounting. After the client finishes, interrupt the server with Ctrl+C; the
final accounting line is appended to the same log.

The startup line records the server hostname and OS alongside its bound port;
the client runner records the same fields for the client machine. Preserve both
records with the results. Run the client on the second physical machine itself;
a WSL, VM, or container reports its guest identity and does not prove which
physical host ran it.

On the benchmark machine, use the server's reachable address as the base URL.
The repeat runner executes five invocations, validates each CSV and cancellation
observation, and saves every raw output plus a host/toolchain summary in a new
output directory:

```sh
python3 cxnet/benchmarks/http_benchmark_remote_client.py \
  --executable .build/m26-http-benchmark-linux/cxnet_http_benchmarks \
  --base-url http://192.0.2.10:8080 \
  --build-label 'Linux x64, GCC version, static/shared, build configuration' \
  --output-dir m26-http-benchmark-results
```

Each invocation provides one cancellation-latency observation and three
throughput samples. The runner records the client hostname, OS, Python version,
a caller-supplied compiler/target/link/configuration label, benchmark executable
SHA-256, and a Markdown summary beside the five CSV and stderr files. Five
default invocations should produce 165 requests, 10 connections, and 155
reuses; compare the server's final
accounting with the runner summary. This split mode retains the same
deterministic `/body` and `/cancel` routes and client instrumentation as the
loopback runs.

On 2026-11-26, the split server/client processes passed a local integration
check in Ubuntu 24.04 with a 256 KiB body: five requests completed over two
connections, with three inferred pooled reuses. This verifies the split-mode
protocol and accounting; at the time, the separate-host repeat was still open
and was later waived by the user.

On 2026-11-27, the new repeat runner completed five independent benchmark
invocations against the separately running fixture in two containers. It
validated all 15 CSV samples and five cancellation observations;
the fixture reported exactly 165 requests, 10 connections, and 155 pooled
reuses. Both containers ran on the same WSL2 workstation, so this validates the
runner and accounting only; it does not satisfy the physical-host requirement.

On 2026-11-27, the staged Fedora 44 benchmark binary and fixture were run in
five independent local invocations with 10 requests, three samples, and 256 KiB
bodies. Each invocation reported 33 fixture requests over two connections with
31 pooled reuses, confirming the runbook's aggregate expectation of 165
requests, 10 connections, and 155 reuses. Throughput ranged from 11.25 to
12.36 MiB/s, and cancellation latency from 23.5 to 56.7 µs. These runs share
the same physical workstation and do not satisfy the separate-host gate.

On 2026-11-27, rebuilt the current-source Windows x64 static benchmark with
Visual Studio 18 Release and the pinned Windows curl assets, then ran the new
collector natively on Windows 11 against the split WSL2 fixture. All five runs
passed CSV and cancellation validation; the server reported 165 requests, 10
connections, and 155 pooled reuses. Throughput ranged from 7.146 to 9.853
MiB/s, and cancellation latency from 11.2 to 13.1 µs. This verifies the native
Windows client and collector path, but both endpoints still shared one physical
workstation.

| Target | Throughput (MiB/s) | Header mean (µs) | First body byte mean (ms) | Managed allocations / sample | Managed bytes / sample | Retained heap after forced GC (bytes) | Cancellation latency |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Linux x64 static | 10.5–12.6 | 716–873 | 5.45–6.12 | 2,670 | 3,434,560–3,444,960 | 11,756–12,796 | 52 µs |
| Linux x64 shared | 11.1–14.2 | 531–846 | 4.66–5.70 | 2,670 | 3,434,560–3,444,960 | 11,756–12,796 | 3.2–18.3 ms |
| Windows x86 static | 8.7–13.5 | 509–884 | 4.95–7.54 | 2,650–2,667 | 3,364,024–3,369,856 | 10,192–10,752 | 151 µs |
| Windows x86 shared | 12.1–12.6 | 617–641 | 5.01–5.18 | 2,640–2,661 | 3,363,912–3,369,360 | 26,576–27,136 | 75 µs |
| Windows x64 static | 9.5–12.8 | 674–762 | 5.25–6.69 | 2,640–2,663 | 3,432,400–3,443,512 | 11,756–12,796 | 128 µs |
| Windows x64 shared | 8.7–9.9 | 719–832 | 6.12–6.90 | 2,670–2,695 | 3,433,896–3,443,768 | 11,756–12,796 | 61 µs |

Each cell completed 33 fixture requests over two TCP connections, giving 31
reuses including the warmed requests. These are local baselines, not direct
cross-platform comparisons: build configuration differs, and scheduler timing
varied substantially (especially shared Linux cancellation). Repeat runs on a
target host and preserve raw samples when comparing changes. The harness is
diagnostic and does not promise allocation-free requests or backend parity.

### Repeat cancellation samples across the six target cells (2026-11-18)

Rebuilt the benchmark from the current workspace sources in Ubuntu 24.04.5
(glibc 2.39, GCC 13.3.0, OpenSSL 3.0.13, CMake 4.4.4) under the WSL2 kernel
5.15.167.4. Five independent fixture invocations per link mode each ran three
samples of ten requests with 256 KiB bodies. Every invocation completed 33
fixture requests over two TCP connections, with 31 pooled reuses; each also
received a four-byte body prefix before cancellation.

| Link mode | Cancellation latency per invocation (µs) |
| --- | --- |
| Linux x64 static | 45.6, 44.2, 43.7, 54.3, 63.7 |
| Linux x64 shared | 54.5, 53.8, 154.0, 59.9, 62.0 |

The post-disposal heap measurement was 96,341 bytes in all ten runs. The shared
results include a 154 µs sample; these measurements are local diagnostics, not
a latency guarantee.

On the Windows 11 Pro 10.0.26300 host, rebuilt the same benchmark in Visual
Studio 18 Release and ran five fixture invocations per link mode, again with
three samples of ten requests and 256 KiB bodies per invocation. Cancellation
latencies were:

| Link mode | Cancellation latency per invocation (µs) |
| --- | --- |
| Windows x64 static | 125.4, 69.1, 80.7, 91.6, 74.5 |
| Windows x64 shared | 75.3, 58.9, 89.1, 76.7, 79.9 |

Every Windows run also completed 33 fixture requests over two connections with
31 pooled reuses. Post-disposal heap measurements ranged from 95,607 to 96,119
bytes.

On the same Windows 11 host, repeated five times in each x86 link mode:

| Link mode | Cancellation latency per invocation (µs) |
| --- | --- |
| Windows x86 static | 59.7, 99.6, 129.9, 66.9, 70.7 |
| Windows x86 shared | 69.4, 58.1, 83.6, 62.7, 97.2 |

Every x86 run completed 33 fixture requests over two connections with 31 pooled
reuses. The measurements now cover all six supported target cells on this
workstation. They are diagnostics, not service-level guarantees; repeated
measurements on separate physical hosts and pristine-target environments
remain open.

### Fedora 44 isolated-container sample (2026-11-25)

Rebuilt the Linux x64 static benchmark from current workspace sources with GCC
13.3.0, then copied only the executable and benchmark fixture into a clean Fedora
44 container (glibc 2.43, OpenSSL 3.5.9). The container had no repository
checkout. Three samples each ran ten requests with 256 KiB bodies; all 33
fixture requests used two TCP connections, for 31 pooled reuses. `ldd -r`
reported no unresolved symbols.

| Metric | Fedora 44 sample |
| --- | --- |
| Throughput | 11.70–11.93 MiB/s |
| Mean response-header latency | 720–818 µs |
| Mean first-body-byte latency | 5.02–5.17 ms |
| Managed allocations per sample | 3,010 |
| Managed bytes per sample | 3,439,470 |
| Retained heap after forced collection | 11,452 bytes per sample |
| Post-disposal heap | 98,773 bytes |
| Cancellation latency | 52.8 µs |

This container uses the same WSL2 host as the other local measurements, so it
adds a clean distribution environment but does not satisfy the separate
physical-host repeat requirement.

### Debian 13.7 isolated-container benchmark repeat (2026-11-26)

Reconfigured and rebuilt the Linux x64 static benchmark from the current
workspace sources inside a clean Debian 13.7 container (glibc 2.41, GCC
14.2.0, CMake 3.31.6). The container mounted the workspace at its WSL path to
reuse the pinned curl/zlib build inputs, then used only the benchmark and
fixture at runtime. The resulting executable SHA-256 is
`92436FF6C6067691F953FAA8C7B60CF3D6B233D8D47035DC6C4F9F1A0F7C5BFE`. Three
samples each transferred ten 256 KiB bodies; all 33 fixture requests used two
TCP connections (31 pooled reuses). `ldd -r` reported no unresolved symbols.

| Metric | Debian 13.7 sample |
| --- | --- |
| Throughput | 11.02–11.74 MiB/s |
| Mean response-header latency | 755–784 µs |
| Mean first-body-byte latency | 4.80–5.10 ms |
| Managed allocations per sample | 3,010 |
| Managed bytes per sample | 3,439,460 |
| Retained heap after forced collection | 11,451 bytes per sample |
| Post-disposal heap | 98,771 bytes |
| Cancellation latency | 27.1 µs |

This adds another clean distro environment on the same Windows/WSL physical
host. It does not satisfy the separate physical-host repeat requirement.

### Current-source benchmark refresh (2026-11-26)

Rebuilt the benchmark from the current workspace sources and ran three samples
of ten requests with 256 KiB bodies in Linux x64 GCC 13.3 and Clang 14
static/shared, plus Windows 11 MSVC Release x86/x64 static/shared. Each
invocation also measured cancellation latency once while a body read was
pending. Every cell completed 33 fixture requests over two TCP connections,
with 31 pooled reuses. Both Linux compiler builds used the pinned curl and
zlib inputs; the Clang build used GCC 11.4 for C++ support files.

| Target | Throughput (MiB/s) | Header mean (µs) | First body byte mean (ms) | Managed allocations / sample | Managed bytes / sample | Retained heap after forced GC (bytes) | Post-disposal heap (bytes) | Cancellation latency |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Linux x64 GCC static | 15.169–15.184 | 467–528 | 4.056–4.081 | 3,010 | 3,439,460 | 11,451 | 98,771 | 724 µs |
| Linux x64 GCC shared | 13.964–14.399 | 453–555 | 4.385–4.562 | 3,010 | 3,439,460 | 11,451 | 98,771 | 16.8 µs |
| Linux x64 Clang 14 static | 10.341–12.169 | 634–869 | 4.399–6.244 | 3,010 | 3,439,470 | 11,500 | 98,773 | 14.1 µs |
| Linux x64 Clang 14 shared | 12.223–12.646 | 708–742 | 4.993–5.181 | 3,010 | 3,439,470 | 11,500 | 98,773 | 20.9 µs |
| Windows x64 static | 10.788–13.055 | 658–875 | 4.955–6.144 | 2,980–2,999 | 3,437,310–3,437,998 | 11,452 | 98,557 | 12.4 µs |
| Windows x64 shared | 14.286–15.022 | 502–712 | 4.352–4.638 | 2,980–2,990 | 3,437,310–3,437,614 | 11,452 | 98,557 | 11.8 µs |
| Windows x86 static | 13.476–13.933 | 634–676 | 4.640–4.882 | 2,996–3,004 | 3,366,374–3,366,526 | 10,101 | 92,294 | 11.3 µs |
| Windows x86 shared | 9.472–13.928 | 644–862 | 4.679–6.208 | 2,996–3,020 | 3,366,374–3,366,734 | 10,101 | 92,294 | 13.4 µs |

These are local diagnostics and one cancellation observation per cell, not a
repeatability claim or a latency guarantee. Scheduler and host load affect the
timings. A repeat on a separate physical host remains open; earlier
five-invocation cancellation samples remain documented above.

### Current-source cancellation repeats on the workstation (2026-11-26)

Rebuilt the benchmark from current sources and ran five fixture invocations in
each of the eight local target cells: Linux x64 GCC 11.4 and Clang 14
static/shared, plus Windows 11 MSVC Release x86/x64 static/shared. Each
invocation used three samples of ten 256 KiB responses and received a four-byte
body prefix before cancellation. All 40 invocations completed 33 fixture
requests over two TCP connections, with 31 pooled reuses.

| Target | Cancellation latency per invocation (µs) | Post-disposal heap (bytes) |
| --- | --- | ---: |
| Linux x64 GCC 11.4 static | 12.0, 11.7, 12.3, 12.0, 10.1 | 98,773–98,845 |
| Linux x64 GCC 11.4 shared | 21.4, 18.4, 21.2, 19.6, 18.0 | 98,773 |
| Linux x64 Clang 14 static | 10.4, 9.5, 9.8, 8.6, 9.5 | 98,773 |
| Linux x64 Clang 14 shared | 17.5, 18.9, 19.1, 16.9, 17.3 | 98,773 |
| Windows x64 static | 15.2, 12.1, 14.3, 17.9, 14.5 | 98,557 |
| Windows x64 shared | 17.6, 15.8, 18.2, 15.5, 17.5 | 98,557 |
| Windows x86 static | 15.1, 12.5, 13.3, 14.6, 13.2 | 91,930–157,826 |
| Windows x86 shared | 20.6, 15.8, 12.2, 12.2, 14.9 | 92,289–174,214 |

Each cancellation value is one observation within a fixture invocation. The
Windows x86 post-disposal heap values varied between runs; the measurements
are diagnostic and do not imply a latency or memory guarantee. All runs share
the same physical workstation, so a separate-host repeat remains open.

### Current-source Clang static benchmark on isolated Fedora 44 (2026-11-26)

Ran the current-source Linux x64 Clang 14 static benchmark in the existing clean
Fedora 44 image (glibc 2.43), with networking disabled. The container received
only the benchmark build directory and Python fixture as read-only mounts; it
had no repository checkout. `ldd -r` found no unresolved symbols. Three
samples each transferred ten 256 KiB bodies, and all 33 fixture requests used
two TCP connections with 31 pooled reuses.

| Metric | Fedora 44 Clang static sample |
| --- | --- |
| Throughput | 11.71–12.18 MiB/s |
| Mean response-header latency | 524–544 µs |
| Mean first-body-byte latency | 4.95–5.13 ms |
| Managed allocations per sample | 3,010 |
| Managed bytes per sample | 3,439,470 |
| Retained heap after forced collection | 11,500 bytes per sample |
| Post-disposal heap | 98,773 bytes |
| Cancellation latency | 8.9 µs |

This verifies the current static binary in a clean distribution image, but the
container still shares the Windows/WSL physical host.

The Clang shared benchmark also passed in the same Fedora 44 image with
networking disabled. Only its build tree and the fixture were mounted read-only;
the build tree supplied the `cxnet` and `cxcore` shared modules through
`LD_LIBRARY_PATH`, and `ldd -r` found no unresolved symbols. The run completed
33 fixture requests over two connections with 31 pooled reuses.

| Metric | Fedora 44 Clang shared sample |
| --- | --- |
| Throughput | 10.26–10.52 MiB/s |
| Mean response-header latency | 598–649 µs |
| Mean first-body-byte latency | 5.74–5.93 ms |
| Managed allocations per sample | 3,010 |
| Managed bytes per sample | 3,439,470 |
| Retained heap after forced collection | 11,500 bytes per sample |
| Post-disposal heap | 98,773 bytes |
| Cancellation latency | 17.9 µs |

This adds clean-distro runtime evidence for both current-source Clang link
modes, but both container runs still share the Windows/WSL physical host.

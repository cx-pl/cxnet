# CX networking design (Milestone 26)

## Current status (2026-11-27)

Milestone 26 acceptance is complete. The user asked to skip the optional repeat
benchmark from a second physical host because no second machine is available;
no independent-host result is claimed. Loopback performance/cancellation
baselines and the remaining acceptance groups below are complete.

The latest complete CTest matrix passed in all eight Windows x86/x64 MSVC and
Linux x64 GCC 13.3/Clang 18 static/shared cells: Windows x86 static/shared
passed 113/113 in 486.20/488.01 seconds; Windows x64 passed 113/113 in
530.69/473.34 seconds; Linux GCC passed 113/113 in 424.00/449.68 seconds;
Linux Clang shared passed 113/113 in 633.10 seconds; and Linux Clang static
passed 114/114 in 761.32 seconds, including its additional chunk-parser fuzz
test. The matrix includes the upload timeout-versus-client-disposal race and
fifteen malformed-framing recovery cases. Linux builds use strict warnings.
The TCP echo, streamed HTTP download, and generated REST CRUD examples pass
current-source smoke checks in all eight cells.

Current-source package-only HTTP/HTTPS and REST/CXDATA JSON consumers build and
run from isolated stages on multiple Linux distributions and Windows x86/x64.
The latest openSUSE Tumbleweed checks pass strict static/shared builds, loopback
wire checks, and shared-library loading with `LD_LIBRARY_PATH` unset. Package
hashes and platform details are in
[`../tests/package_consumer/README.md`](../tests/package_consumer/README.md).
The REST generator runs normally for the current fixture; profiling and
optimization results are in
[`../generators/rest/PROFILE_RESULTS.md`](../generators/rest/PROFILE_RESULTS.md).

Five local benchmark invocations verify accounting and provide workstation
baselines. The user explicitly asked to skip the separate-physical-host repeat;
it is recorded as waived, not as a completed measurement. The runbook and raw
measurements are in [`http-backend-spike.md`](http-backend-spike.md).

## Historical acceptance notes

The dated notes below are a chronological implementation log. For current
status and outstanding gates, use the summary at the top of this document.

TCP, HTTP/HTTPS, and generated REST clients have implemented baselines
with coverage across the current Windows x86/x64 and Linux x64 build cells. The
latest current-source full M26 CTest matrix passes in all eight Windows
x86/x64 MSVC and Linux x64 GCC 13.3/Clang 18 static/shared cells: Windows x86
static/shared passed 113/113 in 486.20/488.01 seconds; Windows x64 static/shared
passed 113/113 in 530.69/473.34 seconds; Linux GCC static/shared passed 113/113
in 424.00/449.68 seconds; Linux Clang shared passed 113/113 in 633.10 seconds;
Linux Clang static passed 114/114 in 761.32 seconds including its additional
chunk-parser fuzz test. All suites include the upload timeout-versus-client-
disposal race and the expanded fifteen-case framing recovery fixture. The
focused disposal race also passed five runs in all eight cells. The matrix runs
were concurrent, so their durations include host contention.
The TCP echo, streamed HTTP download, and generated REST CRUD examples also pass
current-source smoke checks in all eight cells.
The eight-cell full-suite run included twelve malformed-response framing
recovery cases, non-owning stream pending-read/write socket-disposal tests, Socket and
NetworkStream overlapping-read/write contract tests, HTTP/HTTPS, REST,
socket/DNS, and scheduler-shutdown coverage. Linux full-suite builds use
`-Wall -Wextra -Werror -Wno-unused-value`.
This refresh found and fixed an incomplete shared-library test environment
list: every registered `cxnet` test now receives the runtime library path in
Linux shared builds. The concurrent-accept and twelve-case framing-recovery
tests each passed five consecutive focused runs in all eight cells. Since that
full matrix, the framing fixture has gained three more cases, and the
socket connect-reset race now covers buffer-size options. The fifteen-case
framing-recovery CTest passes five consecutive runs in all eight cells, and the
Linux GCC static full suite passes 112/112 in 383.15 seconds after the latest
fixture additions. The three new cases reject a parameterized `chunked` coding
and leading or trailing empty transfer-coding list entries, then verify
same-client recovery on a new connection. Milestone 26 remains open for broader lifecycle
acceptance, additional pristine-target coverage, and separate-host benchmark
measurements. An earlier
Clang shared full run exposed a fixture timing failure: the server's two-second
receive timeout could expire before caller cancellation after forced GC. The
fixture timeout now matches the test's ten-second cancellation window. The
partial-header cancellation test passed five runs in all eight cells, and the
refreshed current-source full suite passed in all eight cells.
This document defines the compatibility boundary for `cxnet`; it does not
claim full .NET or Refit compatibility.
On 2026-11-26, the REST and JSON generators were rebuilt and the full REST
recording/live-wire fixture was regenerated from current sources. Both fixture
modes pass in all eight Windows x86/x64 MSVC and Linux x64 GCC 13.3/Clang 18
static/shared cells, including the query-map binding-shape matrix.
On 2026-11-26, the fresh current-source package-only cxcore/cxnet archives were
restored into an isolated Linux x64 stage and rebuilt with strict warnings.
Static and shared HTTP/HTTPS smokes pass on CentOS Stream 10 (GCC 14.4.1,
CMake 3.31.8); static also builds and passes on AlmaLinux 10.3, and the shared
executable passes there. Both modes verify plain HTTP, system-trust HTTPS,
explicit-CA HTTPS, wrong-host rejection, and ldd -r with LD_LIBRARY_PATH
cleared. The fixture CA was installed only in disposable containers. Archive
hashes and details are in the package-consumer README.
On 2026-11-27, a freshly published current-source package feed was restored
into an isolated Ubuntu 24.04 Linux x64 HTTP consumer. Static and shared
builds pass plain HTTP, system-trust HTTPS, explicit-CA HTTPS, and wrong-host
rejection with strict warnings; `ldd -r` reports no unresolved symbols. A
checkout-free REST/CXDATA JSON consumer restored from the same feed passes its
loopback wire smoke in static and shared modes on Linux x64 and Windows x86/x64.
The Windows x86 builds use the Win32 generator platform and a staged
`CXCORE_SOURCE_DIR`. Linux shared execution passes with `LD_LIBRARY_PATH`
unset; `ldd -r` resolves the staged CX libraries and system OpenSSL. Package
hashes and staging details are recorded in the package-consumer README.
The fresh-feed Linux HTTP and REST consumers also pass in Ubuntu 26.10
(glibc 2.44, GCC 15.3.0, CMake 4.3.4, OpenSSL 4.0.3) with strict warnings.
HTTP verifies plain HTTP, system-trust HTTPS, explicit-CA HTTPS, and wrong-host
rejection; REST verifies the generated JSON client over loopback. Both shared
executables pass with `LD_LIBRARY_PATH` unset, and `ldd -r` reports no
unresolved symbols.

The malformed-framing recovery fixture also rejects identical duplicate
`Content-Length` fields combined with chunked transfer encoding, then verifies
that the same client succeeds on a fresh connection. The focused recovery CTest
passes five consecutive runs in all eight Windows x86/x64 MSVC and Linux x64
GCC 13.3/Clang 18 static/shared cells; Linux builds use strict warnings. This
case was added after the latest full-suite run. After the addition, all 111
registered CTests passed in the current-source Linux x64 GCC static tree in
347.68 seconds. This complements the earlier eight-cell full-suite matrix.
Other interacting malformed-field combinations and the separate-host
benchmark remain open.

The failed-connect reset race now also covers `ReceiveBufferSize` and
`SendBufferSize`, in addition to `NoDelay`, `KeepAlive`, and `ReuseAddress`.
It checks that successful buffer changes survive socket recreation after
connection refusal and that both options remain settable afterward. The focused
buffer-option CTest passes five consecutive runs in all eight Windows x86/x64
MSVC and Linux x64 GCC 13.3/Clang 18 static/shared cells; Linux uses strict
warnings. The updated current-source Linux x64 GCC static full suite passes
112/112 tests in 358.24 seconds; GCC shared passes 112/112 in 439.71 seconds;
Clang 18 shared passes 112/112 in 559.01 seconds; and Clang 18 static passes
113/113 in 613.01 seconds, including the chunk-parser fuzz test. Windows x64
MSVC static/shared pass 112/112 in 521.96/456.83 seconds. Every run includes
the focused case and omnibus DNS/socket fixture. Broader lifecycle and
separate-host benchmark acceptance remain open.
The pending-write readiness/cancellation fixture now covers both
Socket.WriteAsync and non-owning NetworkStream.WriteAsync in a 12-trial race.
It requires both valid winners across the trials, exactly-once completion,
callback cleanup, connection preservation, intact queued peer bytes, and
delivery of the write payload when readiness wins.
Both focused CTests pass in all eight Windows x86/x64 MSVC and Linux x64 GCC
13.3/Clang 18 static/shared cells; Linux uses strict warnings and pinned
libcurl 8.23.0. The integrated DNS/socket fixture also passes in Linux GCC
static and Windows x64 MSVC static.
On 2026-11-26, the focused 48-test managed HTTP/HTTPS CTest suite passed in
Linux x64 GCC 11.4 and Clang 14 static/shared builds with strict warnings.
GCC shared's first parallel run had one failure in the HTTPS-enabled end-to-end
run: cancellation during a partial response header did not observe peer
closure. The dedicated partial-header test and isolated HTTPS-enabled rerun
passed, followed by a serial full-suite pass. The serial Windows x86 static
full suite also passed, including HTTPS end-to-end in 85.53 seconds. The three
wrapper tests initially missing executables passed in Windows x86/x64
static/shared after their targets were built. All eight Windows x86/x64 MSVC
and Linux x64 GCC 11.4/Clang 14 static/shared cells now have evidence for all
48 managed HTTP/HTTPS tests. Milestone 26 remains open.
On 2026-11-26, all eight REST-focused CTests also passed in every Windows
x86/x64 MSVC and Linux x64 GCC 11.4/Clang 14 static/shared cell. The generated
recording fixture includes inherited methods from ordinary and closed-generic
contracts in its referenced `RestModels` library; cancellation, response
publication/reentrancy, multipart setup, and continuation-registration cases
are covered by the rest of the suite.
The focused map probe also verifies direct and wrapped nullable DTO map values,
including DTO deserialization, JSON-null map entries, wrapper metadata, and
disposal. Its 26-request wire run passes in all eight Windows x86/x64 MSVC and
Linux x64 GCC 11.4/Clang 14 static/shared cells; Linux shared executables pass
`ldd -r` without unresolved symbols.
The REST generator runs normally for the current fixture build. Its recording
and live-wire fixtures pass in all eight Windows x86/x64 MSVC and Linux x64
GCC/Clang static/shared cells, including nullable scalar and collection result
matrices. The complete native CTest matrix passes in the same eight cells.
Linux builds use strict warning flags. Rebuilt multipart setup-failure and
generated-continuation registration fixtures pass with their live-wire checks
in the same matrix. The current-source package-only REST/CXDATA JSON consumer passes Windows
x86/x64 MSVC and Linux GCC static/shared builds, including Debian 12, Fedora
44, CentOS Stream 10, Debian 13.7, openSUSE Tumbleweed, AlmaLinux 10.3, and
Rocky Linux 9.9. Current-source HTTP/HTTPS package smokes pass strict
static and shared builds on Ubuntu 24.04.5, Rocky Linux 9.9, and Debian 13.7,
including system and explicit CA trust plus wrong-host rejection. Rocky's test
verifies fallback from libcurl's missing compiled CA path to the system bundle.
The same current-source package HTTPS smoke passes Windows x86/x64 MSVC
static/shared, with temporary CurrentUser trust for the fixture CA and Schannel
best-effort handling of unavailable revocation status. On 2026-11-26 the
current-source HTTP benchmark was refreshed in all eight local compiler,
architecture, and static/shared cells; five cancellation invocations per cell
completed 33 fixture requests over two connections with 31 pooled reuses. The
current-source Clang static and shared benchmarks also passed in a clean
Fedora 44 image with no repository checkout. The separate physical-host repeat, remaining
lifecycle, and broader pristine-target gates stay open.
The package-only REST/CXDATA JSON consumer also passes static and shared
openSUSE Tumbleweed builds with current locked packages and separately built
Linux host generator tools; each run completed one loopback request and decoded
the DTO. `ldd -r` found no unresolved symbols, and shared mode resolved the
staged `libcxdata`, `libcxnet`, and `libcxcore` modules. The consumer stage had
no repository checkout; hashes and the host-tool staging details are in the
package-consumer README.
On 2026-11-26, a fresh package-only Linux x64 consumer also passed the four
HTTP/HTTPS smoke cases on openSUSE Tumbleweed (GCC 16.2.1, CMake 4.5.0,
OpenSSL 3.5.3, .NET 10.0.404) with strict warnings, cleared
`LD_LIBRARY_PATH`, static and shared CX package linkage, and no unresolved
symbols from `ldd -r`; shared mode loads the staged consumer, `cxnet`, and
`cxcore` through `$ORIGIN`. The local fixture CA was added only to the
disposable container trust store. The stage had no repository checkout.
Package hashes and details are in the package-consumer README.
The REST live-wire query matrix also verifies repeated `const Array<ulong>`
values including `ulong.MaxValue` in all eight runtime cells; strict Linux
GCC 14.2 and Clang 19 static/shared builds pass.
The same matrix now combines unsigned boundary formatting with null-element
omission in `const Array<ulong?>`, passing all eight runtime cells.

On 2026-11-26, the current-source full M26 CTest suite passed 105/105 in all
eight Windows x86/x64 MSVC and Linux x64 GCC 11.4/Clang 14 static/shared cells.
Linux builds use strict warning flags. The six malformed-response recovery
cases and non-owning stream write canceled by socket disposal are included.
Milestone 26 remains open for broader lifecycle acceptance, additional
pristine-target coverage, and separate-host benchmark measurements.

On 2026-11-26, a non-owning `NetworkStream.ReadAsync` pending-read test now
disposes its underlying `Socket` twice after forcing GC. It verifies task
cancellation, `CanRead == false`, and peer-observed EOF. The standalone CTest
passes in all eight Windows x86/x64 MSVC and Linux x64 GCC 11.4/Clang 14
static/shared cells, and the integrated DNS/socket test passes in the same
matrix. The refreshed full suite passes 107/107 in all eight cells. Broader
lifecycle, pristine-target, and separate-host benchmark acceptance remains
open.

The `cxnet_network_stream_concurrent_read_rejection` CTest exercises the
`NetworkStream` task contract: a second read while one is pending returns a
faulted task containing `InvalidOperationException`; after the first read
completes, a new read is admitted, and disposing the underlying socket cancels
that pending read and closes the peer connection. It forces GC while the first
read is pending. The focused test passes all eight Windows x86/x64 MSVC and
Linux x64 GCC 11.4/Clang 14 static/shared cells, with strict Linux warnings.

## Ownership and delivery boundary

`cxnet` owns IP endpoint, DNS, socket, network stream, HTTP, REST annotation,
transport, and generated REST client APIs. `cxcore` owns general-purpose URI,
stream, task/cancellation, and shared exception contracts when those contracts
are accepted there. `cxdata` remains an optional REST JSON adapter; raw sockets
and HTTP do not depend on it. TCP server support means application-managed
listeners and accepted sockets. HTTP server routing belongs to `cxweb`.

The implementation sequence is TCP first, then HTTP/HTTPS, then generated REST
after the Milestone 29 generator can run CX code against compiler snapshots.
This document freezes the intended user-visible shape below; implementation
may add overloads without changing the existing transfer behavior.

## Implemented TCP baseline

`IPAddress` and `IPEndPoint` cover IPv4/IPv6 parsing and formatting, IPv4-mapped
IPv6 text, numeric IPv6 scope identifiers, endpoint comparison, and port
validation. TCP sockets support IPv4 and IPv6 stream creation, bind/listen,
synchronous and asynchronous endpoint connect/accept, shutdown, endpoint
queries, and `NoDelay`, keepalive, reuse, dual-mode, and send/receive buffer
options. `Send`/`Receive` and their async aliases each report one partial
transfer; existing `ReadAsync`/`WriteAsync` signatures remain unchanged.
The IPv4 loopback fixture sets and reads back `ReuseAddress` before bind and
`NoDelay` plus `KeepAlive` before connect, alongside the IPv6 dual-mode and
buffer-size checks. This option fixture passes Windows x86/x64 MSVC and Linux
x64 GCC/Clang static/shared builds. Concurrent socket disposal/connect/reset/
transfer coverage remains open beyond the 24-iteration `Socket.ReadAsync`/
`Socket.Dispose` race, failed-connect reset races, and the option/disposal races for `NoDelay`, `KeepAlive`,
`ReuseAddress`, `ReceiveBufferSize`, and `SendBufferSize`. These checks verify
legal task/option outcomes, `Connected` clearing, and peer-observed closure in
all eight Windows and Linux compiler/link-mode cells.
An owning `NetworkStream` disposal case fills the socket send buffer, starts a
pending `WriteAsync`, forces GC, and disposes the stream. It requires the task to
cancel, the owned socket handle to close, and the peer to receive the previously
queued bytes unchanged followed by EOF. The focused CTest passes Windows x86/x64
MSVC and Linux x64 GCC/Clang in static/shared configurations.
A dedicated listener-disposal case accepts a loopback client, disposes the
listener twice, forces GC, and transfers bytes in both directions over the
already accepted connection. The accepted client remains connected and usable;
the focused CTest passes Windows x86/x64 MSVC and Linux x64 GCC/Clang
static/shared builds, with strict Linux warning flags.
A second listener-disposal regression starts two pending accepts, forces GC,
and disposes the listener twice. Both tasks must fault with
`ObjectDisposedException`; the focused CTest passes all eight Windows x86/x64
MSVC and Linux x64 GCC/Clang static/shared cells.
The full `cxnet_dns_async_tests` executable also passes Windows x64 MSVC
static after this case was added.
A non-owning counterpart disposes the stream twice while its write is pending
and verifies that the socket and write remain live. A peer worker drains and
checks both the prequeued bytes and the pending byte, replies on the connection,
and then observes EOF only after explicit socket disposal. The focused CTest
passes Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared builds, with
strict Linux warning flags.
HTTP same-origin redirect checks and cookie host matching compare bracketed
IPv6 hosts by parsed address value, so alternate zero-compression spellings
match. Percent-encoded octets in zone identifiers are decoded for comparison,
while identifier character case remains significant. RFC 5952 describes the
equivalent address spellings and recommends a canonical form.
The native address fixture checks value equality and hash consistency for equal
scoped IPv6 addresses, distinguishes different scope identifiers, and verifies
endpoint equality includes both the scoped address and port in Windows x86/x64
and Linux x64 static/shared cells, including strict Linux GCC and Clang builds.
Synchronous socket operations cross `cx_gc_run_blocking`; accepted handles are
nonblocking and independently owned. `cxcore.Stream` now exposes cancellable
async byte read/write operations. `NetworkStream` overrides them with socket
readiness transfers and iterative full-write loops across partial sends, so a
sequence of synchronously completed short writes does not recurse. Other stream
implementations currently inherit completed-task adapters over synchronous I/O;
they validate buffer ranges before returning, and capture cancellation or I/O
failures in canceled or faulted tasks, but they may block and do not yet provide
native async file operations.
`AcceptAsync` rechecks caller cancellation after native `accept()` returns. If
cancellation wins in that interval, it closes the accepted handle before
completing the task as canceled. A deterministic native hook verifies canceled
task state, peer-observed EOF, listener reuse, and a subsequent byte transfer;
the focused test passes all eight Windows x86/x64 MSVC and Linux x64 GCC 11.4/
Clang 14 static/shared cells, with strict Linux warning flags.
The socket-pair regression disposes a non-owning `NetworkStream` during a
pending read and confirms the read still receives peer data while the socket
stays connected. Its owning-stream counterpart confirms disposal cancels the
pending read, detaches the socket handle, and sends EOF to the peer. Both cases
pass Windows x86/x64 and Linux x64 GCC/Clang static/shared builds; Linux uses
strict warning flags.
The pending full-duplex disposal regression also closes the managed socket
from an attached worker while both a read and a saturated write are pending.
Native operation cleanup is posted to the scheduler owner, and the test forces
GC before dispatch, checks both tasks cancel, verifies `Connected` clears, and
reads the prefilled payload followed by EOF at the peer. Its 24 iterations pass
in Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared builds, with
strict Linux warnings. A delayed-DNS hostname connect is also disposed from an
attached worker; timer and caller-registration cleanup plus task completion are
posted to the owner, forced GC runs before dispatch, and the late DNS result is
ignored. Both focused CTests pass in the same eight cells.

`Dns.GetHostAddresses` uses the operating-system resolver through the GC-safe
blocking boundary. `GetHostAddressesAsync` runs the resolver on a native worker,
roots its managed output while the worker is active, and resumes completion on
the scheduler owner thread through an event wait. Both methods preserve
resolver order, filter and de-duplicate IPv4/IPv6 stream results, cap output at
64 addresses, and report resolver failures through `SocketException`.
If scheduler shutdown abandons the event wait while native resolution is still
active, the worker retains ownership of the rooted state and releases it after
the resolver returns. Normal result completion remains on the scheduler owner
thread, and callback exceptions do not skip root or native-host cleanup.
`Socket.Connect(string, int)` and its async counterpart try resolved addresses
that match the socket family, including IPv4 on a dual-mode IPv6 socket. The
async hostname overload composes DNS completion, cancellation, and nonblocking
connect readiness through task continuations. Its `ConnectTimeoutMilliseconds`
defaults to 15 seconds, spans asynchronous DNS and every address attempt, and
can be set to zero to disable the deadline. Expiry closes and recreates an
in-progress socket and completes the task with `TimeoutException`; the resolver
may finish later, but its continuation discards the result after timeout.
Terminal hostname-connect paths release the timer and caller registration even
if socket reset or task completion fails; reset failures fault the pending task.
The per-operation finalization guard checks the active completion identity
under the lifecycle mutex before claiming finalization. Cancellation and error
finalizers then perform reset through the lifecycle-locked reset path without
reading shared socket state outside its mutex. If disposal already won, reset
leaves the detached socket closed.
The successful-connect path also completes the task and releases timer and
registration state independently, preserving the first completion or cleanup
exception if either operation fails.
A deterministic `CXNET_TESTING` resolver failure verifies that synchronous DNS and async
hostname connect preserve the native error through `SocketException` in all
six Windows x86/x64 and Linux x64 static/shared cells. An overall fallback
deadline regression uses a delayed test resolver to verify `TimeoutException`,
ignoring the late DNS result, socket reuse after timeout, and caller cancellation
during DNS. A second delayed lookup verifies the 15-second default and that a
zero timeout disables the deadline. A pre-canceled hostname connect returns a
canceled task without starting DNS, and that socket remains reusable. These
checks pass in all six Windows x86/x64 and Linux x64 static/shared cells. Under
`CXNET_TESTING`, the platform resolver now consults a configurable callback
before native DNS; the callback can supply addresses or a native error and can
decline a host to continue with the OS resolver. Deterministic failure, delay,
and multi-address fixtures are registered by the DNS test executable rather
than hardcoded in platform code. The callback-driven DNS suite passes on
Windows x86/x64 static/shared and Linux x64 with GCC and strict Clang 14 in
static/shared modes. The multi-address fixture returns
`127.0.0.2` before `127.0.0.1` for a listener bound only on the latter;
hostname `ConnectAsync` falls through the refused first address and connects to
the second. The endpoint connect fixture also runs 16 scheduler-timed
cancellation races at delays from 0 through 34 ms, forces GC before scheduler
dispatch, and checks exactly-once cancellation and task completion. Each race
ends either canceled with a disconnected socket or successfully connected. The
full Windows x86/x64 and Linux GCC/Clang static/shared matrix passes.
The 2026-11-22 hostname-connect finalizer synchronization change was regenerated
and compiled, then the DNS async CTest passed in all four Windows x86/x64 MSVC
static/shared cells and all four Linux x64 GCC/Clang static/shared cells; Linux
uses `-Wall -Wextra -Werror -Wno-unused-value`.
`SocketException.SocketErrorCode`
maps common POSIX, Winsock, and DNS failures to matching `SocketError`
categories while preserving the original native domain and code; mapping
tests cover connection refusal/reset/timeout on POSIX and Winsock plus DNS
not-found/retry on Linux x64 and Windows x86/x64. A managed peer-reset loopback
also verifies public `Socket.ReadAsync` faults with `ConnectionReset` and
preserves native error details in all six Windows x86/x64 and Linux x64
static/shared cells plus strict GCC Linux x64 static/shared. Deterministic synchronous
and asynchronous resolver-failure integration fixtures pass in all six
Windows x86/x64 and Linux x64 static/shared cells. The async fixture repeats
resolver-failure and refused-connect cases five times per invocation, forces GC
while each task is pending, and checks exactly-once completion; it passes with
strict GCC Linux x64 static/shared, strict Clang 14.0.6 Linux x64 static/shared,
and Windows x86/x64 static/shared. The 16-timing connect/cancel sweep passes in
the same cells; further HTTP and streaming cancellation races remain open.

The existing socket tests pass on Windows x86/x64 static/shared and Linux x64
static/shared. The TCP echo executable exercises hostname resolution, TCP
connect/accept, and a complete asynchronous `NetworkStream` echo round trip in
all eight Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared cells. Its
client sends the four-byte request in coordinated two-byte chunks while a
four-byte server read is pending, forcing the server to continue after a short
read; the client then reads and verifies the echoed payload. Linux builds use
strict warnings, and shared runs pass with `LD_LIBRARY_PATH` unset. The native
IP-address fixture binds a dual-mode IPv6
wildcard listener to an ephemeral port, verifies a byte-for-byte transfer from
an IPv6 `::1` client, then verifies an IPv4 loopback client can connect through
the listener's IPv4-mapped path. It passes in all six Windows x86/x64 and Linux
x64 static/shared cells, plus Linux x64 static/shared with Clang 14.0.6 and
`-Wall -Wextra -Werror`. The IPv6 loopback pair also checks TCP half-close:
client send shutdown delivers EOF to the accepted socket while preserving the
reverse direction for a response. This passes in all six Windows x86/x64 and
Linux x64 static/shared cells. `Socket.Shutdown(Receive)` is covered separately
by a 24-iteration loopback regression. After forced GC, Windows reads fail with
`SocketError.Shutdown`, while POSIX reads complete with zero-byte EOF; repeated
reads preserve the same platform result and the socket stays connected until
disposal, after which the peer observes EOF. The focused CTest passes all eight
Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared cells. GNU builds
disable `-Wclobbered` only for the omnibus async socket test target. The socket
send half-close is also covered while a managed write is pending: each of 24
iterations fills the native send buffer, starts `Socket.WriteAsync`, forces GC,
and calls `Shutdown(Send)`. The peer receives the queued payload and then EOF;
the write faults with `SocketError.Shutdown` (`WSAESHUTDOWN` on Windows and
`EPIPE` on POSIX), while reads in the reverse direction continue to work.
Subsequent writes still report shutdown. The focused case passes all eight
Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared cells; POSIX `EPIPE`
is mapped to `SocketError.Shutdown`. A matching non-owning
`NetworkStream.WriteAsync` case also passes those eight cells and confirms that
after the scheduled write fault, the same stream successfully reads a peer
byte, confirming that the wrapper's send half-close leaves its receive
direction available.
A complementary receive-side shutdown case fills the send buffer and starts a
pending socket or non-owning stream write before `Shutdown(Receive)`. After the
peer drains queued bytes, the pending write completes successfully and the
peer receives its final byte; the socket remains connected until disposal.
Both 24-iteration CTests pass in all eight Windows x86/x64 MSVC and Linux x64
GCC/Clang static/shared cells.
runtime and live IPv6 fixtures request
eight bytes, send five, and require completion with the short five-byte count
and payload. The live case goes through public `Socket.ReadAsync`; both pass in
those same six cells. A focused async DNS plus hostname-connect fixture now
passes in all six supported build cells: Windows x86/x64 static/shared and
Linux x64 static/shared. It also starts 16 delayed `GetHostAddressesAsync`
lookups concurrently, forces GC while they are pending, and verifies that each
task completes with its address result in the same six cells. The fixture starts
`AcceptAsync` before endpoint
`ConnectAsync`, forces GC while accept is pending and again after starting the
connect, calls `GetResult` on each task, and verifies both sockets are connected;
it also retains the hostname-connect case. Endpoint connect and accept use
explicit completion sources so immediate and readiness completion follow the
same task contract. The fixture also cancels a pending `AcceptAsync` through a
scheduler timer, forces GC, and verifies the task is canceled on all six build
cells. Windows testing exposed `ULong.MaxValue` using C `ULONG_MAX` (32-bit on
Windows despite CX `ulong` being 64-bit); it now uses `ULLONG_MAX`. Resolver
failure and deterministic accept/pending-connect cancellation cases pass in all
six static/shared cells. The fixture adds 16 scheduler-timed endpoint
connect/cancel races with delays from 0 through 34 ms, forced GC before
dispatch, and exactly-once cancellation and task completion checks; all six
Windows x86/x64 and Linux x64 static/shared cells pass, along with strict GCC
and Clang 14 Linux x64 static/shared builds. The async fixture also binds a
second socket to the active loopback listener's
ephemeral port and verifies the portable `AddressAlreadyInUse` category plus
the native error domain and code in all six cells. Both endpoint and
hostname connect callbacks now avoid a second reset after cancellation, and a
reset does not recreate a handle after socket disposal. A local regression
starts endpoint `ConnectAsync`, disposes the client while the operation is
pending, and verifies an `ObjectDisposedException` plus repeat disposal. The
async fixture passes this check in all eight Windows and Linux compiler/link-
mode cells. Socket disposal, connect-failure reset, and endpoint/hostname
connect start, wait registration, and completion now share a lifecycle mutex
around native handle use and connected-state publication. This prevents
duplicate closes, handle replacement after disposal, and completion-state
writes after disposal wins. Socket option reads/writes and the cached values
restored after a failed connect also use the mutex. Bind/listen/shutdown,
endpoint queries, async accept readiness setup, and async read/write transfer
setup hold the mutex while validating state and starting their native
operation. `Connected`, `IsBound`, and `IsListening` state reads also use it.
Synchronous hostname-connect admission is synchronized, and endpoint connect
publishes its connecting and connected states under the mutex. Blocking
connect/accept/send/receive validate state and capture the native handle under
the mutex, then release it before the blocking call.
The hostname-connect timeout is captured when the operation starts, and its
timer callback carries that operation's completion identity so a late timer
cannot time out a later connection. Timer and cancellation-registration state
publication and detachment use the mutex; timer cancellation and registration
disposal run after releasing it.
Cancellation-source unregistration compacts removed callback entries so its
active callback count returns to zero after a successful hostname connection.
A regression attaches a throwing completion callback, verifies that the
connection task remains successful and the socket remains connected, catches
the callback exception once, and checks that both timeout and caller
registration state were released. The DNS/hostname-connect fixture passes in
Windows x86/x64 and Linux x64 static/shared builds.
The same fixture disposes a hostname connect during a delayed resolver call,
checks `ObjectDisposedException` and detached caller/timer state, then waits for
the late DNS result and verifies it cannot revive the disposed socket; repeated
disposal and later caller cancellation are harmless. This case also passes in
all six Windows x86/x64 and Linux x64 static/shared cells. Linux x64 strict
GCC 11.4 and Clang 14 builds and runs pass in static/shared mode with
`-Wall -Wextra -Werror -Wno-unused-value`.
A disposal regression attaches a completion callback to a hostname connect held
in DNS; the callback reenters `Socket.Dispose()` and then throws. It verifies
the handle and cancellation state are detached, the task keeps its
`ObjectDisposedException`, and the callback exception is observed once. This
passes Windows x86/x64 and Linux GCC/Clang static/shared builds;
Linux builds use strict warnings. Native close-error injection now verifies
that disposal cancels a pending read, detaches the handle, and closes the peer
before surfacing the native error code and domain. The focused DNS async CTest
passes Windows x86/x64 and Linux GCC/Clang static/shared builds, with strict
Linux warnings.
A refused endpoint `ConnectAsync` case also verifies `ConnectionRefused`,
restored `Created` state, and a successful synchronous connection on the same
socket to a fresh loopback listener. It passes in the same six cells and strict
Linux GCC/Clang static/shared builds.
The `cxnet` Release build and TCP echo consumer build pass; broader concurrent
dispose/connect/reset/option/transfer runtime coverage remains open. The
fixture sweeps 16 scheduler-timed connect/cancel races at
delays from 0 through 34 ms, forces GC before scheduler dispatch, and checks
exactly-once cancellation and task completion. Each outcome is either canceled
and disconnected or successfully connected. Five consecutive fixture runs pass
in all Windows x86/x64 and Linux x64 static/shared cells; the expanded suite
also passes strict GCC and Clang 14 Linux x64 static/shared builds.

On 2026-11-23, the TCP echo, delayed 256 KiB HTTP download, and generated REST
CRUD examples were rebuilt and run together on Windows x64 MSVC static. The
REST check includes both its in-process handler assertions and local HTTP wire
fixture. Its README now points to the CMake-built executable under `.obj`
instead of the stale `.bin/Release` path. This is a focused example smoke, not
the full platform/link-mode acceptance matrix.

A separate managed TCP reset case creates a loopback connection, sends an RST
from its peer, and reads through public `Socket.ReadAsync`. It requires a
faulted, non-canceled task with `SocketException.ConnectionReset` and preserved
native error domain/code. It passes in all six Windows x86/x64 and Linux x64
static/shared cells and strict GCC and Clang 14 Linux x64 static/shared builds.

## Compatibility matrix

| .NET / Refit concept | CX initial contract | Deliberate boundary |
|---|---|---|
| `IPAddress`, `IPEndPoint`, `Dns` | `System.Net` values and synchronous/asynchronous OS resolver | IPv4/IPv6 stream results; configurable resolver callback is test-only |
| `Socket` | `System.Net.Sockets.Socket` with explicit family/type/protocol, bind/listen/accept/connect/send/receive/shutdown/dispose | One pending receive and one pending send may coexist per connected socket; a duplicate same-direction transfer throws `InvalidOperationException`; `ReadBytesAsync` holds the receive slot for its full enumeration; `Connected` is last observed state |
| `Socket.ReadAsync` / `WriteAsync` | Preserve existing `Task<uint>` partial transfers | Do not change to .NET's full-buffer `Socket` semantics; reject native transfer counts larger than requested; new `Receive*`/`Send*` adapters also report one transfer |
| `NetworkStream` | `System.IO.Stream` adapter over a connected socket with `ownsSocket` | Sync operations use the GC-safe blocking boundary; async reads report one transfer and async writes complete the full range, rejecting send counts larger than the remaining range; async setup failures return faulted tasks and pre-canceled tokens return canceled tasks after stream and buffer validation; operations capture the socket under a mutex so concurrent disposal cannot null the field between validation and transfer |
| `HttpClient` | `SendAsync` and verb/convenience methods, handler injection, configurable `MaxResponseContentBufferSize`, and bounded streaming | HTTP/1.1 only initially; no standalone TLS stream, HTTP/2/3, automatic retry framework, or server stack |
| HTTP message/content | Request/response, method/status/version/headers, string/byte/stream/form/multipart content | Separate content headers; `CopyToAsync` requires a writable destination for buffered and streaming content; bounded parsing and decompression; explicit response/body ownership |
| `RestClient.For<T>()` | Exact return type `RestClient`; `GetContract<T>()` obtains a generated closed-interface implementation | Compile-time ordinary classes; no runtime proxy, JIT, or reflection-based DTO guessing |
| Refit route attributes | CX `@Get`, `@Post`, `@Put`, `@Patch`, `@Delete`, `@Head`, `@Options`, path/query/header/body/form/part/auth annotations | Route and mapping errors are compile-time diagnostics; cancellation tokens are control inputs |
| REST result forms | `Task`, `Task<string>`, `Task<byte[]>`, `Task<Stream>`, `Task<HttpResponseMessage>`, `Task<RestResponse<string>>`, `Task<RestResponse<byte[]>>`, `Task<RestResponse<Stream>>`, `Task<RestResponse<scalar>>`, `Task<RestResponse<DTO>>`, `Task<RestResponse<DTO?>>`, `Task<DTO>`, `Task<DTO?>` | Materialized string/byte-array/DTO results dispose responses; stream and response-wrapper results retain ownership until disposed; nullable DTO JSON `null` maps to a null direct result or a successful wrapper with `HasValue == true`; visible closed generic DTO requests work across project references |

The generated REST return set also includes `Task<RestResponse<Stream>>`; its
wrapper completes from response headers and owns the live stream until disposal.

## Implemented HTTP model slice

`System.Net.Http` includes standard/custom methods and statuses, validated
case-insensitive headers, request/response messages, buffered and streamed
content, an injectable handler boundary, redirect handling, proxy settings,
response decompression, cancellation, and HTTP/HTTPS transport through pinned
libcurl. The response transport supports bounded buffered completion and
headers-first live streaming with a 64 KiB backpressure queue. The later
transport section records current platform evidence and the open gates; do not
treat this implementation summary as proof of full milestone completion.

## TCP API and lifecycle

Initial declarations will use these namespace/type names:

```cx
namespace System.Net;
public enum AddressFamily { InterNetwork, InterNetworkV6 }
public abstract class EndPoint;
public sealed class IPAddress;
public sealed class IPEndPoint : EndPoint;
public sealed class DnsEndPoint : EndPoint;
public static class Dns;

namespace System.Net.Sockets;
public enum SocketType { Stream }
public enum ProtocolType { Tcp }
public enum SocketShutdown { Receive, Send, Both }
public sealed class Socket : IDisposable;
public sealed class NetworkStream : System.IO.Stream;
```

`Socket(AddressFamily, SocketType, ProtocolType)` creates a socket. The existing
connected-native-handle constructor remains an internal/native integration
surface and must use the platform invalid-handle sentinel, not zero as a
universal sentinel. Initial endpoint operations are `Bind`, `Listen`,
`Accept`/`AcceptAsync`, `Connect`/`ConnectAsync`, `Send`/`SendAsync`,
`Receive`/`ReceiveAsync`, `Shutdown`, `Close`, and `Dispose`; asynchronous
operations accept `CancellationToken`. Endpoint properties are `LocalEndPoint`
and `RemoteEndPoint`; `Connected` is last observed state only. A port of zero is
allowed for bind and resolves to the allocated local endpoint.

Lifecycle is `Created -> Bound -> Listening` or `Created/Bound -> Connecting ->
Connected -> HalfClosed -> Disposed`. Bind/listen/connect state violations throw
`InvalidOperationException`; use-after-dispose throws
`ObjectDisposedException`. Connect failure returns to `Created` only after the
failed native handle and all readiness registrations have been detached; until
that cleanup is complete the object is not reusable. Accepted sockets are
independently owned, connected, nonblocking, and do not inherit listener
ownership. Closing a listener faults/cancels pending accepts and does not close
already accepted clients. Shutdown affects only selected directions. There is
at most one active receive and one active send per connected socket. Multiple
same-direction operations are rejected with `InvalidOperationException` while
the existing transfer remains pending; one receive and one send may overlap.
`AcceptAsync` calls may wait on one listening socket concurrently; each pending
call owns its readiness wait and returns a distinct accepted connection.
The focused `cxnet_socket_concurrent_accepts_succeed` case starts two accepts
before either of two clients connects, forces GC while all four operations are
pending, then verifies both accepts complete with distinct connected sockets
that receive the clients' independent payloads. It passes five consecutive
runs in all eight Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared
cells; Linux builds use strict warnings.
Canceling one pending accept leaves its peer wait active. Disposing the listener
completes every pending accept with an `ObjectDisposedException`; sockets
returned by earlier accepts retain their connected state.
Socket disposal marks the managed socket disposed before completing pending
operations, so their continuations cannot start new work on the closing handle.
Native close detaches every operation and closes the handle even if cancellation
registration cleanup or a completion callback throws; the first such exception
is rethrown after close. Host-connect timers and caller registrations are
cleared before their cleanup runs, so cleanup failures cannot leave the socket
referencing a completed connect operation.
The async socket fixture verifies that a second pending receive and a second
pending send are rejected without canceling or faulting the original transfer.
The async DNS fixture also races timed cancellation against a pending accept
while a loopback client connects. Sixteen delays from 0 through 34 ms run per
invocation; each case forces GC, checks exactly-once accept completion and
cancellation callbacks, verifies canceled-then-reused or successful acceptance,
and transfers a byte over the resulting connection. The matrix passes Windows
x86/x64 static/shared and strict Linux GCC/Clang x64 static/shared builds.
The same fixture races `Socket.ReadAsync` against `Socket.Dispose` 24 times per
invocation, accepts either a returned read task or a rejected read start,
requires returned reads to cancel, and checks `Connected` plus peer-observed
closure after disposal. A separate 24-iteration write-disposal case fills the
native send buffer with a known payload, starts a one-byte managed write and
confirms it remains pending, then disposes the socket. The write task must be
canceled, the peer must receive the full prefilled payload unchanged, and the
peer must then observe EOF. Both cases pass in Windows x86/x64 static/shared and
Linux x64 GCC/Clang static/shared builds with strict Linux warnings. Further
option/`Dispose` races cover `NoDelay`, `KeepAlive`, `ReuseAddress`,
`ReceiveBufferSize`, and `SendBufferSize` for 24 iterations each. They permit a
successful option set before disposal or `ObjectDisposedException` after
disposal, and verify the connected handle closes in either case. They pass in
all eight Windows x86/x64 MSVC and Linux GCC/Clang static/shared cells.
The failed endpoint-connect reset race also updates `NoDelay`, `KeepAlive`, and
`ReuseAddress` for 24 iterations each, checking `ConnectionRefused`, reset
state, and option access afterward; it passes in all eight cells.
The focused `cxnet_endpoint_connect_failure_reuse` test also retries
`ConnectAsync` on the successfully reused, connected socket. The duplicate
connect must fault with `InvalidOperationException` while the original
connection remains connected and transfers a byte to its accepted peer. It
passes all eight Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared
cells, including strict Linux warning builds.
The focused `cxnet_endpoint_connect_duplicate_pending` test calls
`ConnectAsync` a second time while the first task is pending. The second task
must fault with `InvalidOperationException`; after forced GC, the first task
must connect successfully, be accepted by the listener, and transfer a byte.
It passes all eight Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared
cells, with strict Linux warning builds. The full DNS/socket omnibus also passes
on Windows x64 static.
The managed `cxnet_socket_pending_read_cancel` and
`cxnet_socket_pending_write_cancel` tests cancel pending public
`Socket.ReadAsync` and `Socket.WriteAsync` operations, verify token-callback
cleanup and connection preservation, and reuse the corresponding transfer
slot; both pass Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared
(eight cells). Other connect/reset races and broader transfer races remain
open.

The `cxnet_socket_full_duplex_independent_cancel` CTest repeats 24 times with a
pending cancellable receive and a saturated pending send on the same socket. It
forces GC, cancels only the receive, verifies the write remains pending and the
connection stays open, drains the queued payload, then checks the write
completes and the receive slot accepts a later byte. It passes all eight Windows
x86/x64 MSVC and Linux x64 GCC 13.3/Clang 18.1.3 static/shared cells; Linux
builds use `-Wall -Wextra -Werror -Wno-unused-value`. Other connect/reset races
and broader transfer races remain open.

The `cxnet_socket_write_dispose_start_race` CTest repeats 24 times with a
saturated send buffer while a worker races socket disposal against
`Socket.WriteAsync` admission. Each run requires either an `ObjectDisposedException`
before a task is returned or a canceled task after admission, a closed socket,
and the exact queued peer payload followed by EOF. It passes all eight Windows
x86/x64 MSVC and Linux x64 GCC 13.3/Clang 18.1.3 static/shared cells; Linux
builds use strict warnings. Other connect/reset and broader transfer races
remain open.

Cross-thread socket close queues owner-thread cancellation for native operations.
The queued callback now captures the highest operation ID allocated before the
native handle closes, and only cancels matching-handle operations at or below
that cutoff. This prevents a callback delayed past close from canceling a new
socket operation after the OS reuses the numeric handle. The
`cxnet_socket_write_dispose_handle_reuse` CTest runs the write/dispose race
immediately before a pending receive and passes all eight Windows x86/x64 MSVC
and Linux x64 GCC 13.3/Clang 18.1.3 static/shared cells. The full Linux GCC
static DNS async fixture also passes with this ordering included.

The `cxnet_endpoint_connect_cross_thread_dispose` CTest adds 24 local endpoint
connect/dispose races per invocation, forces GC before scheduler dispatch, and
checks that a pending connect completes once with cancellation or
`ObjectDisposedException` and leaves the socket closed. It passes 20 consecutive
runs in all eight Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared
cells. The Windows readiness poller now marks only closed sockets error-ready
when `WSAPoll` reports `WSAENOTSOCK`, allowing the owning scheduler to finish
their pending operation without failing its external-event wait. Other
connect/reset races and broader transfer races remain open.

Connect uses nonblocking completion plus native completion-error inspection.
DNS must not block the executor: use an asynchronous resolver or native-owned
registered worker state. Address ordering preserves resolver order, attempts
IPv6 and IPv4 candidates under one total deadline, and closes each failed
candidate deterministically. Cancellation after accept has created a native
socket closes that socket before completing canceled. Readiness races retry the
native operation. EOF is a zero-byte receive; short reads/writes are successful
partial transfers. POSIX and Winsock errors are captured immediately and mapped
to `SocketException` with the portable `SocketErrorCode` plus the original
domain and native code. Synchronous operations may
wait only through the existing GC-safe blocking boundary and may not drive a
nested executor wait.

The `cxnet_socket_saturated_write` CTest saturates a connected socket, starts a
1 MiB `Socket.WriteAsync`, forces GC while the operation is pending, then drains
the peer and validates the returned count and exact payload. Linux reports a
positive partial count; Winsock may accept the whole requested range after the
socket becomes writable. The test passes Windows x86/x64 MSVC and Linux x64
GCC/Clang static/shared cells.

The `cxnet_socket_pending_write_cancel` and
`cxnet_socket_pending_read_cancel` CTests force GC during a pending managed
transfer, cancel it through its caller token, and verify cancellation callback
cleanup, preserved connection state, and subsequent reuse of the send or receive
slot. Both pass in the same eight cells.

The asynchronous DNS worker keeps its operation state and copied hostname
rooted, then detaches from the GC thread rendezvous while the native resolver
blocks. It reattaches before writing the result buffer and resolving the
worker/scheduler/shutdown race. The dedicated shutdown fixture blocks resolution
behind a gate, shuts down the scheduler, forces GC, then releases the resolver;
the worker exits without completing the abandoned task or invoking a late
continuation. It passes Windows x86/x64 static/shared and Linux x64 GCC/Clang
static/shared, with strict Linux warnings.

## HTTP client baseline

The public HTTP surface is in `System.Net.Http`: `HttpClient`, `HttpMethod`,
`HttpRequestMessage`, `HttpResponseMessage`, `HttpContent`, `StringContent`,
`ByteArrayContent`, `StreamContent`, `FormUrlEncodedContent`, multipart form
content, `HttpMessageHandler`, and delegating handlers. It includes request and
response headers, status/reason/protocol version, `BaseAddress`, default
headers, timeout/cancellation, buffered or headers-first completion, redirects,
decompression, explicit/system proxy, optional cookies, connection reuse, and
bounded per-origin connection, idle, and lifetime settings. The parameterless
client owns its built-in handler; `HttpClient(handler)` owns the supplied
handler by default, while `HttpClient(handler, false)` borrows it. A delegating
handler owns its inner handler. Client disposal disposes only its owned
handler. Disposing a delegating handler marks it closed even if inner disposal
throws, and the inner exception still propagates. Requests cannot be sent twice
concurrently. `HttpClient.SendAsync` resolves a relative request URI and marks
the request in-flight under the request mutex before applying its transport
snapshot, preventing concurrent sends or setters from interleaving with URI
normalization and per-request transport configuration. `DelegatingHandler`
rejects self-reference and cycles in the inner handler chain before assignment.
The request-reservation regression holds the first response at a loopback peer,
then verifies a duplicate send and `Method`/`RequestUri` setters fail while the
request is in flight. The original response completes successfully, and the
setters work again after queued send-completion cleanup clears the reservation.
The managed HTTP e2e passes this case on Windows x86/x64 and Linux x64 with
static/shared linking; Linux GCC builds use strict warning flags.
A two-worker barrier regression also races the request's `BeginSend` admission
directly. The request mutex admits one reservation and rejects the other with
`InvalidOperationException`; after `EndSend`, the request is mutable again.
The dedicated `cxnet_managed_http_concurrent_request_send_reservation` CTest
passes in all six Windows x86/x64 and Linux x64 static/shared cells, with Linux
GCC and Clang.
The dedicated `cxnet_managed_http_concurrent_request_mutation_send` CTest races
send admission against both `Method` and `RequestUri` setters. A setter either
completes before the reservation or receives `InvalidOperationException` while
the send is active; both setters succeed again after `EndSend`. It passes in
the same six cells, with Linux GCC and Clang static/shared builds.
The dedicated
`cxnet_managed_http_request_content_replacement_disposal` CTest verifies
content replacement is rejected while a request is in flight, replacement
after send completion leaves the prior content under caller ownership, and
request disposal releases only the currently assigned content. It passes in
all six Windows x86/x64 and Linux x64 static/shared cells, including strict
Linux Clang static/shared builds.
The dedicated `cxnet_managed_http_response_send_lifetime_dispose_race` CTest
races `SetSendLifetime` with response disposal. It verifies disposal detaches
the owner's lifetime reference and releases the attached disposable whether
attachment or disposal acquires the response mutex first. It passes in all six
Windows x86/x64 and Linux x64 static/shared cells, with strict Linux GCC and
Clang builds; GCC static also passes 50 repeated runs.
Cleanup catches in the HTTP and REST send paths now retain the caught exception
while attempting cleanup, then rethrow that exception even if a nested cleanup
catch clears the active exception state. The generated module and e2e fixture
pass the six-cell matrix; strict Clang 14 compilation of both also passes.
The GET/POST/PUT/PATCH/DELETE/HEAD/OPTIONS
helpers and string/byte/stream getters accept both `Uri` and string request
targets, with overloads that forward cancellation tokens or use
`CancellationToken.None`.
Public `HttpClient.SendAsync` leaves the supplied request and content owned by
the caller. Convenience verbs, generated REST calls, and `GetStreamAsync` own
their internally created requests; their disposal is registered with send
completion before transport dispatch. Generated REST result continuations are
also registered before dispatch, so setup failure occurs before a response can
become unobserved. Streamed responses retain their separate response ownership
contract.
HTTP request and base-address URIs reject embedded credentials; callers use
explicit authorization headers or the configured proxy credentials instead.
`StreamContent` owns and closes its input stream by default; construct it with
`leaveOpen: true` when the caller retains stream ownership.
Synchronous and asynchronous byte/string materialization honor its configured
buffer limit. `CopyToAsync()` accepts an optional cancellation token, with the
no-token overload using `CancellationToken.None`; it streams an unbuffered
source directly to its destination. Once materialized, even a non-seekable
source can be replayed.
If materialization fails or is canceled, seekable sources rewind to their
initial position; non-seekable sources become unusable because the consumed
prefix cannot be restored.
`CopyToAsync()` starts seekable sources at their construction-time position and
marks an unbuffered non-seekable source consumed, preventing later operations
from mistaking its remaining suffix for the full content.
The curl transport reserves an unbuffered non-seekable source during request
setup and commits consumption only after native request setup succeeds; a setup
failure releases the reservation so the same source can be retried. While
reserved, or after transport consumption, the content cannot expose the stream
for a second read, materialization, or copy.
The `cxnet_managed_http_upload_setup_failure_reservation_rollback` CTest injects
a failure after reservation and before native transport setup. It verifies the
send faults, request cleanup releases its reservation, and `StreamContent`
returns the original source afterward. It also holds a loopback GET active on
the same client while a second POST hits the injected setup failure; the GET
remains pending, then completes with its expected response after release. Both
the upload source rollback and the overlapping request are verified. The
fixture passes 20 consecutive runs in all eight Windows x86/x64 MSVC and Linux
x64 GCC/Clang static/shared cells. Linux shared outputs pass `ldd -r` with
their build runtime directories. Other stream lifecycle and HTTP concurrency
gaps remain open.
`ByteArrayContent` copies its input bytes and leaves `Content-Type` unset.
Concurrent `ReadAsStreamAsync()` calls on the same byte content share one task
and one stream. If content disposal races those calls, each caller either gets
that shared result or `ObjectDisposedException`; any stream returned before
disposal remains readable. The dedicated
`cxnet_managed_http_concurrent_content_stream_read` and
`cxnet_managed_http_concurrent_content_stream_dispose` fixtures pass in all six
Windows x86/x64 and Linux x64 static/shared cells, with strict Linux GCC and
Clang builds; the disposal race also passes 50 repeated GCC static runs. The
integrated managed HTTP e2e passes in all six cells with both fixtures included.
HTTP UTF-8 conversion operates on CX string scalar values, including
supplementary scalars, rather than combining UTF-16 surrogate pairs.
The managed HTTP e2e fixture checks a supplementary scalar's exact UTF-8
encoding and verifies that decoding returns the original CX scalar values.
The libcurl adapter suppresses its implicit form media type when request content
has no content type, preserving the `HttpContent` headers on the wire. A live
REST wire fixture verifies binary payload preservation and the absent header in
all six supported Windows x86/x64 and Linux x64 static/shared cells, including
strict Linux GCC static/shared builds.
The native HTTP types fixture verifies the default `StreamContent` constructor
owns and closes its source stream, while the `leaveOpen` overload preserves a
borrowed stream. Both assertions pass in Windows x86/x64 and Linux x64 static/
shared cells; the Linux GCC static/shared builds also pass with strict warnings.
`StreamContent.ReadAsByteArrayAsync()` now enforces the same configured buffer
limit as synchronous materialization; `StreamContent.CopyToAsync()` copies from
the input stream without first materializing the entire body. Synchronous
materialization rejects negative and over-reported source read counts.

URI construction and relative resolution use a common `System.Uri` contract;
HTTP code never joins URLs with `Path` or ad hoc string concatenation. Accept
only `http` and `https`, reject CR/LF header injection and ambiguous framing,
and separate content headers from general headers. HTTP/1.1 must support fixed,
chunked, and close-delimited response bodies, interim responses, HEAD/bodyless
statuses, persistent connections, and bounded headers/content. HTTPS validates
chain and hostname, sends SNI, and reports the active trust-store behavior.
Negotiation is restricted to HTTP/1.1 in this release.

Live streamed bodies retain their connection until consumed or disposed. A
partially consumed response is drained only within configured bounds; otherwise
the connection is closed, never returned to the pool with unread bytes.
Headers-first completion permits body consumption before EOF. Upload/download
buffers and decompression are bounded. Redirects have a finite limit, strip
credentials on cross-origin changes, reject HTTPS downgrade by default, and do
not replay non-seekable bodies. A replay request remains locally owned until
the redirect dispatcher accepts it; failures during request/content setup,
header copying, or response and prior-request cleanup dispose partial replay
objects and their content. Redirect-hop setup failures also complete the caller
task after independently attempting `EndSend()` and request disposal, so a
cleanup error cannot strand the send. Automatic
retry of non-idempotent methods is excluded. `SendAsync` returns ordinary 4xx/5xx responses; convenience content
helpers and `EnsureSuccessStatusCode` have explicit status behavior.

Redirect resolution inherits the request URI's fragment only when `Location`
omits a fragment delimiter. An explicit nonempty or empty fragment replaces
the original fragment; `System.Uri.Fragment` retains `"#"` for the empty case.

HTTP upload cancellation fixtures cover two distinct orderings: cancellation
after a partial body transfer, and cancellation after the complete streamed
request body has reached the server while its response is still pending. Both
require a canceled caller task and transport closure. The latter 64 KiB case
passes in Windows x86/x64 and Linux x64 static/shared cells, plus strict GCC and
Clang 14.0.6 Linux x64 static/shared builds. A separate 8 MiB streamed upload
race schedules caller cancellation before and after the 1-second
`HttpClient.TimeoutMilliseconds` deadline. It checks that exactly one source
wins, verifies the expected winner, forces GC while the race is pending, and
confirms that the peer sees a closed connection after only a partial upload.
The focused CTest passes all eight Windows x86/x64 MSVC and Linux x64 GCC/Clang
static/shared cells; Linux runs use strict warnings. The regular integrated
managed HTTP end-to-end CTest also passes all eight cells with both race
orderings enabled. The focused race also passes three consecutive runs in
each of the eight cells. Further
upload/cancellation-order races remain part of the open HTTP lifecycle
coverage.

## REST contract baseline

The first CX API surface now defines `Get`, `Post`, `Put`, `Patch`, `Delete`,
and `Head` verb annotations; `Path`, `AliasAs`, `Query`, `QueryMap`, and `Header` parameter
bindings; `Body`, `FormField`, and `MultipartPart` bindings; and interface/method
`StaticHeader` and `Authorize` annotations. `RestResponse<T>` owns its wrapped
`HttpResponseMessage` until disposal, exposes status/headers/content, and carries
an optional materialized value plus bounded error text. `ApiException` extends
`HttpRequestException` with status, request URI, response headers, bounded error
content, and an optional inner exception. `ResponseStatusCode` exposes the
non-null status directly for CX callers. A newly constructed `RestResponse<T>`
must be initialized with `SetResponse` before its status, headers, content, or
success-status properties are read, and before `SetHasValue` or
`SetErrorContent` is called. These reads and initialization setters throw
`InvalidOperationException` while no response is attached. The first CX
generator slice emits ordinary implementations for explicitly requested
contracts with `Task`,
`Task<HttpResponseMessage>`, `Task<string>`, `Task<byte[]>`, `Task<Stream>`, and
`Task<RestResponse<string>>`, `Task<RestResponse<byte[]>>`, and
`Task<RestResponse<DTO>>` methods, plus
`Task<DTO>` and `@Body DTO` when the DTO is `@Serializable` and the CXDATA JSON
generator is configured. Closed generic DTOs work for same-project definitions
and public definitions from referenced projects; the consuming project emits a
local codec for each closed generic use visible in its declaration snapshot.
Generated code calls the CXDATA codec directly; `cxnet` has no `cxdata`
dependency. The generator supports scalar path/query/header parameters,
including `System.Uuid` values formatted with the standard hyphenated UUID
representation. The generated `rest-crud` client compiles in all eight Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared build cells. Its in-process UUID path assertion and loopback UUID wire check pass in all eight cells; Linux GCC and Clang builds use strict warnings.
The recording REST fixture references `RestModels` as a separate project. Its
derived contracts inherit both ordinary and closed-generic REST methods from
that library; the fixture verifies base routes, generic query encoding, and
materialized results through generated implementations. The current
`cxnet_managed_rest_generator_recording` CTest passes Windows x86/x64 MSVC and
Linux x64 GCC 11.4/Clang 14 static/shared builds, with strict Linux warning
flags.
Generated query values are inserted before route fragments, which remain on the
managed request URI while the HTTP transport omits fragments from the wire
target. The recording-handler and loopback checks pass in all eight Windows
x86/x64 MSVC and Linux x64 GCC/Clang static/shared cells. Linux GCC and Clang
fixture binaries pass the live-wire check with the current fixture.
Collection immutability is expressed by applying `const` to the standard
collection type. Generated contracts preserve that type and qualifier (for
example, `const List<T>` and `const Dictionary<string, List<T>>`); response
materialization constructs the corresponding standard collection type.
DTO collection results accept `List<DTO>`, `Array<DTO>`, `LinkedList<DTO>`,
`Queue<DTO>`, and `Stack<DTO>`, including `const`-qualified results and
`RestResponse` wrappers. Generated materializers preserve the declared
standard collection type and dispose the parsed JSON document after
materialization. Stack results are populated in reverse JSON order so the
standard top-first stack enumeration preserves the JSON array order.
Direct and wrapped nullable outer collection results (for example,
`Task<const List<DTO>?>` and
`Task<RestResponse<const List<DTO>?>>`) accept a JSON `null` root. Direct
live-wire coverage and recording-handler coverage for both null and populated
values pass in all eight Windows x86/x64 MSVC and Linux x64 GCC/Clang
static/shared cells, including strict Linux builds. Wrapped results retain
`HasValue == true`, return null from `GetValue()`, and keep response content
owned until wrapper disposal. The nullable outer result matrix covers
`const List<DTO>?`, `const Array<DTO>?`, `const LinkedList<DTO>?`,
`const Queue<DTO>?`, and `const Stack<DTO>?`.
Mutable and const-qualified `Array<DTO>` responses, including a wrapped const
result, pass recording-handler checks in the Windows x86/x64 and Linux x64
static/shared cells. Direct and wrapped `const Array<DTO>` routes also pass
live-wire checks in all six cells, including strict Linux GCC and Clang
static/shared builds. Direct and wrapped `const LinkedList<DTO>`,
`const Queue<DTO>`, and `const Stack<DTO>` live-wire routes pass in all six
Windows x86/x64 and Linux x64 static/shared cells; the Linux cases also pass
strict GCC and Clang static/shared builds.
Generated bindings support repeated query keys for supported scalar arrays,
`Array<T>` (including
`const Array<T>`), `List<T>`, `IEnumerable<T>`,
and `ICollection<T>`, plus `@QueryMap` over insertion-ordered
`Dictionary<string, T>` values for supported scalars or collections of supported
scalars. Each collection element becomes a repeated pair for its key, preserving
map and collection order; keys and values are escaped once. Nullable
string query/header omission, string or `HttpContent` bodies, scalar and repeated
URL-encoded form fields, multipart string/content/scalar parts, nullable
string/scalar omission with empty strings preserved, and repeated string or
supported scalar sequences with the same field name and no shared filename.
Generic `Array<T>` collections are also
accepted for repeated scalar bindings; the live wire fixture checks
`const Array<int>` form encoding and `const Array<string>` query, header, and
multipart order in all six Windows/Linux static/shared cells and strict Linux
GCC/Clang builds. A `Dictionary<string, Array<string>>` query map also verifies
escaped key/value encoding and repeated value order in that matrix. Static
interface/method
headers, and cancellation forwarding. Interface static
headers are emitted before method static headers; method values replace matching
interface values, and dynamic `@Header` parameters replace either. A
recording-handler consumer checks escaped string and numeric path/query values,
repeated and escaped collection query values for arrays, lists, and collection
interfaces, escaped scalar and collection-valued query maps, static/dynamic header
precedence and optional omission, interface/method `@Authorize` schemes,
inherited methods and base/derived headers, empty maps, string/raw/DTO body forms, DTO JSON round-trips,
URL-encoded forms, repeated form-key ordering and escaping, multipart
filename/content handling and repeated string-part ordering for arrays and lists,
task completion,
and request/content disposal through the CX scheduler. Same-name overloaded
contract methods with numeric versus string query parameters are checked against
distinct request targets on the live wire in all six Windows x86/x64 and Linux
x64 static/shared cells, plus strict Linux GCC and Clang 14.0.6 static/shared
builds. Generated request setup now guards body, form, multipart, header, and
authorization assembly with request disposal on synchronous failure. The
recording fixture injects an invalid dynamic header after attaching a
`StreamContent` body and confirms stream disposal in all six Windows x86/x64 and
Linux x64 static/shared cells. Multipart content is attached before parts are
added so a later setup failure also releases already attached parts. The
recording fixture injects an invalid dynamic header after a `StringContent`
multipart part is attached and verifies setup rejects the header and disposes
the part. This regression passes in all eight Windows x86/x64 MSVC and Linux
x64 GCC/Clang static/shared cells. Collection-valued query
maps are covered with `List<string>`, `IEnumerable<int>`, and `string[]` values;
escaped keys, repeated values, insertion order, and empty collection entries
pass on Windows x64/x86 and Linux x64. An empty string `@Body` wire call also
requires zero body bytes, `Content-Length: 0`, no transfer encoding, and the
UTF-8 text media type; it passes all six static/shared cells and strict Linux
GCC/Clang static/shared builds. Nullable string `@Body` parameters omit request
content when null; empty strings remain explicit zero-length text content.
Recording and live-wire checks pass in all six Windows x86/x64 and Linux x64
static/shared cells, including strict Linux GCC static/shared builds.
Nullable `@Body const byte[]?` parameters omit content when null and preserve
an explicit empty `ByteArrayContent` when given an empty array. Recording checks
cover both states, and the wire fixture verifies a populated binary body in all
six Windows x86/x64 and Linux x64 static/shared cells, including strict Linux
GCC static/shared builds. The Linux
GCC Release fixture passes with
`-Wall -Wextra -Werror`, suppressing existing vendored libyaml `unused-value`
diagnostics. The CX compiler now casts closed generic reference parameters in
direct interface dispatch thunks, avoiding the Win32 dictionary pointer warning;
its test suite passes 1543/1543. `HttpMessageHandler` exposes an explicit public
constructor so consumer-defined handlers can subclass it without an undeclared
base-constructor call. Typed string, byte-array,
and DTO results dispose successful responses and raise `ApiException` on
unsuccessful status, with a 4096-character error-body bound. Stream results
retain the response until stream disposal; string response-wrapper results
preserve status/error details and response ownership until caller disposal. DTO
response wrappers preserve HTTP status and headers for both success and error,
materialize successful JSON values, bound error content to 4096 characters, and
retain response ownership until caller disposal; the recording-handler fixture
checks both outcomes and disposal. Headers-first stream errors read a bounded
16 KiB prefix asynchronously, trim incomplete trailing UTF-8 only when the
prefix limit cuts through a code point, and retain at most 4096 decoded
characters; malformed UTF-8 at an earlier end-of-body remains a decode error.
Stream response wrappers keep ownership of the response after prefix reading.
Buffered REST error paths decode the same
bounded prefix before creating `ApiException` or wrapper error content, while
successful body decoding remains complete. Byte-array and materialized stream
content decode the prefix directly from backing storage without cloning the
entire error body. A prefix-read operation mutex serializes buffer updates with
terminal state changes and cleanup, so success, cancellation, and failure
complete the task and release buffers once. If an error prefix cannot be read
or decoded, direct result
methods preserve the HTTP status in `ApiException` and attach that failure as
its inner exception; response wrappers retain status and use empty error text.
Generated DTO results require
`application/json` or a structured `+json` media type before decoding; media type
parameters such as `charset` are allowed. Missing or non-JSON types fault the
generated task and dispose the response. The fixture covers parameterized JSON,
`application/problem+json`, and rejection of `text/plain`. An empty successful
`RestResponse<DTO>` has no value and retains its status/response; direct DTO
returns fault when the body is missing or empty. A no-content response-wrapper
case verifies the 204 behavior. Byte-array response wrappers preserve status,
headers, and response ownership while materializing successful bytes; error
responses retain bounded text and expose no value. The recording-handler fixture
checks success, HTTP error, and content disposal, and passes in all six Windows
x64/x86 and Linux x64 static/shared cells. Closed `@Serializable` generic DTOs use the
generated CXDATA codec request; the fixture covers same-project
`RestEnvelope<string>` and referenced-project `RestModels.GenericEnvelope<string>`
as serialized bodies, direct results, and `RestResponse<T>` materialization. A
separate referenced model project provides non-generic and closed generic
annotated base interfaces; the consumer fixture verifies inherited dispatch,
query parameter substitution, and a `Task<T>` result closed to `Task<string>`.
The fixture also checks closed generic top-level REST contracts, including
separate string and integer constructions of one interface, and discovers a
closed contract when `RestClient.For<T>()` is forwarded through a generic
consumer helper and a generic factory in a referenced project. Open generic
contract requests are intentionally unsupported; the invalid-project fixture
checks that one is rejected with a source-located diagnostic before generation.
Additional end-to-end combinations across the supported return shapes remain
part of the REST acceptance work.
The recording-handler fixture also closes `IGenericRestApi<T>` over a serializable
DTO and verifies direct `Task<T>` and wrapped `Task<RestResponse<T>>` result
materialization, route substitution, wrapper value presence, and response
content disposal. The live-wire fixture also verifies direct and wrapped DTO
results through the default HTTP transport, including status/header preservation.
Recording and live-wire checks pass all eight Windows x86/x64 MSVC and Linux x64
GCC/Clang static/shared cells.
Generated DTO bodies and results use their CXDATA codec by default. A client can
register an `IRestSerializer<T>` through
`RegisterSerializer(new RestSerializerRegistration<MyDto>(typeof(MyDto),
serializer, serializer.ContentType))` to override that codec or support an
otherwise unannotated public DTO. The serializer
declares its media type; generated requests use it, and generated results check
the response media type against it, allowing parameters such as `charset`.
`RestClient.SerializerOptions` supplies an opaque options object to every
registered serializer on that client. Serializer registration and options are
configurable before the first request and frozen afterward. A per-client mutex
orders generated-method entry against these configuration writes, so a request
cannot start assembling bindings while a concurrent setter is still changing
the client. Public serializer lookups and serialization calls now snapshot the
registration and options under the same mutex; serializer code runs after the
lock is released. The `BaseAddress` getter reads the address under that mutex
as well, so concurrent `RestClient.Dispose()` cannot dispose the `HttpClient`
between client acquisition and the property read. The fixture covers
custom serialization and deserialization of an unannotated DTO, replacing a
generated JSON codec for an annotated DTO, and rejecting a mismatched response
media type. A recording-handler fixture starts two requests on one generated
client before completing either task and
verifies each result remains associated with its own route.

The generated REST wire fixture also sends a multipart string part through the
default HTTP transport. Its local HTTP/1.1 server checks the multipart media
type and boundary, content disposition, part media type, exact bytes, and
generated static/auth headers. A 409 wire response also verifies generated
`RestResponse<string>` status, retained error headers, bounded error text, and
no materialized value. A generated `@Body HttpContent` request also streams a
256 KiB patterned body across the transport ring buffer; the server verifies
every byte in order. A generated `Task<byte[]>` response materializes and checks
every byte of a 96 KiB body, larger than the 64 KiB transport queue capacity.
A generated `RestResponse<RestItem>` wire response checks DTO materialization,
status, and a retained response header. A generated
`Task<RestResponse<Stream>>` response uses headers-first completion: the server
holds its 32 KiB body until a second request arrives, then the consumer checks
status and content headers, reads asynchronously through EOF, and verifies every
byte before disposing the wrapper. A separate 204 route preserves status with
`HasValue == false`. These cases pass in all six Windows x86/x64 and Linux x64
static/shared fixture cells, plus strict Linux GCC and Clang static/shared
builds. Disposing the wrapper during a pending body read cancels the read and
closes the client connection, also verified in that matrix.
Nullable DTO result contracts support direct `Task<RestItem?>` and wrapped
`Task<RestResponse<RestItem?>>` returns. JSON `null`, including surrounding
whitespace, maps to a null direct result; the wrapper retains the HTTP response
and reports `HasValue == true`. Populated DTO results, empty-body behavior,
status, and wrapper headers are covered by recording-handler and live-wire
fixtures across all six Windows x86/x64 and Linux x64 static/shared cells, plus
strict Linux GCC and Clang static/shared builds.
The form binding also sends two `@FormField("role")` values as
`ICollection<string>`, `IEnumerable<string>`, `string[]`, `List<string>`, and
`const List<string>`. The server checks the exact ordered body
`role=admin+user&role=a%26b` for each shape. Const-qualified standard
collections are covered across query, header, form, and multipart bindings:
`const List<T>` exercises all four, with `const LinkedList<T>`, `const Queue<T>`,
and `const Stack<T>` exercising query, form, and multipart. The fixtures verify
encoded values and order across all six Windows x86/x64 and Linux x64 static/
shared cells, plus strict Linux GCC and Clang 14.0.6 static/shared builds.
The REST CRUD contract also compile-checks const-qualified linked-list, queue,
and stack header parameters through generated implementations.
Dictionary query maps continue to use the existing supported scalar and
standard collection value types.
The default-`HttpClient` wire fixture registers a custom serializer for an
unannotated DTO and checks the request/response media types, exact request
payload, and materialized fields. It also registers a serializer for an
annotated DTO and verifies its custom payload, media type, and response
materialization in place of the generated CXDATA codec. The recording-handler
fixture covers this override as well. Both cases pass in all six Windows x86/x64
and Linux x64 static/shared cells and strict Linux GCC and Clang 14.0.6
static/shared builds.
Two generated `Task<string>` calls also overlap on one REST client. The local
server holds the slow route until the fast request arrives, and the fixture
checks each task receives its matching body. This same-client concurrency case
passes all six Windows x86/x64 and Linux x64 static/shared cells, plus strict
Linux GCC and Clang 14.0.6 static/shared builds.
Nullable query/header wire checks verify null omission, preservation of an empty
query value as `q=`, and transmission of an explicitly empty header field.
They pass in the same six cells and strict Linux GCC and Clang static/shared
builds.
The recording-handler fixture verifies `RestClient` disposal ownership: a
borrowed wrapper leaves its supplied client usable, and an owning wrapper closes
its supplied client. It passes the same six cells and strict Linux GCC and
Clang 14.0.6 static/shared builds.
The local server also verifies generated PUT, PATCH, DELETE, and HEAD requests,
including their route, response status, generated headers, and response body
semantics; this verb matrix passes in all six fixture cells.
A live 400 DTO response with a 5,000-character body verifies the typed
`ApiException.ResponseStatusCode`, request URI, retained response header, and
the 4,096-character bound in all six fixture cells and strict Clang 14.0.6
Linux x64 static/shared builds. Two live 204 routes also verify that a
`RestResponse<RestItem>` preserves the status/header without a value, while a
direct DTO result faults on the missing JSON body; both pass in the same matrix.
Generated REST contracts now also materialize boolean and primitive numeric JSON
results directly and through `RestResponse<T>`. The live wire cases cover direct
`int`, `bool`, and `long` values, a maximum 64-bit integer, wrapped success
status/header preservation, an empty 204 wrapper, wrapped error content without
a value, and malformed numeric JSON. They pass all six Windows x86/x64 and
Linux x64 static/shared cells and strict Linux GCC and Clang static/shared
builds. `RestResponse<T>` keeps its shared response state ahead of its generic
value field so non-generic property accessors retain stable field offsets for
primitive specializations.
The generated contract also verifies caller cancellation after receiving a
body prefix: cancellation faults the pending stream read as canceled, and the
loopback server observes connection closure. This case passes in all six
Windows x86/x64 and Linux x64 static/shared fixture cells.
It also verifies explicit disposal of the generated stream while another body
read is pending: disposal cancels that read and the server observes connection
closure. The disposal case passes in all six Windows x86/x64 and Linux x64
static/shared fixture cells. The cancellation case immediately disposes the
stream after canceling the pending read and still requires a canceled task;
this cancellation/disposal overlap also passes in all six cells.

`RestClient.AuthorizationProvider` is configured before the first request and
receives the selected scheme, returning the scheme-specific credential. The
generated client writes `Authorization: <scheme> <credential>`; a method-level
`@Authorize` overrides the interface annotation. Missing or empty credentials
throw before transport dispatch. Schemes must be non-empty HTTP tokens; duplicate
`@Authorize` annotations or malformed static schemes produce `CXREST021`, and
the public runtime API validates dynamic schemes before calling the provider. A
dynamic `@Header("Authorization")` is
replaced when `@Authorize` is present.
`@Header` and `@StaticHeader` names must be HTTP tokens; static values reject
control characters other than horizontal tab. Dynamic `@Header` values pass
through `HttpHeaders.Add`, which rejects line breaks and disallowed controls.
REST contracts cannot set `Content-Length` or `Transfer-Encoding`; request
framing remains owned by the transport. Invalid bindings produce `CXREST022`
during generation.

`RestClient.For<T>()` is a builder returning `RestClient`; it does not perform
network I/O or return `T`. `GetContract<T>()` returns a generated implementation
of the configured interface, rejects a different contract type, and is usable
only after its settings are frozen on the first request. A client constructed
with an existing `HttpClient` carries an explicit ownership flag. The generated
implementation is an ordinary CX class specialized for closed interface
contracts at compile time. Non-generic inherited methods are included, with
diagnostics for duplicate inherited signatures; non-generic inheritance from
referenced projects and closed generic base interfaces are supported. Closed
generic top-level REST contracts are generated per requested construction;
open generic contracts remain unsupported. The compiler rejects a
`RestClient.For<OpenGenericContract<T>>()` request with an unresolved type
argument at the source before generator/native output; the dedicated fixture
passes that rejection check. Each generated method stores its
verb and route in immutable static constants and
emits length-prefixed constants for parameter bindings, static headers,
authorization, and JSON serializer entry points. Generated code binds runtime
parameter values into a fresh request; no descriptor object is allocated at
module startup.
Generated contract factories share a process-wide registry protected by a mutex
initialized during `cxnet` module initialization. Registration and lookup hold
that mutex while accessing the registry, allowing independent consumer modules
to initialize or request contracts concurrently. Registration is idempotent by
closed contract type: the first generated factory is kept and later registrations
for that type are ignored, so repeated emission across referenced modules does
not fail module initialization.

The annotation vocabulary covers HTTP verbs/routes, path aliases, query values,
static and dynamic headers, body, form/multipart, and authorization. The initial
generator supports `@Get`, `@Post`, `@Put`, `@Patch`, `@Delete`, `@Head`, and
`@Options` routes, and binds scalar path/query/header values using invariant core
formatters. The REST CRUD consumer compiles a generated `OPTIONS` method; the
wire fixture verifies its request and response in all six Windows x86/x64 and
Linux x64 static/shared cells. `char` is supported for path, query, header,
form, multipart, and
JSON scalar-response bindings. A JSON `char` response must contain exactly one
Unicode scalar; standard JSON escapes and valid surrogate pairs are decoded.
Generated request bindings validate that each `char` is a Unicode scalar before
formatting it, so invalid cast values fail consistently before reaching a
transport encoder.
The `rest-crud` contract compile-checks `char` path/query/header bindings,
`const List<char>` query/form/multipart bindings, and a `Task<char>` result.
The wire fixture checks UTF-8 path, query, form, and multipart formatting plus
escaped JSON `char` response parsing. These cases pass in all six Windows
x86/x64 and Linux x64 static/shared cells.
Direct JSON scalar results also cover the full primitive numeric parser set:
`byte`, `sbyte`, `short`, `ushort`, `int`, `uint`, `long`, `ulong`, `float`,
`double`, and `decimal`. New live-wire cases exercise all seven previously
missing types, including the maximum `ulong`, both directly and through a
wrapped `RestResponse<ulong>`. They exposed an `int` fallback that prevented
small integer result types from compiling; generated `TryParse` fallbacks now
use the declared numeric type. The new wire cases pass on Windows x86 static
and x64 and Linux GCC/Clang static/shared. The recording, multipart setup
failure, and generated-result continuation-registration CTests pass all eight
Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared cells.
Nullable direct and wrapped results now also cover `short?`, `ushort?`, and
`uint?` JSON `null` and signed-minimum/unsigned-maximum boundaries. Recording
and live-wire checks verify values, wrapper status and headers, and response
disposal in all eight Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared
cells; Linux builds use strict warnings and shared loading has no unresolved
symbols.
Repeated query keys work for supported scalar arrays, `List<T>`,
`LinkedList<T>`, `Queue<T>`, `Stack<T>`, `IEnumerable<T>`, and
`ICollection<T>` values, nullable string and scalar query/header
omission, string or raw `HttpContent` bodies, scalar or repeated scalar
`FormField` values through
`FormUrlEncodedContent`, `@QueryMap` over string-keyed dictionaries of supported
scalars or scalar collections (including `const Dictionary<string, T>`), and
`MultipartPart` string, content, supported scalar, and supported scalar-sequence
values; nullable string/scalar parts are omitted when absent, while empty strings
remain explicit parts. Null elements in supported nullable scalar sequences are
omitted for query/header/form/multipart bindings. A nullable sequence or query
map is also omitted as a whole when null. Optional filenames apply to single
parts. `@QueryMap`
omits the query delimiter for an empty dictionary
or empty collection values and rejects non-string keys or unsupported value
types with `CXREST001`; a recording-handler fixture verifies escaped scalar and
repeated collection-valued keys/values, including arrays, lists, enumerable
values, and `ICollection<string>` values backed by a list. The live wire fixture
also verifies LinkedList, Queue, and Stack query sequences and collection-valued
query maps. Nullable scalar `@QueryMap` values now omit null entries while
preserving empty strings as `key=`. Recording-handler and live wire fixtures
verify null-element omission, escaping, empty-string preservation, and order for
nullable-element sequences across query, header, form, and multipart bindings.
These cases pass on Windows x86/x64 and Linux x64 with static and shared
linking, including strict Linux GCC and Clang static/shared builds. Recording
and live-wire fixtures now cover nullable scalar query-map values, nullable
collection values and elements, and nullable whole-map omission. Null scalar
values and null collection values are omitted; null sequence elements are
skipped; empty strings are preserved, and a null map adds no query delimiter.
The generator copies nullable values to locals before coalescing and reads map
counts through the existing `ICollection<KeyValuePair<string, T>>` contract,
avoiding incompatible C pointers for nullable generic dictionary values.
These cases pass in all six Windows x86/x64 static/shared cells and strict
Linux GCC and Clang x64 static/shared builds, with recording and live-wire runs
in each cell.

JSON object responses also support nullable collection values, such as
`Dictionary<string, List<RestItem?>?>`, including nullable DTO elements inside
the list. Direct and wrapped recording-handler and live-wire cases cover
populated, empty, and null collection values, plus wrapper metadata and content
ownership. The generated parser inserts the completed list directly into the
map so emitted C does not create an invalid nullable collection temporary. The
fixture passes all eight Windows x86/x64 MSVC and Linux x64 GCC/Clang
static/shared cells; Linux GCC uses strict warnings-as-errors.

`RestResponse<const Dictionary<string, RestItem?>>` is also covered for
nullable DTO map values. Recording and live-wire responses retain a populated
DTO and a null entry, and verify wrapper metadata and content ownership through
disposal in all eight Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared
cells.

The REST generator reports `CXREST014` for literal multipart `name` and
`filename` values that are empty where disallowed or contain control characters,
quotes, or backslashes. `MultipartFormDataContent` repeats these checks at
runtime before building the quoted `Content-Disposition` field. The focused
`cxnet_managed_http_multipart_disposition_validation` CTest checks null and
empty field names, C0 controls, DEL, quotes, and backslashes for both fields
and filenames, and verifies the `ArgumentException.ParamName`. It passes
Windows x86/x64 and Linux x64 static/shared builds, including strict Linux GCC
and Clang builds. Valid multipart serialization remains covered by the REST
wire fixture.
LinkedList, Queue, and Stack header, form, and multipart bindings
preserve enumeration order on the wire. Queue order is FIFO; Stack order is top-first.
Wire checks cover repeated `int[]` queries and invariant `bool`, `double`, and
`decimal` query formatting (`enabled=True&ratio=-1.25&amount=0.125`) in all six
Windows x86/x64 and Linux x64 static/shared cells, plus strict Linux GCC and
Clang 14 static/shared builds.
Nullable `int?` and `decimal?` query values and a nullable `int?` header also
pass omitted and populated live-wire cases in all six Windows x86/x64 and Linux
x64 static/shared cells, plus strict Linux GCC and Clang 14 static/shared builds
with `-Wall -Wextra -Werror -Wno-unused-value`.
Nullable numeric elements in const-qualified `List<T>` query and
`Array<T>` header sequences now omit nulls and preserve invariant formatting.
The C backend now uses the closed array setter parameter type when lowering
`Array<T>.Set`, so non-null values assigned to nullable element arrays retain
the nullable storage layout. The focused compiler regression passes. REST
recording and live wire fixtures pass in all six Windows x86/x64 and Linux x64
static/shared cells; Linux GCC and Clang also pass strict static/shared builds
with `-Wall -Wextra -Werror -Wno-unused-value`. Expanded nullable numeric
sequence coverage verifies query and header bindings for const `Array<T>`,
`List<T>`, `LinkedList<T>`, `Queue<T>`, `Stack<T>`, `IEnumerable<T>`, and
`ICollection<T>`, plus FIFO form values through `Queue<int?>` and top-first
multipart values through `Stack<decimal?>`. Null elements are omitted and
populated values retain invariant formatting and sequence order. Recording and
wire checks pass in all six Windows x86/x64 and Linux x64 static/shared cells,
including strict Linux GCC and Clang static/shared builds. Generated REST clients
also materialize direct and wrapped JSON object responses as the declared
`Dictionary<string, DTO>` type, preserving `const` qualification. Object roots
are required, duplicate JSON property names are rejected, and response content
is disposed on both success and failure. Recording-handler and live-wire runs
pass on Windows x86/x64 and Linux x64 with GCC and Clang in static and shared
configurations; Linux uses strict warning flags. Broader REST bindings and
return shapes remain open.
REST JSON objects with non-null scalar values are now generated as standard
`Dictionary<string, T>` results as well. String values use the JSON string
reader; supported scalar values use the strict scalar response parser. Direct
and `RestResponse<T>` forms preserve `const`, reject duplicate keys, and dispose
the content. Recording-handler and live-wire coverage exercises escaped string
values and wrapped positive/negative integer values.
These cases pass the recording and live-wire runs on Windows x86/x64 MSVC and
Linux x64 GCC/Clang, in static and shared configurations with strict Linux
warnings.
Nullable integer values are also supported for JSON object responses as
`const Dictionary<string, int?>`, directly and under `RestResponse<T>`. The
recording-handler and live-wire checks preserve a JSON null alongside positive
and negative integers; the wrapped case checks metadata and content ownership.
The focused Windows x64 Release recording and wire runs pass.
Nullable UUID object values are now covered directly and through
`RestResponse<const Dictionary<string, Uuid?>>`. Recording and live-wire
fixtures preserve canonical UUID strings around a null entry; wrapped results
also check metadata and content disposal. Both modes pass in all eight Windows
x86/x64 MSVC and Linux x64 GCC/Clang static/shared cells.
JSON objects may also map property names to standard collection values:
`List<T>`, `Array<T>`, `LinkedList<T>`, `Queue<T>`, and `Stack<T>`. The generated
client validates each value as an array, materializes each item, and preserves
the declared dictionary/collection `const` types, including `RestResponse<T>`
wrappers. The full fixture now builds warning-free in all eight Windows x86/x64
MSVC and Linux x64 GCC/Clang static/shared cells. Recording-handler and
live-wire checks verify populated and empty maps, wrapper metadata, response
ownership, malformed collection values, and duplicate keys across those cells.
Linux GCC uses `-Wall -Wextra -Werror`; Linux Clang uses `-Wall -Wextra`.
The const-qualified `Dictionary<string, List<string>>` query map preserves
escaped keys, repeated collection values, and insertion order in all six Windows
x86/x64 and Linux x64 static/shared cells. Strict Linux GCC/Clang builds for
this addition pass in both static and shared modes with the same warning flags.
Nullable `int?`, `decimal?`, and `bool?` form values omit absent fields and keep
invariant formatting for present fields; live-wire checks pass in the same six
cells and strict Linux GCC/Clang static/shared builds.
Nullable scalar form sequences now cover FIFO `Queue<int?>`, ordered
`LinkedList<decimal?>`, and top-first `Stack<bool?>`. Null elements are omitted,
decimal and boolean values keep invariant formatting, and recording-handler plus
live-wire checks pass in all eight Windows x86/x64 MSVC and Linux x64 GCC/Clang
static/shared cells.
Nullable DTO `@Body` parameters omit content when null and serialize non-null
values with the generated JSON codec. Recording-handler and live-wire cases
check both states in all six Windows x86/x64 and Linux x64 static/shared cells,
plus strict Linux GCC and Clang 14 static/shared builds.
Nullable `HttpContent?` `@Body` parameters also leave request content unset
when null and preserve a supplied `HttpContent` when present. The focused
fixture passes recording-handler and live-wire checks in all six Windows
x86/x64 and Linux x64 static/shared cells, including strict Linux GCC and
Clang 14 builds. The handler verifies null content and supplied-content
identity. For a null POST, the default transport emits `Content-Length: 0` with
no content type or transfer encoding, and populated content preserves the exact
bytes.
The full generated REST fixture also passes with nullable DTO body cases in all
six Windows x86/x64 and Linux x64 static/shared cells, plus strict Linux GCC and
Clang 14 static/shared builds. Recording-handler and local wire runs pass in
those cells. The generator assembles each contract's output with `StringBuilder`
to avoid repeatedly copying the accumulated source for large contracts.
The query-map
collection fixture passes in Windows x64/x86 and Linux x64 static/shared builds.
Collection-valued `@Header` bindings also support arrays, lists, linked lists,
queues, stacks, `IEnumerable<T>`, and `ICollection<T>`; generation emits duplicate header fields
in element order. The recording-handler and live wire fixtures verify the
ordered values in all six Windows x86/x64 and Linux x64 static/shared cells,
plus strict Linux GCC and Clang builds.
`const IEnumerable<T>` and `const ICollection<T>` preserve `const` in the
generated implementation signatures and are exercised for query, header, form,
and multipart bindings. Generated signatures use the standard collection types
directly, with `const` providing read-only access.
`RestClient.For<T>()` requests targeting non-public or non-interface
types, and interfaces with no HTTP-annotated methods, fail with source-located
generator diagnostics. Scalar query/header/form/multipart bindings cover arrays,
`List<T>`, `LinkedList<T>`, `Queue<T>`, `Stack<T>`, `IEnumerable<T>`, and
`ICollection<T>`; dictionary query maps accept scalar and scalar-collection
values. JSON response collections
now accept non-null scalar and string elements directly and under
`RestResponse<T>`; the recording fixture verifies `const Array<int>`,
`const Array<string>`, and `const List<int>` in direct and wrapped results,
including escaped strings, wrapper metadata, and response ownership in all six
runtime cells and strict Linux GCC/Clang static/shared builds. Live-wire checks
cover populated direct and wrapped `Array<int>`, `List<int>`, `LinkedList<int>`,
`Queue<int>`, and `Stack<int>` results in sequence order, including wrapper
metadata and disposal. The recording fixture now also verifies direct and
wrapped `const LinkedList<int>`, `const Queue<int>`, and `const Stack<int>`
results, sequence order, wrapper status/headers, and response-content disposal
in all eight Windows x86/x64 and Linux GCC/Clang static/shared cells. Direct and
wrapped `Array<string>`, `List<string>`,
`LinkedList<string>`, `Queue<string>`, and `Stack<string>` results preserve
escaped strings and sequence order, with wrapper status, headers, and disposal
checked. Direct and wrapped DTO results are covered live for `List<DTO>`,
`Array<DTO>`, `LinkedList<DTO>`, `Queue<DTO>`, and `Stack<DTO>`. The recording
fixture checks direct and wrapped `const List<RestItem>`, `const Array<RestItem>`,
`const LinkedList<RestItem>`, `const Queue<RestItem>`, and `const Stack<RestItem>`
results for DTO fields, order, wrapper metadata, and response-content ownership
in all eight Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared cells.
Nullable scalar
elements in JSON collections and nullable dictionary values are exercised by
recording and live-wire fixtures, including direct `const List<string?>`,
wrapped `const Array<string?>`, and `const Dictionary<string, string?>` results.
Nullable integer collection results now also have direct `const Array<int?>`,
`const List<int?>`, `const LinkedList<int?>`, `const Queue<int?>`, and
`const Stack<int?>` live-wire cases, plus wrapped `const Array<int?>`,
`const List<int?>`, `const LinkedList<int?>`, `const Queue<int?>`, and
`const Stack<int?>` results. Each response includes a null between positive and
negative values; wrappers verify status, headers, and response-content
ownership. JSON maps also cover nullable `List<int?>` values with populated,
empty, and null entries directly and under `RestResponse<T>`. Recording-handler
and live-wire checks for these numeric cases pass in all eight Windows x86/x64
MSVC and Linux x64 GCC/Clang static/shared cells; Linux builds use strict
warning flags.
Nullable Boolean JSON arrays support direct and wrapped results for
`Array<bool?>`, `List<bool?>`, `LinkedList<bool?>`, `Queue<bool?>`, and
`Stack<bool?>`. The fixtures preserve a null between true and false values;
wrapped wire cases verify status and headers, and the recording case verifies
response-content ownership through disposal. Recording and live-wire runs pass
all eight Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared cells, with
strict Linux warning flags.

Nullable decimal JSON collections cover direct and wrapped
`const Array<decimal?>`, `const List<decimal?>`, `const LinkedList<decimal?>`,
`const Queue<decimal?>`, and `const Stack<decimal?>` results. Responses preserve
positive and negative fractional values around a null; wrapped responses also
parse exponent notation and check status, headers, and content ownership through
disposal. Recording, live-wire, and generated-result
continuation-registration CTest checks pass in all eight Windows x86/x64 MSVC
and Linux x64 GCC/Clang static/shared cells.
JSON maps also cover direct and wrapped `const Dictionary<string, decimal?>`
results with populated fractional values, a null value, and exponent notation.
Recording CTests and live-wire checks pass in all eight Windows x86/x64 MSVC and
Linux x64 GCC/Clang static/shared cells; Linux builds use strict warning flags.
Nullable floating-point JSON collection results also cover direct and wrapped
`const Array<float?>`, `const List<float?>`, `const Array<double?>`, and
`const List<double?>` responses. Each sequence keeps a null between positive
and negative values; wrapped responses parse exponent notation and verify
status, headers, and response-content ownership through disposal. Recording and
live-wire checks pass in all eight Windows x86/x64 MSVC and Linux x64 GCC
13.3/Clang 18.1.3 static/shared cells, with strict Linux warning flags.
Collection-valued JSON maps now exercise direct and wrapped
`const Dictionary<string, const Array<RestItem>>`,
`const Dictionary<string, const LinkedList<RestItem>>`,
`const Dictionary<string, const Queue<RestItem>>`, and
`const Dictionary<string, const Stack<RestItem>>` results. Populated values
preserve JSON order during enumeration and empty collections stay empty. The
wrapper checks status, headers, and content ownership through disposal.
Recording and live-wire checks pass all eight Windows x86/x64 MSVC and Linux
x64 GCC/Clang static/shared cells with strict Linux builds.

Nullable string JSON arrays materialize directly as `Array<string?>`,
`List<string?>`, `LinkedList<string?>`, `Queue<string?>`, and `Stack<string?>`,
and as each corresponding `RestResponse<const Collection<string?>>` wrapper.
Recording and live-wire cases preserve escaped strings and an intervening JSON
null; wrapped cases also verify status, headers, and response-content ownership
through disposal. Both fixtures pass in all eight Windows x86/x64 MSVC and
Linux x64 GCC/Clang static/shared cells, with strict Linux warning flags.
Nullable DTO wire cases also cover direct and wrapped `const List<RestItem?>`,
`const Array<RestItem?>`,
`const LinkedList<RestItem?>`, `const Queue<RestItem?>`, and
`const Stack<RestItem?>` through both the recording handler and live-wire
fixture, preserving a null element between populated DTOs and checking wrapper
status, headers, and disposal in all eight cells. No-result methods
dispose their response; raw-response methods transfer the response to the caller.
Nullable string path values are accepted; a null value throws
`ArgumentNullException` before the request is constructed or sent.
Inherited conflicts, methods with multiple HTTP verb annotations, unresolved
placeholders, duplicate path bindings, multiple bodies, unsupported members/open
types, and unsupported returns produce source-located diagnostics before
execution. Route placeholders must use balanced, non-empty `{name}` syntax;
unmatched closing braces and nested or unclosed placeholders produce `CXREST010`.
The generator also rejects two `@Path` parameters that target the same route
placeholder with `CXREST010`, instead of silently discarding the later value.
Repeated binding annotations on one parameter, such as duplicate `@Query` or
`@Body`, produce `CXREST007` rather than selecting the first annotation.
`CancellationToken` parameters are never serialized. JSON uses the Milestone 20
CXDATA generated mapping adapter by default. `IRestSerializer<T>` registrations
can override a generated codec or provide a codec for a public DTO without
`@Serializable`; each registration supplies the request/response media type.
`RestClient.SerializerOptions` passes one opaque options object to registered
codecs on that client. Both are configured before the first request.
Typed DTO methods raise `ApiException` for unsuccessful status with bounded
error content. Response wrappers expose status without that exception.

## Ownership, threading, and errors

| Object/resource | Owner and release point |
|---|---|
| Socket handle | `Socket`; `Dispose` detaches waits then closes exactly once |
| Accepted socket | Returned socket caller; independent from listener |
| `NetworkStream` socket | `ownsSocket`; disposal always releases the wrapper reference and closes the socket only when true |
| HTTP client/handler | Parameterless and one-argument constructors own the handler; `HttpClient(handler, false)` borrows it. Dispose cancels outstanding work and releases the pool |
| Request/content | Request owns its content; generated REST calls dispose the request and content on send completion |
| Response/body | Caller owns response; response disposal attempts both body and send-lifetime cleanup, preserving the first disposal exception; the body either drains safely within limits or closes the connection |
| REST client | Owns its internally created `HttpClient`; supplied client follows explicit flag |

`ByteArrayContent`, `StreamContent`, live curl content, and multipart content
mark themselves disposed before clearing or releasing subclass state, so a new
asynchronous materialization cannot pass the base open check while its storage
is being torn down. `StreamContent` serializes detachment of its input stream
across concurrent dispose calls, so an owned stream is closed once. Multipart
additions, serialization snapshots, and disposal detachment share a mutex; a
serialized body is cached only if the part list has not changed during creation.
Disposal detaches the part list without allocating a replacement, then releases
parts outside the mutex. The focused
`cxnet_managed_http_multipart_serialization_dispose_race` CTest races a 64 KiB
part serialization against disposal, accepts a completed body or
`ObjectDisposedException`, and verifies later reads observe disposal. It passes
in Win32/Win64 and Linux x64 GCC/Clang static/shared configurations; GCC static
passes 20 repeat-until-fail runs. Live HTTP content and response-owned streams clear their owned
stream/response references during disposal. Multipart disposal attempts
every part, clears its collection, then propagates the first part-disposal
exception so one failing part does not skip cleanup of later parts.
The `cxnet_managed_http_multipart_dispose_failure` CTest injects a failure in
the first `ByteArrayContent` part and checks that the second part is still
disposed, multipart ownership is detached, the original exception is
propagated, and repeated disposal is harmless. It passes in all eight Windows
x86/x64 MSVC and Linux x64 GCC/Clang static/shared cells, with strict Linux
warning flags.
The standalone `http_content_setup` fixture also injects a multipart part-read
failure after serialization starts using a `StreamContent` part over its 4-byte
buffer limit. A second serialization attempt must fail rather than return a
cached partial body. It passes Windows x86/x64 MSVC and Linux x64 GCC/Clang
static/shared cells, with strict Linux warning flags.
The dedicated `cxnet_managed_http_response_owned_stream_dispose_race` CTest
races a read against disposal of its owning response. The read either delivers
the complete `pong` payload or observes `ObjectDisposedException`; later reads
must observe the wrapper as disposed. It passes in all six Windows x86/x64 and
Linux x64 static/shared cells, with strict Linux GCC and Clang builds; GCC
static also passes 50 repeated runs.
The generated REST live-wire fixture returns a streamed response through
`RestResponse<Stream>`, reads a body prefix, leaves a second read pending, then
disposes the wrapper and verifies that read completes canceled. The REST
generator and model projects plus the fixture were rebuilt, and its Windows x64
Release live-wire run passes.
The recording-handler fixture also races repeated `SetHasValue` and
`SetErrorContent` calls with `RestResponse<T>.Dispose()`. A setter may finish
before disposal or receive `ObjectDisposedException`; other errors fail the
fixture. The rebuilt REST fixture passes on Windows x64 Release and in 20
consecutive standalone runs. The updated fixture also passes 20 standalone
runs and a live-wire run in Linux x64 GCC shared, plus 20 standalone runs in
GCC static and Clang static/shared. Linux shared `ldd -r` resolves dependencies.
This closes the setter/disposal race matrix across Windows x86/x64 and Linux
x64 static/shared, with Linux GCC and Clang.
The `cxnet_managed_rest_response_publication_failure` CTest injects a scheduler
post failure while publishing wrapped string and byte-array results. It checks
that the publication exception propagates and the response is disposed. The
case passes 20 consecutive runs in Linux x64 GCC and Clang static/shared builds;
it also passes 20 consecutive runs in Windows x86/x64 MSVC static/shared
builds. Direct generated DTO/scalar callback-registration coverage is closed;
the generated response-publication and response-lifetime failure case is also
covered by a dedicated fixture below.
The Linux shared CTest environment now includes the cxcore, cxnet, and cxnet
runtime output directories for the focused managed HTTP failure cases. Ten
continuation-setup, REST publication, and generated callback-registration tests
pass in GCC/Clang x64 static/shared cells. The generated REST request-cancellation
test receives the same environment when configured and passes in Linux GCC and
Clang shared builds.
The `cxnet_managed_rest_result_continuation_registration_failure` CTest injects
failure after the request-cleanup callback registers and before the REST result
callback can be registered. `RestClient.SendAsStringAsync` returns a faulted
`InvalidOperationException` task and releases the request before transport
dispatch. The regression also verifies the request's owned `StringContent` is
disposed; this caught and fixed a skipped-content-disposal path in request
cleanup. It passes 20 consecutive runs in Linux x64 GCC and Clang static/shared
and Windows x86/x64 MSVC static/shared builds. Generated DTO/scalar methods share
this `HttpClient` boundary. The generated `RestGeneratorFixture` now includes a
test-only native injector and invokes a generated string route and a generated
JSON DTO route with failure scheduled after request-cleanup registration but
before each method result callback registers. Both returned tasks must be faulted
with `InvalidOperationException`. The dedicated
`cxnet_managed_rest_generated_result_continuation_registration_failure` CTest
passes Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared builds. Direct
generated response-publication and response-lifetime failure coverage is now
covered by `cxnet_managed_rest_generated_result_publication_failure`. CX
exception lowering suspends and clears the pending exception while a `catch`
handler or `finally` body runs, then restores it unless cleanup throws. This
lets response cleanup dispose content before rethrowing a generated result
publication failure. The generated DTO route test verifies the dispatch
exception, skipped subscriber continuation, and response/content disposal. It
also checks rethrow from a nested `finally` and exception restoration after a
rejected catch filter. The CTest passes on Windows x86/x64 MSVC and Linux x64
GCC/Clang, static/shared.
The public `HttpClient.GetStringAsync`, `GetByteArrayAsync`, and `GetStreamAsync`
methods now register their result callbacks before transport dispatch. If
registration fails, their internally owned request is disposed and the method
returns a faulted `InvalidOperationException` task. The
`cxnet_managed_http_convenience_continuation_registration_failure` CTest injects
failure for all three result shapes and passes Windows x86/x64 MSVC static/shared
and Linux x64 GCC/Clang static/shared builds.
Response disposal detaches its send lifetime before cleanup and attempts its
release even if body disposal throws. `CurlResponseStream` disposal attempts
both registration releases, pending-read cancellation, and transport cancellation
while preserving the first failure; `CurlStreamingContent` still detaches and
disposes its stream when base content cleanup fails. Send-cancellation cleanup
attempts both token-registration releases.
| REST typed result | Fully materialized DTO result disposes response after conversion |
| REST raw/stream result | Caller owns response/stream and must dispose it |

Native callback state, buffers, and managed roots outlive readiness, cancellation,
and close races. Native work is detached before roots are released or a handle
is reused. Managed completion is marshaled onto an attached CX thread. No
exception crosses an unsupported C callback boundary. HTTP distinguishes
caller cancellation, timeout, transport/protocol/TLS failure, HTTP status, and
content/serialization failure; diagnostics redact credentials and authorization
headers.

## Native HTTP backend spike (source/build gate partially validated)

The selected backend candidate is libcurl's multi socket API. Version 8.23.0
is pinned by archive SHA-256 and its release signature is verified; static
builds report `AsynchDNS`, SSL, and IPv6 on Linux x64 with OpenSSL and Windows
x86/x64 with Schannel. The managed transport checks asynchronous DNS, TLS,
HTTP/HTTPS protocols, and zlib when decompression is enabled before requests.
Evidence and reproduction instructions are in
[`http-backend-spike.md`](http-backend-spike.md). These checks validate source
acquisition, configuration, and a narrow scheduler-integration gate. The
`cxnet` scheduler registers readiness waits and timers; socket waits return
read/write/error flags and can be canceled and replaced when libcurl changes
its interest. Its timer queue also supports cancelable owner-thread timers.
`cx_scheduler_multi_spike.c` uses these APIs to run libcurl multi socket
transfers through the CX scheduler and platform poll providers. It starts 16
concurrent localhost transfers with per-transfer DNS caching disabled and
requires a resolver-start callback for every request. Five consecutive runs
pass on Linux x64 and Windows x86/x64 in static/shared mode; Linux GCC and Clang
14.0 builds also pass strict warnings. This validates socket/timer callback
integration and concurrent threaded-resolver requests in the spike, not the
production HTTP adapter or resolver latency.

Outbound `StreamContent` now uses an asynchronous producer and a fixed 64 KiB
queue feeding libcurl's read callback. The callback pauses when the queue is
empty, and the producer waits when it is full; it does not buffer the complete
request body. A 128 KiB chunked loopback upload with a 1 KiB content buffer cap
verifies byte order and content type on Linux x64 and Windows x86/x64. Seekable
stream content replays from its original position across 307/308 redirects;
non-seekable content is rejected when a redirect requires replay. A
missing source task or continuation-registration failure now terminates the
upload producer and surfaces as a request failure instead of leaving curl
waiting for more bytes; an over-reported read count is rejected before copying
outside the source buffer. The shared stream-copy and content-buffering paths
also fault on over-reported counts. The same terminal-task handling applies to
asynchronous content buffering. Buffering setup now publishes its shared
operation only after `OnBufferingStarted()` succeeds; if the hook throws, the
content receives `OnBufferingFailed()` and later reads cannot attach to an
operation that was never started. The `http_content_setup` fixture covers a
synchronous stream-position setup failure, concurrent observers with a throwing
continuation, pending-read failure, cached-failure behavior, and successful
retry in all six Windows x86/x64 and Linux x64 static/shared cells. A dedicated
CTest injects an `IOException` at the `OnBufferingStarted()` boundary and
verifies the failed operation is cleared so a later read can start and succeed;
it passes Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared builds. A
headers-first request canceled after receiving a body prefix cancels its
pending read, keeps later reads canceled instead of reporting EOF, and closes
the transfer. The managed HTTP e2e passes three consecutive runs on Windows
x64, Windows x86, and Linux x64. Concurrent upload cancellation isolation now
passes in all six static/shared cells; the five-delay cancellation matrix now
covers both disposal orders. Other upload/streaming races and pristine-OS
packaged dependency checks remain open. Download
streaming, certificate chain and hostname validation, SNI, trust-store
configuration, and deterministic HTTPS failure cases have passed on Linux x64
and Windows x86/x64. The native adapter explicitly enables peer-certificate
and hostname verification. The HTTPS fixture verifies trusted-CA success, default-trust,
wrong-host, expired-certificate, and unrelated-CA rejection. The pinned static
curl builds include zlib 1.3.2, Brotli 1.2.1, and zstd 1.5.7, and advertise all
three decoding features. When `CurlHttpMessageHandler.AutomaticDecompression`
is enabled, the handler requires those features and negotiates gzip, deflate,
Brotli, and zstd. Managed loopback cases verify decoding of all four encodings,
removal of stale wire encoding/length headers, and rejection when the expanded
gzip body exceeds `MaxResponseContentBufferSize`. The callback limit applies
after curl decoding. Restrict allowed protocols to HTTP/HTTPS and force
HTTP/1.1 for the initial contract.

An initial managed adapter is now present in
`System.Net.Http.CurlHttpMessageHandler`. The parameterless `HttpClient`
constructs and owns this handler; applications may still inject another
handler. Its native bridge uses a separate libcurl multi/easy pair per request,
registers each curl socket with `cxnet_socket_post_on_readiness_mask`, and
schedules curl timer callbacks with cancelable CX scheduler timers. It limits
protocols to HTTP/HTTPS, forces HTTP/1.1, explicitly requires peer-certificate
and hostname verification, disables redirects, and buffers response-content
reads up to 64 MiB with at most 128 response headers and 16 KiB of header bytes. Errors fault the
normal `Task<HttpResponseMessage>`. Managed loopback fixtures verify GET and
POST routing, outgoing body/content-type mapping, response status/body/header
mapping, in-flight cancellation, and configured timeout; they pass with pinned
static curl builds on Linux x64 and Windows x86/x64. The handler exposes
`MaxResponseContentBufferSize` (default 64 MiB, configurable from 1 byte through
64 MiB); the receive callback rejects a response before growing its buffer past
that setting. `HttpClient` exposes the same setting and snapshots it per request,
so clients sharing one handler retain independent buffer limits. A focused
4-byte response/3-byte limit case passes on Linux x64
and Windows x86/x64. The native parser faults explicitly on either the 128
header-count or 16 KiB aggregate-header cap; 129-header and 17,000-byte fixtures
verify both limit faults on Linux x64 and Windows x86/x64. HEAD now sets
libcurl's bodyless-request mode, and local
fixtures verify a HEAD response with a nonzero representation length, 204 with
no framing headers, 205 with either `Content-Length: 0` or a zero chunk, and a
304 carrying a nonzero representation `Content-Length` without a body. A 205
with a declared or chunked payload and a 204 carrying `Content-Length` or
`Transfer-Encoding` fault as invalid framing; headers-first calls fault before
returning malformed headers. Response status lines now require HTTP/1.0 or
HTTP/1.1, a valid status code, and a CRLF terminator; the parsed version is
copied to `HttpResponseMessage.Version`. The complete reason phrase is retained
up to the configured response-header limit. Fields require a nonempty RFC token
name, a colon, CRLF, and values without disallowed controls; malformed lines
fail the request. Response reason phrases and field values map accepted
high-byte HTTP octets to the corresponding U+0080–U+00FF characters before
they enter CX's UTF-8 `String` representation. A live response checks `0x80`
and `0xFF` in both fields decode correctly in all six Windows x86/x64 and Linux
x64 static/shared cells. The native callback validates a status line before
computing version or reason-phrase offsets, including for a truncated `HTTP/`
prefix.
Response-body and header callbacks check `size * count` for overflow before
using the supplied byte count.
The `Content-Length` parser accepts comma-separated repeated values only when
each decimal value is syntactically valid and numerically identical; empty,
conflicting, or overflowing entries fail framing. Loopback fixtures accept
`Content-Length: 4, 4` with its matching body and reject conflicting comma-list
values in all six Windows x86/x64 and Linux x64 static/shared e2e cells. Fixtures
reject malformed status codes, field names and values, and a malformed trailer
field name in the same six cells. The valid chunked response includes token
and quoted extensions, including an escaped quote, while a separate case rejects
an unterminated quoted extension. Expanded cases reject empty and overflowing
chunk sizes, malformed data terminators, invalid extension names and values,
and truncated trailers under strict GCC and Clang 14 Linux x64 static runs and
the Windows x86 static run. The chunk parser accepts 16,384-byte chunk-size
lines and trailers and rejects the 16,385th byte in either. Direct parser
regressions check both boundaries across one-byte, two-byte, seven-byte,
31-byte, and whole-input callback fragments; the expanded Clang 14 ASan/UBSan
libFuzzer campaign passes 50,000 runs with payload, trailer-termination, C0
trailer-value, and folded-trailer seeds. The focused wire grammar CTest passes
Windows x86/x64 MSVC static/shared
and Linux x64 strict GCC/Clang static/shared builds. It also rejects C0 and DEL
bytes in trailer values, obsolete folded trailer lines, and malformed trailer
CRLF boundaries. The integrated managed HTTP e2e passes all eight Windows
x86/x64 MSVC and Linux GCC/Clang static/shared cells. Broader malformed chunk
boundaries and remaining
Milestone 26 lifecycle, REST, and server gates remain open.
Existing framing cases pass on Linux x64 and
Windows x86/x64, static and shared. `HttpClient` also has
`HeadAsync` and `OptionsAsync` convenience overloads; local wire fixtures verify
their methods and empty response handling. Additional wire fixtures verify PUT
and PATCH upload bodies/content type, DELETE, and escaping spaces in URI path
and query components. These managed HTTP e2e checks pass on Linux x64 and
Windows x86/x64. The transport validates fixed-length response byte counts and
chunked transfer framing; loopback cases verify a truncated fixed-length body
and missing chunk terminator fault, while a complete chunked body with a trailer
returns the decoded payload on Linux x64 and Windows x86/x64. Since
`HttpResponseMessage` has no separate trailer collection, trailers are consumed
for framing and omitted from both response and content headers. The native
chunk parser validates trailer field-name tokens, the name/value separator, and
field-value controls while streaming. Wire cases reject a malformed trailer
field name, reject an unterminated quoted extension, and accept token and
quoted chunk extensions, including an escaped quote, in all six Windows x86/x64
and Linux x64 static/shared cells plus strict Linux Clang static/shared builds.
A 17,000-byte chunk-size line is rejected in those same Windows and Linux cells.
The expanded strict GCC and Clang 14 Linux x64 static runs and the Windows x86
static run also reject empty and overflowing sizes, malformed data
terminators, invalid extension names and values, and truncated trailers. Added
boundary cases reject a truncated chunk payload, an LF-only chunk-size line,
and a chunk-data terminator missing its CR; these pass the Windows x86/x64 and
Linux x64 static/shared matrix, including strict Linux GCC and Clang builds.
Additional wire cases reject repeated CR bytes in the chunk-size and chunk-data
delimiters and a bare LF in a trailer field. The focused chunk grammar CTest
passes all eight Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared
cells. The fixture also mutates every CR/LF byte in its valid chunk body and
tests every truncated body prefix, in addition to the invalid size-byte,
extension, data-terminator, and trailer cases above. Responses combining
`Content-Length` with chunked `Transfer-Encoding`, repeating the chunked
`Transfer-Encoding` field, or combining repeated chunked codings in one
comma-list are rejected before body reads. A response combining conflicting
duplicate `Content-Length` fields with chunked `Transfer-Encoding` is also
rejected. A nonnumeric `Content-Length` combined with chunked transfer encoding
is rejected as well. A comma-list containing conflicting lengths combined with
chunked transfer encoding is also rejected. A matching comma-list of lengths
combined with chunked transfer encoding is rejected too. Empty and whitespace-
rejected. Overflowing and explicitly signed `Content-Length` values combined
with chunked transfer encoding are rejected as well. Same-client recovery
confirms the next request succeeds on a fresh connection after each of these
fifteen malformed combinations, including `Transfer-Encoding:
chunked;foo=bar`, leading empty coding-list entries, and trailing empty entries.
The focused recovery CTest passes five consecutive runs for these latest cases
in all eight Windows x86/x64 MSVC and Linux x64 GCC 13.3/Clang 18 static/shared
cells; the Linux GCC static full suite also passes 112/112 after their addition.
The focused managed HTTP end-to-end and recovery
tests pass in all eight Windows x86/x64 MSVC and Linux x64 GCC 13.3/Clang 18
static/shared cells for the first four forms. The fifth form passes both tests
in all eight cells; the sixth passes the focused recovery CTest in all four
Windows x86/x64 MSVC static/shared cells and fresh Linux GCC 11.4 and Clang 14.0
static/shared builds with strict warnings. The seventh passes the focused
recovery CTest in those same eight Windows/Linux build cells. The empty and
whitespace-only length cases, plus the overflowing and signed length cases,
pass five consecutive recovery CTest runs in all eight cells. Other
combinations of interacting malformed fields remain open.
The headers-first fixed-length truncation case also verifies pool recovery:
after delivering the four-byte prefix and faulting the next read, a second
request on the same `HttpClient` must succeed over a new TCP connection. The
focused CTest is registered and passes ten consecutive runs in a disposable
Linux x64 GCC 13.3 build with strict warnings, using system libcurl 8.5.0. It
also passes GCC 13.3 and Clang 18 static/shared builds with strict warnings.
The full managed HTTP end-to-end CTest passes in the GCC 13.3 static build in
170.91 seconds. The source passes an MSVC `/Zs` syntax check. These diagnostic
builds use system libcurl; the pinned-libcurl Windows/Linux matrix remains to
be run.
The focused grammar fixture also rejects an unescaped control byte and an
escaped control byte in quoted chunk extensions, plus DEL in a trailer field
value. These three cases pass all eight Windows x86/x64 MSVC and Linux x64
GCC/Clang static/shared cells.
The HTTP client grammar fixture also mutates each CR and LF byte in a valid
single-chunk response independently and requires the public `HttpClient` call
to fault. It also truncates that response at every body byte offset, including
before the first chunk byte; each incomplete response must fault. Both sets of
cases pass all eight Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared
cells; Linux builds use strict warning flags.
An opt-in Clang libFuzzer target instruments the chunk parser with ASan and
UBSan and compares parser state and decoded body across one-byte, two-byte,
seven-byte, 31-byte, and whole-input callback fragments. The 23-seed corpus
covers valid framing, multiple and empty chunks, extensions and trailers,
leading-zero sizes, tab boundaries, truncation, overflow, malformed trailers,
empty extension names and values, optional extension whitespace, the chunk-line
boundary, repeated CR delimiters, and an LF-only trailer delimiter. The Linux
x64 Clang static campaign passes 100,000 runs with a 20,000-byte input limit.
The harness also requires empty extension names and values to be rejected and
optional extension whitespace to be accepted across callback fragments. A
directed regression now checks every truncation point in a valid chunked body
with an extension and trailer, replaying each prefix at every fixed callback
fragment size up to that prefix's length; only the complete message may reach
the parser's done state. The directed check and full 100,000-run libFuzzer CTest
pass under Clang 14 ASan/UBSan; the updated campaign completed in 194.99
seconds. A second directed check flips every CR and LF byte independently in
valid single- and multi-chunk messages, requiring rejection across five callback
fragment sizes. The expanded Clang 18 ASan/UBSan CTest passes 100,000 runs with
the new delimiter mutations in 118.33 seconds. Broader generated
malformed-boundary coverage remains open.
The parser validates chunk
extensions as token names with optional token or quoted-string values,
including permitted whitespace and quoted-pair escapes. Each chunk-size line is
bounded by the configured header-byte limit, including its extensions. The
valid-trailer wire test checks omission on Linux x64 and Windows x86/x64, static
and shared. The informational response fixture sends `103 Early Hints` with a
`Link` header, then `100 Continue`, before the final `200`; the client returns
only the final response and its `pong` body on Windows x86/x64 and Linux x64.
`101 Switching Protocols` is rejected promptly with `HttpRequestException`
retaining status 101 because this handler does not expose an upgraded transport.
A loopback 101 response faults promptly, and the exception's nullable
`StatusCode` is asserted to contain 101 in all six Windows x86/x64 and Linux x64
static/shared HTTP e2e cells.

This adapter is an integration baseline, not the production HTTP contract yet.
Active cancellation posts native cleanup to the scheduler owner thread, cancels
registered socket/timer waits, and completes the task as canceled. The client
timeout is copied to the request and enforced by libcurl; expiry faults with
`TimeoutException`. The local fixtures verify those outcomes and connection
closure on cancellation. Other libcurl failures fault with `HttpRequestException`
and an inner `IOException` that preserves the `libcurl` error domain and numeric
code; protocol and callback failures carry an I/O cause. Failures after a final
response status is parsed retain that status on the exception, while timeouts
remain `TimeoutException`. `HttpClient.Dispose` cancels its lifetime token, which
is linked with each caller token. `SendAsync` registers the link while holding
the client state mutex, serializing registration with disposal's cancellation.
A local slow-request fixture verifies that
disposing the client cancels an active curl request on Linux x64 and Windows
x86/x64. The public shared-module fixture also verifies caller cancellation
after headers-first completion and a delivered body prefix: a pending body read
and later reads are canceled, and the server observes connection closure. It
passes three consecutive times on Windows x64/x86 and Linux x64 in static and
shared link modes. A deterministic static fixture waits until a stalled loopback
server has received the request, then requests caller cancellation twice while
disposing the client; the task completes canceled and the server observes
transport closure on Windows x86/x64 and Linux x64. The shared public-API smoke
fixture now also schedules duplicate caller cancellation with client disposal
against an in-flight slow request; it passes on Windows x86/x64 and Linux x64.
Cancellation callbacks requested from an attached non-owner thread are queued
for the scheduler owner, which runs them between event waits; the wait interval
is bounded while cross-thread work may be pending. The dedicated
`cxnet_managed_http_cross_thread_client_dispose_send` fixture disposes a client
on a worker while the owner scheduler is servicing a stalled request. It
verifies canceled task state, native transport cancellation, server-observed
closure, and request release across Windows x86/x64 and Linux x64 static/shared
cells, and passes Linux GCC and Clang static/shared builds as well.
The public smoke fixture now starts two headers-first requests on one client
and reads a prefix from each. It cancels one pending body read while verifying
the other remains pending, then cancels the second; subsequent reads remain
canceled and the server observes both connections close. This case passes
three consecutive times in all six Windows x86/x64 and Linux x64 static/shared
cells (18 runs total). A second race cancels one caller token, disposes their
common `HttpClient` with both body reads pending, then cancels the peer token
before scheduler dispatch; both reads must cancel and both server connections
must close. It passes in all six Windows x86/x64 and Linux x64 static/shared
cells. The shared public-API smoke sends POST, PUT, and PATCH `StringContent` bodies to loopback echo routes and sends DELETE to its own route; it checks each status and response body. It also calls `HttpClient.HeadAsync` against a route with a nonzero representation length and verifies HTTP 200 with no response body. A pre-canceled `HttpClient.PostAsync` with streamed content must return a canceled task without reaching the loopback server. These cases pass in all eight Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared cells. Additional upload and cancellation-order
races remain open. Requests on one
`HttpClient` share a libcurl connection cache; a local HTTP/1.1 fixture verifies
two sequential requests reuse one TCP connection on Linux x64 and Windows
x86/x64. Pooled connections default to a 90 second idle timeout and a 300 second
lifetime; `PooledConnectionIdleTimeoutMilliseconds` and
`PooledConnectionLifetimeMilliseconds` configure those policies. Setting the
lifetime to zero disables the lifetime limit. `MaxConnectionsPerServer` defaults
to 10 (allowed range 1 through 1024); a concurrent loopback fixture verifies a
limit of one across separate request multi handles on Linux x64 and Windows
x86/x64. Separate loopback fixtures set the idle timeout and maximum lifetime
to one second, then verify that a later request opens a new connection after
1.3 seconds on those same targets. libcurl applies both ages at whole-second
granularity. While a per-origin slot is occupied, local fixtures verify a
queued request observes its total timeout and caller cancellation without
opening a second connection on Linux x64 and Windows x86/x64. For a queued
request, the focused `cxnet_managed_http_queued_timeout_cancel_race`
case schedules caller cancellation at 50 ms or 150 ms against a 100 ms client
timeout. Both expected winners are checked, and the first held request must
complete while the queued request must not open another connection. It then
sends a third request through the same client and requires that request to
complete over a second server-observed connection, proving the canceled or
timed-out queue entry released its per-origin slot. The focused CTest passes in
all eight Windows x86/x64 MSVC and Linux x64 GCC 13.3/Clang 18 static/shared
cells, with five consecutive passes in each cell. The existing 8 MiB
`HttpClient.Dispose()` while a connection slot is occupied cancels both the
active request and a second queued request, closes the active TCP connection,
and does not admit the queued request to the server; the focused
`cxnet_managed_http_queued_client_dispose` case passes Windows x64 MSVC static
and Linux x64 GCC static/shared, three consecutive runs per cell. The full
integrated HTTP e2e also passes on Windows x64 MSVC static with this case
included.
The existing 8 MiB
`StreamContent` upload uses a 1 KiB content
buffer; the local server signals
after observing request-body bytes and pauses its reader. The client then
cancels and the server resumes until transport EOF; the fixture requires a
nonzero partial upload, canceled task, and TCP closure. This deterministic
mid-upload cancellation case passes on Linux x64 and Windows x86/x64. A
concurrent streaming-upload case cancels an 8 MiB caller-token request after
partial body transfer while a second upload on the same client remains pending
after sending its body; releasing the second response completes that request.
It passes three consecutive full HTTP e2e runs in all six Windows x86/x64 and
Linux x64 static/shared cells, and strict GCC and Clang 14.0.6 Linux static/shared
builds. A separate pending-upload fixture then checks both cancellation and
client-disposal orderings: caller cancellation followed by client disposal, and
client disposal followed by caller cancellation. Both require the upload task
to cancel and the server to observe transport closure after a partial body; the
full HTTP e2e passes in all six Windows x86/x64 and Linux x64 static/shared
cells, plus strict GCC and Clang 14.0.6 Linux static/shared builds. The
full-body response-wait ordering now forces GC after the server reads the upload
and before cancellation, then verifies the task remains pending; the updated
fixture passes in the same six cells and strict GCC/Clang Linux static/shared
builds. Further
upload cancellation coverage now also schedules cancellation while an 8 MiB
upload is stalled, covering all ten combinations of five delays (0, 1, 5, 15,
and 34 ms) and both client-disposal orders. The full HTTP e2e passes with this
coverage in all six Windows x86/x64 and Linux x64 static/shared cells, plus
strict GCC Linux x64 static/shared and strict Clang 14.0.6 Linux x64 static/shared builds. Three consecutive
additional full-fixture runs also pass on Windows x64 static and strict GCC
Linux x64 static. A separate 250 ms `HttpClient.TimeoutMilliseconds` case stalls
the same 8 MiB upload after the server observes body bytes, then requires a
timed-out task, partial body, and server-observed connection closure. It forces
GC while the upload and timeout are pending. It passes in all six Windows
x86/x64 and Linux x64 static/shared cells, plus strict GCC and Clang 14.0.6
Linux x64 static/shared builds. The dedicated
`cxnet_managed_http_headers_first_client_dispose` case verifies that disposing
the owning `HttpClient` cancels a pending headers-first body read after a
delivered prefix and closes the server connection. It passes on Windows x86/x64
and Linux x64 in static/shared builds (Linux GCC and Clang). Other HTTP lifecycle
races remain open. A
pre-canceled `SendAsync` now returns an already-canceled task without beginning
the request; a 1 MiB streaming upload is not observed by the loopback server.
The public API smoke passes this case in all six Windows x86/x64 and Linux x64
static/shared cells. Direct `CurlHttpMessageHandler.SendAsync` and response-body
`ReadAsync` also return canceled tasks for pre-canceled tokens after validating
the handler/request or stream/buffer state.
Disposing a `CurlHttpMessageHandler` during a direct in-flight `SendAsync`
preserves that request until its response arrives, while a later send is
rejected with `ObjectDisposedException`; the loopback fixture verifies the
response body and passes in all eight Windows x86/x64 and Linux GCC/Clang
static/shared cells.
`HttpClient.SendAsync` installs its request-end and send-lifetime completion
continuation before dispatching transport work, so a continuation-registration
failure follows setup cleanup before a request reaches the handler. If building
the combined caller/client cancellation state fails while registering its
second token, it unregisters the first callback before propagating the error.
If a handler returns an active task but continuation registration fails, the
client cancels the shared send token before faulting the request chain, avoiding
an orphaned transfer. The dedicated
`cxnet_managed_http_send_completion_registration_failure` CTest injects failure
at the initial cleanup-continuation registration and verifies that `SendAsync`
throws before transport dispatch and releases the request reservation. It passes
in all six Windows x86/x64 and Linux x64 static/shared cells, including strict
Linux GCC and Clang 14 static/shared builds. This test exposed
a `setjmp` lifetime issue in the setup catch path; `BeginSend` now has its own
failure scope, while post-reservation failures unconditionally release that
reservation. The dedicated
`cxnet_managed_http_send_handler_continuation_failure` CTest injects failure
after the handler returns its task, then verifies the send faults and request
reservation is released. It passes all six Windows x86/x64 and Linux x64
static/shared cells, including strict Linux GCC and Clang 14 static/shared
builds. The dedicated
`cxnet_managed_http_send_cancellation_registration_rollback` CTest fails the
second token registration and verifies `HttpSendCancellation` removes its first
lifetime callback before propagating the exception. It passes Windows x86/x64
and Linux x64 static/shared cells, including strict Linux GCC and Clang builds.
The dedicated
`cxnet_managed_http_response_read_cancellation_registration_failure` CTest
injects failure while a headers-first response stream registers a pending read's
cancellation callback. It verifies that the read faults, the pending slot and
callback are cleared, and a subsequent read on the same stream still receives
the complete body. It passes all eight Windows x86/x64 MSVC and Linux x64
GCC/Clang static/shared cells; Linux builds use strict warnings.
The dedicated
`cxnet_managed_http_content_buffer_continuation_registration_failure` CTest
injects failure while `StreamContent.ReadAsByteArrayAsync()` registers its
continuation, then verifies the operation faults and its cached task remains
available. The focused test passes in all eight Windows x86/x64 MSVC and Linux
x64 GCC/Clang static/shared cells; Linux builds use strict warning flags. The
dedicated
`cxnet_managed_http_curl_upload_continuation_registration_failure` CTest injects
a failure at the upload read-continuation boundary and verifies the producer
records an `InvalidOperationException` as terminal input failure. The pump
catches exceptions at both initial and follow-on read entry points. The focused
test passes in all eight Windows x86/x64 MSVC and Linux x64 GCC/Clang
static/shared cells; Linux builds use strict warning flags. The full managed
HTTP e2e also passes on Windows x64 static and Linux GCC x64 static.
headers-first request canceled after receiving
a body prefix cancels its pending read, keeps later reads canceled instead of
reporting EOF, and closes the transfer. The managed HTTP e2e passes three
consecutive runs on Windows x64, Windows x86, and Linux x64. The focused
`cxnet_managed_http_upload_cancel_after_headers` CTest adds a streaming upload
with `Expect: 100-continue`; it cancels once the server observes request headers
and checks cancellation, both client-disposal orderings, connection closure, and
that the full body was not sent. It passes all eight Windows x86/x64 MSVC and
Linux x64 GCC/Clang static/shared cells. Further upload cancellation-order races
and pristine-OS package checks remain open. A new
headers-first case disposes `HttpClient` after a body prefix while the next read
is pending; it verifies the read is canceled and the server observes closure in
all six Windows x86/x64 and Linux x64 static/shared cells.
`HttpClient.Proxy` supports an explicit absolute HTTP(S) proxy URI without
embedded credentials. `null` leaves libcurl's environment/default proxy
selection enabled; an empty string disables proxy selection; any other value
selects that proxy. `ProxyBypassList` accepts comma-separated libcurl
`CURLOPT_NOPROXY` patterns. Both settings are snapshotted per send and carried
across redirects. A local fixture verifies explicit proxy routing, loopback
bypass, and explicit proxy disable on Linux x64 and Windows x86/x64. Proxy
TLS peer and hostname verification are explicitly enabled for HTTPS proxies.
`CurlHttpMessageHandler.ProxyCaCertificateFile` adds a CA bundle for an HTTPS
proxy while keeping those checks enabled; an empty value uses the platform
trust store. The local HTTPS CONNECT fixture verifies trusted tunneling and
rejects an untrusted proxy CA and a trusted certificate with the wrong proxy
hostname in all six Windows x86/x64 and Linux x64 static/shared cells.
Schannel's best-effort unavailable-revocation option applies to proxy TLS too.
`HttpClient.ProxyCredentials`
snapshots an immutable username/password pair per send and redirect and applies
it only to libcurl's proxy-auth options; it is never added to destination
request headers. libcurl negotiates a supported proxy HTTP authentication
scheme. Use an HTTPS proxy when credentials must be protected in transit; a
local fixture verifies challenge handling, redirect retention, and separation
from destination `Authorization` headers on Linux x64 and Windows x86/x64.
Shared `cxnet` and `cxcore` libraries also build and pass a public authenticated-
proxy smoke request on Windows x86/x64 and Linux x64.
`HttpClient.UseCookies` defaults to true and owns a per-client
`CookieContainer`. It captures `Set-Cookie` response headers and applies
host-only/domain, path-boundary, Secure, Max-Age, and RFC 6265 cookie-date
Expires matching. Empty and trailing-dot `Domain` attributes are ignored, and
only one leading dot is removed before domain matching. Domain attributes are
canonicalized with the shared URI host parser, including IDNA-to-ASCII mapping;
authority syntax such as ports, userinfo, and path delimiters is rejected.
IPv4 and IPv6 domain literals are normalized before matching. A request host
that is an IP literal can match a cookie domain only by exact equality; domain
suffix matching applies to hostnames. DNS host canonicalization follows
[RFC 6265 section 5.1.2](https://www.rfc-editor.org/rfc/rfc6265.html#section-5.1.2).
Focused native checks verify Unicode Domain canonicalization and matching,
rejection of port/userinfo syntax, and equality across equivalent IPv6 spellings
on Linux x64. HTTP e2e checks reject IPv4 and IPv6 Domain suffixes for
IP-literal request hosts while preserving exact-address matches in all six
Windows x86/x64 and Linux x64 static/shared cells. A cookie-container case
matches an expanded IPv6 `Domain` attribute to a compressed request host, and
the redirect wire fixture starts at expanded `::1`, redirects to compressed
`::1`, and confirms same-origin authorization is preserved; both pass in all six
Windows x86/x64 and Linux x64 static/shared cells. The invalid-205 response
framing case now passes after faulted
headers-first send cleanup was changed to avoid reading a failed task from its
completion callback. Client disposal now cancels a pending response-body read
immediately. The full managed HTTP e2e executable passes on Linux x64, including
upload/cancellation races, connection and queue behavior, buffered readers,
streaming backpressure/truncation, and cookie coverage. The fixture roots objects
retained across its large backpressure read and uses the matching concrete stream
type for disposal. Pending non-generic tasks now use the incomplete constructor
or `TaskCompletionSource`; the completed no-argument `TaskHandle` constructor is
not reused for pending operations. `Task` and `Task<T>` expose fault state for
completion handling. The integrated managed HTTP e2e, including this cookie
coverage, now passes all eight Windows x86/x64 MSVC and Linux x64 GCC/Clang
static/shared cells. The broader CTest matrix and other M26 acceptance gates
remain open.
Expiry tests cover RFC
1123, RFC 850, and asctime date forms,
one-digit date/time components, alternate token ordering, and rejection of
three-digit minute/second fields on Linux x64 and Windows x86/x64. Domain
attributes use the pinned Public Suffix List snapshot in
[`public_suffix_list.md`](public-suffix-list.md), including exact, wildcard,
exception, and private rules. Cookie tests reject `com`, `co.uk`, `blogspot.com`,
and wildcard suffix domains; they also check exception rules, ordinary
registrable domains, and the exact-public-suffix-host exception on Windows x86/x64
and Linux x64.
Redirect responses update the jar before the next hop. Local fixtures verify
path scope, expiry, deletion, per-client isolation, disabling cookies, and
cookie use after a redirect on Linux x64 and Windows x86/x64.
`DefaultRequestHeaders` snapshots defaults per send and lets request headers
override them. Same-origin redirects preserve the snapshot; cross-origin
redirects drop authorization, cookie, and host defaults. A local managed HTTP
fixture verifies default transmission, request-level precedence, and the
in-flight snapshot on Linux x64 and Windows x86/x64. The redirect fixture also
checks cross-origin stripping of default authorization and cookie headers.
Equivalent expanded/compressed IPv6 origins are also verified on the wire: the
redirect preserves authorization across the spelling change in all six Windows
x86/x64 and Linux x64 static/shared cells.
The dedicated `cxnet_managed_http_concurrent_header_mutation_send` fixture
starts a request while an attached worker removes and replaces a request
header, then adds another. It checks that the wire contains a valid point-in-
time header collection state, that later mutations remain on the request, and
that the in-flight response completes. It passes on Windows x86/x64 and Linux
x64 with static and shared linking. Linux GCC and Clang static/shared builds
pass with `-Wall -Wextra -Werror -Wno-unused-value`.
The dedicated `cxnet_managed_http_concurrent_client_setting_send` fixture
races `BaseAddress`, `TimeoutMilliseconds`,
`MaxResponseContentBufferSize`, `UseCookies`, `MaxConnectionsPerServer`,
`PooledConnectionIdleTimeoutMilliseconds`, and
`PooledConnectionLifetimeMilliseconds`, `Proxy`, `ProxyBypassList`, and
`ProxyCredentials` updates against send startup.
It sends a relative URI while switching the base host from `localhost` to
`127.0.0.1`; the request must resolve against either captured base, and a
host-only cookie makes the held response confirm which host was used. The
other captured request values must be either their previous or new settings,
while the client retains all updates. The CTest passes on Windows x86/x64 MSVC
and Linux x64 GCC/Clang, static/shared. Linux builds use
`-Wall -Wextra -Werror -Wno-unused-value`.
Mutable `HttpClient` settings are read and changed under one state mutex. A send
captures its base address, timeout, proxy and cookie configuration, redirect
policy, response-buffer limit, and connection-pool limits together before
building the request. The concurrent-setting fixture checks captured base
address, timeout, buffer limit, cookie mode, maximum connections, pooled idle
timeout, pooled lifetime, proxy URI, proxy bypass list, and proxy credentials
while settings change at send startup; a concurrent `AllowHttpsDowngrade`
setter race is checked by the dedicated HTTPS policy fixture below. The
redirect-setting races are covered by the fixtures described next.
The redirect end-to-end fixture delays a first redirect response while
scheduled updates disable `AllowAutoRedirect` or lower
`MaxAutomaticRedirections`; the HTTPS fixture also disables
`AllowHttpsDowngrade` while the first response is delayed. Each send still
follows the policy captured at startup. The HTTP redirect and HTTPS fixture
checks pass in all eight Windows x86/x64 MSVC and Linux x64 GCC/Clang
static/shared cells. A barrier-controlled worker also races
`AllowAutoRedirect` and `MaxAutomaticRedirections` updates against `GetAsync`
startup; responses must match the old or new policy. The redirect CTest passes
in all eight cells. The HTTPS policy CTest also races
`AllowHttpsDowngrade` against send startup and accepts only a fault with the old
false setting or the final response with the new true setting; it passes all
eight cells.
The `cxnet_managed_http_scheduler_shutdown` fixture starts an `HttpClient`
buffered GET against a local server. After the server begins a held response,
scheduler shutdown must cancel the task and close the connection. The focused
CTest passes Windows x86/x64 MSVC and Linux x64 GCC in static/shared builds.
HTTP readiness waits and its cancelable transfer/connection-slot timers now
register the existing cancellation cleanup path for scheduler shutdown.
Fresh Linux x64 Clang 14 static and shared builds with strict warnings also pass
the managed HTTP end-to-end, direct curl-handler send/headers-first/cancellation/
disposal cases, public smoke, scheduler shutdown, redirect, and DNS shutdown
CTests. The shared build passes 13 focused CTests with `LD_LIBRARY_PATH` unset;
`ldd -r` reports no unresolved dependencies. The scheduler timer-shutdown
callback and timer-arena release CTests pass in strict Clang 14 static and
shared `cxcore` builds.
`HttpResponseMessage.Content` reads and assigns under the response state mutex.
Disposal marks the response disposed and captures its selected content under
that same mutex, then disposes the content after releasing the lock. The
`cxnet_managed_http_response_content_replacement_disposal_race` fixture races a
worker setter against disposal and checks selected-content disposal and
replacement ownership for the ordering observed. It also verifies successful
replacement leaves the detached content caller-owned and later assignment
after disposal fails. The dedicated Linux x64 GCC static test passes 100
consecutive runs. The same CTest also passes Windows x86/x64 MSVC and Linux
x64 GCC/Clang in static/shared modes; Linux builds use strict warnings.
The curl adapter uses libcurl's explicit empty-header form for an empty request
value; the generated REST wire fixture verifies that the empty field reaches
the local server.
Request content headers are sent, and common response content headers are
copied into `HttpContent.Headers`. `GetStringAsync` and `GetByteArrayAsync` use
buffered completion, then dispose the response. `SendAsync` and `GetAsync` accept
`HttpCompletionOption`; `GetAsync` exposes the completion choice for both `Uri`
and string request targets, with cancellation-token and no-token overloads.
`ResponseHeadersRead` completes when final response headers arrive. A separate
loopback fixture exercises the public headers-first send path directly and reads
through its response content stream. `GetStreamAsync` uses
headers-first completion and returns a response-owned live stream. The curl
write callback feeds a bounded 64 KiB managed ring buffer; libcurl pauses and
resumes the transfer to apply backpressure when the consumer is slow. Disposing
the stream disposes its response and cancels the transfer. Zero-length callbacks
do not complete pending reads as EOF, and a pending
read's cancellation token is checked under the stream lock before data, EOF, or
failure completes it. If cancellation wins before data delivery, the bytes
remain queued for a later read. If cancellation-callback registration fails,
the pending slot is cleared and that read faults instead of blocking later reads.
Pending read tasks still complete if disposing a cancellation registration throws;
unregister failures are contained at native callback boundaries.
A pending-read cancellation callback posts completion to the task scheduler; if
posting fails, it records that outcome in the catch block and cancels the read
after the exception has been cleared. The focused scheduler-post-failure CTest
injects a failed post and verifies that the pending read completes as canceled
on Win32/Win64 and Linux x64 with GCC and Clang, using static and shared builds.
A delayed-body loopback fixture verifies that `GetStreamAsync` returns before
body EOF, then
reads the payload and EOF on Linux x64 and Windows x86/x64. A paced HTTP/1.1
loopback fixture sends a 256 KiB body across the 64 KiB ring-buffer capacity and
verifies byte order through EOF on the same targets. This exposed a one-shot
readiness rearm gap; the curl bridge now rearms the last socket interest after
each action when libcurl leaves it unchanged, unless the body is paused.
`MaxResponseContentBufferSize` applies to buffered completion and to explicit
`ReadAsByteArrayAsync()`/`ReadAsStringAsync()` buffering of headers-first
content. Direct headers-first reads and `CopyToAsync()` remain streaming and
are not capped by that buffering setting. The streaming queue stays at 64 KiB
regardless of response size. Callback chunks larger than the CX stream length
type are rejected before narrowing their lengths. A 256 KiB stream succeeds
with a 4-byte buffer limit on Linux x64 and Windows x86/x64. A truncated
fixed-length response returns its received prefix, then faults the next read on
Linux x64 and Windows
x86/x64. A loopback case also cancels a pending body read, then reads the body
successfully on those targets. `HttpContent.ReadAsByteArrayAsync()` and
`ReadAsStringAsync()` now buffer a headers-first response by repeatedly reading
its live stream asynchronously. Repeated calls reuse the same task and buffered
result, including concurrent calls that race to start a read. A per-content
mutex publishes each cached task before stream creation or asynchronous
buffering begins; stream setup runs outside the mutex, and the active buffering
operation is published only after setup succeeds. The base disposed flag is
atomic because entry checks also occur outside that mutex; the mutex still
coordinates disposal with cached buffering state. A separate operation mutex
serializes buffering completion against cancellation/disposal, and pending
reads retain their source/buffer references while the read starts. Buffering
terminal paths run cleanup even if task completion throws. A setup that failed before operation
publication clears its cached task in `finally`, including when a completion
continuation throws, so a later read can retry. Buffer-limit aborts dispose their
source before rethrowing a completion failure.
The standalone `tests/http_content_setup` consumer verifies a failed setup returns
a faulted task, a second byte-buffer read receives a distinct faulted task, and
default and explicit-charset string reads preserve the setup failure. It builds
and passes in all six Windows x86/x64 and Linux x64 static/shared cells,
including strict GCC and Clang Linux builds. The fixture also injects a
throwing continuation while a pending buffer read fails and verifies the
original IOException remains cached; that case passes the same six cells and
strict Linux GCC/Clang static/shared builds. A second standalone case disposes
content with a buffer read pending, checks cancellation and owned-source
disposal, then delivers a late read completion and verifies the task remains
canceled. Its Windows x64 Release build and run pass.
The fixture also races EOF completion on the scheduler owner against disposal
on a worker thread. It accepts only successful empty-buffer completion or
cancellation, verifies the source is disposed, and passes 20 consecutive
Windows x64 Release runs. Linux x64 GCC and Clang 14 static/shared builds pass
strict warnings and each pass 20 consecutive runs; both shared executables
pass `ldd -r`.
Disposing the content cancels an in-progress buffer operation; a
loopback fixture verifies byte-for-byte and string results, forced GC during
pending reads, and cancellation/connection closure on disposal across Windows
x86/x64 static/shared builds and strict GCC Linux x64 static/shared builds. The
`HttpContent.ReadAsString()` and `ReadAsStringAsync()` use the `charset`
parameter in `Content-Type`, defaulting to UTF-8 when it is absent. Callers can
also select a charset with the explicit string-read overloads. `StringContent`
encodes according to its media type's declared charset; its three-argument
constructor selects a charset and adds that parameter when absent. The current
text encoding set is UTF-8, US-ASCII, and ISO-8859-1 (including common aliases).
Unsupported charsets fail with `NotSupportedException`, and bytes outside the
selected encoding fail with `EncodingException`. Missing values and duplicate
charset parameters fail with `FormatException`. Response wire fixtures verify
ISO-8859-1 and US-ASCII decoding, plus rejection of unsupported and duplicate
charset declarations, across all six Windows x86/x64 and Linux x64 static/shared
cells. Explicit charset overloads validate the charset before
buffering content, so unsupported encodings fail without consuming the body;
async reads return a faulted task for invalid charset declarations. Failures
while starting byte buffering or registering its completion now also produce a
faulted or canceled task rather than escaping during continuation setup.
A standalone `tests/http_charset` CX consumer verifies `StringContent` Latin-1
and ASCII bytes, automatic and explicit synchronous/asynchronous decoding,
invalid ASCII input, unsupported charsets, duplicate charset parameters, and
conflicting media-type and encoding declarations. It passes all six Windows
x86/x64 and Linux x64 static/shared runtime cells. Strict GCC and Clang 14 Linux
x64 static/shared builds pass; Clang shared dependencies resolve with `ldd -r`.
Bounded UTF-8 error-prefix reads trim only a potentially valid incomplete final
sequence; impossible lead/continuation prefixes remain intact and fail decoding.
The expanded managed HTTP e2e, including this buffering path, passes strict Clang 14 Linux x64 static/shared builds.
A fast 16 MiB headers-first fixture constrains the server send buffer, pauses
the consumer until the sender stalls short of the body length, then resumes and
checks every patterned byte through EOF in all six Windows x86/x64 and Linux x64
static/shared cells. It exposed registration-record exhaustion across repeated
libcurl socket remove/re-add events; the transport now reuses detached records
and waits for sufficient ring capacity before unpausing a response chunk.
The full 12-test `cxnet` CTest suite passes in all six Windows x86/x64 and Linux
x64 static/shared cells.
An abandoned-response fixture holds a headers-first body open, starts a pending
body read, verifies disposing the response cancels that read and closes the
transfer, and confirms the same `HttpClient` can complete a later request on a
fresh connection on Linux x64 and Windows x86/x64. The fixture now disposes the
response twice and forces GC while the body read is pending, then verifies the
read remains pending through collection, is canceled by disposal, and recovery
remains intact in all six Windows x86/x64 and Linux x64 static/shared cells. It
also tests the inverse order, explicitly disposing the caller-held
`CurlResponseStream` before disposing the response twice. Both orders preserve
the canceled read and same-client recovery in all six cells.
`CurlHttpMessageHandler.CaCertificateFile` adds a CA bundle while peer and
hostname verification remain enabled. `AllowUnknownCertificateRevocation`
defaults to false and only enables Schannel's best-effort behavior when
revocation status is unavailable; it does not disable chain or hostname checks.
TLS option getters and setters synchronize with request startup. Updating a CA
or revocation setting creates and installs a replacement connection share
before committing the setting and releasing the previous share, so share
creation failure leaves the old configuration intact. Handler disposal marks
the handler closed even if releasing the native share throws.
A local HTTPS fixture passes trusted-CA, default-trust rejection, wrong-host,
expired-certificate, and unrelated-CA cases on Linux x64 and Windows x86/x64.
libcurl result codes now fault requests even when its diagnostic error buffer is
empty.

Static curl/zlib assets and transitive CX module dependencies are packaged and
consumer compile/link has passed on all supported RIDs. An isolated module-load
smoke test passes on Windows x86/x64 and Linux x64, using only the module
directory plus System32 on Windows and an empty `LD_LIBRARY_PATH` on Linux.
Linux's matching system OpenSSL `libssl`/`libcrypto` libraries and the Windows
VC runtime remain system requirements. Package-only build and isolated loading pass on pristine Debian
12 (glibc 2.36), Debian 13.7 (glibc 2.41), Ubuntu 22.04 (glibc 2.35), Ubuntu
24.04 (glibc 2.39, GCC 13.3.0, OpenSSL 3.0.13), Rocky Linux 9.9
(glibc 2.34), and AlmaLinux 10.3 (glibc 2.39) with
system OpenSSL 3. AlmaLinux's package-only consumer passes both standard and
`-Wall -Wextra -Werror -Wno-unused-value` builds after refreshing `cxcore` and
`cxnet`. The strict module loads with an empty `LD_LIBRARY_PATH`, and `ldd -r`
reports no unresolved symbols. Earlier stale package artifacts had triggered
GCC 14.4 warnings in `TaskScheduler_RunUntil` and left unused delegate
declarations in generated headers; the current scheduler, ASCII comparison,
and generated-header sources are verified in the refreshed package. Ubuntu
24.04 also passes the strict build, isolated load, and `ldd -r` checks against
the current package-only stage. CMake 3.31.6 was installed in its disposable
container because the distribution package is older than the generated
project's minimum.
Debian 13.7
uses GCC 14.2 and OpenSSL 3.5.7; its package-only consumer also builds with
`-Wall -Wextra -Werror -Wno-unused-value`. Rocky's system `libcrypto.so.3` adds a
transitive `libz.so.1` dependency while the consumer links the package's static zlib archive. Other
pristine OS images remain open. Continue the socket, timer, DNS, streaming,
cancellation, TLS, and platform-link gates before relying on the backend. If
that integration gate fails, reassess a maintained alternative rather than silently falling
back to a blocking request API.

The Linux curl archive was rebuilt with OpenSSL Engine support disabled while
retaining zlib, Brotli, and zstd. This removes the `ENGINE_*` references that
prevented loading against OpenSSL 4. A fresh-feed package-only consumer now
passes strict compile, isolated load, and `ldd -r` on Ubuntu 22.04.5 with
OpenSSL 3.0.2, Ubuntu 26.04.1 with OpenSSL 3.5.5, and Ubuntu 26.10 with
OpenSSL 4.0.3. The rebuilt archive also passes all 12 Linux x64 CTests in both
static and shared modes on Ubuntu 26.10, including managed HTTP, HTTPS,
redirect, and streaming cases. Full package hashes and stage details are in
[`tests/package_consumer/README.md`](../tests/package_consumer/README.md).
The current fresh-feed packages also pass the strict package-only build,
isolated load with an empty `LD_LIBRARY_PATH`, and `ldd -r` on Debian 12.15
(glibc 2.36, GCC 12.2.0, OpenSSL 3.0.22). Broader pristine OS image coverage
and the remaining HTTP lifecycle gates remain open. A clean Rocky Linux 9
stage (glibc 2.34, GCC 11.5.0, OpenSSL 3.5.9) also passes the fresh-feed
package build with `-Wall -Wextra -Werror -Wno-unused-value`, isolated load,
and `ldd -r`.

On 2026-11-22, the package archives validated on Arch Linux also passed the
strict package-only consumer on Ubuntu 26.10 (glibc 2.44, GCC 15.3.0, CMake
4.3.4, OpenSSL 4.0.3, .NET runtime 10.0.12). Restore used the exact archived
feed; the isolated load passed with `LD_LIBRARY_PATH` unset, and `ldd -r`
reported no unresolved symbols. Broader pristine OS image coverage and the
remaining HTTP lifecycle gates stay open. Package hashes and stage details
are recorded in [`tests/package_consumer/README.md`](../tests/package_consumer/README.md).

On 2026-11-22, the freshly published packages also passed on pristine openSUSE
Tumbleweed (`VERSION_ID=20261116`, glibc 2.44, GCC 16.2.1, OpenSSL 3.5.3,
CMake 4.5.0, .NET SDK 10.0.404). Strict compilation with
`-Wall -Wextra -Werror -Wno-unused-value`, isolated loading with
`LD_LIBRARY_PATH` unset, and `ldd -r` passed without unresolved symbols. The
stage contained only the consumer source, manifest, lock file, and restored
packages; the compiler and loader helper were mounted separately. Archive
hashes and stage details are recorded in
[`tests/package_consumer/README.md`](../tests/package_consumer/README.md).
The same stage also built a package-only executable that completed a loopback
`HttpClient.GetAsync`, checked status 200 and the response body, and ran with
`LD_LIBRARY_PATH` unset.

On 2026-11-22, the package-only HTTP smoke was repeated in a clean Ubuntu
26.10 container against its OpenSSL 4.0.3 packages. The staged consumer and
packaged runtime sources compiled with GCC 15.3.0, CMake 4.3.4, and strict
`-Wall -Wextra -Werror -Wno-unused-value`; the loopback request returned status
200 and body `m26-package-http-ok` with `LD_LIBRARY_PATH` unset. The compiler
was mounted separately and no source checkout was present. Package hashes and
stage details are recorded in
[`tests/package_consumer/README.md`](../tests/package_consumer/README.md).

On 2026-11-23, freshly rebuilt current-source packages also passed the strict
isolated consumer build, module load with `LD_LIBRARY_PATH` unset, and `ldd -r`
on CentOS Stream 10 (glibc 2.39-148, GCC 14.4.1, OpenSSL 3.5.9, CMake 4.4.4,
.NET SDK 10.0.401). Only the consumer, lock file, and restored packages were
staged; compiler and toolchain files were mounted separately. Archive hashes
are recorded in the package-consumer README. Other pristine OS images and the
remaining HTTP lifecycle gates remain open.

The current locked packages also pass the package-only HTTP smoke in a clean
Arch Linux container (glibc 2.44, GCC 16.2.1, OpenSSL 4.1.0, CMake 4.5.1,
.NET SDK 10.0.114). The Release build and loopback `HttpClient.GetAsync` pass
with `LD_LIBRARY_PATH` unset; `ldd -r` reports no unresolved symbols. The stage
contained only the consumer source, manifest, lock file, and restored packages;
the compiler and feed were mounted separately. Package hashes and staging
details are recorded in the package-consumer README. Further pristine OS images
and the remaining HTTP lifecycle gates remain open.

The package-consumer fixture now includes a separate executable that references
the restored consumer library and performs `HttpClient.GetAsync` against a
loopback fixture. Fresh current-source packages compiled with GCC 11.4.0,
CMake 3.31.6, and strict warnings; static and shared executables passed in a
clean Debian 12 container (glibc 2.36, system OpenSSL 3) with
`LD_LIBRARY_PATH` unset. `ldd -r` reported no unresolved symbols. The shared
case exposed missing transitive CX libraries in the final executable directory;
the CMake generator now propagates shared project runtime files through
references and copies them beside the consumer. The package hashes and
repeatable runner are in
[`tests/package_consumer/README.md`](../tests/package_consumer/README.md).

On 2026-11-20, freshly published current-source packages also
passed a package-only consumer build on Debian 13 (glibc 2.41, GCC 14.2.0,
CMake 4.4.4, .NET 10.0.401) with `-Wall -Wextra -Werror -Wno-unused-value`.
The isolated loader passed with `LD_LIBRARY_PATH` empty and `ldd -r` found no
unresolved symbols. The stage held only the consumer source, manifest, lock,
and restored packages; the current compiler and loader helper were mounted
separately. The tested `cxcore.0.1.0.zip` SHA-256 is
`ECBC50766E1E3AAA97CE80C71CCC65DDAD5801A87C9491EF728F88C347517048`, and
`cxnet.0.1.0.zip` is
`B8637C997B0AC108A7306B2440A66D00D25065EDBFCCF6870987C3E2ECAAB9CF`.
After the HTTP upgrade rejection was added, a second fresh feed also passed the
same strict Debian 13 package-only build, isolated load, and `ldd -r` check.
That package contains the updated status handling; its `cxnet.0.1.0.zip`
SHA-256 is
`B58A08D30CCB5176C511733AC5CA85C5BA7FCA507F71CD34539A167C976497E4`.
A subsequent fresh package containing the bounded UTF-8 prefix correction
passed the same strict build, isolated load, and `ldd -r` check. Its
`cxnet.0.1.0.zip` SHA-256 is
`AC15D9A567DF5DDF038D8DCA2854B9CE729C32BE8AA7C219EE44D50393F4F6F8`.
A fresh package containing the RFC 6265 Domain attribute handling also passed
the Debian 13 strict package-only build, isolated load, and `ldd -r` check.
Its `cxnet.0.1.0.zip` SHA-256 is
`32ECA8B9FE34661E1C604D0A0245F13E098EF98B485427E2A5F450498AE89757`.
After the IP-literal domain-suffix check was added, a fresh package-only stage
again compiled with `-Wall -Wextra -Werror -Wno-unused-value` on Debian 13,
loaded with `LD_LIBRARY_PATH` empty, and passed `ldd -r` without unresolved
symbols. The stage contained only the consumer source, manifest, lock file,
and restored packages; compiler and loader helper were mounted separately.
The tested `cxcore.0.1.0.zip` SHA-256 is
`ECBC50766E1E3AAA97CE80C71CCC65DDAD5801A87C9491EF728F88C347517048`, and
`cxnet.0.1.0.zip` is
`C73CA7FC619B9A8AFE9ACAF38F16E3A40743FE67711CB79CABE86040AEF7E46B`.
After replacing the IP-literal probe with an allocation-free IPv4 check, the
current `cxnet` package was republished and passed the same strict Debian 13
package-only compile, isolated load, and `ldd -r` checks. Its SHA-256 is
`C316E7859BB96CBCE56528783B023CA8309CA2CDE432CFB3FE8E203DF6157FAB`.
After IPv6 URI-host equivalence was added to same-origin redirect and cookie
matching, the fresh package passed those Debian 13 checks again. Its SHA-256 is
`AED48E090210B7FE13C0DD3AE5374ECD9C2157CC28023C99CC3BD55E3E7C3633`.
After URI percent escapes in zone identifiers were normalized during
comparison, the package-only consumer passed the same strict build, isolated
load, and `ldd -r` checks. The `cxnet.0.1.0.zip` SHA-256 is
`D98D91ED35F6A74FA262A8CD521BF2C07FAD3C4BD4E4D04E9A726B5E11CA517E`.
After cookie Domain values began using shared URI host canonicalization, a fresh
package-only Debian 13 consumer passed the strict compile, isolated load, and
`ldd -r` checks. The `cxnet.0.1.0.zip` SHA-256 is
`8428FF2F6C851F9B3D2D9EFC093BB305476B23CF747D6EA7AC271C13BE743D6B`.

Current-source Windows packages also pass package-only Release compilation in
win-x64 and win-x86 stages outside the workspace. The CMake caches resolve
`cxcore` from the restored package; isolated loaders pass in both architectures
using the system-only DLL search policy. These are compile/load checks; the
Windows x86 and x64 package consumers now also pass checkout-free loopback HTTP
GETs in static and shared modes, checking response status and body and
confirming one request. A trusted custom-CA HTTPS request to `localhost` also
passes, and a wrong-host HTTPS request is rejected before reaching the server.
Shared execution loads only the staged CX DLLs and OS libraries. Archive hashes
and staging details are recorded in
[`tests/package_consumer_windows/README.md`](../tests/package_consumer_windows/README.md).

## Dependency and implementation gates

**2026-11-26 NetworkStream peer-reset propagation:** added a focused managed
`NetworkStream.ReadAsync` case using a loopback TCP peer configured to reset the
connection. It forces GC while the read is pending and requires the task to
fault exactly as `Socket.ReadAsync` does, preserving `SocketError.ConnectionReset`
and the native error domain/code. The new `cxnet_network_stream_peer_reset`
CTest passes five consecutive runs in all eight Windows x86/x64 MSVC and Linux
x64 GCC 13.3/Clang 18 static/shared cells; Linux builds use strict warnings.
The same case is also included in the integrated DNS/socket fixture, which
passes in Windows x64 MSVC static mode (172.43 seconds) with the managed socket
and stream write-reset cases below. Other connect/reset, transfer, and lifecycle
gates remain open.

**2026-11-26 NetworkStream pending-write reset propagation:** added
`cxnet_network_stream_pending_write_peer_reset`. It fills the socket send
buffer, starts a one-byte managed `NetworkStream.WriteAsync`, verifies the
operation remains pending through forced GC, then resets the peer and requires
the task to fault with `SocketError.ConnectionReset` and native error metadata.
The focused CTest passes five consecutive runs in all eight Windows x86/x64
MSVC and Linux x64 GCC 13.3/Clang 18 static/shared cells, with strict Linux
warnings. The integrated DNS/socket fixture also passes in Windows x64 MSVC
static mode (172.43 seconds). Added companion
`cxnet_socket_pending_write_peer_reset` for managed `Socket.WriteAsync`; it
checks the same pending-through-GC reset behavior and error metadata. Both
write-reset CTests pass five consecutive runs in all eight Windows x86/x64
MSVC and Linux x64 GCC 13.3/Clang 18 static/shared cells, with strict Linux
warnings. Other connect/reset, transfer, and lifecycle gates remain open.

Caller-token cancellation now also has a focused
`cxnet_network_stream_pending_read_cancel` case. It keeps a stream read pending
through forced GC, cancels it, checks the socket remains connected and the
token callback is detached, then reads a peer byte through the same stream.
The CTest passes GCC 13.3 and Clang 18 static/shared builds with strict Linux
warnings and Windows x86/x64 MSVC static/shared builds. The matching Socket
CTest passes the same matrix.

The readiness-versus-cancellation race now also runs through
`NetworkStream.ReadAsync`: twelve trials require both cancellation and
readiness to win, exactly-once task completion, byte integrity, and successful
retry of queued data after cancellation. The focused CTest passes GCC 13.3 and
Clang 18 static/shared builds with strict Linux warnings and Windows x86/x64
MSVC static/shared builds. The matching Socket race CTest passes the same
matrix.

The full-duplex independent-cancellation fixture now also runs through
`NetworkStream`: across 24 iterations it cancels a pending read while a write
remains pending, drains the queued write from the peer, and transfers a byte in
the reverse direction through the stream. The focused
`cxnet_network_stream_full_duplex_independent_cancel` CTest passes GCC 13.3 and
Clang 18 static/shared builds with strict Linux warnings and Windows x86/x64
MSVC static/shared builds. The matching Socket CTest passes the same matrix.
The integrated DNS/socket fixture, including both cases, passes in Windows
x86/x64 MSVC static/shared modes in 176.05–176.28 seconds.

The partial-response-header cancellation fixture uses 10-second wall-clock
deadlines with state diagnostics and a 30-second CTest timeout. Its focused
CTest passes ten repeats in Windows x86/x64 MSVC static/shared builds and five
repeats in strict Linux GCC 13.3/Clang 18 static/shared builds using system
libcurl 8.5.0. The pinned-libcurl HTTP/REST/HTTPS selection passes 58/58 CTests
in Windows x86/x64 MSVC static/shared builds.

The REST recording fixture now also covers direct and wrapped
`const Dictionary<string, bool?>` JSON results, including `true`, `false`, and
`null`, plus wrapped status/header metadata and response-content ownership.
This new fixture shape passes the recording fixture in all eight Windows x86/x64
MSVC and Linux x64 GCC/Clang static/shared cells. Direct and wrapped live-wire
responses pass the same eight cells.

| Gate | Initial decision/requirement | Status |
|---|---|---|
| Shared URI | Add/reuse `System.Uri` in `cxcore` and record ownership | Hierarchical and opaque URI forms pass the native test in all six Windows x86/x64 and Linux x64 static/shared cells. Unicode host labels Punycode-encode to ASCII `xn--` labels; common German and Japanese examples, a trailing root dot, and RFC 3986 section 5.4 normal/abnormal reference cases (including empty references and absolute paths) pass the expanded native fixture in all six cells. Pinned Unicode 18.0.0 UTS #46 mapping, canonical NFC normalization, leading-combining-mark rejection, ContextJ, all registered ContextO checks, RFC 5893 bidi validation, and IDNA hyphen/ASCII label syntax are implemented before Punycode encoding; representative mapping/NFC, ContextJ, ContextO, bidi digit-mixing, hyphen, and canonical A-label acceptance/rejection cases now pass in all six cells. Escaped UTF-8 and ASCII-unreserved host normalization, uppercase retained reserved escapes, malformed escaped UTF-8 rejection, and case-preserving IPv6 zone identifiers also pass in all six cells. `cxcore` and downstream `cxnet` Release compiles pass. Unicode 18 UTS #46 Greek final-sigma/A-label, ideographic separator plus fullwidth ASCII mapping, and valid-symbol label vectors and a V7 compatibility-character rejection pass in all eight Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared cells; the fixture uses vectors from lines 193–196, 224–235, 289–294, and 306–316 of [Unicode 18.0.0 IdnaTestV2.txt](https://www.unicode.org/Public/18.0.0/idna/IdnaTestV2.txt). The data-driven native URI test checks 558 accepted DNS-safe host vectors and rejects 5,248 DNS-shaped vectors carrying UTS #46 bidi, ContextJ, Punycode/STD3, label-validity, and applicable DNS-length errors, including five ASCII-only names with invalid label hyphen positions. It includes U+11C3A, a General_Category=Mark scalar with canonical combining class zero. Before the DNS-length adjustment, both URI tests passed all eight Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared cells, plus strict Linux GCC 11.4 and Clang 14 static/shared builds; the updated 5,248-vector corpus and focused tests pass all eight Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared cells; the current Linux reruns use GCC 14.2.0 and Clang 19.1.7. The generator now includes Unicode 18 UnicodeData.txt mark categories. The focused mapping-property CTest rejects 2,316 leading-Mark scalars, checks canonical output for 6,465 mapped and 294 ignored scalars, rejects all 304 mappings to non-DNS ASCII, and rejects 1,321 scalar endpoints sampled from every disallowed mapping range; it passes the eight Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared cells, with strict Linux warning flags. The IdnaTestV2 corpus excludes non-host URI characters, general ASCII URI reg-names, and DNS-length-only cases attributable solely to a trailing root dot, which this parser preserves; UTS #46 status/error and broader URI conformance remain open. Supplementary label boundaries are covered at 63/64 bytes; hostname boundaries distinguish 253 unrooted bytes, 254 rooted bytes, and rejection beyond those limits. HTTP rejects opaque request/base URIs; managed checks pass on Linux x64 and Windows x64. |
| Async streams | Coordinate `ReadAsync`/`WriteAsync` byte operations in `cxcore` | Base contract and NetworkStream async overrides implemented; `Stream.CopyToAsync` uses bounded task-continuation copying and powers streamed HTTP content copies. A standalone fixture verifies copy positions, replay after seekable materialization, buffer-limit failure and rewind, non-seekable streaming and one-shot behavior, over-reported reads, read/write faults including synchronous exceptions thrown while starting async reads and writes and null tasks returned by async reads or writes, pre-cancellation, cancellation during a pending read, cancellation after a pending source read later succeeds (the destination write is skipped even when it ignores the canceled token), `StreamContent.CopyToAsync` cancellation while a destination write is pending (late successful completion does not start another source read), disposal during a pending destination write (the write completes, the next source read faults with `ObjectDisposedException`, and the source is disposed exactly once), disposal during a pending streaming-copy read (the source read and copy fault with `ObjectDisposedException`, with exactly-once source disposal), cancellation winning over a later EOF completion, and shared-task identity, cancellation, and exactly-once source disposal when `StreamContent` is disposed during pending byte materialization; a pending source read that later completes with data leaves the task canceled and starts no follow-up read. Synchronous `ReadAsync` setup failure during byte materialization is cached as the same faulted task across retries, and repeated disposal closes the owned source exactly once; this case passes all eight Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared cells. The default `ReadAsStringAsync()` path also verifies shared-task identity, disposal cancellation during a pending buffer read, and cancellation winning over a late EOF. The fixture passes in all eight Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared runtime cells. Linux x64 static/shared builds pass strict GCC and Clang 14 builds; Clang shared runtime dependencies resolve with `ldd -r`. The `HttpContentSetup` fixture also gates synchronous `StreamContent.ReadAsByteArray()` against disposal and passes all eight Windows x86/x64 MSVC and Linux GCC/Clang static/shared cells. With test hooks enabled, the same fixture injects failures at source-read and internal buffer-write continuation registration during `StreamContent` byte materialization. Both paths require a faulted, cached task, one source read, and disposal of the owned source; they pass all eight Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared cells, with strict Linux builds using `-Wall -Wextra -Werror -Wno-unused-value`. The `cxnet_managed_stream_copy_continuation_registration_failure` CTest injects failure while registering the source-read continuation and destination-write continuation; each copy faults, and the write case confirms the source stops at the copied byte. It passes all eight Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared cells. The standalone `stream_copy` fixture also races `StreamContent` disposal and caller cancellation for 24 iterations while a copy waits on a pending destination write. After the write completes, it accepts cancellation before another source read, `ObjectDisposedException` on the next read, or successful completion if the copy wins; every outcome must start no second destination write and dispose the source exactly once. This case passes all eight Windows x86/x64 MSVC and Linux x64 GCC 13.3/Clang 18 static/shared cells, with strict Linux warnings. The fixture also verifies faulted-task behavior for a null destination, a non-writable destination, and a non-readable source in those eight cells. Disposing a destination during a pending write faults the copy with `ObjectDisposedException`, starts no further source read, and remains safe under repeated disposal; this fixture case passes in all eight cells. Broader stream lifecycle and other injected setup-failure shapes remain open |
| StreamContent materialization/disposal | Keep synchronous byte materialization consistent with concurrent content disposal | The `stream_copy` fixture races `StreamContent.ReadAsByteArray()` with disposal twelve times over a 4 MiB payload; only a complete byte-checked result or `ObjectDisposedException` is accepted, and the owned stream must be disposed exactly once. It passes all eight Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared cells; Linux builds use strict warning flags; a pending asynchronous byte-materialization read that returns data after content disposal leaves the task canceled and starts no additional source read. This case passes all eight cells, with strict Linux GCC and Clang static/shared builds |
| Shared exceptions | Reuse Milestone 24 exceptions; `SocketException` remains in `cxnet` | Common POSIX/Winsock connection refusal/reset/timeout and DNS not-found/retry mappings pass on Linux x64 and Windows x86/x64; deterministic sync/async resolver-failure identity passes in all six Windows x86/x64 and Linux x64 static/shared cells; async resolver-failure/refused-connect stress repeats five times per invocation with GC during pending work and exactly-once completion; 16 scheduler-timed connect/cancel races check exactly-once completion and legal canceled/connected outcomes. These pass strict GCC and Clang 14 Linux static/shared plus Windows x86/x64 static/shared |
| REST generator | CX-authored generator consuming compiler snapshots | Closed public-interface generation, closed generic contracts, scalar and dictionary query bindings, `char` path/query/header/form/multipart request bindings and JSON response parsing are implemented and compile-checked; live `char` path/query/JSON response/form/multipart wire cases pass in all six Windows x86/x64 and Linux x64 static/shared cells. Direct and wrapped boolean/numeric JSON results include nullable null/value handling with JSON whitespace, empty, and HTTP-error cases; direct and wrapped non-null scalar/string collection results are implemented, with `const Array<int>`, `const Array<string>`, and `const List<int>` recording cases covering values, escaped strings, wrapper metadata, and ownership in all six cells and strict Linux GCC/Clang static/shared builds; direct and wrapped numeric collection results for `Array<int>`, `List<int>`, `LinkedList<int>`, `Queue<int>`, and `Stack<int>` pass live-wire checks with sequence order and wrapper metadata; direct and wrapped `const Array<int>` and `const List<int>` results also pass live-wire checks in all eight Windows x86/x64 and Linux x64 GCC/Clang static/shared cells, including wrapper status, header, and disposal; direct and wrapped `const Array<string>`, `const List<string>`, `const LinkedList<string>`, `const Queue<string>`, and `const Stack<string>` results pass live-wire checks in the same cells, including escaped quotes/newlines, sequence order, and wrapper status, header, and disposal; direct and wrapped DTO wire results cover `List`, `Array`, `LinkedList`, `Queue`, and `Stack`. Nullable DTO collection wire results cover direct and wrapped `const List<RestItem?>`, `const Array<RestItem?>`, `const LinkedList<RestItem?>`, `const Queue<RestItem?>`, and `const Stack<RestItem?>` with an intervening null element, wrapper metadata, and disposal in all eight Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared cells. Nullable string-array and string-map JSON results retain direct recording/live-wire coverage, with wrapped nullable string arrays, DTO dictionaries, and `const Dictionary<string, string?>` results. The wrapped nullable string-map response checks a populated value, a null value, an explicitly empty string, wrapper metadata, and content ownership through disposal in the recording and live-wire fixtures across all eight Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared cells; common form/multipart bindings, DTO serialization, ownership, and errors are implemented. Wire fixtures verify escaped GET/JSON POST, PUT/PATCH/DELETE/HEAD, repeated query values, an escaped dictionary query map with repeated collection values, insertion order, and empty-value omission, URL-encoded form fields with repeated ordered values for `ICollection<string>`, `IEnumerable<string>`, arrays, `List<string>`, `LinkedList<string>`, `Queue<string>`, and `Stack<string>` (`role=admin+user&role=a%26b`); nullable form sequences for `Queue<int?>`, `LinkedList<decimal?>`, and `Stack<bool?>` omit nulls, preserve invariant formatting and FIFO/top-first order, and pass recording/wire fixtures in all eight cells; live repeated multipart fields for `IEnumerable<string>`, arrays, `List<string>`, `ICollection<string>`, `LinkedList<string>`, queues, and stacks, plus a filename-bearing `HttpContent` part with preserved media type in all six cells and strict Linux GCC/Clang builds; 96 KiB byte-array and wrapped DTO responses, a bounded DTO `ApiException`, 204 wrapper/direct-DTO behavior, generated static/dynamic/auth headers, nullable query/header omission and empty-value transmission, same-client concurrent calls with distinct responses, owned/borrowed `HttpClient` disposal, streamed response reads, and cancellation, explicit disposal, and their overlap while a headers-first body read is pending, with server-observed connection closure in all six Windows x86/x64 and Linux x64 static/shared cells; repeated form-field, concurrency, nullable-value, and ownership assertions also pass strict Linux GCC and Clang static/shared builds; the recording-handler and wire fixtures force GC during pending request/body work and while DTO, response-wrapper, and API error values are live, passing all six cells; invalid-contract fixtures verify `CXREST001` for an open generic `Task<T>` result and source diagnostics for unresolved type arguments in open contract requests. Open generic requests are intentionally unsupported and rejected before native generation; additional bindings/shapes and broader wire-level verification remain open |
| JSON | Thin adapter to Milestone 20 CXDATA mappings, optional package dependency | Generated request/result codecs and closed generic DTO codecs are wired through the generator; `cxnet` remains independent of `cxdata` unless the consuming project configures the JSON generator |
| HTTP engine | libcurl 8.23.0 static, Schannel on Windows and OpenSSL on Linux; require threaded resolver and feature checks | Source/signature/hash/build matrix validated; runtime asynchronous DNS/TLS/protocol/zlib/Brotli/zstd feature checks and managed GET/POST/cancellation/timeout pass on Linux x64 and Windows x86/x64; static curl/zlib/Brotli/zstd assets pack and restore by RID; shared consumer modules compile and link on all supported RIDs; managed gzip/deflate/Brotli/zstd decoding and encoded-header cleanup pass in all six Windows x86/x64 and Linux x64 static/shared cells; a stacked double-gzip body with `Content-Encoding: gzip ,  gzip` verifies comma-token and optional-whitespace parsing in the same six cells; a conflicting duplicate response `Content-Length` case is rejected as invalid framing in all six cells; shared-mode native socket/scheduler/DNS/HTTP-type tests, authenticated-proxy public smoke, buffered GET with relative redirect, trusted custom-CA HTTPS success, wrong-host HTTPS rejection, and caller cancellation of a slow request pass on Windows x86/x64 and Linux x64; headers-first caller cancellation after a body prefix, pending and subsequent read cancellation, and transport closure pass three consecutive runs in all six static/shared Windows x86/x64 and Linux x64 cells; a concurrent two-request headers-first case verifies canceling one request leaves the other's pending read intact before canceling it, then confirms both later reads remain canceled and both connections close; it passes three runs in all six static/shared cells; a second concurrent case cancels one caller token while disposing their shared client, requiring both pending reads canceled and both connections closed; it passes three runs in all six static/shared cells; a held GET disposed through the owning scheduler while its send is active verifies task cancellation, transport closure, and request reservation release in all six Windows x86/x64 and Linux x64 static/shared cells; trusted-CA, default-trust, wrong-host, expired-certificate, and unrelated-CA HTTPS cases pass on all three targets; isolated module loading with only OS dependencies passes on all three RIDs; bounded streaming uploads, upload cancellation, seekable redirect replay, paced 256 KiB downloads, bounded streaming-size behavior, connection reuse, per-origin concurrent cap, and idle/lifetime expiry checks pass on Linux x86/x64 and Windows x86/x64; the comprehensive managed HTTP fixture also passes three consecutive runs in all six Windows x86/x64 and Linux x86/x64 static/shared cells; direct `HttpClient` `PostAsync`, `PutAsync`, `PatchAsync`, and `DeleteAsync` operations pass the full managed HTTP fixture in all eight Windows x86/x64 MSVC and Linux GCC/Clang static/shared cells; direct `CurlHttpMessageHandler.SendAsync()` loopback cases verify buffered bodies, headers-first stream reads, cancellation with connection closure, disposal during an active send, and disposal while a headers-first response-body read is pending (the read still returns its full body) in those same eight cells; the shared public-API smoke checks POST, PUT, and PATCH `StringContent` echo responses, a DELETE response, and `HttpClient.HeadAsync` returning HTTP 200 with an empty body despite a nonzero representation length; the same smoke verifies a pre-canceled streamed `HttpClient.PostAsync` returns canceled without reaching the server. Both cases pass all eight Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared cells; the 50-test HTTP/REST suite passes in each shared Windows x86/x64 MSVC and Linux x64 GCC/Clang build; the current full Linux HTTP/REST CTest matrix passes 77/77 in GCC static and shared, 78/78 in Clang static, and 77/77 in Clang shared, using an isolated `CX_OUTPUT_DIRECTORY_ROOT` for each cell; pristine OS installation checks and broader streaming lifecycle coverage remain open |
| Caller request disposal during response wait | Disposing the caller-owned `HttpRequestMessage` after upload while response headers are pending does not complete or corrupt the send task | The loopback fixture receives the full request body and holds its response; it disposes the request, forces GC, and verifies the task remains pending. Caller cancellation then cancels the task and closes the connection. Focused CTest passes all eight Windows x86/x64 MSVC and strict Linux x64 GCC/Clang static/shared cells |
| `Expect: 100-continue` rejection | Streamed POST rejected before its body is sent | Raw-socket fixture returns 417 after validating the POST and Expect header, then checks for body bytes before closing; managed HTTP e2e passes this case in all six Windows x86/x64 and Linux x64 static/shared cells |
| Early rejection during a streamed upload | Server may reject after receiving only part of the request body | A raw-socket fixture sends `100 Continue`, receives part of an 8 MiB streamed body, then returns 413; the client returns the response and the server observes under 1 MiB uploaded in all six Windows x86/x64 and Linux x64 static/shared cells and strict GCC Linux x64 static/shared builds |
| Streaming upload source failure and client recovery | A source `ReadAsync` faults after request-body bytes have reached the transport | The standalone `upload_source_failure` fixture releases a pending source read with `IOException` after the loopback peer receives the four-byte prefix. `SendAsync` must fault with `HttpRequestException` retaining an inner exception, the failed connection must close, the request-owned source must be disposed exactly once, and the same `HttpClient` must complete a later GET on a fresh connection. It passes five consecutive runs in all eight Windows x86/x64 MSVC and Linux x64 GCC 11.4/Clang 14 static/shared cells, with strict Linux warning flags. |

The adapter keeps libcurl's default `CURLOPT_KEEP_SENDING_ON_ERROR` behavior:
an early HTTP response with status 300 or higher stops the upload and closes
the stream/connection ([libcurl option documentation](https://curl.se/libcurl/c/CURLOPT_KEEP_SENDING_ON_ERROR.html)).
| TCP native APIs | Extend common runtime and Windows/Linux backends | Create/connect/bind/listen/accept/shutdown/transfer and async DNS/hostname connect implemented; configurable 15-second default async hostname-connect deadline covers DNS and address fallback, with delayed-resolver timeout, late-result, socket-reuse, and cancellation-during-DNS checks passing in all six Windows x86/x64 and Linux x64 static/shared cells; the focused async DNS/connect fixture passes five consecutive runs in every one of those six cells, including 16 scheduler-timed connect/readiness cancellation attempts per invocation with delays spanning 0–34 ms and exactly-once task continuation checks; the same fixture includes 16 timed accept/cancel races with GC, exactly-once callbacks, canceled-then-reused and successful accepts, and byte transfer, passing Windows x86/x64 static/shared and strict Linux GCC/Clang x64 static/shared; resolver-failure and refused-connect attempts force GC and assert exactly-once completion, passing strict Linux GCC/Clang x64 static/shared plus Windows x86/x64 static/shared; dual-mode IPv6 wildcard bind, `::1` transfer, and IPv4-mapped transfer pass Windows x86/x64 and Linux x64 static/shared; `Socket.ReadAsync` versus `Socket.Dispose` repeats 24 times and verifies returned-task cancellation or rejected read admission, cleared `Connected`, and peer-observed connection closure in all eight Windows x86/x64 and Linux GCC/Clang static/shared cells; 24-iteration failed endpoint-connect reset races update `NoDelay`, `KeepAlive`, and `ReuseAddress` concurrently and verify connection-refused completion, reset socket state, and post-reset option access in those same eight cells; five connected-socket options each race disposal for 24 iterations and pass all eight cells; a pending `Socket.ReadAsync` followed by `Shutdown(Receive)` completes with POSIX EOF, while Windows completes with `ConnectionAborted` after peer traffic wakes the pending readiness wait; both preserve `Connected` and pass 24-iteration focused CTests in all eight Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared cells. The same 24-iteration case through a non-owning `NetworkStream.ReadAsync` also passes all eight cells; a 24-iteration non-owning `NetworkStream.WriteAsync`/`Dispose` admission race also passes its focused CTest five consecutive times in all eight cells, preserving the underlying socket and reverse-direction traffic when the write is admitted and reporting disposal on rejected admission; pending `Socket.WriteAsync` followed by `Shutdown(Send)` preserves queued peer delivery and reverse-direction receives while faulting with `SocketError.Shutdown` (`EPIPE` on POSIX, `WSAESHUTDOWN` on Windows), passing all eight cells; the matching non-owning `NetworkStream.WriteAsync` half-close case also passes all eight cells and reads the reverse-direction byte through the same stream; pending socket and non-owning stream writes also survive `Shutdown(Receive)`, deliver the queued and pending bytes, and pass 24-iteration tests in all eight cells; Linux builds use strict warning flags; pending nonblocking `ReadAsync` and saturated `WriteAsync` operations also fault after a loopback peer reset, passing all eight Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared cells; the nine socket/DNS CTests pass in each shared Windows x86/x64 MSVC and Linux x64 GCC/Clang build; managed `Socket.ReadAsync` and `Socket.WriteAsync` caller-token cancellation tests verify cancellation, callback cleanup, connection preservation, and transfer-slot reuse in all eight Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared cells; other connect/reset and broader transfer races, broader HTTP lifecycle, platform/package verification, and remaining M26 acceptance items remain open |

## References

- [libcurl multi interface](https://curl.se/libcurl/c/libcurl-multi.html) and
  [multi socket action](https://curl.se/libcurl/c/curl_multi_socket_action.html)
- [libcurl SSL options](https://curl.se/libcurl/c/CURLOPT_SSL_OPTIONS.html) and
  [CA certificate files](https://curl.se/libcurl/c/CURLOPT_CAINFO.html)
- [libcurl source installation and TLS trust configuration](https://curl.se/docs/install.html)
- [RFC 6265](https://www.rfc-editor.org/rfc/rfc6265.html)
- [.NET Socket API](https://learn.microsoft.com/en-us/dotnet/api/system.net.sockets.socket)
- [.NET HttpClient API](https://learn.microsoft.com/en-us/dotnet/api/system.net.http.httpclient)
- [Refit generated REST clients](https://www.reactiveui.net/documentation/refit/)
- [RFC 9110](https://www.rfc-editor.org/rfc/rfc9110.html) and
  [RFC 9112](https://www.rfc-editor.org/rfc/rfc9112.html)



On 2026-11-23, the current-source CentOS Stream 10 package-only consumer also
built and ran a standalone loopback `HttpClient.GetAsync` smoke. The request
returned status 200 and `m26-package-http-ok` with `LD_LIBRARY_PATH` unset;
strict compilation and `ldd -r` passed. Package hashes and stage details are
recorded in [`tests/package_consumer/README.md`](../tests/package_consumer/README.md).

On 2026-11-25, a package-only HTTPS smoke on CentOS Stream 10 found that the
libcurl build-time CA path was absent on that distribution. When no explicit
`CaCertificateFile` is configured, the Linux handler now checks libcurl's
default CA file and searches readable distro bundle paths if needed; certificate
peer and hostname checks remain enabled. Fresh static and shared package-only
consumers pass HTTP, system-trust HTTPS, explicit-CA HTTPS, and wrong-host
rejection with `LD_LIBRARY_PATH` unset. The detailed environment and archive
hashes are in the package-consumer README.

The current-source package-only HTTPS smoke also passes in Rocky Linux 9.9
(glibc 2.34, GCC 11.5.0, OpenSSL 3.5.9) in strict static and shared builds.
System-trust HTTPS and an explicit CA both succeed, while wrong-host TLS fails
before an HTTP request is delivered. The Rocky bundle fallback works with peer
and hostname verification enabled; `ldd -r` reports no unresolved symbols in
either mode. The package hash and staging details are in the package-consumer
README.

The REST generator's recording and live-wire fixtures also cover nullable UUID
query and header parameters. They verify that null values are omitted and
populated values use canonical hyphenated formatting in all eight Windows
x86/x64 MSVC and Linux x64 GCC/Clang static/shared cells.

Nullable UUID collection query binding is also checked with a `const
Array<Uuid?>` containing two values around a null element. Recording and
live-wire checks verify omission, value order, and canonical hyphenated UUID
formatting in all eight Windows x86/x64 MSVC and Linux x64 GCC/Clang
static/shared cells, with strict Linux warning flags.

`Stream.CopyToAsync` checks cancellation again after a pending source read
completes and before starting a destination write. A deterministic fixture
uses a source that returns data after cancellation and a destination that
ignores the canceled token; no write is started and the copy remains canceled.
The stream-copy fixture passes Windows x86/x64 and Linux x64 GCC/Clang
static/shared builds. Strict Linux GCC and Clang static/shared builds also pass.

On 2026-11-24, fresh packages built from the current workspace, including the ASCII IDNA hyphen validation update, passed static
and shared package-only consumer checks in Debian 13.7 (glibc 2.41, GCC 14.2.0,
OpenSSL 3.5.7, CMake 4.4.4, .NET SDK 10.0.401). Both Release builds used
`-Wall -Wextra -Werror -Wno-unused-value`; isolated loopback `HttpClient` GETs
returned status 200 and `m26-package-http-ok` with `LD_LIBRARY_PATH` cleared.
`ldd -r` found no unresolved symbols; in shared mode it resolved the colocated
consumer, `cxnet`, and `cxcore` modules. The staged builds contained only the
consumer/smoke sources, manifests, lock file, and restored packages; the
current compiler was mounted separately. Archive hashes and details are in
[`tests/package_consumer/README.md`](../tests/package_consumer/README.md).

On 2026-11-25, the package-only REST/CXDATA JSON consumer also passed in a clean
Fedora 44 container (glibc 2.43, GCC 16.2.1, CMake 4.3.0, OpenSSL 3.5.9).
Strict static and shared builds, the loopback generated REST request, isolated
execution with `LD_LIBRARY_PATH` unset, and `ldd -r` all passed. The stage had
no repository checkout; package archive hashes and build details are recorded
in [`tests/package_consumer/README.md`](../tests/package_consumer/README.md).

The managed socket read and write tests now race peer readiness against
caller-token cancellation. Across 12 trials per direction, each run requires
both valid winners and exactly one task continuation. The write case validates
the peer payload and clean socket closure; the read case validates byte
integrity and recovers queued data after cancellation to prove the receive slot
was released. Both focused CTests pass in all eight Windows x86/x64 MSVC and
Linux x64 GCC/Clang static/shared cells. The write race also passes five
consecutive runs on Windows x64 static and Linux x64 Clang static; the read
race passes five consecutive runs in those same cells. The full
`cxnet_dns_async_tests` executable passes with both new cases integrated in
Linux Clang static and Windows x64 MSVC static.

The managed HTTP chunk grammar fixture mutates the first byte of a chunk size
across all 256 byte values, skipping the 22 hexadecimal digits and letters that
form valid size digits. The other 234 values, including NUL and high bytes, must
fault through the public `HttpClient` call. It also mutates each CR/LF delimiter
in a valid chunked body and truncates the body at every prefix length; separate
cases cover malformed extensions, data terminators, trailers, and oversized
size lines. The focused grammar CTest passes Windows x86/x64 MSVC and Linux x64
GCC/Clang static/shared builds with strict Linux warnings. More combinations of
interacting malformed fields and the remaining Milestone 26 gates remain open.

On 2026-11-26, the selected 58-test managed HTTP/REST/HTTPS suite passed using
pinned libcurl 8.23.0 in Linux x64 GCC 13.3 and Clang 18 static/shared builds
with strict warnings. Durations were 284.69s (GCC static), 360.08s (GCC
shared), 304.13s (Clang static), and 262.82s (Clang shared). A first Clang
shared run exposed a fixture issue: cancellation may produce an abortive TCP
reset rather than clean EOF. The test fixture now accepts reset/abort as peer
closure and still fails on timeout or unrelated socket errors. The corrected
Clang shared suite passed 58/58, and the focused partial-header cancellation
test passed five repeats in the other three Linux cells. Windows x86/x64 MSVC
static/shared with pinned curl each pass 58/58. The remaining Milestone 26
lifecycle, pristine-target, and separate-host benchmark gates are open.

The reset-aware partial-header cancellation fixture was also rebuilt with MSVC
19.51 and pinned libcurl 8.23.0 in Windows x86/x64 static/shared builds. Its
focused CTest passes in all four cells, covering the Winsock reset/abort path.

Five consecutive focused cancellation runs also passed in each Windows MSVC
cell, giving 20/20 Windows repeats. Linux GCC static/shared and Clang static
also passed five repeats; Clang shared passed the case in its corrected 58/58
suite. Every cell has a post-fix pass, with repeat runs in seven cells.

The integrated `cxnet_dns_async_tests` fixture, including NetworkStream peer-
reset and pending-write reset cases, passes in all eight cells. Windows x86/x64
MSVC static/shared took 176.46–177.55s; Linux x64 GCC 13.3/Clang 18
static/shared took 30.99–31.44s with strict warnings and pinned libcurl 8.23.0.

On 2026-11-26, the current-source TCP echo and streamed HTTP download examples
were rebuilt through the Release compiler and passed their loopback fixtures;
the HTTP download verified all 262,144 bytes. The generated REST CRUD example
was rebuilt and passed both in-process handler checks and its loopback HTTP wire
fixture on Windows x64 MSVC.

A fresh pinned-curl Linux x64 Clang 18.1.3 static build also passed the
sanitizer-backed `cxnet_managed_http_chunk_parser_fuzz` CTest: 100,000
libFuzzer runs, 20,000-byte input limit, and two-second per-input timeout in
49.78 seconds. Strict warning flags were enabled. This refreshes the chunk
boundary evidence against current sources and pinned libcurl 8.23.0.

The current compiler also passes all six REST generator lookup regression tests
and the invalid-contract diagnostics fixture, including the open-generic
`RestClient.For<T>()` rejection before generation.

The owning `NetworkStream.Dispose` versus `WriteAsync` admission race runs 24
iterations. Disposal may cancel an admitted write, or the returned task may
fault with `ObjectDisposedException`/`IOException` when disposal wins during
setup. In either case the socket must be disposed, already queued peer bytes
must remain intact, and the peer must observe EOF. The focused case and its
Socket counterpart pass Windows x86/x64 MSVC and Linux x64 GCC 13.3/Clang 18 in
static and shared modes; Linux builds use strict warnings and pinned libcurl
8.23.0. The integrated DNS/socket fixture passes in all eight Windows x86/x64 MSVC and Linux x64 GCC 13.3/Clang 18 static/shared
cells. Windows runs took 177.74–178.35s; Linux runs took 34.14–34.33s.

A 24-iteration owning NetworkStream read/disposal race starts `ReadAsync` on
the CX owner thread while a worker disposes the stream. The task must be
canceled after admission or fault with `ObjectDisposedException`/`IOException`
if disposal wins during setup; the peer must observe EOF. The focused CTest
passes all eight Windows x86/x64 MSVC and Linux x64 GCC 13.3/Clang 18
static/shared cells, using strict Linux warnings and pinned libcurl 8.23.0.
The integrated DNS/socket fixture passes all eight Windows x86/x64 MSVC and Linux x64 GCC 13.3/Clang 18 static/shared
cells. Windows runs took 177.62–179.28s; Linux runs took 34.08–34.98s.

The current-source package-only HTTP/HTTPS consumer also passes in a clean
Fedora 44 container (glibc 2.43, GCC/G++ 16.2.1, CMake 4.3.0, OpenSSL 3.5.9)
with strict static and shared builds. The smoke covers HTTP, system-trust HTTPS,
explicit-CA HTTPS, and wrong-host rejection. The isolated stage has no repository
checkout; the runner clears `LD_LIBRARY_PATH`, the fixture CA is installed only
in the disposable container, and `ldd -r` reports no unresolved symbols.
Archive hashes are in the package-consumer README.

The current locked package-only REST/CXDATA JSON consumer also passes on Fedora
44 (glibc 2.43, GCC/G++ 16.2.1, CMake 4.3.0, OpenSSL 3.5.9). Strict static and
shared builds decode the expected DTO from one loopback request; the isolated
stage has no repository checkout, `LD_LIBRARY_PATH` is cleared, and `ldd -r`
reports no unresolved symbols. Shared mode loads the staged `cxdata`, `cxnet`,
and `cxcore` modules. The refreshed locked `cxnet` package hash is
`4E0206799FBB46CE184C20AB8A823254FCB578DB19B2EC37E265B4EC36B4E3D2`; the
remaining archive hashes and detailed strict-build results are in the
package-consumer README.

**2026-11-26 non-owning NetworkStream read/disposal admission race:** added a
24-iteration race between starting `NetworkStream.ReadAsync` and disposing a
non-owning stream from a worker. If stream disposal wins, the test retries the
read through the still-owned socket; either way it verifies the byte arrives,
the socket remains connected until explicitly disposed, and the peer then sees
EOF. The focused CTest passes five consecutive runs in all eight Windows x86/x64
MSVC and Linux x64 GCC/Clang static/shared cells. The full current-source suite
passes 102/102 tests in every cell.

**2026-11-26 StreamContent streaming-copy dispose/cancel race:** added a
24-iteration race while a copy is waiting on a pending destination write. Once
the write completes, the valid outcomes are cancellation before another read,
`ObjectDisposedException` on the next read, or successful completion if the
copy wins; all paths require one destination write and exactly-once disposal
of the owned source. The stream-copy fixture builds and passes Windows x86/x64
MSVC and Linux x64 GCC 13.3/Clang 18 static/shared cells, with strict Linux
warnings. Other lifecycle, pristine-target, and separate-host benchmark gates
remain open.

**2026-11-26 ambiguous HTTP framing connection recovery:** after rejecting
responses that combine `Content-Length` with chunked `Transfer-Encoding`,
repeat the `Transfer-Encoding` field, repeat chunked coding in one field, or
combine conflicting duplicate `Content-Length` fields with chunked encoding,
reject a nonnumeric `Content-Length` combined with chunked encoding as a fifth
case and reject a comma-list containing conflicting lengths with chunked
encoding as a sixth case. The same `HttpClient` successfully sends a second
request over a fresh TCP connection after each rejection. The first four cases
passed in all eight Windows x86/x64 MSVC and Linux x64 GCC 13.3/Clang 18
static/shared cells. The fifth case also passed the integrated HTTP e2e and
focused recovery CTests in all eight cells. The sixth passed the focused
recovery CTest in all four Windows MSVC x86/x64 static/shared cells, plus fresh
Linux GCC 11.4 and Clang 14.0 static/shared builds with strict warnings. Broader
lifecycle, pristine-target, and separate-host benchmark gates remain open.

**2026-11-26 non-owning stream write canceled by socket disposal:** added a
focused case that saturates a connected socket's send buffer, starts a pending
write through a non-owning `NetworkStream`, forces GC, then disposes the
underlying socket twice. It requires the stream write to cancel, preserves
queued peer bytes before EOF, and keeps repeat stream disposal safe. The focused
CTest passes five consecutive runs in all eight Windows x86/x64 MSVC and Linux
x64 GCC/Clang static/shared cells. The full `cxnet_dns_async_tests` fixture also
passes all eight cells with the new case integrated.

**2026-11-26 NetworkStream concurrent-write contract:** added
`cxnet_network_stream_concurrent_write_rejection`. The test saturates the
socket send buffer, starts a pending write through a non-owning `NetworkStream`,
forces GC, and verifies that a second `WriteAsync` returns a faulted task with
`InvalidOperationException`. It then drains and validates the queued bytes,
confirms the first write completes, and checks that a later write can reuse the
send slot. The focused case passes all eight Windows x86/x64 MSVC and Linux x64
GCC 11.4/Clang 14 static/shared cells with strict Linux warnings. The refreshed
full suite passes 108/108 in all eight cells. Durations in seconds: Windows x64
static/shared 480.92/403.44, Windows x86 static/shared 447.41/463.73, Linux GCC
static/shared 478.40/400.55, and Linux Clang static/shared 533.33/517.86.
Broader lifecycle, pristine-target, and separate-host benchmark acceptance
gates remain open.

**2026-11-26 Socket concurrent-write contract:** added
`cxnet_socket_concurrent_write_rejection`. With the send buffer saturated, the
fixture starts a pending `Socket.WriteAsync`, forces GC, then confirms a second
write throws `InvalidOperationException` without completing or replacing the
first task. After the peer drains the queued bytes and the pending write
completes, a later write succeeds and reaches the peer. The focused CTest passes
five consecutive runs in all eight Windows x86/x64 MSVC and Linux x64 GCC
11.4/Clang 14 static/shared cells; Linux GCC and Clang builds use strict
warnings. The refreshed full suite passes 109/109 in all eight cells. Durations
in seconds: Windows x64 static/shared 453.88/426.81, Windows x86 static/shared
431.34/466.67, Linux GCC static/shared 373.87/385.56, and Linux Clang
static/shared 585.67/540.37. Broader lifecycle, pristine-target, and separate-
host benchmark acceptance gates remain open.

**2026-11-26 pristine AlmaLinux REST/CXDATA package consumer:** rebuilt the
current locked package-only REST/JSON smoke in a clean AlmaLinux 10.3 container
(glibc 2.39, GCC 14.4.1, CMake 3.31.8). The read-only stage had no repository
checkout. Strict static and shared builds both completed the loopback
`GET /package-json`, decoded the expected DTO, and confirmed exactly one
request. `LD_LIBRARY_PATH` was empty during runtime checks; shared `ldd -r`
resolved staged `cxdata`, `cxnet`, and `cxcore` modules with system OpenSSL and
no unresolved symbols. Package hashes and stage details are recorded in
`cxnet/tests/package_consumer/README.md`.

**2026-11-26 pristine Rocky Linux REST/CXDATA package consumer:** rebuilt the
current locked package-only REST/JSON smoke in a clean Rocky Linux 9.9 container
(glibc 2.34, GCC 11.5.0, CMake 4.2.1). The read-only stage had no repository
checkout. Strict static and shared builds both completed the loopback
`GET /package-json`, decoded the expected DTO, and confirmed exactly one
request. `LD_LIBRARY_PATH` was empty during runtime checks; `ldd -r` resolved
the staged `cxdata`, `cxnet`, and `cxcore` modules with system OpenSSL and found
no unresolved symbols. Package hashes and stage details are recorded in
`cxnet/tests/package_consumer/README.md`.

**2026-11-26 pristine Debian 12 REST/CXDATA package consumer:** rebuilt the
current locked package-only REST/JSON smoke in a clean Debian 12 container
(glibc 2.36, GCC 12.2.0, CMake 4.2.1, OpenSSL 3.0.17). The read-only stage had
no repository checkout. Strict static and shared builds both completed the
loopback `GET /package-json`, decoded the expected DTO, and confirmed exactly
one request. `LD_LIBRARY_PATH` was empty during runtime checks; `ldd -r`
resolved staged `cxdata`, `cxnet`, and `cxcore` with system OpenSSL and found no
unresolved symbols. Package hashes and stage details are recorded in
`cxnet/tests/package_consumer/README.md`.


On 2026-11-27, the current-source TCP echo, delayed streamed HTTP download,
and generated REST CRUD examples passed smoke checks across all eight Windows
x86/x64 MSVC and Linux x64 GCC 11.4/Clang 14 static/shared cells. The download
fixture verified 262,144 bytes; REST passed its in-process assertions and local
HTTP wire fixture. Linux builds used strict warnings, and `ldd -r` found no
unresolved symbols in the shared example executables. Broader lifecycle,
pristine-target, and separate-host benchmark gates remain open.

On 2026-11-27, the upload lifecycle suite gained a timeout-versus-
`HttpClient.Dispose()` race. It exercises both winner orderings against an
active stalled upload, forces GC, checks that a partial body reached the
server, verifies transport closure, and requires the task's final state to
match the winner. The focused CTest passed five consecutive runs in all eight
Windows x86/x64 MSVC and Linux x64 GCC 13.3/Clang 18 static/shared cells. The
integrated Linux GCC static suite passed 113/113 in 363.26 seconds. This adds
one test beyond the preceding eight-cell full-suite matrix; broader lifecycle,
pristine-target, and separate-host benchmark gates remain open.

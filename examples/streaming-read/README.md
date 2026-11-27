# Streaming socket bytes

`Socket.ReadBytesAsync` turns an already-connected socket into a pull-based
`IAsyncEnumerable<byte>`. It starts a nonblocking read when the consumer asks
for the next item, yields every byte from that transfer, and requests another
read only after the current transfer has been consumed. A zero-byte read ends
the sequence at EOF. Cancellation is passed through to the pending socket read.

`StreamingRead.cx` shows an async-iterator consumer forwarding values with
`await foreach`. This keeps the pipeline streaming: callers can process each
byte while the peer still has the connection open instead of waiting for a
`byte[]` containing the entire stream.

The same source contains `ImmediateBytes`, a lazy async generator that produces
values only when `MoveNextAsync` is consumed. The
`examples/11-coroutines/Coroutines.cx` program demonstrates a pending
`Task.Delay` and explicitly drives it with `TaskScheduler.RunUntil`.

The library requires a connected native socket handle. Its owner creates the
`System.Net.Sockets.Socket`, supplies a reusable buffer, and pulls
`ObserveBytesAsync(...).GetAsyncEnumerator(token)` one move at a time. The
socket and buffer must remain valid for the enumeration; disposing the socket
closes a pending read, and disposing the enumerator cancels its pending move.
The enumeration reserves the socket's receive slot for its lifetime, so a
direct `ReadAsync` or a second `ReadBytesAsync` sequence on that socket is
rejected until the first enumerator finishes or is disposed. Sends can proceed
while the sequence owns the receive slot.

The native end-to-end check uses connected local socket pairs. A public
`StreamingReadSession` wrapper keeps the test behind the generated library API
instead of reaching into the private compiler-generated iterator type. The test
proves a byte is yielded while the peer remains open, EOF ends the sequence,
early disposal runs iterator cleanup, and canceling a pending move completes the
read and cleanup. It also verifies that a direct socket read is rejected while
the iterator has a pending read, without disrupting the iterator's result.
Under the Milestone 18 G6 contract, `MoveNextAsync()` returns
`ValueTask<bool>` and `DisposeAsync()` returns `ValueTask`; consume each move
result before requesting another move. The native fixture also warms an
immediate async iterator and verifies 64 completed moves, terminal completion,
and completed disposal allocate zero CX objects after enumerator creation.

It forces collection during a pending public `Socket.ReadAsync` and checks its
`Task<uint>` result before and after normal cancellation and after queued-
readiness cancellation. A second readiness registration makes cancellation run
after the read callback is queued; the canceled move completes and the iterator
finalizer runs exactly once. The native fixture keeps the Task session in a
registered GC root while collecting. Without that root, x86 shared runs
intermittently reported a canceled Task or segfaulted while the C test still
held the session in a local variable. With the explicit root, the Windows
matrix passed and x86 shared passed 100 consecutive CTest repetitions.

That allocation check covers synchronous iterator moves/disposal. The streaming
iterator now uses an internal reusable `ValueTask<uint>` completion source for
its socket reads; the public `Socket.ReadAsync` API remains `Task<uint>`. Across
Windows x64 and x86, static and shared, the first pending read plus move counted
14 CX allocations during operation start and 2 during completion (16 total).
A warmed repeated read counted 7 at start and 0 during completion (7 total). The
fixture asserts both phase budgets. The compiler preserves the source-backed
`ValueTask<uint>` through async-iterator lowering, which uses direct completion
checks and registration instead of converting the await operand with `AsTask()`.
After a pending read completes, the fixture consumes the yielded move result and
then starts a second pending read through the same enumerator; that second read
verifies reusable-source reset after the completion callback has drained. The
cross-module reusable-source fixture separately rejects Reset from inside its
queued callback.
The earlier Task-backed baseline counted 20 for one pending read plus move. The
first-use count includes iterator/source setup, so it is not directly
comparable to the warmed repeated count. A separate probe roots the live
iterator across forced collection, confirms a nonzero live CX heap, then
disposes and collects after
each of 16 more pending stream cycles. The final Windows x64/x86 static and
shared cells show no retained CX heap growth over one cycle; x86 shared observed
884 live bytes in the latest x86 shared run and 0 bytes after collection.
These checks do not measure native allocations. The fixture also reports managed
allocation-size buckets for setup and completion. A compiled MSVC `sizeof`
probe and generated allocation sites map the 168x2/100x2 first-use buckets to
the original and cloned `Socket.ReadBytesAsync` iterator frames; 64x3/40x3 to
two reusable move sources and one reusable read source; and 56x2/28x2 to the two
lazy continuation-state objects. The reusable socket operation no longer
reserves its Task-only fault and cancellation callbacks. Its single-completion
allocation ends at `offsetof(faulted)`, reducing the operation from 216 to 136
bytes on x64 and from 128 to 80 bytes on x86; the Task-backed public API still
allocates the full structure and retains all three callback slots. The
readiness registration remains a per-read setup allocation. Reusable
continuation states retain one completion queue item each: first use seeds two
136/80-byte nodes during completion, and warmed completion reuses them with
zero queue-item allocations. The scheduler item allocation counter confirms 2
allocations on first completion and 0 on repeated completion in Windows x64
static; the streaming allocation budgets confirm the same phase counts across
Windows x64/x86 and Linux x64, static/shared. The two 8/4-byte setup allocations match
completion-source and operation pointer
cells in `StartReusableTransfer`; 32/16 bytes matches its two-capture lambda
environment. Captured callback environments correspond to the 24/12-byte
buckets. The previous 216/128-byte operation buckets disappear from all six
streaming cells; the operation now shares a bucket size with other objects on
some architectures, so the fixture histogram is not an isolated type-size
measurement. See the dated G6 and G7 evidence records.
The Windows shared x64 and x86 DLL export tables contain both
`Socket.ReadBytesAsync` overloads, generated iterator type metadata/vtable, and
the closed source/ValueTask methods used by the fixture. Iterator implementation
methods are internal and dispatched through the exported vtable. The native
consumer also checks the live iterator interface map, reflected factory and
iterator method signatures/visibility, and both socket factory signatures
through the public generated header in x64/x86 static/shared builds. This covers
the streaming API's reflection/header contract; it is not a module-wide ABI
audit. Linux x64 static/shared also passes CTest 1/1 using the available
temporary WSL CMake 3.31.8 at `/tmp/cmake-3.31.8/bin/cmake`; shared `ldd -r`
resolves all three modules. The default WSL CMake is still 3.22.1. GCC reports
missing declarations for the generated cross-module reusable continuation-state
constructor and `Clear`, along with generic placeholder pointer warnings; the
runtime checks pass, but generated code is not a strict-warning build. See the
dated Milestone 18 G6 evidence record for buckets, exact commands and remaining
lifecycle limits.

The same native fixture contains a generated `Socket.WriteAsync` probe with 32
synchronous writes per sample, three samples, a 4 MiB request, and 4 KiB socket
buffers. Setup and peer draining are outside the measured call. Linux's AF_UNIX
pair returns partial counts; Windows loopback TCP returns the full request in
this setup. The fixture asserts exact allocation totals and reports transfer
counts without comparing Windows and Linux timing. See
`cxc/tests/native/results/milestone18-g8-socket-write-20261020.txt`.

The current source tree's native streaming fixture records 7 objects / 400
bytes per `Socket.WriteAsync` on x64 and 7 / 228 on x86. Its first-use iterator
frames are 184 bytes on x64 and 108 on x86 after adding the receive-sequence
lease; warmed reads still allocate 6 objects at setup and 0 on completion.

Run from the workspace root after building `CxCompiler.dll` with the solution.
The runner emits both `src/cxnet.cxproj` and `streaming-read.cxproj` into unique
temporary directories with `--emit-only` and builds their CMake outputs under
unique temporary roots. The compiler also generates dependency sources under
`cxcore/src/.obj` and `cxnet/src/.obj` in the checked-out trees; the runner does not
clean those directories. CMake 3.31 or newer is required.

```powershell
& .\cxnet\examples\streaming-read\native\run-streaming-read-e2e.ps1 `
    -Cxc .\cxc\src\CxCompiler\bin\Debug\net10.0\CxCompiler.dll `
    -Platform Windows -Architecture x64 -LinkMode both
& .\cxnet\examples\streaming-read\native\run-streaming-read-e2e.ps1 `
    -Cxc .\cxc\src\CxCompiler\bin\Debug\net10.0\CxCompiler.dll `
    -Platform Windows -Architecture x86 -LinkMode both
& .\cxnet\examples\streaming-read\native\run-streaming-read-e2e.ps1 `
    -Cxc .\cxc\src\CxCompiler\bin\Debug\net10.0\CxCompiler.dll `
    -Platform Linux -Architecture x64 -LinkMode both `
    -CMake /tmp/cmake-3.31.8/bin/cmake -Distribution Ubuntu
```

`-LinkMode` accepts `static`, `shared`, or `both`; the shared mode builds
`cxcore`, `cxnet`, and the streaming adapter as shared libraries. Windows x86
requires a Visual Studio generator; Windows x64 uses the default Visual Studio
generator unless `-Generator Ninja` is supplied. Linux supports x64 only and
requires the named WSL distribution with CMake 3.31 or newer installed. The
CTest assertion verifies a move yields the peer's byte before it closes, a
later move ends at EOF, and cancellation/disposal runs cleanup. Each cell
prints `PASS streaming-read <link-mode> <architecture>`. `-TestRepeat` repeats
CTest until the first failure (default 1).

The `cxnet` and `streaming-read` CX sources generate, and the consumer links on
Windows x86 and x64 in both static and shared modes.
The core build includes a narrow generated support source for generic async task
methods, constructed task metadata, collection interface metadata, and the
`Array<T>.Count` bridge while the handwritten runtime retains its `Array.Length`
implementation. Dynamic type metadata initializes the read-only
`TypeInfo.BaseType` field during module setup. `cxnet` declares its Windows
`ws2_32` OS dependency for generated socket targets. Linux x64 static and shared
consumer builds pass with GCC 11.4 and CMake 3.31.6; the shared modules pass
`ldd -r`. Generated Linux static-link projects enable position-independent code
so static runtime archives can be linked into the shared consumer. The generated
core, network library, and consumer C sources compile with strict warnings on
Linux x64 and Windows x86/x64 in static and shared modes. GCC uses
`-Wno-unused-parameter`; MSVC uses `/wd4100 /wd4130 /wd4702` for generated
parameters, constant comparisons, and unreachable coroutine continuation paths,
with other warnings treated as errors.

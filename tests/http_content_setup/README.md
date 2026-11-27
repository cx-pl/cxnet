# HttpContent setup failure fixture

Build and run the standalone CX consumer to verify asynchronous buffering setup
faults become faulted tasks, failed cached buffering is cleared for retry, and
both default and explicit-charset string reads preserve setup failures. A
pending byte read also faults after a throwing continuation is attached; the
fixture checks the fault remains cached even when that continuation throws.
It also blocks buffering setup before an operation is published, lets a second
caller attach a throwing continuation to the shared task, and injects a
synchronous stream-position failure. The task retains the original `IOException`,
and a subsequent read starts a new successful buffering attempt. A disposal
case closes `StreamContent` during a pending read, verifies cancellation and
source disposal, then completes the source late and confirms the task stays
canceled. A concurrent case races owner-thread EOF completion against
disposal on a worker, accepts only success or cancellation, and verifies the
source is disposed; it passes 20 consecutive Windows x64 Release runs.
The default `ReadAsStringAsync()` path also verifies that concurrent callers
share one task, content disposal cancels its pending underlying buffer read, and
a late EOF cannot change the canceled string result.
The byte-buffer path also completes a pending source read with one byte after
disposal. It requires the task to remain canceled and the disposed source to
receive no additional reads.

The established setup-failure cases pass all six Windows x86/x64 and Linux x64
static/shared runtime cells. Linux x64 builds also pass strict GCC and Clang with
`-Wall -Wextra -Werror -Wno-unused-value` in both link modes.
The disposal-order and concurrent completion/disposal cases pass 20 consecutive
Windows x64 Release runs and 20 runs in each Linux x64 GCC/Clang static/shared
cell. Both shared Linux executables pass `ldd -r`.

The HTTP backend CTest build also injects an `IOException` at the
`OnBufferingStarted()` boundary. It verifies that the first read faults and the
next read starts a fresh operation and succeeds. This case passes Windows
x86/x64 MSVC and Linux x64 GCC/Clang static/shared builds.

A deterministic synchronous race gates `StreamContent.ReadAsByteArray()` inside
the source read, disposes the content while that read is pending, then releases
the read and requires `ObjectDisposedException` plus source disposal. This case
passes in all eight Windows x86/x64 and Linux GCC/Clang static/shared cells.

The fixture also races `ByteArrayContent.ReadAsByteArray()` against content
disposal twelve times using a 4 MiB payload. Each run accepts a complete,
byte-checked copy or `ObjectDisposedException`; any other outcome fails. The
test passes Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared builds,
with strict Linux warning flags.

When test hooks are enabled, the fixture injects failure while registering the
source-read and internal buffer-write continuations for `StreamContent` byte
materialization. Both cases require a faulted, cached task, one source read,
and disposal of the owned source. These cases pass all eight Windows x86/x64
MSVC and Linux x64 GCC/Clang static/shared configurations. Linux builds use
`-Wall -Wextra -Werror -Wno-unused-value`.

```powershell
dotnet cxc/src/CxCompiler/bin/Release/net10.0/CxCompiler.dll `
  cxnet/tests/http_content_setup/http-content-setup.cxproj --compile `
  --configuration Release
& cxnet/tests/http_content_setup/.bin/Release/HttpContentSetup.exe
```

The `http_content_setup` fixture also forces a multipart part-read failure after
serialization has started by using a stream part that exceeds its 4-byte buffer
limit. A second `ReadAsByteArray()` must fail again, proving the partial body was
not cached. The standalone fixture passes Windows x86/x64 MSVC and Linux x64
GCC/Clang in static and shared configurations; Linux builds use strict warning
flags.

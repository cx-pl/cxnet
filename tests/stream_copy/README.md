# Stream copy fixture

Build and run this standalone CX consumer to verify that `Stream.CopyToAsync`
copies from the current source position and returns faulted tasks for a null
destination, a non-writable destination, or a non-readable source. It also
verifies that `StreamContent.CopyToAsync`
starts at its construction-time position, and that seekable content can be
replayed after materialization. It also checks buffer-limit failure and source
rewind, streaming from a non-seekable source and its one-shot behavior, read
and write fault propagation, pre-cancellation, and cancellation during a
pending read. It verifies that synchronous exceptions thrown while an async
source read or destination write is being started become faulted copy tasks.
It also verifies that source and destination streams returning null async tasks
fault the copy with `InvalidOperationException`. Disposing the destination
while its write is pending faults the copy with `ObjectDisposedException`,
starts no further source read, and remains safe when destination disposal is
repeated.
For `StreamContent` byte materialization, a source that throws synchronously
while starting `ReadAsync` must produce a cached faulted task; retrying returns
that same task, and repeated content disposal closes the owned source exactly
once. This case passes Windows x86/x64 MSVC and Linux x64 GCC/Clang
static/shared cells.
It also cancels `StreamContent.CopyToAsync` while the destination
write is pending, then completes that write and verifies the copy is canceled
before another source read starts. Disposing `StreamContent` during a pending materialization read
must make repeated reads share the active buffering task, cancel that task, and
close its owned source exactly once, even when content disposal is repeated.
Disposing `StreamContent` while `CopyToAsync` waits on a destination write lets
that write finish, then faults the copy with `ObjectDisposedException` on the
next source read; the owned source is disposed exactly once.
The fixture also races content disposal and caller cancellation while a
streaming copy waits on a destination write, for 24 iterations. After the write
completes, it accepts cancellation before another read, an
`ObjectDisposedException` on the next read, or successful completion if the
copy wins; every outcome must start no second destination write and dispose the
owned source exactly once.
Disposing during a pending streaming-copy source read faults the copy with
`ObjectDisposedException`; repeated content disposal still disposes the source
exactly once.
The fixture also races synchronous `StreamContent.ReadAsByteArray()` against
content disposal twelve times with a 4 MiB payload. It accepts only a complete
byte-checked buffer or `ObjectDisposedException`, and checks exactly-once source
disposal.
It also cancels after a byte has been copied while the next read is pending,
then completes that read with EOF and verifies cancellation wins over successful
EOF completion. Both `Stream.CopyToAsync` and async `StreamContent` materialization
reject a source that reports more bytes than requested. A throwing completion
continuation on a faulted materialization is also observed without changing the
cached faulted task returned by subsequent reads.
If caller cancellation arrives while a source read is pending and that read
later succeeds with data, the copy now stops before starting a destination
write. A destination that ignores its cancellation token verifies no write is
started and the copy completes canceled.

```powershell
dotnet cxc/src/CxCompiler/bin/Release/net10.0/CxCompiler.dll `
  cxnet/tests/stream_copy/stream-copy.cxproj --compile `
  --configuration Release
& cxnet/tests/stream_copy/.bin/Release/StreamCopy.exe
```

It passes all eight Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared
runtime cells. Linux x64 static/shared builds pass strict GCC and Clang 14
builds with `-Wall -Wextra -Werror -Wno-unused-value`; Clang shared runtime
dependencies resolve with `ldd -r`.

# TCP loopback exchange

This executable opens an IPv4 loopback listener on an ephemeral port, connects
a TCP client, accepts it, and exchanges a short payload through the public CX
`Socket` API. It runs without a native helper creating connected handles.

Compile and run from the workspace root:

```powershell
dotnet run --project cxc/src/CxCompiler/CxCompiler.csproj -- --compile --cxcore-dir cxcore cxnet/examples/tcp-echo/tcp-echo.cxproj
& .\cxnet\examples\tcp-echo\.bin\Release\tcpEcho.exe
```

The sample starts `AcceptAsync` before endpoint `ConnectAsync`, then exercises
`NetworkStream.WriteAsync` and `ReadAsync` through the CX task scheduler. The
client sends the payload in two writes while the server has a four-byte read
pending; the sample verifies that the first read is short and continues until
the full payload arrives. The server writes the payload back, and the client
reads and verifies the echoed bytes. Early EOF is treated as an error. The
sample disposes streams through `IDisposable` to cover cross-module interface
dispatch. The executable builds and prints `tcp echo pass` in all eight Windows
x86/x64 MSVC and Linux x64 GCC/Clang static/shared cells. Linux builds use strict
warnings.

# Stream an HTTP download to a file

This executable uses `HttpClient.GetStreamAsync` so it can write response bytes
to the destination file before the full response has arrived. Its copy loop
uses the asynchronous body read contract and disposes the response stream,
destination, and client when it completes.

Compile and run from the workspace root:

```powershell
dotnet run --project cxc/src/CxCompiler/CxCompiler.csproj -- --compile --cxcore-dir cxcore cxnet/examples/http-download/http-download.cxproj
& .\cxnet\examples\http-download\.bin\Release\httpDownload.exe http://127.0.0.1:8080/payload .\download.bin
python cxnet/examples/http-download/run_http_download_fixture.py --executable cxnet/examples/http-download/.bin/Release/httpDownload.exe
```

The fixture command starts a delayed loopback HTTP server, runs the executable,
and verifies the downloaded 256 KiB payload byte-for-byte. After compiling for
Linux, pass the generated `httpDownload` executable path instead.
The fixture passes in all eight Windows x86/x64 MSVC and Linux x64 GCC/Clang
static/shared cells. Linux GCC and Clang builds use strict warnings
(`-Wall -Wextra -Werror -Wno-unused-value`).

The URL can point at a local HTTP fixture or an HTTPS endpoint with a trusted
certificate. The [managed HTTP e2e fixture](../../tests/http_client_e2e.c)
exercises the same headers-first streaming path, including delayed body arrival,
per-read cancellation, a 256 KiB response that exceeds the 64 KiB transport
buffer, and a truncated fixed-length response.

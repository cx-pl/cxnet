# Streaming HTTP upload source failure

This fixture makes a `StreamContent` source return a four-byte prefix, then
holds its next asynchronous read pending. The loopback server waits until that
prefix arrives before releasing the read with an `IOException`. It verifies
that the upload connection closes, `HttpClient.SendAsync` faults with
`HttpRequestException` retaining an inner exception, and disposing the request
content closes the source exactly once. The same `HttpClient` then sends a new
GET on a fresh connection and reads the expected response body.

The focused fixture passes five consecutive runs in all eight supported local
matrix cells: Windows x86/x64 MSVC and Linux x64 GCC 11.4/Clang 14, each with
static and shared linking. Linux builds use `-Wall -Wextra -Werror
-Wno-unused-value`; shared `ldd -r` checks find no unresolved symbols.

Run from the workspace root after compiling the fixture:

```powershell
dotnet cxc/src/CxCompiler/bin/Release/net10.0/CxCompiler.dll cxnet/tests/upload_source_failure/upload-source-failure.cxproj --compile --configuration Release
python cxnet/tests/upload_source_failure/http_fixture.py cxnet/tests/upload_source_failure/.bin/Release/UploadSourceFailure.exe
```

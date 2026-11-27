# Curl upload stream disposal while read is pending

This consumer fixture gives a custom upload stream one byte, then holds its
second `ReadAsync` task pending. The Python loopback peer accepts the transport
socket before releasing the CX program to dispose the client and content. The
fixture verifies that the owned stream is disposed once, the in-flight read's
late completion is harmless, the send task is canceled, and the peer observes
connection closure.

The fixture passes three consecutive runs in each of these build cells:

- Windows x86/x64 with MSVC, static and shared linking.
- Linux x64 with GCC 13, static and shared linking.
- Linux x64 with Clang 14, static and shared linking.

Linux builds use `-Wall -Wextra -Werror -Wno-unused-value`; `ldd -r` resolves
all static and shared outputs without unresolved symbols. The Linux builds use
a stage containing the fixture and freshly restored `cxcore`/`cxnet` packages,
without a source checkout. Shared builds configure CMake with
`-DCX_STATIC_LINK=OFF`; Windows x86 and x64 builds select the corresponding
`Win32` and `x64` CMake platforms.

From the workspace root, compile and run it with:

```powershell
dotnet cxc/src/CxCompiler/bin/Release/net10.0/CxCompiler.dll cxnet/tests/upload_dispose_race/upload-dispose-race.cxproj --compile --configuration Release
python cxnet/tests/upload_dispose_race/http_fixture.py cxnet/tests/upload_dispose_race/.bin/Release/UploadDisposeRace.exe
```

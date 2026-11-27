# HTTP charset fixture

Build and run this standalone CX consumer to check public HTTP content charset
behavior: ISO-8859-1 and ASCII `StringContent` bytes, response decoding through
the declared and explicit charsets (including asynchronous reads), rejection of
unrepresentable ASCII text and unknown charsets, duplicate charset parameters,
and conflicting `Content-Type` charset values.

```powershell
dotnet cxc/src/CxCompiler/bin/Release/net10.0/CxCompiler.dll `
  cxnet/tests/http_charset/http-charset.cxproj --compile `
  --configuration Release
& cxnet/tests/http_charset/.bin/Release/HttpCharset.exe
```

The fixture passes all six Windows x86/x64 and Linux x64 static/shared runtime
cells. Linux x64 static/shared builds also pass strict GCC and Clang 14 builds
with `-Wall -Wextra -Werror -Wno-unused-value`; Clang shared runtime
dependencies resolve with `ldd -r`.

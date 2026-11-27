# Nullable `HttpContent?` REST body fixture

This focused fixture checks generated `@Body HttpContent?` behavior against a
local HTTP/1.1 server. A null value leaves `HttpRequestMessage.Content` unset;
the default transport sends the POST with `Content-Length: 0`, without a
`Content-Type` or `Transfer-Encoding` header. A populated value preserves the
provided bytes and leaves the content type unset.

Compile from the repository root with `cxc` and CMake 3.31 or newer:

```powershell
dotnet cxc/src/CxCompiler/bin/Release/net10.0/CxCompiler.dll cxnet/tests/rest_optional_content/rest-optional-content.cxproj --compile --configuration Release
python cxnet/tests/rest_optional_content/wire_fixture.py cxnet/tests/rest_optional_content/.bin/Release/RestOptionalContent.exe
```

On Linux, the same compile command produces `.bin/RestOptionalContent`:

```sh
python3 cxnet/tests/rest_optional_content/wire_fixture.py cxnet/tests/rest_optional_content/.bin/RestOptionalContent
```

The recording handler verifies null maps to an unset `request.Content` and a
provided `HttpContent` is passed through by identity. The wire fixture verifies
authorization and static headers, null-body framing, and the exact populated
payload (`00 7f ff`). Both checks pass in all six Windows x86/x64 and Linux x64
static/shared cells, including strict Linux GCC and Clang 14 builds.

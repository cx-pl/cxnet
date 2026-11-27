# REST response disposal reentrancy fixture

This consumer fixture gives a `RestResponse<T>` ownership of an HTTP response
whose `StreamContent` source calls `RestResponse.Dispose()` again from its own
`Dispose()` callback. The outer disposal must return, dispose the stream once,
and leave wrapper properties rejecting access with `ObjectDisposedException`.

```powershell
dotnet cxc/src/CxCompiler/bin/Release/net10.0/CxCompiler.dll `
  cxnet/tests/rest_response_reentrancy/rest-response-reentrancy.cxproj `
  --compile --configuration Release
& cxnet/tests/rest_response_reentrancy/.bin/Release/RestResponseReentrancy.exe
```

The fixture also injects an exception from the owned stream's disposal. It
checks that the first `RestResponse<T>.Dispose()` propagates the cleanup error
after detaching ownership, that repeated disposal remains harmless, and that
wrapper access stays rejected. It also races `SetResponse()` against wrapper
disposal for 20 iterations, requiring exactly one owner to dispose the
response. The fixture is registered as
`cxnet_managed_rest_response_reentrancy` in the HTTP backend CTest suite. It
passes Windows x86/x64 MSVC static/shared and Linux x64 strict GCC/Clang
static/shared builds.

# HTTP redirect setup cleanup fixture

Build and run the standalone CX consumer to verify that a synchronous failure
while starting a redirected request faults the original send task and disposes
the replayed redirect request content. It also injects a handler-disposal
failure and checks that `HttpClient.Dispose()` propagates it once while leaving
the client terminal and idempotent. It also makes a response's `StreamContent`
stream throw during disposal and checks that `HttpResponseMessage.Dispose()`
propagates the failure once. A headers-first response also checks that its send
lifetime registration is released despite the content failure by canceling the
caller token afterward and confirming the handler's effective-token callback
does not run.

```powershell
dotnet cxc/src/CxCompiler/bin/Release/net10.0/CxCompiler.dll `
  cxnet/tests/http_redirect_cleanup/http-redirect-cleanup.cxproj --compile `
  --configuration Release
& cxnet/tests/http_redirect_cleanup/.bin/Release/HttpRedirectCleanup.exe
```

All fixture cases pass Windows x86/x64 and Linux x64 static/shared runtime
cells.
Linux x64 builds also pass strict GCC and Clang with
`-Wall -Wextra -Werror -Wno-unused-value` in both link modes. Failures from
`EndSend()` and redirected request-content disposal are also injected by the
`cxnet_managed_http_redirect_cleanup_injection` CTest case. It calls the same
redirect cleanup helper used after a synchronous redirected-handler failure,
injects one-shot failures from `EndSend()` and `ByteArrayContent.Dispose()`,
and verifies both failures were observed while the request content was still
released. This test passes Windows x86/x64 and Linux x64 static/shared, including
strict Linux GCC and Clang builds.

Run that case from a configured HTTP backend spike build with:

```powershell
cmake --build .build/m26-http-deps/spike-cx-win64 --config Release --target cxnet_managed_http_e2e
ctest --test-dir .build/m26-http-deps/spike-cx-win64 -C Release `
  -R cxnet_managed_http_redirect_cleanup_injection --output-on-failure
```

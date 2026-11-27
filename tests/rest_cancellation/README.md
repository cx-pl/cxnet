# Generated REST request cancellation

This fixture uses a generated REST interface whose method accepts a
`CancellationToken` and returns `Task<HttpResponseMessage>`. First, a custom
handler keeps the response pending and verifies that cancellation reaches the
handler and generated call. Then a loopback TCP server receives a real HTTP
request, holds its response, and verifies that caller cancellation cancels the
generated task and closes the transport connection. A separate owned-client
case disposes the `RestClient` while a generated request is pending and checks
that the task is canceled through the client's `HttpClient` lifetime token. It
then reuses a live generated client after caller cancellation and requires the
next request to succeed. Another case cancels the caller token and disposes the
owning `RestClient` before the generated completion is dispatched, then verifies
that the request stays canceled through both lifetime sources. The fixture also
verifies that a new
`RestResponse<string>` rejects response properties and initialization setters
until `SetResponse` has attached an HTTP response.

Build the REST generator first if its executable is missing, then build and run
the fixture:

```powershell
dotnet cxc/src/CxCompiler/bin/Release/net10.0/CxCompiler.dll `
  cxnet/tests/rest_cancellation/rest-cancellation.cxproj `
  --compile --configuration Release
& cxnet/tests/rest_cancellation/.bin/Release/RestCancellationFixture.exe
```

The executable returns zero when every cancellation and response-publication
case passes. The caller-cancellation case also disposes its owning `RestClient`
after caller cancellation but before scheduler dispatch, then verifies that the
generated request still completes as canceled and the effective handler token
is canceled.

All cases pass Windows x86/x64 with MSVC 19.51 static/shared linking and Linux
x64 with GCC 11.4 and Clang 14 strict static/shared builds. The standalone
fixture enables `CXNET_TESTING` and `CXCORE_TESTING` for its native dependencies;
the REST fixture hooks use the cxnet export macro so Windows shared builds can
link them. When the generated project exists, `cxnet/spikes/http-backend`
registers the fixture as the `cxnet_managed_rest_request_cancellation` CTest.

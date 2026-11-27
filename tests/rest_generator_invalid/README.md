# Invalid REST contract diagnostic fixture

Compile from the workspace root:

```powershell
dotnet cxc/src/CxCompiler/bin/Release/net10.0/CxCompiler.dll cxnet/tests/rest_generator_invalid/invalid-rest-contract.cxproj --emit-only --configuration Release
```

Run the automated source-diagnostic regression from the workspace root with:

```powershell
python cxnet/tests/rest_generator_invalid_test.py
```

Compilation must fail with `CXREST019` for `InvalidContract` because REST
contracts must be public interfaces, and `CXREST020` for `EmptyContract`
because a supported contract must declare or inherit an HTTP-annotated method.
It must also report `CXREST001` for each `InvalidQueryMapContract` method because
query maps require string keys and supported scalar values.
`OpenMethodContract.Get<T>()` returns `Task<T>` and must also report `CXREST001`
at the annotated method location because generated REST methods do not support
an unresolved generic result type. The fixture must reject generation before
native code is emitted. It must report `CXREST022` for invalid static header
names and values, attempts to set both `Content-Length` and `Transfer-Encoding`
through `@StaticHeader`, an invalid `@Header` name, and attempts to set both
transport-managed fields through `@Header`. The separate
`MalformedRouteContract` methods must report `CXREST010` for an unmatched
closing brace, nested placeholder, unclosed placeholder, and duplicate
`@Path` bindings of one placeholder. The separate
[`rest_generator_open_generic_invalid`](../rest_generator_open_generic_invalid/README.md)
fixture verifies that `RestClient.For<OpenGenericContract<T>>()` fails earlier
with a source diagnostic when `T` is unresolved, before the REST generator or
native emission can accept an open contract request.
`InvalidMultipartDispositionContract` must report `CXREST014` for a part name
or filename containing a quote, backslash, or control character, and for a
repeated part that supplies one filename for every item. Generation must fail
before native code is emitted.

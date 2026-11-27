# Generated REST CRUD example

This executable exercises a generated `IProductsApi` contract for create, read,
update, and delete operations, plus a UUID path parameter for a raw response
and a JSON UUID scalar result. It also materializes a JSON product array into a
`const List<Product>` through a typed REST result.
With no arguments it uses an in-process `HttpMessageHandler`. Pass a base
address to run the CRUD calls through the default `HttpClient` transport.

Build and run it from the repository root after building the two generators:

```powershell
dotnet cxc/src/CxCompiler/bin/Release/net10.0/CxCompiler.dll `
  cxnet/examples/rest-crud/rest-crud.cxproj --compile --configuration Release
cmake --build cxnet/examples/rest-crud/.obj/build --config Release `
  --target RestCrudExample
& cxnet/examples/rest-crud/.obj/output-m26-win64-static/RestCrudExample/Release/RestCrudExample.exe
python cxnet/examples/rest-crud/run_rest_crud_fixture.py `
  cxnet/examples/rest-crud/.obj/output-m26-win64-static/RestCrudExample/Release/RestCrudExample.exe
```

The compiler writes generated sources under `.obj`; CMake places the built
executable under `.obj/output-m26-<target>-<link-mode>/RestCrudExample/Release/`.
Use the path for the target and link mode you built when invoking the fixture.
For example, on Linux x64 static, pass
`cxnet/examples/rest-crud/.obj/output-m26-linux-x64-static/RestCrudExample/RestCrudExample`
to `run_rest_crud_fixture.py`.

The in-process handler records the generated method, URI, and JSON body, and
checks canonical UUID path formatting and UUID scalar response parsing. The
wire fixture starts a local HTTP/1.1 server and checks the generated POST, GET,
product-list GET, UUID path GET, PUT, and DELETE requests, JSON bodies, media
types, and returned DTOs. Both modes compile and the runtime assertions and
loopback fixture pass in all eight Windows x86/x64 MSVC and Linux x64 GCC/Clang
static/shared cells. Linux GCC and Clang static/shared builds pass with
`-Wall -Wextra -Werror -Wno-unused-value`.

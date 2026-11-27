# REST dictionary collection value probe

This focused generator regression exercises direct and `RestResponse<T>` JSON
maps whose values are `const List<int>`, `const Array<int>`,
`const LinkedList<int>`, `const Queue<int>`, and `const Stack<int>`. For each
collection, it checks a populated sequence, an empty sequence, and preserved
enumeration order. It also checks direct and wrapped nullable `List<int>` map
values with populated, empty, and JSON-null entries. Direct and wrapped
`Dictionary<string, RestItem?>` responses check DTO materialization, a JSON-null
map value, and wrapper status and headers. Direct and wrapped maps with
`List<int?>`, `Array<int?>`, `LinkedList<int?>`, `Queue<int?>`, and `Stack<int?>` values each check a
populated sequence containing a null element and an empty sequence. The same
loopback run checks direct and wrapped nullable `ulong` maps, including zero,
`ulong.MaxValue`, and JSON null values:

```powershell
dotnet cxc/src/CxCompiler/bin/Release/net10.0/CxCompiler.dll `
  cxnet/generators/rest/CxnetRestGenerator.cxproj --compile --configuration Release
dotnet cxc/src/CxCompiler/bin/Release/net10.0/CxCompiler.dll `
  cxnet/tests/rest_map_collection_probe/rest-map-collection-probe.cxproj `
  --compile --configuration Release
python cxnet/tests/rest_map_collection_probe/run_wire.py `
  cxnet/tests/rest_map_collection_probe/.bin/Release/RestMapCollectionProbe.exe
```

The generated client must implement each interface signature with the declared
standard collection types and preserve `const`. The probe runs all five regular
and all five nullable-element map response shapes, nullable collection-map
values, and nullable DTO map values against the loopback server, while also
compiling the generated JSON array parser for each dictionary collection value
and the DTO codec. The nullable maps check JSON null and nullable element
materialization. The full wire run expects twenty-six requests and is registered
as `cxnet_managed_rest_map_collection_wire` in the HTTP backend test matrix. It
passes all eight Windows x86/x64 MSVC and Linux x64 GCC 11.4/Clang 14
static/shared cells. Linux builds use strict warning flags, and both shared
probe executables pass `ldd -r` without unresolved symbols.

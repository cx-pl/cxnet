# Open generic REST contract rejection fixture

Run from the workspace root:

```powershell
dotnet cxc/src/CxCompiler/bin/Release/net10.0/CxCompiler.dll cxnet/tests/rest_generator_open_generic_invalid/open-generic-rest-contract.cxproj --emit-only --configuration Release
```

Compilation must fail at `OpenGenericContract<T>` because the compiler cannot
resolve `T` as a closed generic argument to `RestClient.For<T>()`. This source
diagnostic rejects the open contract request before generator or native output.

# REST contract registry fixture

Build and run the registry fixture with `cxc`:

```powershell
dotnet cxc/src/CxCompiler/bin/Release/net10.0/CxCompiler.dll `
  cxnet/tests/rest_contract_registry/rest-contract-registry.cxproj `
  --compile --configuration Release
& cxnet/tests/rest_contract_registry/.bin/Release/RestContractRegistryFixture.exe
```

The fixture verifies ordinary generated-factory lookup, missing and mismatched
contract errors, and concurrent first registration for one closed interface
type. Four threads race the same factory into the registry; exactly one must
report successful registration, and `GetContract<T>()` must construct through
the retained factory. The race passes in all six Windows x86/x64 and Linux x64
static/shared cells, plus strict Linux GCC and Clang static/shared builds with
`-Wall -Wextra -Werror -Wno-unused-value`.

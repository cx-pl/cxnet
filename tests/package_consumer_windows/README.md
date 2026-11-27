# Checkout-free Windows package consumer

This fixture restores `cxnet` and its transitive `cxcore` package for Windows
x64, builds a shared consumer, and loads the resulting DLL with dependency
search restricted to the module directory and `System32`.

From the workspace root, publish both packages to a fresh local feed:

```powershell
$feed = '.build/m26-win-http-smoke-feed-20261126'
if (Test-Path $feed) { Remove-Item $feed -Recurse -Force }
dotnet cxpm/src/Cxpm.Cli/bin/Release/net10.0/Cxpm.Cli.dll build --project cxcore/src/cxcore.cxproj
dotnet cxpm/src/Cxpm.Cli/bin/Release/net10.0/Cxpm.Cli.dll publish --project cxcore/src/cxcore.cxproj --feed $feed
dotnet cxpm/src/Cxpm.Cli/bin/Release/net10.0/Cxpm.Cli.dll build --project cxnet/src/cxnet.cxproj
dotnet cxpm/src/Cxpm.Cli/bin/Release/net10.0/Cxpm.Cli.dll publish --project cxnet/src/cxnet.cxproj --feed $feed
```

Point `package-consumer.cxproj` at that feed, then restore and compile:

```powershell
dotnet cxpm/src/Cxpm.Cli/bin/Release/net10.0/Cxpm.Cli.dll restore --target win-x64 --project cxnet/tests/package_consumer_windows/package-consumer.cxproj
dotnet cxc/src/CxCompiler/bin/Release/net10.0/CxCompiler.dll cxnet/tests/package_consumer_windows/package-consumer.cxproj --compile --configuration Release
```

To verify without any repository checkout, stage only the consumer project,
lock, source, restored packages, and compiler outside the workspace. The
Windows SDK, CMake, and the Visual C++ runtime must be installed on the host.

```powershell
$stage = Join-Path $env:TEMP ('cx-m26-win-package-only-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $stage, (Join-Path $stage '.packages'), (Join-Path $stage 'compiler') | Out-Null
Copy-Item cxnet/tests/package_consumer_windows/package-consumer.cxproj, cxnet/tests/package_consumer_windows/cxpm.lock, cxnet/tests/package_consumer_windows/Consumer.cx -Destination $stage
robocopy cxnet/tests/package_consumer_windows/.packages (Join-Path $stage '.packages') /E
robocopy cxc/src/CxCompiler/bin/Release/net10.0 (Join-Path $stage 'compiler') /E
dotnet (Join-Path $stage 'compiler/CxCompiler.dll') (Join-Path $stage 'package-consumer.cxproj') --compile --configuration Release
python cxnet/tests/package_runtime_load.py (Join-Path $stage '.bin/Release/M26WindowsPackageConsumer.dll')
```

The checkout-free build should set `CXCORE_SOURCE_DIR` in its CMake cache to
`<stage>/.packages/cxcore/0.1.0/source`. The runtime loader uses
`LoadLibraryExW` with `LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32`.

## Windows x86

The `win-x86/` fixture has its own target and lock file. Restore it from the
workspace root:

```powershell
dotnet cxpm/src/Cxpm.Cli/bin/Release/net10.0/Cxpm.Cli.dll restore --target win-x86 --project cxnet/tests/package_consumer_windows/win-x86/package-consumer.cxproj
```

Stage the consumer outside the workspace so the compiler cannot discover a
source checkout beside it:

```powershell
$stage = Join-Path $env:TEMP ('cx-m26-win32-package-only-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $stage, (Join-Path $stage '.packages'), (Join-Path $stage 'compiler'), (Join-Path $stage 'runtime-loader') | Out-Null
Copy-Item cxnet/tests/package_consumer_windows/win-x86/package-consumer.cxproj, cxnet/tests/package_consumer_windows/win-x86/cxpm.lock, cxnet/tests/package_consumer_windows/win-x86/Consumer.cx -Destination $stage
robocopy cxnet/tests/package_consumer_windows/win-x86/.packages (Join-Path $stage '.packages') /E
robocopy cxc/src/CxCompiler/bin/Release/net10.0 (Join-Path $stage 'compiler') /E
robocopy cxnet/tests/package_consumer_windows/runtime-loader (Join-Path $stage 'runtime-loader') /E
dotnet (Join-Path $stage 'compiler/CxCompiler.dll') (Join-Path $stage 'package-consumer.cxproj') --compile --configuration Release
```

The single `win-x86` target makes `cxc` configure Visual Studio with the Win32
platform. The generated CMake cache should point `CXCORE_SOURCE_DIR` into
`$stage/.packages/cxcore/0.1.0/source` and set
`CMAKE_GENERATOR_PLATFORM` to `Win32`. Build and run the architecture-matched
loader with the same isolated DLL search policy:

```powershell
$loaderSource = Join-Path $stage 'runtime-loader'
$loaderBuild = Join-Path $stage 'loader-build'
cmake -S "$loaderSource" -B "$loaderBuild" -A Win32
cmake --build "$loaderBuild" --config Release
& (Join-Path $stage 'loader-build/Release/CxPackageRuntimeLoader.exe') (Join-Path $stage '.bin/Release/M26WindowsPackageConsumerX86.dll')
```

This checkout-free x86 build and isolated module load pass on the current
Windows host.

## Windows x86/x64 package-only HTTP and HTTPS runtime smoke (2026-11-26)

The `http_smoke/` executable references the x64 package consumer and sends
HTTP and HTTPS GETs to local Python loopback servers. The runner checks the
response status/body, custom CA trust, wrong-host rejection, and request counts.
Build fresh packages into the feed configured by `package-consumer.cxproj`,
restore both projects, and compile the smoke executable:

```powershell
$feed = '.build/m26-win-http-smoke-feed-20261126'
dotnet cxpm/src/Cxpm.Cli/bin/Release/net10.0/Cxpm.Cli.dll build --project cxcore/src/cxcore.cxproj
dotnet cxpm/src/Cxpm.Cli/bin/Release/net10.0/Cxpm.Cli.dll publish --project cxcore/src/cxcore.cxproj --feed $feed
dotnet cxpm/src/Cxpm.Cli/bin/Release/net10.0/Cxpm.Cli.dll build --project cxnet/src/cxnet.cxproj
dotnet cxpm/src/Cxpm.Cli/bin/Release/net10.0/Cxpm.Cli.dll publish --project cxnet/src/cxnet.cxproj --feed $feed
dotnet cxpm/src/Cxpm.Cli/bin/Release/net10.0/Cxpm.Cli.dll restore --target win-x64 --project cxnet/tests/package_consumer_windows/package-consumer.cxproj
dotnet cxpm/src/Cxpm.Cli/bin/Release/net10.0/Cxpm.Cli.dll restore --target win-x64 --project cxnet/tests/package_consumer_windows/http_smoke/http-smoke.cxproj
dotnet cxc/src/CxCompiler/bin/Release/net10.0/CxCompiler.dll cxnet/tests/package_consumer_windows/http_smoke/http-smoke.cxproj --compile --configuration Release
```

The generated CMake project can be built with the restored `cxcore` package
source in both link modes. Keep the outputs separate:

```powershell
$source = 'cxnet/tests/package_consumer_windows/http_smoke/.obj'
$packageCore = (Resolve-Path 'cxnet/tests/package_consumer_windows/.packages/cxcore/0.1.0/source').Path.Replace('\', '/')
cmake -S $source -B .build/m26-win-http-smoke-static-cmake -DCX_STATIC_LINK=ON -DCX_BUILD_STATIC=ON "-DCXCORE_SOURCE_DIR=$packageCore" -DCXCORE_ABI_VERSION=1 -DCX_OUTPUT_DIRECTORY_ROOT="$PWD/.build/m26-win-http-smoke-static-output"
cmake --build .build/m26-win-http-smoke-static-cmake --config Release
cmake -S $source -B .build/m26-win-http-smoke-shared-cmake -DCX_STATIC_LINK=OFF -DCX_BUILD_STATIC=OFF "-DCXCORE_SOURCE_DIR=$packageCore" -DCXCORE_ABI_VERSION=1 -DCX_OUTPUT_DIRECTORY_ROOT="$PWD/.build/m26-win-http-smoke-shared-output"
cmake --build .build/m26-win-http-smoke-shared-cmake --config Release
```

Stage the static executable by itself and the shared executable with its three
CX DLLs, then run each from outside the checkout:

```powershell
$stage = Join-Path $env:TEMP ('cx-m26-win-http-smoke-' + [guid]::NewGuid().ToString('N'))
$staticStage = Join-Path $stage 'static'
$sharedStage = Join-Path $stage 'shared'
New-Item -ItemType Directory -Path $staticStage, $sharedStage | Out-Null
Copy-Item .build/m26-win-http-smoke-static-output/M26WindowsPackageHttpSmoke/Release/M26WindowsPackageHttpSmoke.exe $staticStage
Copy-Item .build/m26-win-http-smoke-shared-output/M26WindowsPackageHttpSmoke/Release/* $sharedStage
python cxnet/tests/package_consumer_windows/http_smoke/run_smoke.py (Join-Path $staticStage 'M26WindowsPackageHttpSmoke.exe')
python cxnet/tests/package_consumer_windows/http_smoke/run_smoke.py (Join-Path $sharedStage 'M26WindowsPackageHttpSmoke.exe')
```

On 2026-11-26, fresh `cxcore` and `cxnet` packages built from workspace
sources. The x64 and x86 package-source CMake builds passed in Release for
both link modes. All four isolated executables passed plain HTTP, trusted-CA
HTTPS to `localhost`, and wrong-host HTTPS rejection. Each successful request
returned status 200 and the expected body; each server saw exactly one request,
and the rejected hostname never reached the server. The shared x64 stage contains
`cxcore.dll`, `cxnet.dll`, and `M26WindowsPackageConsumer.dll`; the shared x86
stage contains `cxcore.dll`, `cxnet.dll`, and
`M26WindowsPackageConsumerX86.dll`. The runner restricts each executable's
`PATH` to Windows system locations. This covers package-only Windows x86/x64
HTTP and HTTPS execution in static and shared modes.

For x86, restore and generate the project beside the x86 package consumer, then
build each link mode with the Win32 generator platform:

```powershell
dotnet cxpm/src/Cxpm.Cli/bin/Release/net10.0/Cxpm.Cli.dll restore --target win-x86 --project cxnet/tests/package_consumer_windows/win-x86/package-consumer.cxproj
$project = 'cxnet/tests/package_consumer_windows/win-x86/http_smoke/http-smoke.cxproj'
dotnet cxpm/src/Cxpm.Cli/bin/Release/net10.0/Cxpm.Cli.dll restore --target win-x86 --project $project
dotnet cxc/src/CxCompiler/bin/Release/net10.0/CxCompiler.dll $project --compile --configuration Release
$source = 'cxnet/tests/package_consumer_windows/win-x86/http_smoke/.obj'
$packageCore = (Resolve-Path 'cxnet/tests/package_consumer_windows/win-x86/.packages/cxcore/0.1.0/source').Path.Replace('\', '/')
cmake -S $source -B .build/m26-win-http-smoke-x86-static-cmake -A Win32 -DCX_STATIC_LINK=ON -DCX_BUILD_STATIC=ON "-DCXCORE_SOURCE_DIR=$packageCore" -DCXCORE_ABI_VERSION=1 -DCX_OUTPUT_DIRECTORY_ROOT="$PWD/.build/m26-win-http-smoke-x86-static-output"
cmake --build .build/m26-win-http-smoke-x86-static-cmake --config Release
cmake -S $source -B .build/m26-win-http-smoke-x86-shared-cmake -A Win32 -DCX_STATIC_LINK=OFF -DCX_BUILD_STATIC=OFF "-DCXCORE_SOURCE_DIR=$packageCore" -DCXCORE_ABI_VERSION=1 -DCX_OUTPUT_DIRECTORY_ROOT="$PWD/.build/m26-win-http-smoke-x86-shared-output"
cmake --build .build/m26-win-http-smoke-x86-shared-cmake --config Release
```

Stage `M26WindowsPackageHttpSmokeX86.exe` by itself for static mode. For shared
mode, copy the files from
`.build/m26-win-http-smoke-x86-shared-output/M26WindowsPackageHttpSmokeX86/Release/`
with the executable. Pass both staged executables to
`cxnet/tests/package_consumer_windows/http_smoke/run_smoke.py`; it runs plain
HTTP, custom-CA HTTPS to `localhost`, and a wrong-host HTTPS rejection case.

Package SHA-256 values:

- `cxcore.0.1.0.zip`: `57BCE69F46FDDFFFF701BCCAAD64B3F91FB5FB5E9C649F2AC216C625737AD082`
- `cxnet.0.1.0.zip`: `EDF6DE5140F2DE05FA11D021016C80C5E31EC27CE1DC26FF14D52B8C03B2EB3A`

## Fresh current-source package validation (2026-11-20)

Fresh `cxcore` and `cxnet` packages from a local feed were restored into
separate package-only consumer stages for `win-x64` and `win-x86`. Both
consumers compiled in Release from the restored packages. The x64 module loaded
with the isolated Python loader; the x86 module loaded with the architecture-
matched `CxPackageRuntimeLoader.exe`, using an absolute module path and the
`DLL_LOAD_DIR | SYSTEM32` search policy. The compiler and loader were outside
the staged package contents. These are package compile/load checks, not HTTP
runtime checks.

The feed archive hashes were:

- `cxcore.0.1.0.zip`: `ECBC50766E1E3AAA97CE80C71CCC65DDAD5801A87C9491EF728F88C347517048`
- `cxnet.0.1.0.zip`: `C73CA7FC619B9A8AFE9ACAF38F16E3A40743FE67711CB79CABE86040AEF7E46B`

## Fresh current-source package validation (2026-11-22)

Fresh packages from the current source were restored into separate `win-x64`
and `win-x86` consumers. Each consumer was staged under the Windows temporary
directory with only its source, manifest, lock file, and restored packages;
the compiler and runtime loader were staged separately. Both consumers built
in Release without a source checkout. The x64 module passed the isolated
Python loader; the x86 module passed the architecture-matched Win32 loader
using the `DLL_LOAD_DIR | SYSTEM32` search policy. The x86 CMake cache pointed
`CXCORE_SOURCE_DIR` at the staged package and selected `Win32`.

Both lock files used these package archive hashes:

- `cxcore.0.1.0.zip`: `77A3816C6F56A24791A4159649B6B4917F5B820E437C7C4B8330DFDBEF8AB2F3`
- `cxnet.0.1.0.zip`: `74819F1F6899199E732C69B033ED271EA45E07FE7BFEB4D4AEF6BAC74E224209`

## Fresh package-only Windows validation (2026-11-24)

Rebuilt and published current `cxcore` and `cxnet` packages to
`.build/m26-current-source-feed-win-20261124`, then restored them into separate
`win-x64` and `win-x86` stages under the Windows temporary directory. The stages
contained the consumer project/source, refreshed lock, and restored packages;
the compiler and runtime loader were outside the stages. Both Release consumers
compiled without a workspace checkout. Their CMake caches set
`CXCORE_SOURCE_DIR` to the restored `.packages/cxcore/0.1.0/source`; x86 also
selected `CMAKE_GENERATOR_PLATFORM=Win32`. The x64 module loaded through the
isolated Python loader, and the x86 module loaded through an architecture-
matched Win32 loader using `DLL_LOAD_DIR | SYSTEM32` search flags.

Archive SHA-256 values:

- `cxcore.0.1.0.zip`: `57BCE69F46FDDFFFF701BCCAAD64B3F91FB5FB5E9C649F2AC216C625737AD082`
- `cxnet.0.1.0.zip`: `83608D46827C9FD8727CA887A3A21A68A23E7ED76F3A62C29646191C8D55DE11`

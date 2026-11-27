# Packaged cxnet consumer

This fixture restores `cxnet` and its transitive `cxcore` package from a fresh
local feed, compiles a Linux x64 shared consumer that references `HttpClient`,
and checks the resulting module in an isolated runtime image.

From the workspace root, build `cxpm` and `cxc`, then publish the packages in
dependency order:

```powershell
if (Test-Path .build/m26-compression-feed-parser) { Remove-Item .build/m26-compression-feed-parser -Recurse -Force }
dotnet cxpm/src/Cxpm.Cli/bin/Release/net10.0/Cxpm.Cli.dll build --project cxcore/src/cxcore.cxproj
dotnet cxpm/src/Cxpm.Cli/bin/Release/net10.0/Cxpm.Cli.dll publish --project cxcore/src/cxcore.cxproj --feed .build/m26-compression-feed-parser
dotnet cxpm/src/Cxpm.Cli/bin/Release/net10.0/Cxpm.Cli.dll build --project cxnet/src/cxnet.cxproj
dotnet cxpm/src/Cxpm.Cli/bin/Release/net10.0/Cxpm.Cli.dll publish --project cxnet/src/cxnet.cxproj --feed .build/m26-compression-feed-parser
dotnet cxpm/src/Cxpm.Cli/bin/Release/net10.0/Cxpm.Cli.dll restore --target linux-x64 --project cxnet/tests/package_consumer/package-consumer.cxproj
```

Compile the consumer on a Linux x64 host with CMake 3.31 or newer, a C
toolchain, and the OpenSSL development package installed:

```sh
CFLAGS="-Wall -Wextra -Werror" dotnet cxc/src/CxCompiler/bin/Release/net10.0/CxCompiler.dll \
  cxnet/tests/package_consumer/package-consumer.cxproj --compile --configuration Release
```

Use `cxnet/tests/package_runtime_load.py` to load the output with
`LD_LIBRARY_PATH` cleared. The fixture should resolve its required system
OpenSSL libraries from the OS and the static curl, zlib, Brotli, and zstd
archives from the restored `cxnet` package.

## Package-only HTTP runtime smoke

The `http_smoke/` executable references the consumer library above, which in
turn references only the restored `cxnet` and `cxcore` packages. To run it in a
package-only stage, stage `Consumer.cx`, `package-consumer.cxproj`,
`cxpm.lock`, the complete `.packages/` tree, and the `http_smoke/` directory
together so the local project reference remains valid. Mount the compiler
separately, then compile and run:

```sh
dotnet CxCompiler.dll http_smoke/http-smoke.cxproj --compile --configuration Release
python3 http_smoke/run_smoke.py http_smoke/.bin/M26PackageHttpSmoke
ldd -r http_smoke/.bin/M26PackageHttpSmoke
```

The runner hosts a loopback HTTP response, requires status 200 and the exact
body `m26-package-http-ok`, and removes `LD_LIBRARY_PATH` from the executable's
environment. `ldd -r` should report no unresolved symbols.

To build the shared project-reference path, generate the CMake files without
building, then configure with shared CX modules:

```sh
dotnet CxCompiler.dll http_smoke/http-smoke.cxproj --emit-only --configuration Release
cmake -S http_smoke/.obj -B http_smoke/.obj/build-shared \
  -DCXCORE_SOURCE_DIR="$PWD/.packages/cxcore/0.1.0/source" \
  -DCX_STATIC_LINK=OFF -DCX_BUILD_STATIC=OFF \
  -DCMAKE_C_FLAGS="-Wall -Wextra -Werror -Wno-unused-value"
cmake --build http_smoke/.obj/build-shared --parallel
python3 http_smoke/run_smoke.py http_smoke/.bin/M26PackageHttpSmoke
ldd -r http_smoke/.bin/M26PackageHttpSmoke
```

The generated CMake copies transitive shared CX project dependencies beside
the final executable so it can load them without `LD_LIBRARY_PATH`.

### Ubuntu 26.10 REST/CXDATA package consumer

On 2026-11-26, a fresh Ubuntu 26.10 container (glibc 2.44) built the Linux
REST and JSON generator workers from a temporary source copy with the current
compiler, then compiled the package-only REST/CXDATA consumer from its
manifest, lock file, source, and restored packages. The consumer project
resolved `cxcore`, `cxnet`, and `cxdata` from the restored package tree; the
generator build used the temporary source copy separately. The static Release
build passed with `-Wall -Wextra -Werror -Wno-unused-value`. A separate fresh
CMake tree also built the shared consumer and package libraries with the same
strict flags. Both executables completed the loopback REST JSON request,
decoded the expected DTO, and confirmed exactly one request with
`LD_LIBRARY_PATH` unset. The shared wire executable passed 20 consecutive
runs. `ldd -r` resolved the shared `cxdata`, `cxnet`, and `cxcore` modules
and system OpenSSL, zlib, and zstd without unresolved symbols.

To verify a build without a `cxcore` checkout, stage only the consumer manifest,
lock file, and restored `.packages/` tree outside this repository, then run:

```sh
staging="$(mktemp -d)"
cp cxnet/tests/package_consumer/package-consumer.cxproj "$staging/"
cp cxnet/tests/package_consumer/cxpm.lock "$staging/"
cp -R cxnet/tests/package_consumer/.packages "$staging/"
dotnet cxc/src/CxCompiler/bin/Release/net10.0/CxCompiler.dll \
  "$staging/package-consumer.cxproj" --compile --configuration Release
python3 cxnet/tests/package_runtime_load.py "$staging/.bin/libM26Consumer.so"
```

## Recorded pristine Linux validation

On 2026-11-17, the current `cxcore` and `cxnet` manifests were published to a
fresh local feed and restored into a Debian 12 stage containing only the
consumer manifest, lock file, and `.packages/` tree. The stage was built in
Debian 12 (glibc 2.36, GCC 12.2) with CMake 4.4.4 and system OpenSSL 3; the
compiler and loader helper were mounted separately, with no source checkout
available. `cxc --compile` passed, `package_runtime_load.py` loaded
`libM26Consumer.so` with `LD_LIBRARY_PATH` empty, and `ldd -r` reported only
`libm.so.6`, `libssl.so.3`, `libcrypto.so.3`, libc, and the system loader.

The tested feed archives had SHA-256 values:

- `cxcore.0.1.0.zip`: `D3296663629A7096CE9D3E34E58EB5CD3C783FBA1A02479F26E137645DADF4C4`
- `cxnet.0.1.0.zip`: `3A1490F3FDEDF550114479067F79EDEA34751C9A27A0525F2788405835A8B9B4`

On 2026-11-17, the package-only consumer was copied into a clean Debian 13.7
stage containing only its manifest, lock file, and `.packages/` tree. It
compiled and loaded on Debian 13 (glibc 2.41, GCC 14.2.0, OpenSSL 3.5.7,
CMake 4.4.4, .NET 10.0.401); the compiler and loader helper were mounted
separately, with no source checkout available. The loader passed with
`LD_LIBRARY_PATH` empty, and `ldd -r` resolved libm, OpenSSL, libc, the system
loader, zlib, and zstd without unresolved symbols. A fresh stage also compiles
with `CFLAGS="-Wall -Wextra -Werror -Wno-unused-value"` and passes the same
isolated loader and `ldd -r` checks.

On 2026-11-17, the same package-only consumer was copied into a clean Rocky
Linux 9.9 stage containing only its manifest, lock file, and `.packages/`
tree. It compiled and loaded in Rocky Linux 9.9 (glibc 2.34, GCC 11.5, OpenSSL
3.5.9, CMake 3.31.10, .NET 10.0.404); the compiler was mounted separately and
no cxcore/cxnet checkout was available. `ldd -r` resolved all dependencies.
The consumer directly needs only libm, libssl, libcrypto, libc, and the loader;
Rocky's system `libcrypto.so.3` has a transitive `libz.so.1` dependency. The
consumer link command uses the `libcurl.a` and `libz.a` archives from the
restored package.

On 2026-11-17, the same package-only consumer compiled and loaded on pristine
Ubuntu 22.04 (glibc 2.35, GCC 11.4, OpenSSL 3.0.2, CMake 3.31.10, .NET
10.0.401). The stage contained only the consumer manifest, lock file, source,
and restored `.packages/` tree; the compiler and runtime-load helper were
mounted separately. `package_runtime_load.py` passed with `LD_LIBRARY_PATH`
empty, and `ldd -r` resolved only `libm.so.6`, `libssl.so.3`, `libcrypto.so.3`,
libc, and the system loader.

On 2026-11-18, the consumer was copied into a fresh AlmaLinux 10.3 stage with
only its manifest, lock file, and restored `.packages/` tree. It compiled on
AlmaLinux 10.3 (glibc 2.39, GCC 14.4.1, OpenSSL 3.5.9, CMake 4.4.4, .NET
10.0.404) with no cxcore/cxnet checkout available. The standard Release build
passed; `package_runtime_load.py` loaded the module with `LD_LIBRARY_PATH`
empty, and `ldd -r` resolved libm, OpenSSL, libc, the system loader, and
transitive zlib without unresolved symbols. The compiler/runtime and loader
helper were mounted separately. The first strict `-Wall -Wextra -Werror
-Wno-unused-value` attempt against an older package snapshot found GCC 14.4.1
warnings in `TaskScheduler_RunUntil` around `setjmp` and in CX ASCII
comparisons. The current source adds a no-inline boundary to that helper and
explicit `char` casts to those comparisons. After publishing fresh `cxcore` and
`cxnet` packages, a clean AlmaLinux 10.3 package-only consumer compiled with
those strict flags, loaded with an empty `LD_LIBRARY_PATH`, and passed `ldd -r`
with no unresolved symbols. This also confirmed the current generated headers
do not contain the unused delegate declarations seen in the stale stage.

On 2026-11-18, a fresh package-only stage compiled on Fedora 44 (glibc 2.43,
GCC 16.2.1, OpenSSL 3.5.9, CMake 4.3.0, .NET 10.0.113). The stage contained
only the consumer manifest, lock file, and restored packages; the current
`cxcore` and `cxnet` archives came from a fresh local feed. The strict
`-Wall -Wextra -Werror -Wno-unused-value` build passed, as did
`package_runtime_load.py` with `LD_LIBRARY_PATH` empty and `ldd -r` with no
unresolved symbols. The compiler/runtime and loader helper were mounted
separately, with no source checkout available.

On 2026-11-18, a fresh package-only stage compiled in Ubuntu 24.04 (glibc
2.39, GCC 13.3.0, OpenSSL 3.0.13, CMake 3.31.6). The strict
`-Wall -Wextra -Werror -Wno-unused-value` build passed. The package-only
module loaded with `LD_LIBRARY_PATH` empty, and `ldd -r` resolved libm,
OpenSSL, libc, and the system loader without unresolved symbols. The container
held only the consumer manifest, lock file, generated consumer sources, and
restored packages; no `cxcore` or `cxnet` checkout was mounted.

On 2026-11-18, the same fresh-feed package-only stage compiled on OpenSUSE
Tumbleweed (`VERSION_ID=20261116`, glibc 2.44, GCC/G++ 16.2.1, OpenSSL 3.5.3,
CMake 4.4.4, .NET runtime 10.0.12). The strict
`-Wall -Wextra -Werror -Wno-unused-value` build passed. The module loaded with
`LD_LIBRARY_PATH` empty, and `ldd -r` resolved libm, OpenSSL, libc, and the
system loader without unresolved symbols. Only the package consumer manifest,
lock file, and restored packages were staged; the compiler and loader helper
were mounted separately, with no `cxcore` or `cxnet` checkout available.

On 2026-11-18, the package-only consumer also compiled on CentOS Stream 10
(glibc 2.39, GCC/G++ 14.4.1, OpenSSL 3.5.9, CMake 3.31.8, .NET SDK 10.0.401).
The stage contained only the consumer manifest, lock file, and restored
packages; the compiler and runtime-load helper were mounted separately. The
strict `-Wall -Wextra -Werror -Wno-unused-value` build passed. The module
loaded with `LD_LIBRARY_PATH` empty, and `ldd -r` resolved libm, OpenSSL, libc,
the system loader, and transitive zlib without unresolved symbols.

On 2026-11-18, a clean Ubuntu 26.04.1 LTS stage (glibc 2.43, GCC 15.2.0,
OpenSSL 3.5.5, CMake 4.4.5, .NET SDK 10.0.404) compiled the package-only
consumer with `-Wall -Wextra -Werror -Wno-unused-value`. The stage contained
only `Consumer.cx`, the manifest, lock file, and restored `cxcore`/`cxnet`
packages; compiler and loader helper were mounted separately, with no source
checkout. `package_runtime_load.py` passed with `LD_LIBRARY_PATH` empty, and
`ldd -r` resolved libm, OpenSSL, libc, the system loader, zlib, and zstd with
no unresolved symbols.

On 2026-11-19, the Linux curl archive was rebuilt from pinned curl 8.23.0 on
Ubuntu 24.04.5 (GCC 13.3.0, OpenSSL 3.0.13, CMake 3.28.3) with
`CFLAGS=-DOPENSSL_NO_ENGINE`. The archive retains zlib, Brotli, and zstd
support, contains no OpenSSL `ENGINE_*` references, and has SHA-256
`2C782DE553D5D664A0B197C74C2E797F5D404695CF19BC2E31E58819AB30B928`.
The fresh-feed `cxnet.0.1.0.zip` containing this archive has SHA-256
`9E0DFEFF01740C6B6926BD3D8664D9DAB3EBBDFE3AB1F9951C13E0F0078B4B7F`.

That fresh-feed package-only consumer passes the strict compile, isolated load
with `LD_LIBRARY_PATH` empty, and `ldd -r` without unresolved symbols on
Ubuntu 22.04.5 (glibc 2.35, GCC 11.4.0, OpenSSL 3.0.2, CMake 3.31.6), Ubuntu
26.04.1 (glibc 2.43, GCC 15.2.0, OpenSSL 3.5.5, CMake 4.2.3), and Ubuntu
26.10 (glibc 2.44, GCC 15.3.0, OpenSSL 4.0.3, CMake 4.3.4). Each stage
contains only `Consumer.cx`, the manifest, lock file, and restored packages;
the compiler and loader helper are mounted separately.

On 2026-11-19, the same current-source fresh-feed packages passed the strict
package-only build on Debian 12.15 (glibc 2.36-9+deb12u14, GCC 12.2.0,
OpenSSL 3.0.22, CMake 3.31.6, .NET runtime 10.0.15). `cxcore.0.1.0.zip` has
SHA-256 `50047FBB56ACD4CB3E0D4035CBCBDC20032B7463570264A085951DD30A4A2E35`;
`cxnet.0.1.0.zip` has SHA-256
`9E0DFEFF01740C6B6926BD3D8664D9DAB3EBBDFE3AB1F9951C13E0F0078B4B7F`.
The isolated module load passed with `LD_LIBRARY_PATH` empty, and `ldd -r`
resolved only system libm, OpenSSL, libc, and the system loader. The stage
contained the consumer source, manifest, lock file, and restored packages;
compiler and loader helper were mounted separately, with no source checkout.

On 2026-11-19, the current-source fresh-feed packages also passed a clean
Rocky Linux 9 package-only build (glibc 2.34-290.el9_9, GCC 11.5.0, OpenSSL
3.5.9, CMake 3.31.8, .NET runtime 10.0.15) with `-Wall -Wextra -Werror
-Wno-unused-value`. The stage restored both packages from the fresh feed using
the locked hashes above and held only the consumer source, manifest, lock file,
and restored packages; compiler and loader helper were mounted separately,
with no source checkout. The isolated module load passed with
`LD_LIBRARY_PATH` empty, and `ldd -r` resolved libm, OpenSSL, libc, the system
loader, and transitive zlib without unresolved symbols.

On 2026-11-20, the package-only consumer was staged with only its source,
manifest, lock file, and restored `cxcore`/`cxnet` packages. The Release
compiler was mounted separately into Debian 13.7 (glibc 2.41, GCC 14.2.0,
OpenSSL 3.5.7, CMake 4.4.4, .NET 10.0.401). The Linux x64 consumer compiled
and linked with `-Wall -Wextra -Werror`; `ldd -r` resolved libm, OpenSSL,
libc, the system loader, zlib, and zstd without unresolved symbols. Runtime
module loading was not part of this check.

On 2026-11-20, freshly published current-source packages passed the same
package-only consumer build on Debian 13 with
`-Wall -Wextra -Werror -Wno-unused-value`. The isolated loader passed with
`LD_LIBRARY_PATH` empty, and `ldd -r` reported no unresolved symbols. The
stage contained only the consumer source, manifest, lock file, and restored
packages; the current compiler and loader helper were mounted separately. The
tested package SHA-256 values are:

- `cxcore.0.1.0.zip`: `ECBC50766E1E3AAA97CE80C71CCC65DDAD5801A87C9491EF728F88C347517048`
- `cxnet.0.1.0.zip`: `B8637C997B0AC108A7306B2440A66D00D25065EDBFCCF6870987C3E2ECAAB9CF`

After the HTTP parser began rejecting unsupported `101 Switching Protocols`,
a second fresh `cxnet` package passed the same strict Debian 13 consumer build,
isolated loader, and `ldd -r` checks. The `cxnet.0.1.0.zip` SHA-256 was
`B58A08D30CCB5176C511733AC5CA85C5BA7FCA507F71CD34539A167C976497E4`.

A subsequent current-source package containing the bounded UTF-8 prefix
correction passed the same strict consumer build, isolated loader, and `ldd -r`
checks. Its `cxnet.0.1.0.zip` SHA-256 is
`AC15D9A567DF5DDF038D8DCA2854B9CE729C32BE8AA7C219EE44D50393F4F6F8`.

A fresh package containing the RFC 6265 Domain attribute handling also passed
the strict Debian 13 package-only build, isolated loader, and `ldd -r` checks.
The `cxnet.0.1.0.zip` SHA-256 is
`32ECA8B9FE34661E1C604D0A0245F13E098EF98B485427E2A5F450498AE89757`.

After IPv6 URI-host equivalence was added to same-origin redirect and cookie
matching, freshly published packages passed the strict Debian 13 package-only
compile, isolated load with an empty `LD_LIBRARY_PATH`, and `ldd -r` checks.
The tested SHA-256 values are:

- `cxcore.0.1.0.zip`: `ECBC50766E1E3AAA97CE80C71CCC65DDAD5801A87C9491EF728F88C347517048`
- `cxnet.0.1.0.zip`: `AED48E090210B7FE13C0DD3AE5374ECD9C2157CC28023C99CC3BD55E3E7C3633`

After URI percent escapes in zone identifiers were normalized during
comparison, the package-only consumer passed the same strict Debian 13 build,
isolated load with an empty `LD_LIBRARY_PATH`, and `ldd -r` checks. The current
`cxnet.0.1.0.zip` SHA-256 is
`D98D91ED35F6A74FA262A8CD521BF2C07FAD3C4BD4E4D04E9A726B5E11CA517E`.

After cookie Domain values began using shared URI host canonicalization, a
fresh package-only Debian 13 consumer passed the strict compile, isolated load
with an empty `LD_LIBRARY_PATH`, and `ldd -r` checks. The
`cxnet.0.1.0.zip` SHA-256 is
`8428FF2F6C851F9B3D2D9EFC093BB305476B23CF747D6EA7AC271C13BE743D6B`.

On 2026-11-20, refreshed `cxcore` and `cxnet` packages containing the task
completion and HTTP lifetime fixes passed a strict Linux x64 package consumer
build with GCC 11.4 and `-Wall -Wextra -Werror -Wno-unused-value`. The isolated
loader passed with `LD_LIBRARY_PATH` empty, and `ldd -r` found no unresolved
symbols. The tested package SHA-256 values are:

- `cxcore.0.1.0.zip`: `E8D5F9E31A3FA0CF73A60FEB4EC3717DEF373B864C449C7D7CDA23FD2AEEDF2D`
- `cxnet.0.1.0.zip`: `D0E04E254984930ACE605721F7502901E54570A1E758D59824536A5CDB20077D`

On 2026-11-21, packages freshly published from the current source compiled in
an isolated Ubuntu 24.04.5 stage (glibc 2.39, GCC 13.3.0, OpenSSL 3.0.13,
CMake 4.4.4, .NET runtime 10.0.12). The stage contained only the consumer
source, manifest, lock file, and restored packages. The compiler and loader
helper were mounted separately, with no source checkout visible. Compilation
passed with `-Wall -Wextra -Werror -Wno-unused-value`; the isolated loader
passed with `LD_LIBRARY_PATH` empty, and `ldd -r` resolved only system
dependencies. The package archives have these SHA-256 values:

- `cxcore.0.1.0.zip`: `85C2EED6F2AF53CD466EEF99C64B6E808718F30BE45C56BE22DFE25EB210627D`
- `cxnet.0.1.0.zip`: `EBCAEAC443D0E657F1718B9086B5E835ACED625327386DB2C4242FE531D663BE`

On 2026-11-21, freshly published packages from the current source passed the
strict package-only consumer build on Arch Linux (`VERSION_ID=20261108.0.623074`,
glibc 2.44, GCC 16.2.1, OpenSSL 4.1.0, CMake 4.5.1, .NET SDK 10.0.114). The
stage held only the consumer source, manifest, lock file, and restored
`cxcore`/`cxnet` packages; the compiler and loader helper were mounted
separately, with no source checkout. Compilation passed with
`-Wall -Wextra -Werror -Wno-unused-value`; the isolated loader passed with
`LD_LIBRARY_PATH` empty, and `ldd -r` found no unresolved symbols. This GCC
version exposed a `-Wclobbered` warning in `SocketRuntime_Close`; moving its
exception boundary to a no-inline helper keeps the loop cursor outside
`setjmp`/`longjmp` and passes the strict build. The package archive SHA-256
values are:

- `cxcore.0.1.0.zip`: `77A3816C6F56A24791A4159649B6B4917F5B820E437C7C4B8330DFDBEF8AB2F3`
- `cxnet.0.1.0.zip`: `74819F1F6899199E732C69B033ED271EA45E07FE7BFEB4D4AEF6BAC74E224209`

On 2026-11-22, those same freshly published packages passed the package-only
consumer on Ubuntu 26.10 (glibc 2.44, GCC 15.3.0, CMake 4.3.4, OpenSSL
4.0.3, .NET runtime 10.0.12). The isolated stage contained only the consumer
source, manifest, lock file, and restored packages; the package feed, compiler,
package manager, .NET runtime, and loader helper were mounted separately. The
consumer compiled with `-Wall -Wextra -Werror -Wno-unused-value`, loaded with
`LD_LIBRARY_PATH` unset, and passed `ldd -r` with no unresolved symbols. The
`cxcore` and `cxnet` archive hashes match the preceding Arch Linux entry.

On 2026-11-22, a package-only executable built from the same Ubuntu 26.10
packages completed a loopback `HttpClient.GetAsync`, checked status 200 and
the body `m26-package-http-ok`, and exited successfully with `LD_LIBRARY_PATH`
unset. The clean Ubuntu 26.10 container used GCC 15.3.0, CMake 4.3.4, OpenSSL
4.0.3, Python 3.14.7, and .NET runtime 10.0.15. Only the smoke source,
manifest, lock file, and restored packages were staged; the compiler was
mounted separately. The consumer and its packaged `cxcore`/`cxnet` sources
compiled with `-Wall -Wextra -Werror -Wno-unused-value` before the request ran.
The package hashes match the preceding Arch Linux entry.

On 2026-11-22, freshly published packages also passed on pristine openSUSE
Tumbleweed (`VERSION_ID=20261116`, glibc 2.44, GCC 16.2.1, OpenSSL 3.5.3,
CMake 4.5.0, .NET SDK 10.0.404). The isolated stage contained only the consumer
source, manifest, lock file, and restored packages; the compiler and loader
helper were mounted separately. Compilation passed with
`-Wall -Wextra -Werror -Wno-unused-value`; module loading passed with
`LD_LIBRARY_PATH` unset, and `ldd -r` found no unresolved symbols. Archive
SHA-256 values:

- `cxcore.0.1.0.zip`: `77A3816C6F56A24791A4159649B6B4917F5B820E437C7C4B8330DFDBEF8AB2F3`
- `cxnet.0.1.0.zip`: `74819F1F6899199E732C69B033ED271EA45E07FE7BFEB4D4AEF6BAC74E224209`

The same isolated openSUSE stage also built a standalone executable using only
the restored packages. With a loopback HTTP fixture running inside the image,
the executable completed `HttpClient.GetAsync`, checked status 200 and the
response body, and exited successfully with `LD_LIBRARY_PATH` unset.

To verify a build without a `cxcore` checkout, stage only the consumer manifest,
lock file, and restored `.packages/` tree outside this repository, then run:

```sh
staging="$(mktemp -d)"
cp cxnet/tests/package_consumer/package-consumer.cxproj "$staging/"
cp cxnet/tests/package_consumer/cxpm.lock "$staging/"
cp -R cxnet/tests/package_consumer/.packages "$staging/"
dotnet cxc/src/CxCompiler/bin/Release/net10.0/CxCompiler.dll \
  "$staging/package-consumer.cxproj" --compile --configuration Release
python3 cxnet/tests/package_runtime_load.py "$staging/.bin/libM26Consumer.so"
```

On 2026-11-23, freshly rebuilt `cxcore` and `cxnet` packages were restored
from a new local feed into a clean package-only consumer stage. The stage held
only `Consumer.cx`, the consumer manifest, lock file, and restored `.packages/`
tree; the Release compiler and loader helper were mounted separately, with no
repository checkout visible. The consumer compiled on Debian 13 (glibc 2.41,
GCC 14.2.0, OpenSSL 3.5.7, CMake 4.4.4, .NET runtime 10.0.12) using
`-Wall -Wextra -Werror -Wno-unused-value`. The module loaded with
`LD_LIBRARY_PATH` empty, and `ldd -r` resolved libm, OpenSSL, libc, the system
loader, zlib, and zstd without unresolved symbols. The package archive SHA-256
values are:

- `cxcore.0.1.0.zip`: `77A3816C6F56A24791A4159649B6B4917F5B820E437C7C4B8330DFDBEF8AB2F3`
- `cxnet.0.1.0.zip`: `F21BACCFF71965BF189689DBC2B7E58AFF606F7C92B8C6AF8B3211959232CE38`

After adding scheduler-shutdown continuations to HTTP timers, freshly rebuilt
packages were published to a new local feed and restored into a separate
Ubuntu 22.04.5 WSL consumer stage (glibc 2.35, GCC 11.4.0, OpenSSL 3). The
stage contained only `Consumer.cx`, its manifest and lock file, and the
restored packages; the compiler and loader helper were mounted separately.
The consumer compiled with `-Wall -Wextra -Werror -Wno-unused-value` from the
isolated stage. The module loaded with `LD_LIBRARY_PATH` empty, and `ldd -r`
resolved libm, OpenSSL, libc, and the system loader without unresolved symbols.
This validates the new internal scheduler symbol across the package boundary;
the stage ran under WSL rather than a pristine OS image. Archive SHA-256 values:

- `cxcore.0.1.0.zip`: `4E1C869E96B9B2FBB6EE027CBA2A9D8C9CFA4A7F2E85165459C598DF53AD1001`
- `cxnet.0.1.0.zip`: `5281B1B2418BF30B885FBCD77AC835BB244599739979458724A86F5A7502458E`

On 2026-11-23, newly rebuilt current-source packages also passed on CentOS
Stream 10 (`VERSION_ID=10`, glibc 2.39-148, GCC 14.4.1, OpenSSL 3.5.9,
CMake 4.4.4, .NET SDK 10.0.401). The stage contained only `Consumer.cx`, its
manifest and lock file, and packages restored from that fresh feed; the
compiler, .NET runtime, CMake, and loader helper were mounted separately. The
consumer compiled with `-Wall -Wextra -Werror -Wno-unused-value`; isolated
loading passed with `LD_LIBRARY_PATH` unset, and `ldd -r` reported no unresolved
symbols. Package archive SHA-256 values:

- `cxcore.0.1.0.zip`: `77A3816C6F56A24791A4159649B6B4917F5B820E437C7C4B8330DFDBEF8AB2F3`
- `cxnet.0.1.0.zip`: `C2DF46BB2067EDD00476319840361FCAB1B2A891A2DB880247D03D832FF3EC26`

On 2026-11-23, the same freshly rebuilt packages passed on a pristine Fedora 44
container (glibc 2.43, GCC 16.2.1, CMake 4.3.0, OpenSSL 3.5.9, .NET 10.0.113).
The isolated stage contained only consumer sources, its manifest and lock file,
and restored packages; the Release compiler, loader helper, and package feed
were supplied separately, with no repository checkout visible. The consumer
compiled with `-Wall -Wextra -Werror -Wno-unused-value`; isolated loading passed
with `LD_LIBRARY_PATH` unset, and `ldd -r` found no unresolved symbols. A
separate package-only executable completed a loopback `HttpClient.GetAsync`,
verified status 200 and body `m26-package-http-ok`, and exited successfully
with `LD_LIBRARY_PATH` unset. Archive SHA-256 values:

- `cxcore.0.1.0.zip`: `77A3816C6F56A24791A4159649B6B4917F5B820E437C7C4B8330DFDBEF8AB2F3`
- `cxnet.0.1.0.zip`: `F21BACCFF71965BF189689DBC2B7E58AFF606F7C92B8C6AF8B3211959232CE38`

On 2026-11-23, a package-only executable was built from the same current-source
CentOS Stream 10 packages in a clean CentOS Stream 10 container (glibc
2.39-148, GCC 14.4.1, OpenSSL 3.5.9, CMake 3.31.8, .NET SDK 10.0.401). The
isolated stage contained the smoke source, manifest, lock file, and restored
packages; the compiler and .NET runtime were mounted separately. Compilation
passed with `-Wall -Wextra -Werror -Wno-unused-value`. With
`LD_LIBRARY_PATH` unset, `HttpClient.GetAsync` completed against a loopback
fixture, verified status 200 and body `m26-package-http-ok`, and exited
successfully. `ldd -r` reported no unresolved symbols. Archive SHA-256 values:

- `cxcore.0.1.0.zip`: `77A3816C6F56A24791A4159649B6B4917F5B820E437C7C4B8330DFDBEF8AB2F3`
- `cxnet.0.1.0.zip`: `C2DF46BB2067EDD00476319840361FCAB1B2A891A2DB880247D03D832FF3EC26`

On 2026-11-23, the same package-only HTTP smoke was built and run in a clean
Ubuntu 24.04.5 container (glibc 2.39, GCC 13.3.0, OpenSSL 3.0.13, CMake
3.31.10, .NET SDK 10.0.401). The stage contained only the smoke source,
manifest, lock file, and restored packages; the compiler and feed were mounted
separately, with no repository checkout. The strict
`-Wall -Wextra -Werror -Wno-unused-value` build passed. With
`LD_LIBRARY_PATH` unset, `HttpClient.GetAsync` returned status 200 and body
`m26-package-http-ok`; `ldd -r` reported no unresolved symbols. Archive
SHA-256 values:

- `cxcore.0.1.0.zip`: `77A3816C6F56A24791A4159649B6B4917F5B820E437C7C4B8330DFDBEF8AB2F3`
- `cxnet.0.1.0.zip`: `C2DF46BB2067EDD00476319840361FCAB1B2A891A2DB880247D03D832FF3EC26`

On 2026-11-23, the same isolated package-only HTTP smoke passed in a clean
Arch Linux container (glibc 2.44, GCC 16.2.1, OpenSSL 4.1.0, CMake 4.5.1,
.NET SDK 10.0.114). The stage contained only the smoke source, manifest, lock
file, and restored packages; the current compiler and feed were mounted
separately. Compilation used the current locked `cxcore` and `cxnet` packages.
With `LD_LIBRARY_PATH` unset, `HttpClient.GetAsync` returned status 200 and
body `m26-package-http-ok`; `ldd -r` reported no unresolved symbols. Archive
SHA-256 values:

- `cxcore.0.1.0.zip`: `77A3816C6F56A24791A4159649B6B4917F5B820E437C7C4B8330DFDBEF8AB2F3`
- `cxnet.0.1.0.zip`: `C2DF46BB2067EDD00476319840361FCAB1B2A891A2DB880247D03D832FF3EC26`

On 2026-11-23, the packages were rebuilt from the current workspace sources,
published to a fresh local feed, and restored into new package-only stages for
Fedora 44 (`glibc 2.43`, GCC 16.2.1, CMake 4.3.0, OpenSSL 3.5.9, .NET SDK
10.0.113). The isolated run mounted only the restored consumer stages, the
Release compiler, and the loader helper; it did not mount the repository. Both
consumer modules compiled with `-Wall -Wextra -Werror -Wno-unused-value`. The
library loaded with `LD_LIBRARY_PATH` unset, and `ldd -r` found no unresolved
symbols. A package-only executable also completed loopback `HttpClient.GetAsync`,
verified status 200 and body `m26-package-http-ok`, and passed `ldd -r` with
`LD_LIBRARY_PATH` unset. Archive SHA-256 values for this refreshed build:

- `cxcore.0.1.0.zip`: `5D6CCE5A530FB47B600F8F8713F6523CC5CFE05C1B6ACE5C2489904DAD64DD5B`
- `cxnet.0.1.0.zip`: `AE8B24D3D99362118012B69BEF7042945C0B7709310E2C21E63C17AC5AD6E0DE`

## Fresh current-source package-only HTTP validation (2026-11-24)

Published the current workspace `cxcore` and `cxnet` packages, including the
ASCII IDNA hyphen validation update, into a new local feed and restored them
into a clean package-only consumer stage. The stage
contained the HTTP smoke source/project, consumer source/manifest, lock file,
and restored packages; the current compiler was mounted separately, with no
repository checkout available in the container. In Debian 13.7 (glibc 2.41,
GCC 14.2.0, OpenSSL 3.5.7, CMake 4.4.4, .NET SDK 10.0.401), the Release
consumer compiled with `-Wall -Wextra -Werror -Wno-unused-value`. The loopback
HTTP GET returned status 200 and body `m26-package-http-ok`; the runner cleared
`LD_LIBRARY_PATH`. `ldd -r` found no unresolved symbols.

Archive SHA-256 values:

- `cxcore.0.1.0.zip`: `09C834D4F17801F0628EB60ABF25D82C9E5138DBC6A157051A18916D9E4B25AB`
- `cxnet.0.1.0.zip`: `83608D46827C9FD8727CA887A3A21A68A23E7ED76F3A62C29646191C8D55DE11`

The same fresh feed also passed the shared project-reference path in a second
clean stage. The current compiler emitted CMake files, then CMake built with
`CX_STATIC_LINK=OFF`, `CX_BUILD_STATIC=OFF`, and the same strict warning flags.
The loopback GET passed with `LD_LIBRARY_PATH` cleared. `ldd -r` resolved the
colocated `libM26Consumer.so`, `libcxnet.so`, and `libcxcore.so` plus system
OpenSSL and compression libraries, with no unresolved symbols.

On 2026-11-23, the reusable package-only HTTP smoke was compiled from those
fresh-feed packages with GCC 11.4.0, CMake 3.31.6, and
`-Wall -Wextra -Werror -Wno-unused-value`. The static executable passed in a
clean Debian 12 container (glibc 2.36, system OpenSSL 3). After the compiler
began propagating shared runtime files through project references, the shared
executable also passed in the same clean image. Both runs used only the staged
consumer and `run_smoke.py`, with Python and `libssl3` supplied by the image;
`LD_LIBRARY_PATH` was unset. `HttpClient.GetAsync` returned status 200 and the
expected body in both modes. Static `ldd -r` resolved only OS libraries;
shared `ldd -r` also resolved the colocated `libM26Consumer.so`, `libcxnet.so`,
and `libcxcore.so`. Neither mode had unresolved symbols. Archive SHA-256
values:

- `cxcore.0.1.0.zip`: `5D6CCE5A530FB47B600F8F8713F6523CC5CFE05C1B6ACE5C2489904DAD64DD5B`
- `cxnet.0.1.0.zip`: `9938602140361767F69C3B9F7A89FEA5D5FC305EFA355A7604719A8B037A1740`

On 2026-11-23, a package-only HTTP executable was restored from a fresh feed
and built inside a clean Debian 13.7 container (glibc 2.41, GCC 14.2.0,
OpenSSL 3.5.7, CMake 4.4.4, .NET SDK 10.0.401). The stage contained only the
smoke project, its source, lock file, and restored `cxcore`/`cxnet` packages;
the package feed, compiler, and Python loopback runner were mounted separately,
with no repository checkout. The strict
`-Wall -Wextra -Werror -Wno-unused-value` build passed. The loopback
`HttpClient.GetAsync` verified status 200 and body `m26-package-http-ok`;
`LD_LIBRARY_PATH` was cleared by the runner, and `ldd -r` found no unresolved
symbols. Package SHA-256 values:

- `cxcore.0.1.0.zip`: `5D6CCE5A530FB47B600F8F8713F6523CC5CFE05C1B6ACE5C2489904DAD64DD5B`
- `cxnet.0.1.0.zip`: `AE8B24D3D99362118012B69BEF7042945C0B7709310E2C21E63C17AC5AD6E0DE`

## Current DNS hostname length package validation (2026-11-24)

Republished the current `cxcore` and `cxnet` sources, including the DNS hostname
length correction, to `.build/m26-current-source-feed-20261124-dns-length` and
restored them into clean static and shared consumer stages. Both Debian 13.7
package-only HTTP smoke builds passed with `-Wall -Wextra -Werror
-Wno-unused-value`. Both loopback GETs returned status 200 and
`m26-package-http-ok` with `LD_LIBRARY_PATH` cleared. Static and shared
`ldd -r` found no unresolved symbols; the shared loader resolved the staged
`libM26Consumer.so`, `libcxnet.so`, and `libcxcore.so`. The containers mounted
only the restored stage and compiler, without a repository checkout.

Package hashes:

- `cxcore.0.1.0.zip`: `57BCE69F46FDDFFFF701BCCAAD64B3F91FB5FB5E9C649F2AC216C625737AD082`
- `cxnet.0.1.0.zip`: `83608D46827C9FD8727CA887A3A21A68A23E7ED76F3A62C29646191C8D55DE11`

## Current-source package-only HTTP validation on Fedora 44 (2026-11-24)

Restored the current `cxcore` and `cxnet` packages from the fresh local feed into
an isolated consumer stage, then compiled and ran the HTTP smoke inside Fedora
44 (glibc 2.43, GCC 16.2.1, OpenSSL 3.5.9, CMake 4.3.0, .NET SDK 10.0.401).
The container had no repository checkout; it mounted the stage, compiler, and
.NET SDK separately. Both static and shared project-reference builds passed
with `-Wall -Wextra -Werror -Wno-unused-value`. In both modes, loopback
`HttpClient.GetAsync` returned status 200 and body `m26-package-http-ok` with
`LD_LIBRARY_PATH` cleared. `ldd -r` found no unresolved symbols. The shared
loader resolved the staged consumer, `libcxnet.so`, and `libcxcore.so`, along
with system OpenSSL and compression libraries.

Package SHA-256 values:

- `cxcore.0.1.0.zip`: `57BCE69F46FDDFFFF701BCCAAD64B3F91FB5FB5E9C649F2AC216C625737AD082`
- `cxnet.0.1.0.zip`: `83608D46827C9FD8727CA887A3A21A68A23E7ED76F3A62C29646191C8D55DE11`

## Current-source package-only HTTPS validation on Rocky Linux 9 (2026-11-25)

Using the current-source packages from the CentOS HTTPS feed, compiled the
package-only smoke from its restored package sources inside Rocky Linux 9.9
(glibc 2.34, GCC 11.5.0, OpenSSL 3.5.9, CMake 3.31.10). The stage contained
only the consumer, lock file, restored packages, and HTTP/HTTPS smoke sources;
the compiler was mounted separately for C-code generation, and the Rocky
container had no repository checkout. Strict `-Wall -Wextra -Werror
-Wno-unused-value` static and shared builds passed. In both modes, loopback
HTTP, system-trust HTTPS, and explicit-CA HTTPS returned status 200 and the
expected body with `LD_LIBRARY_PATH` unset. A wrong-host certificate was
rejected before the handler received a request. `ldd -r` reported no unresolved
symbols; shared mode resolved the staged consumer, `libcxnet.so`, and
`libcxcore.so` with Rocky's system OpenSSL and compression libraries.

The test CA was added to the disposable Rocky trust store with
`update-ca-trust extract` for the system-trust case. The configured libcurl CA
path was absent; the Linux fallback selected Rocky's readable CA bundle while
peer and hostname verification remained enabled.

Package SHA-256 values:

- `cxcore.0.1.0.zip`: `57BCE69F46FDDFFFF701BCCAAD64B3F91FB5FB5E9C649F2AC216C625737AD082`
- `cxnet.0.1.0.zip`: `4E0206799FBB46CE184C20AB8A823254FCB578DB19B2EC37E265B4EC36B4E3D2`

## Current-source package-only HTTP and HTTPS validation on CentOS Stream 10 (2026-11-25)

Restored the current `cxcore` and `cxnet` packages from a fresh local feed into
an isolated consumer stage, then compiled and ran the HTTP smoke inside a clean
CentOS Stream 10 container (glibc 2.39, GCC 14.4.1, CMake 3.31.8, OpenSSL
3.5.9, .NET SDK 10.0.113). The container had no repository checkout; it mounted
only the consumer stage and compiler. The package-only consumer built in static
and shared modes with `-Wall -Wextra -Werror -Wno-unused-value`. In both modes,
loopback HTTP GET, HTTPS using the system trust store, and HTTPS with an explicit
CA file returned status 200 and the expected body with `LD_LIBRARY_PATH` unset.
A wrong-host certificate was rejected before the HTTP handler received a
request. `ldd -r` found no unresolved symbols in either mode.

This run exposed that libcurl's build-time CA path,
`/etc/ssl/certs/ca-certificates.crt`, does not exist on CentOS. When the caller
does not set `CaCertificateFile`, the Linux handler now checks whether libcurl's
configured default CA file is readable and falls back to a readable distro CA
bundle path. Peer and hostname verification stay enabled. The test CA was
installed into the disposable CentOS trust store with `update-ca-trust` for the
system-trust HTTPS case; the explicit-CA case uses the staged test certificate.

Package SHA-256 values:

- `cxcore.0.1.0.zip`: `57BCE69F46FDDFFFF701BCCAAD64B3F91FB5FB5E9C649F2AC216C625737AD082`
- `cxnet.0.1.0.zip`: `4E0206799FBB46CE184C20AB8A823254FCB578DB19B2EC37E265B4EC36B4E3D2`

## Current-source package-only HTTP validation on Rocky Linux 9 (2026-11-25)

Updated an isolated Linux x64 consumer stage from the current-source feed before
testing, so the lock file matched these package archive hashes. In a clean
Rocky Linux 9.9 container (glibc 2.34-290, GCC 11.5.0, CMake 3.31.8, OpenSSL
3.5.9, ICU 67.1, .NET SDK 10.0.401), both static and shared project-reference
builds passed with `-Wall -Wextra -Werror -Wno-unused-value`. The stage mounted
only consumer sources, the lock file, and restored packages; the compiler and
.NET SDK were mounted separately, with no repository checkout. The loopback
`HttpClient.GetAsync` returned status 200 and body `m26-package-http-ok` in both
modes. The smoke runner cleared `LD_LIBRARY_PATH`; `ldd -r` found no unresolved
symbols. In shared mode it resolved the staged consumer, `libcxnet.so`, and
`libcxcore.so` alongside Rocky's OpenSSL and compression libraries.

Package SHA-256 values:

- `cxcore.0.1.0.zip`: `57BCE69F46FDDFFFF701BCCAAD64B3F91FB5FB5E9C649F2AC216C625737AD082`
- `cxnet.0.1.0.zip`: `83608D46827C9FD8727CA887A3A21A68A23E7ED76F3A62C29646191C8D55DE11`

## Package-only REST and CXDATA JSON validation (2026-11-25)

Added [`rest_json/`](rest_json/) as a package-consumer fixture for the optional
REST JSON path. It restores `cxnet` and `cxdata` (with `cxcore` transitively)
from a local feed, generates the REST interface implementation and DTO codec,
then compiles without a repository source reference. The loopback runner checks
that the generated client sends exactly one `GET /package-json`, decodes the
JSON into the annotated DTO, and returns the expected field values.

On Windows x86 and x64, isolated package stages restored all three packages and
compiled successfully in static and shared modes with the current compiler and
generator tools. All four executables passed the loopback wire check. Shared
output contained `cxcore.dll`, `cxdata.dll`, and `cxnet.dll` beside the
consumer.

The same fixture also passed Linux x64 static and shared builds in a clean
Rocky Linux 9.9 container (glibc 2.34, GCC 11.5.0, CMake 3.31.8, OpenSSL
3.5.9). The packages were restored from the local feed into an isolated stage.
The Windows-hosted compiler and generators emitted CX and native build files;
the Rocky container compiled and ran the stage from package sources, without a
repository checkout. Both builds used `-Wall -Wextra -Werror
-Wno-unused-value`. The loopback request passed with `LD_LIBRARY_PATH` unset.
`ldd -r` found no unresolved symbols: static output linked against system
libraries, and shared output resolved the staged `libcxdata.so`, `libcxnet.so`,
and `libcxcore.so` alongside system OpenSSL and compression libraries.

The successful stage lockfile recorded these archive SHA-256 values:

- `cxcore.0.1.0.zip`: `57BCE69F46FDDFFFF701BCCAAD64B3F91FB5FB5E9C649F2AC216C625737AD082`
- `cxnet.0.1.0.zip`: `4E0206799FBB46CE184C20AB8A823254FCB578DB19B2EC37E265B4EC36B4E3D2`
- `cxdata.0.1.0-alpha.zip`: `7F3C1A3101F627122680977D157F19ECD00AC8A27A7593D0627570F365F3B986`

## Current-source package-only HTTP and REST on openSUSE Tumbleweed (2026-11-27)

Restored the current-source `cxcore`, `cxnet`, and `cxdata` packages into
isolated HTTP and REST consumer stages, then built and ran them in the clean
openSUSE Tumbleweed image (`VERSION_ID=20261116`, glibc 2.44, GCC 16.2.1,
CMake 4.5.0, OpenSSL 3.5.3, .NET SDK 10.0.404). The consumer stages had no
repository checkout; the compiler was mounted separately. For REST generation,
the current-source Linux generator workers were built from a temporary source
copy and staged alongside the restored packages.

The REST/CXDATA JSON consumer passed strict static and shared builds and its
loopback `GET /package-json`, verifying the decoded DTO and exactly one request.
The shared run used an empty `LD_LIBRARY_PATH`; `ldd -r` resolved staged
`libcxdata.so`, `libcxnet.so`, and `libcxcore.so` plus system OpenSSL, with no
unresolved symbols.

The HTTP consumer passed strict static and shared builds and four loopback
cases: plain HTTP, system-trust HTTPS, explicit-CA HTTPS, and wrong-host
rejection. The fixture CA was installed only inside the disposable image. The
shared executable passed with `LD_LIBRARY_PATH` unset; `ldd -r` resolved its
staged consumer, `libcxnet.so`, and `libcxcore.so` plus system OpenSSL, with no
unresolved symbols.

Package SHA-256 values:

- `cxcore.0.1.0.zip`: `57BCE69F46FDDFFFF701BCCAAD64B3F91FB5FB5E9C649F2AC216C625737AD082`
- `cxnet.0.1.0.zip`: `C5A16981590BC552991B7BFEFA90E41E2CA58BFA3BA8674AB788F31742B10E34`
- `cxdata.0.1.0-alpha.zip`: `7F3C1A3101F627122680977D157F19ECD00AC8A27A7593D0627570F365F3B986`

## Current-source package-only HTTPS smoke on Ubuntu and Rocky (2026-11-26)

Restored the current-source `cxcore` and `cxnet` packages into an isolated
consumer stage, emitted the HTTP smoke from its package project reference, and
compiled the stage in a clean Ubuntu 24.04.5 container with GCC 13.3.0, CMake
4.2.1, and `-Wall -Wextra -Werror -Wno-unused-value`. Static and shared builds
passed the plain HTTP control, HTTPS through the system trust store, HTTPS
with the explicit test CA, and wrong-host rejection. The fixture confirms that
the rejected request never reaches the local server. The test CA was installed
into the disposable container trust store for the system-trust case. The
runner clears `LD_LIBRARY_PATH`; `ldd -r` reported no unresolved symbols, and
shared mode resolved the staged consumer, `libcxnet.so`, and `libcxcore.so` with
system OpenSSL. The container had no repository checkout; the compiler was
mounted separately from the isolated stage.

The same current-source stage also passes static and shared builds in a clean
Rocky Linux 9.9 container with GCC 11.5.0, CMake 4.2.1, and the same strict
warnings. Rocky's `/etc/ssl/certs/ca-certificates.crt` is absent while
`/etc/pki/tls/certs/ca-bundle.crt` is readable; with the test CA installed in
Rocky's system trust store, the smoke passed system-trust HTTPS, explicit-CA
HTTPS, wrong-host rejection, and the HTTP control in both modes. This verifies
the packaged handler selects the distro CA bundle when libcurl's compiled
default CA path is unavailable. The runner clears `LD_LIBRARY_PATH`, and
`ldd -r` found no unresolved symbols.

The stage lock file records these package hashes:

- `cxcore.0.1.0.zip`: `57BCE69F46FDDFFFF701BCCAAD64B3F91FB5FB5E9C649F2AC216C625737AD082`
- `cxnet.0.1.0.zip`: `EDF6DE5140F2DE05FA11D021016C80C5E31EC27CE1DC26FF14D52B8C03B2EB3A`

## Current-feed package-only HTTP/HTTPS smoke on AlmaLinux 10.3 and CentOS Stream 10 (2026-11-26)

Restored the fresh current-source archives above into an isolated Linux x64
consumer stage. The strict static and shared builds and HTTP/HTTPS smokes pass
on CentOS Stream 10 (GCC 14.4.1, CMake 3.31.8). The static build and smoke also
pass on AlmaLinux 10.3 (GCC 14.4.1, CMake 3.31.8); the shared executable built
on CentOS Stream 10 also passes its runtime smoke on AlmaLinux 10.3. Both modes
verify plain HTTP, system-trust HTTPS, explicit-CA HTTPS, wrong-host rejection,
and ldd -r with no unresolved symbols. The fixture CA was added only to each
disposable container trust store. The stage had no repository checkout, and
LD_LIBRARY_PATH was unset for the runtime checks.

The current linux-x64 curl archive does not link on Alpine 3.22/musl because
it references glibc-specific symbols. Alpine is not covered by the linux-x64
glibc package target; a musl target would need a separate curl archive.

## Current-source package-only REST/CXDATA JSON on openSUSE Tumbleweed (2026-11-26)

Restored the locked `cxcore`, `cxnet`, and `cxdata` packages into a fresh
Linux x64 consumer stage with no repository checkout. Linux host executables
for the REST and CXDATA generators were built from workspace sources in a
separate disposable container, then staged beside the consumer. In openSUSE
Tumbleweed (GCC 16.2.1, CMake 4.5.0, OpenSSL 3.5.3, .NET 10.0.404), static
and shared builds passed with `-Wall -Wextra -Werror -Wno-unused-value`.
Both executables completed the loopback `GET /package-json`, decoded the
expected DTO, and confirmed exactly one request. `LD_LIBRARY_PATH` was
cleared; `ldd -r` found no unresolved symbols. Shared mode resolved staged
`libcxdata.so`, `libcxnet.so`, and `libcxcore.so` through the generated
`$ORIGIN` layout.

The package lock records these SHA-256 values:

- `cxcore.0.1.0`: `57BCE69F46FDDFFFF701BCCAAD64B3F91FB5FB5E9C649F2AC216C625737AD082`
- `cxnet.0.1.0`: `EDF6DE5140F2DE05FA11D021016C80C5E31EC27CE1DC26FF14D52B8C03B2EB3A`
- `cxdata.0.1.0-alpha`: `7F3C1A3101F627122680977D157F19ECD00AC8A27A7593D0627570F365F3B986`

The same package-only HTTPS smoke passes on Windows with MSVC 19.51 in static
and shared modes for both x86 and x64. Each mode passed the plain HTTP control,
system-trust HTTPS, explicit-CA HTTPS, and wrong-host rejection; the rejected
request did not reach the server. The test CA was added temporarily to the
current-user trust store and removed after each run. The fixture enables
`AllowUnknownCertificateRevocation` because its local certificate has no CRL
endpoint; peer-chain and hostname verification remain enabled. Shared mode
loaded the staged `cxnet.dll` and `cxcore.dll` beside the smoke executable.

## Current-source package-only REST/CXDATA JSON on Windows and Ubuntu (2026-11-25)

Rebuilt the REST and CXDATA generators normally, published current-source
`cxcore`, `cxnet`, and `cxdata` packages to a fresh local feed, and restored
them into an isolated copy of `rest_json/`. The Windows-hosted compiler and
generators emitted the consumer sources and native build files; the clean
Ubuntu 24.04.5 container compiled only the staged consumer and packages with
GCC 13.3.0, CMake 4.2.1, and `-Wall -Wextra -Werror -Wno-unused-value`.
Static and shared builds both passed the loopback `GET /package-json`, decoded
the expected DTO values, and confirmed the server received exactly one
request. `ldd -r` found no unresolved symbols; shared mode resolved the staged
`libcxdata.so`, `libcxnet.so`, and `libcxcore.so` with system OpenSSL.

The same current-source package stage passes on Windows with MSVC 19.51 in
static and shared modes for both x86 and x64. Each executable completed the
same one-request loopback JSON check. Shared mode loaded the staged
`cxcore.dll`, `cxnet.dll`, and `cxdata.dll` beside the consumer.

The stage lock file records these archive hashes:

- `cxcore.0.1.0.zip`: `57BCE69F46FDDFFFF701BCCAAD64B3F91FB5FB5E9C649F2AC216C625737AD082`
- `cxnet.0.1.0.zip`: `EDF6DE5140F2DE05FA11D021016C80C5E31EC27CE1DC26FF14D52B8C03B2EB3A`
- `cxdata.0.1.0-alpha.zip`: `7F3C1A3101F627122680977D157F19ECD00AC8A27A7593D0627570F365F3B986`

## Current-source package-only REST/CXDATA JSON on Fedora 44 (2026-11-25)

Built the staged REST/CXDATA package consumer in a clean Fedora 44 container
(glibc 2.43, GCC 16.2.1, CMake 4.3.0, OpenSSL 3.5.9, Python 3.14.10). The
container mounted the isolated consumer stage and had no repository checkout.
Static and shared Release builds passed with
`-Wall -Wextra -Werror -Wno-unused-value`. Both executables completed the
loopback `GET /package-json`, decoded the DTO, and returned the expected fields;
the smoke runner cleared `LD_LIBRARY_PATH`. `ldd -r` reported no unresolved
symbols. Shared mode resolved the staged `libcxdata.so`, `libcxnet.so`, and
`libcxcore.so` with Fedora's system OpenSSL and compression libraries.

The package SHA-256 values were:

- `cxcore.0.1.0.zip`: `57BCE69F46FDDFFFF701BCCAAD64B3F91FB5FB5E9C649F2AC216C625737AD082`
- `cxnet.0.1.0.zip`: `4E0206799FBB46CE184C20AB8A823254FCB578DB19B2EC37E265B4EC36B4E3D2`
- `cxdata.0.1.0-alpha.zip`: `7F3C1A3101F627122680977D157F19ECD00AC8A27A7593D0627570F365F3B986`

## Current-source package-only HTTPS smoke on Debian 13.7 (2026-11-26)

Restored the current `cxcore` and `cxnet` archives into an isolated Linux x64
consumer stage and ran it in a clean Debian 13.7 container with GCC 14.2.0,
CMake 3.31.6, and strict `-Wall -Wextra -Werror -Wno-unused-value` builds.
Static and shared consumers passed the HTTP control, system-trust HTTPS,
explicit-CA HTTPS, and wrong-host rejection; the failed TLS request did not
reach the local server. The test CA was installed only in the disposable
container. The stage contained no repository checkout, and the runner cleared
`LD_LIBRARY_PATH`. `ldd -r` found no unresolved symbols; shared mode resolved
the staged `libM26Consumer.so`, `libcxnet.so`, and `libcxcore.so` with system
OpenSSL.

The stage lock recorded these archive SHA-256 values:

- `cxcore.0.1.0.zip`: `57BCE69F46FDDFFFF701BCCAAD64B3F91FB5FB5E9C649F2AC216C625737AD082`
- `cxnet.0.1.0.zip`: `EDF6DE5140F2DE05FA11D021016C80C5E31EC27CE1DC26FF14D52B8C03B2EB3A`

## Current-source package-only HTTP consumer on Ubuntu 24.04 (2026-11-25)

Published the current workspace `cxcore` and `cxnet` sources to a fresh local
feed, then restored them into an isolated copy of the package consumer. In a
clean Ubuntu 24.04.5 container, the Linux x64 Release consumer compiled with
`-Wall -Wextra -Werror -Wno-unused-value` using CMake 4.2.1. The container
mounted only the consumer stage and the compiler output; no repository source
checkout was available. The isolated module loader passed with
`LD_LIBRARY_PATH` empty, and `ldd -r` reported no unresolved symbols. The
consumer used static CX package linkage.

The stage lock file records these current-source archive hashes:

- `cxcore.0.1.0.zip`: `57BCE69F46FDDFFFF701BCCAAD64B3F91FB5FB5E9C649F2AC216C625737AD082`
- `cxnet.0.1.0.zip`: `EDF6DE5140F2DE05FA11D021016C80C5E31EC27CE1DC26FF14D52B8C03B2EB3A`

## Current-source package-only REST/CXDATA JSON on CentOS Stream 10 (2026-11-25)

The REST/CXDATA package fixture also passes in a clean CentOS Stream 10
container (glibc 2.39, GCC 14.4.1, CMake 3.31.8, .NET SDK 10.0.113). The
container received only the staged fixture, locked `cxcore`/`cxnet`/`cxdata`
packages, and generated native files; it had no repository checkout. Static
and shared builds pass with `-Wall -Wextra -Werror -Wno-unused-value`. Each
executable completed the loopback `GET /package-json`, decoded the DTO, and
returned the expected field values with `LD_LIBRARY_PATH` unset. `ldd -r`
reported no unresolved symbols; shared mode loaded the staged `libcxdata.so`,
`libcxnet.so`, and `libcxcore.so` alongside CentOS system libraries.

The package SHA-256 values were:

- `cxcore.0.1.0.zip`: `57BCE69F46FDDFFFF701BCCAAD64B3F91FB5FB5E9C649F2AC216C625737AD082`
- `cxnet.0.1.0.zip`: `4E0206799FBB46CE184C20AB8A823254FCB578DB19B2EC37E265B4EC36B4E3D2`
- `cxdata.0.1.0-alpha.zip`: `7F3C1A3101F627122680977D157F19ECD00AC8A27A7593D0627570F365F3B986`

## Current-source package-only REST/CXDATA JSON on Debian 13.7 (2026-11-26)

Restored the current locked `cxcore`, `cxnet`, and `cxdata` packages into a
fresh isolated copy of the REST/CXDATA JSON consumer. In a clean Debian 13.7
container (glibc 2.41, GCC 14.2.0, CMake 3.31.6), static and shared builds
passed using the default CMake build settings and
`-Wall -Wextra -Werror -Wno-unused-value`. Each executable
completed the loopback `GET /package-json`, decoded the expected DTO fields,
and confirmed exactly one request. The container had no repository checkout;
shared mode loaded the staged CX modules and `ldd -r` reported no unresolved
symbols. The runtime check used the container's default empty
`LD_LIBRARY_PATH`.

From the staged `rest_json/` project root, build and run both link modes in a
Debian 13.7 container with the standard C/CMake, OpenSSL, zlib, and Python
packages installed:

```sh
for mode in static shared; do
  if [ "$mode" = static ]; then link=ON; else link=OFF; fi
  build="/tmp/build-$mode"
  cmake -S .obj -B "$build" \
    -DCX_STATIC_LINK="$link" \
    -DCX_BUILD_STATIC="$link" \
    -DCXCORE_SOURCE_DIR="$PWD/.packages/cxcore/0.1.0/source" \
    -DCXCORE_ABI_VERSION=1 \
    -DCMAKE_C_FLAGS='-Wall -Wextra -Werror -Wno-unused-value'
  cmake --build "$build" --parallel 4
  env -u LD_LIBRARY_PATH python3 run_smoke.py .bin/M26PackageRestJsonSmoke
  ldd -r .bin/M26PackageRestJsonSmoke
done
```
The `cxpm.lock` package SHA-256 values were:

- `cxcore.0.1.0`: `57BCE69F46FDDFFFF701BCCAAD64B3F91FB5FB5E9C649F2AC216C625737AD082`
- `cxnet.0.1.0`: `EDF6DE5140F2DE05FA11D021016C80C5E31EC27CE1DC26FF14D52B8C03B2EB3A`
- `cxdata.0.1.0-alpha`: `7F3C1A3101F627122680977D157F19ECD00AC8A27A7593D0627570F365F3B986`

## Current-source package-only HTTP/HTTPS smoke on openSUSE Tumbleweed (2026-11-26)

Published fresh `cxcore` and `cxnet` archives from the current workspace to a
local feed, restored them into an isolated Linux x64 consumer stage, and built
the HTTP smoke in a clean openSUSE Tumbleweed container (GCC 16.2.1, CMake
4.5.0, OpenSSL 3.5.3, .NET 10.0.404). The strict build used
static and shared CX package linkage and `-Wall -Wextra -Werror
-Wno-unused-value`. Both package-only executables passed plain HTTP,
system-trust HTTPS, explicit-CA HTTPS, and wrong-host rejection. The test CA
was installed only in the disposable container's trust store. The stage had
no repository checkout, `LD_LIBRARY_PATH` was cleared, and `ldd -r` found no
unresolved symbols in either mode. Shared mode loaded the staged consumer,
`cxnet`, and `cxcore` libraries through the generated `$ORIGIN` layout.

The stage lock recorded these current-source archive SHA-256 values:

- `cxcore.0.1.0.zip`: `57BCE69F46FDDFFFF701BCCAAD64B3F91FB5FB5E9C649F2AC216C625737AD082`
- `cxnet.0.1.0.zip`: `EDF6DE5140F2DE05FA11D021016C80C5E31EC27CE1DC26FF14D52B8C03B2EB3A`

## Current-source package-only HTTP/HTTPS smoke on Fedora 44 (2026-11-26)

Restored the current locked `cxcore` and `cxnet` archives into the isolated
Linux x64 consumer stage, then compiled the package-generated HTTP smoke inside
a clean Fedora 44 container (glibc 2.43, GCC/G++ 16.2.1, CMake 4.3.0,
OpenSSL 3.5.9). Both static and shared builds passed with
`-Wall -Wextra -Werror -Wno-unused-value`. The test CA was installed only in
the disposable container trust store. The HTTP control, system-trust HTTPS,
explicit-CA HTTPS, and wrong-host rejection all passed; the wrong-host request
did not reach the server. The stage contained no repository checkout, and the
runner cleared `LD_LIBRARY_PATH`. `ldd -r` found no unresolved symbols; shared
mode resolved the staged `libM26Consumer.so`, `libcxnet.so`, and `libcxcore.so`
with Fedora's system OpenSSL.

The `cxpm.lock` package SHA-256 values were:

- `cxcore.0.1.0.zip`: `57BCE69F46FDDFFFF701BCCAAD64B3F91FB5FB5E9C649F2AC216C625737AD082`
- `cxnet.0.1.0.zip`: `EDF6DE5140F2DE05FA11D021016C80C5E31EC27CE1DC26FF14D52B8C03B2EB3A`

## Current-source package-only REST/CXDATA JSON on Fedora 44 (2026-11-26)

Rebuilt the current locked package-only REST/CXDATA JSON consumer in a clean
Fedora 44 container (glibc 2.43, GCC 16.2.1, CMake 4.3.0, OpenSSL 3.5.9).
The consumer stage was mounted read-only and had no repository checkout. Strict
static and shared builds passed with `-Wall -Wextra -Werror -Wno-unused-value`.
Both executables completed the loopback `GET /package-json`, decoded the
expected DTO, and confirmed exactly one request. `LD_LIBRARY_PATH` was empty
during runtime checks; `ldd -r` found no unresolved symbols, and shared mode
loaded the staged `libcxdata.so`, `libcxnet.so`, and `libcxcore.so` with Fedora's
system OpenSSL.

The `cxpm.lock` package SHA-256 values were:

- `cxcore.0.1.0.zip`: `57BCE69F46FDDFFFF701BCCAAD64B3F91FB5FB5E9C649F2AC216C625737AD082`
- `cxnet.0.1.0.zip`: `4E0206799FBB46CE184C20AB8A823254FCB578DB19B2EC37E265B4EC36B4E3D2`
- `cxdata.0.1.0-alpha.zip`: `7F3C1A3101F627122680977D157F19ECD00AC8A27A7593D0627570F365F3B986`

## Current-source package-only REST/CXDATA JSON on AlmaLinux 10.3 (2026-11-26)

Rebuilt the staged package-only REST/CXDATA JSON consumer in a clean AlmaLinux
10.3 container (glibc 2.39, GCC 14.4.1, CMake 3.31.8). The consumer stage was
mounted read-only and contained only the generated project, locked packages,
and generator artifacts; no repository checkout was available. Static and
shared builds passed with `-Wall -Wextra -Werror -Wno-unused-value`. Both
executables completed the loopback `GET /package-json`, decoded the expected
DTO, and the runner confirmed exactly one request. `LD_LIBRARY_PATH` was empty
for runtime checks; `ldd -r` found no unresolved symbols, and shared mode loaded
the staged `libcxdata.so`, `libcxnet.so`, and `libcxcore.so` with system
OpenSSL.

The locked package SHA-256 values were:

- `cxcore.0.1.0.zip`: `57BCE69F46FDDFFFF701BCCAAD64B3F91FB5FB5E9C649F2AC216C625737AD082`
- `cxnet.0.1.0.zip`: `4E0206799FBB46CE184C20AB8A823254FCB578DB19B2EC37E265B4EC36B4E3D2`
- `cxdata.0.1.0-alpha.zip`: `7F3C1A3101F627122680977D157F19ECD00AC8A27A7593D0627570F365F3B986`

## Refreshed current-source HTTP and REST package checks (2026-11-27)

Published `cxcore`, `cxnet`, and `cxdata` from the current workspace into a new
local feed, then restored them into isolated package-only stages. The Linux
x64 HTTP consumer passes static and shared builds and all four loopback cases:
plain HTTP, system-trust HTTPS, explicit-CA HTTPS, and wrong-host rejection.
Linux builds use strict warnings; `ldd -r` reports no unresolved symbols, and
the shared consumer resolves the staged CX libraries. The fixture CA is
installed only inside the disposable container. A checkout-free REST/CXDATA
JSON consumer also builds and passes its local wire smoke in static and shared
modes on Linux x64 (Ubuntu 24.04 SDK container) and Windows x86/x64. The Windows
x86 build uses the Win32 generator platform; both CMake caches point
`CXCORE_SOURCE_DIR` into the restored package. Shared Windows mode stages and
loads `cxcore.dll`, `cxdata.dll`, and `cxnet.dll` with the executable. Linux
shared mode passes with `LD_LIBRARY_PATH` unset, and `ldd -r` resolves staged
`libcxdata.so`, `libcxnet.so`, and `libcxcore.so` plus system OpenSSL.
The Linux REST stage also passes both modes in a clean Ubuntu 26.10 container
(glibc 2.44, GCC 15.3.0, CMake 4.3.4, OpenSSL 4.0.3), using strict warnings.
Shared execution passed with `LD_LIBRARY_PATH` unset; `ldd -r` found no
unresolved symbols and resolved the staged CX modules and system OpenSSL.
The HTTP consumer also passes both modes on Ubuntu 26.10 with strict warnings.
It verifies plain HTTP, system-trust HTTPS, explicit-CA HTTPS, and wrong-host
rejection. Its shared executable ran with `LD_LIBRARY_PATH` unset, and
`ldd -r` resolved the staged `libcxnet.so` and `libcxcore.so` plus OpenSSL 4.

Fresh archive SHA-256 values:

- `cxcore.0.1.0.zip`: `57BCE69F46FDDFFFF701BCCAAD64B3F91FB5FB5E9C649F2AC216C625737AD082`
- `cxnet.0.1.0.zip`: `C5A16981590BC552991B7BFEFA90E41E2CA58BFA3BA8674AB788F31742B10E34`
- `cxdata.0.1.0-alpha.zip`: `7F3C1A3101F627122680977D157F19ECD00AC8A27A7593D0627570F365F3B986`

## Current-source package-only REST/CXDATA JSON on Rocky Linux 9.9 (2026-11-26)

Rebuilt the staged package-only REST/CXDATA JSON consumer in a clean Rocky
Linux 9.9 container (glibc 2.34, GCC 11.5.0, CMake 4.2.1). The read-only
stage contained the generated consumer, lock file, package sources, and
artifacts, with no repository checkout. Static and shared builds passed using
`-Wall -Wextra -Werror -Wno-unused-value`. Both executables completed the
loopback `GET /package-json`, decoded the expected DTO, and confirmed exactly
one request. `LD_LIBRARY_PATH` was empty during runtime checks; `ldd -r` found
no unresolved symbols, and shared mode loaded the staged `libcxdata.so`,
`libcxnet.so`, and `libcxcore.so` with Rocky's system OpenSSL.

The locked package SHA-256 values were:

- `cxcore.0.1.0.zip`: `57BCE69F46FDDFFFF701BCCAAD64B3F91FB5FB5E9C649F2AC216C625737AD082`
- `cxnet.0.1.0.zip`: `4E0206799FBB46CE184C20AB8A823254FCB578DB19B2EC37E265B4EC36B4E3D2`
- `cxdata.0.1.0-alpha.zip`: `7F3C1A3101F627122680977D157F19ECD00AC8A27A7593D0627570F365F3B986`

## Current-source package-only REST/CXDATA JSON on Debian 12 (2026-11-26)

Rebuilt the current locked package-only REST/CXDATA JSON consumer in a clean
Debian 12 container (glibc 2.36, GCC 12.2.0, CMake 4.2.1, OpenSSL 3.0.17).
The consumer stage was mounted read-only and had no repository checkout. Strict
static and shared builds passed with `-Wall -Wextra -Werror -Wno-unused-value`.
Both executables completed the loopback `GET /package-json`, decoded the
expected DTO, and confirmed exactly one request. `LD_LIBRARY_PATH` was empty
during runtime checks; `ldd -r` found no unresolved symbols, and shared mode
loaded the staged `libcxdata.so`, `libcxnet.so`, and `libcxcore.so` with Debian's
system OpenSSL.

The locked package SHA-256 values were:

- `cxcore.0.1.0.zip`: `57BCE69F46FDDFFFF701BCCAAD64B3F91FB5FB5E9C649F2AC216C625737AD082`
- `cxnet.0.1.0.zip`: `4E0206799FBB46CE184C20AB8A823254FCB578DB19B2EC37E265B4EC36B4E3D2`
- `cxdata.0.1.0-alpha.zip`: `7F3C1A3101F627122680977D157F19ECD00AC8A27A7593D0627570F365F3B986`

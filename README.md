# cxnet

`cxnet` is the networking library for the CX programming language. It provides
TCP sockets, asynchronous DNS and network streams, an HTTP/HTTPS client, and a
compile-time generator for interface-based REST clients.

The package manifest is [`src/cxnet.cxproj`](src/cxnet.cxproj), and the project
is distributed under the [MIT License](LICENSE).

## Features

- IPv4 and IPv6 TCP sockets with bind, listen, accept, connect, shutdown, and
  asynchronous reads and writes.
- `NetworkStream` integration with the CX stream and task APIs.
- HTTP/1.1 and HTTPS through `HttpClient`, including streaming request and
  response bodies, cancellation, timeouts, redirects, proxy settings, cookies,
  connection reuse, and content decompression.
- REST clients generated from CX interfaces at compile time. Contracts support
  routes, path/query/header bindings, request bodies, forms and multipart
  content, JSON mapping, and typed response wrappers.
- Native transport dependencies and platform backends included with the
  package. The library depends on `cxcore`.

The REST JSON integration uses the separate `cxdata` package and its JSON
generator. Raw sockets and HTTP use do not require `cxdata`.

## Supported targets

| Target | Architecture | Native toolchain |
| --- | --- | --- |
| Windows | x86, x64 | MSVC |
| Linux | x64 | GCC or Clang |

Static and shared linking are supported. The Linux HTTP transport uses the
system OpenSSL libraries at runtime. Linux builds require CMake 3.31 or newer,
a C toolchain, and OpenSSL development files. Windows builds require CMake and
the Visual C++ toolchain; Windows hosts need the Visual C++ runtime.

## Install

Add `cxnet` to your project's dependencies and configure the package feed used
by your organization:

```yaml
dependencies:
  - cxnet@^0.1.0
```

Restore and compile for the target selected by your project:

```sh
cxpm restore --target linux-x64 --project app.cxproj
cxc app.cxproj --compile --configuration Release
```

Use `win-x64` or `win-x86` for Windows. `cxcore` is restored as a transitive
dependency.

For REST contracts that exchange JSON models, add `cxdata` as a dependency and
configure the REST and CXDATA JSON generators. The
[REST CRUD example](examples/rest-crud/README.md) shows the project setup.

```yaml
dependencies:
  - cxnet@^0.1.0
  - cxdata@^0.1.0-alpha
```

## Examples

- [TCP echo](examples/tcp-echo/README.md): opens a loopback listener, connects
  a client, and exchanges data through `Socket` and `NetworkStream`.
- [HTTP download](examples/http-download/README.md): streams a response to a
  file and verifies the downloaded bytes against a local fixture.
- [REST CRUD](examples/rest-crud/README.md): generates a typed client from an
  interface and exercises create, read, update, and delete calls.

## Build from source

When using the full `cx-pl` workspace, build the compiler and then compile the
library project from the workspace root:

```powershell
dotnet build cxc/CxCompiler.slnx
dotnet run --project cxc/src/CxCompiler/CxCompiler.csproj -- --compile `
  --cxcore-dir cxcore cxnet/src/cxnet.cxproj
```

See the [networking design and API notes](docs/networking-design.md) for
behavior and ownership details. The
[Linux package consumer guide](tests/package_consumer/README.md) and
[Windows package consumer guide](tests/package_consumer_windows/README.md)
describe package restore, isolated builds, and runtime loading.

## License

This project is licensed under the [MIT License](LICENSE).

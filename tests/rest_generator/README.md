# REST generator fixture

Build the REST and CXDATA generators, then build and run the fixture with
`cxc`:

```powershell
dotnet cxc/src/CxCompiler/bin/Release/net10.0/CxCompiler.dll `
  cxnet/generators/rest/CxnetRestGenerator.cxproj --compile --configuration Release
dotnet cxc/src/CxCompiler/bin/Release/net10.0/CxCompiler.dll `
  cxdata-json-generator/src/JsonGenerator.cxproj --compile --configuration Release
dotnet cxc/src/CxCompiler/bin/Release/net10.0/CxCompiler.dll `
  cxnet/tests/rest_generator/rest-generator.cxproj --compile --configuration Release
& cxnet/tests/rest_generator/.bin/Release/RestGeneratorFixture.exe
```

Run the worker lookup regression tests after building the REST generator:

```powershell
python cxnet/tests/rest_generator_lookup_test.py -v
```

These tests exercise generic identity aliases, whitespace normalization, first-match
precedence, missing constructions, duplicate annotations, and unrelated generic
types. See [the profiling results](../../generators/rest/PROFILE_RESULTS.md) for
the performance measurements and output-equivalence checks. The full fixture's
REST worker has a 180-second timeout to catch large performance regressions.

The fixture also verifies ownership of streamed `RestResponse<Stream>` results:
it reads a body prefix, leaves a second read pending, disposes the wrapper, and
checks that the pending read is canceled. Its recording-handler checks also
race `SetHasValue` and `SetErrorContent` with wrapper disposal, accepting
successful writes before disposal or `ObjectDisposedException` afterward.
The rebuilt Windows x64 Release fixture passes its live-wire run and 20
consecutive standalone runs. The updated fixture also passes 20 standalone
runs and a live-wire run in Linux x64 GCC shared, plus 20 standalone runs in
GCC static and Clang static/shared. Linux shared `ldd -r` resolves dependencies.
This covers the setter/disposal race across Windows x86/x64 and Linux x64
static/shared, with Linux GCC and Clang.

Closed generic REST contracts also cover a generic `Task<T>` result specialized
to a serializable DTO and a `Task<RestResponse<T>>` result specialized to the
same DTO. The recording handler verifies route substitution, DTO materialization,
wrapper value presence, and response-content disposal. The live-wire fixture
also verifies direct and wrapped results over the default HTTP transport,
including response status and header preservation. Recording and live-wire
checks pass in all eight Windows x86/x64 MSVC and Linux x64 GCC/Clang
static/shared cells.

The `RestModels` project is a separate referenced library. The fixture derives
REST interfaces from its non-generic and closed-generic base contracts, invokes
the inherited methods through generated implementations, and checks the
referenced routes, generic query encoding, and materialized response. The
`cxnet_managed_rest_generator_recording` CTest passes in all eight Windows
x86/x64 MSVC and Linux x64 GCC/Clang static/shared cells; the current Linux
builds use strict warnings.

JSON collection responses also decode non-null scalar and string elements.
The recording fixture checks `const Array<int>`, `const Array<string>`, and
`const List<int>` in direct and `RestResponse<T>` results, including escaped
strings, wrapper status and headers, and response disposal ownership. The live
wire fixture checks direct and wrapped populated numeric values for
`Array<int>`, `List<int>`, `LinkedList<int>`, `Queue<int>`, and `Stack<int>`,
preserving sequence order. Direct and wrapped `const Array<int>` and
`const List<int>` results are checked on the live wire. Direct and wrapped
`const Array<string>`, `const List<string>`, `const LinkedList<string>`,
`const Queue<string>`, and `const Stack<string>` results verify escaped quotes,
newlines, and sequence order. Wrapper checks include status, headers, and
disposal. The wire fixture also exercises `List<DTO>` results. These cases pass Windows
x86/x64 and Linux x64 static/shared cells, plus strict Linux GCC and Clang
static/shared builds. Collection contracts use the standard CX collection
types with `const`.

JSON collection and map results preserve nullable elements and values. The
recording fixture checks direct `const List<RestItem?>` and
`const List<string?>` results, wrapped `const List<int?>` and
`const Array<string?>` results, and direct
`const Dictionary<string, RestItem?>` and
`const Dictionary<string, string?>` results, including null placement,
populated entries, escaped strings, wrapper status/headers, and response-content
ownership. The
`cxnet_managed_rest_generator_recording` CTest passes in all eight Windows
x86/x64 MSVC and Linux x64 GCC/Clang static/shared cells. It also checks direct
and wrapped nullable DTO results for `const List<RestItem?>`,
`const Array<RestItem?>`, `const LinkedList<RestItem?>`,
`const Queue<RestItem?>`, and `const Stack<RestItem?>` through the recording
handler and live-wire fixture. Each response contains first and last DTOs
around a null element; wrappers also check status, headers, and disposal.
Existing nullable string-array and map cases remain covered.
Recording and live-wire runs pass in all eight Windows x86/x64 MSVC and Linux
x64 GCC/Clang static/shared cells.

Direct nullable outer collection results such as
`Task<const List<RestItem>?>` accept a JSON `null` root and complete with a null
collection. The live-wire case covers the null root; recording-handler cases
cover populated results. This coverage also includes `const Array<RestItem>?`,
`const LinkedList<RestItem>?`, `const Queue<RestItem>?`, and
`const Stack<RestItem>?`. Direct live-wire calls verify JSON null for all five
collection types, while recording calls verify populated direct results. Wrapped
nullable outer results such as
`Task<RestResponse<const List<RestItem>?>>` keep `HasValue == true` for a JSON
`null`, return null from `GetValue()`, and retain response content until wrapper
disposal. Recording checks cover null and populated results for all five
collection types, including response-content ownership through disposal; the
live-wire fixture covers null responses. The recording CTest and live-wire
fixture pass in all eight Windows x86/x64 MSVC and Linux x64 GCC/Clang
static/shared cells, including strict Linux builds.
The fixture now also verifies a wrapped `const Dictionary<string, string?>`
JSON result on the live wire. Its response includes a populated value, a null
value, and an explicitly empty string; the recording case additionally checks
status, headers, and that response content remains owned by the wrapper until
disposal. Recording and live-wire runs pass in all eight cells.

Nullable numeric JSON collections also have live-wire results for direct
`const Array<int?>`, `const List<int?>`, `const LinkedList<int?>`,
`const Queue<int?>`, and `const Stack<int?>`, plus wrapped `const Array<int?>`,
`const List<int?>`, `const LinkedList<int?>`, `const Queue<int?>`, and
`const Stack<int?>`. Each sequence contains positive and negative values around
a null element. Recording and live-wire checks verify wrapper metadata and
content ownership through disposal.
Direct and wrapped JSON maps also cover nullable `List<int?>` values with
populated, empty, and null entries; wrapper cases check metadata and response
ownership through disposal. Recording and wire fixtures pass in all eight
Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared cells, with strict
Linux warning flags.

Nullable floating-point JSON collection results also cover direct and wrapped
`const Array<T?>`, `const List<T?>`, `const LinkedList<T?>`,
`const Queue<T?>`, and `const Stack<T?>` responses for both `float` and
`double`. Each sequence keeps a null between positive and negative values;
wrapped cases parse exponent notation and check status, headers, and
response-content ownership through disposal. Recording and live-wire runs pass
in all eight Windows x86/x64 MSVC and Linux x64 GCC 13.3/Clang 18.1.3
static/shared cells, with strict Linux warning flags.

The HTTP backend CTest configuration also builds the generated fixture with a
test-only continuation-registration injector. Its
`cxnet_managed_rest_generated_result_continuation_registration_failure` case
invokes generated `Task<string>` and JSON DTO `Task<RestItem>` routes, injects
failure after request-cleanup registration, and checks that each method returns
a faulted task. The case passes Windows x86/x64 MSVC and Linux x64 GCC/Clang
static/shared builds.

The optional wire run starts a local HTTP/1.1 server and sends several requests
through the generated contract and the default `HttpClient` transport:

```powershell
python cxnet/tests/rest_wire_fixture.py `
  cxnet/tests/rest_generator/.bin/Release/RestGeneratorFixture.exe
```

The fixture checks generated GET and JSON POST routes with escaped path and
query values, including a generated query placed before a route fragment (the
recording handler checks the managed URI and the server confirms the fragment
is omitted from the HTTP request target), repeated query values, dictionary query maps with escaped keys,
repeated values, preserved insertion order, and omission of empty collections,
URL-encoded forms with repeated `role` values supplied as `ICollection<string>`,
`IEnumerable<string>`, `string[]`, `List<string>`, `LinkedList<string>`,
`Queue<string>`, and `Stack<string>`. The server checks the encoded values and
their enumeration order, including FIFO queue order and top-first stack order.
A const-qualified `IEnumerable<string>` and `ICollection<string>` are also
checked for repeated query, header, URL-encoded form, and multipart values. The
generated implementation signatures preserve `const`, and the methods enumerate
the standard collection interfaces directly. Recording-handler and wire runs
pass on Windows x86/x64 and Linux x64 with static and shared linking; strict
Linux GCC and Clang 14 builds also pass in both link modes with
`-Wall -Wextra -Werror -Wno-unused-value`. Immutable collection contracts use
the standard collection types with `const`.
Repeated unsigned query collections also have live-wire coverage: a
`const Array<ulong>` sends zero, `ulong.MaxValue`, and a trailing value as
repeated `value` parameters, preserving order and invariant decimal formatting.
This case passes all eight Windows x86/x64 MSVC and Linux x64 GCC 14.2/Clang
19 static/shared cells; both Linux compiler families use strict warning flags.
The nullable variant sends the same boundary values through
`const Array<ulong?>`, with a null between zero and `ulong.MaxValue`; the wire
check confirms null omission and preserves order in the same eight cells.
The live wire fixture verifies generated query placement before URI fragments
and canonical UUID path/scalar-result formatting. Those cases pass in all eight
Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared cells; Linux GCC and
Clang use strict warning builds.
A `const Dictionary<string, List<string>>` query map is also checked for the
same escaped, repeated values, and insertion order in all six Windows x86/x64
and Linux x64 static/shared wire cells. Nullable scalar form fields are checked
for absent-value omission and populated invariant formatting in the same matrix.
These nullable scalar and const query-map cases also pass strict Linux GCC and
Clang 14 static/shared builds with
`-Wall -Wextra -Werror -Wno-unused-value`.
Const query maps with integer scalar values and `List<int>` sequence values
also have live-wire checks for invariant formatting, escaped keys, insertion
order, and empty-sequence omission. Recording and wire runs pass in all eight
Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared cells.
The recording handler also checks direct and wrapped numeric JSON results for
`const LinkedList<int>`, `const Queue<int>`, and `const Stack<int>`, including
sequence order, wrapper status/header, and response-content ownership. The
recording CTest passes in all eight Windows and Linux GCC/Clang static/shared
cells.
Direct and wrapped DTO recording results now also cover `const List<RestItem>`,
`const Array<RestItem>`, `const LinkedList<RestItem>`, `const Queue<RestItem>`,
and `const Stack<RestItem>`, checking DTO fields, order, wrapper metadata, and
response-content ownership in the same eight cells.
A numeric repeated form-field route checks `int[]` formatting and element order
alongside a scalar boolean, expecting `quantity=5&quantity=-2&active=False`.
This passes in all six Windows x86/x64 and Linux x64 static/shared cells, plus
strict Linux GCC and Clang static/shared builds.
The standard generic `Array<int>` collection is also covered through a
`const Array<int>` form parameter and must produce the same ordered repeated
fields. Additional `const Array<string>` wire cases check escaped repeated
query values, repeated header order, and multipart field order. These generic
array cases pass in all six Windows x86/x64 and Linux x64 static/shared cells,
plus strict Linux GCC and Clang static/shared builds.
Repeated query formatting is also verified for numeric scalar collections: an
`int[]` emits `value=1&value=42&value=-3` on the wire in all six Windows x86/x64
and Linux x64 static/shared cells, plus strict Linux GCC and Clang 14.0.6
static/shared builds.
The scalar query route also checks invariant boolean, floating-point, and
decimal formatting: `enabled=True&ratio=-1.25&amount=0.125` passes in the same
six target cells and strict Linux static/shared builds.
Linked-list, queue, and stack query/header sequences, form fields, multipart
parts, and collection-valued dictionary query maps are also checked on the wire.
The generic `Array<T>` coverage includes `const Array<string>` query, header,
and multipart bindings.
An `Array<string>` query-map value also preserves repeated value order and
escapes the map key, producing
`/wire/array-map?tag%20name=first%20item&tag%20name=x%26y` in the same matrix.
Repeated multipart fields are verified for `IEnumerable<string>`, arrays,
`List<string>`, `ICollection<string>`, `LinkedList<string>`, queues, and stacks
across all six Windows x86/x64 and Linux x64 static/shared cells, plus strict
Linux GCC and Clang 14.0.6 static builds. A multipart `HttpContent` part also
verifies its filename, payload, and media type on the wire.
Multipart scalar parts now format numeric and boolean values invariantly, and
scalar sequences repeat parts in order. The fixture checks a `count=7` integer,
`active=False` boolean, and an ordered `const List<int>` producing quantity
parts `5` and `-2`; these cases pass in all six Windows x86/x64 and Linux x64
static/shared cells, plus strict Linux GCC and Clang static/shared builds.
Nullable string and scalar multipart parts omit null values, retain an explicitly
empty string part, and format populated integer/decimal values. These wire cases
pass in the same six cells and strict Linux GCC/Clang static/shared builds.
Nullable numeric query and header sequences also omit null elements and preserve
invariant formatting: `List<int?>` produces `value=7&value=-3`, while
`Array<decimal?>` emits the populated header values `0.125` and `-2.5`. The
recording and live wire fixtures pass in all six Windows x86/x64 and Linux x64
static/shared cells. Linux GCC and Clang also pass strict static/shared builds
with `-Wall -Wextra -Werror -Wno-unused-value`.
Additional nullable numeric checks cover query/header sequences through const
arrays, lists, linked lists, queues, stacks, `IEnumerable<T>`, and
`ICollection<T>`, plus FIFO `Queue<int?>` and ordered `LinkedList<decimal?>`
form fields, top-first `Stack<bool?>` form fields, and top-first
`Stack<decimal?>` multipart parts. Null elements are omitted; populated values
preserve order and invariant formatting. Recording and wire checks pass in all
eight Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared cells,
including strict Linux GCC and Clang static/shared builds.
Nullable Boolean sequences now also cover `const Queue<bool?>` query values and
`const Stack<bool?>` header values. The recording and live-wire fixtures verify
null omission, FIFO/top-first order, and invariant `True`/`False` formatting in
all eight Windows/Linux x86/x64 static/shared cells; Linux GCC and Clang use
strict warnings.
It also checks JSON
DTO materialization, a 409 `RestResponse<string>`, a generated 96 KiB
`Task<byte[]>` response, a `RestResponse<RestItem>` response, a streamed
256 KiB request, generated PUT/PATCH/DELETE/HEAD/OPTIONS methods on the wire, and a
delayed 32 KiB `Task<RestResponse<Stream>>` response. It also overlaps slow and fast generated
`Task<string>` calls on one REST client; the server holds the slow route until
the fast request arrives, and each task must return its own response. The
fixture also calls two same-name overloaded methods on the same route, one with
a numeric `id` query and one with a string `name` query; the server checks that
the generated overloads emit the matching request targets and responses. These
overload cases pass in all six Windows x86/x64 and Linux x64 static/shared
cells, plus strict Linux GCC and Clang 14.0.6 static/shared builds. The upload
body crosses the transport ring; a separate generated `@Body` string call sends
an empty value and checks that it has zero body bytes, `Content-Length: 0`, no
transfer encoding, and the UTF-8 text media type.
Nullable DTO results support both `Task<RestItem?>` and
`Task<RestResponse<RestItem?>>`. Direct JSON `null` with surrounding whitespace
returns null; a wrapped JSON `null` retains status and headers with
`HasValue == true`. Populated DTO results still materialize normally, while
empty-body behavior retains the existing direct-error and wrapper-no-value
semantics. Recording-handler and live-wire checks pass in all six Windows
x86/x64 and Linux x64 static/shared cells, plus strict Linux GCC and Clang 14
static/shared builds.
DTO collection results preserve their declared `List<T>`, `Array<T>`,
`LinkedList<T>`, `Queue<T>`, or `Stack<T>` shape, including `const` results and
`RestResponse` wrappers. Stack materialization pushes items in reverse JSON
order so top-first enumeration matches the response array. Recording checks
cover mutable and const `Array<RestItem>` materialization; live-wire checks
cover direct and wrapped const arrays, linked lists, queues, and stacks in all
six Windows x86/x64 and Linux x64 static/shared cells, plus strict Linux GCC
and Clang static/shared builds.
The recording-handler and live-wire fixtures send a generated `@Body const
byte[]` request with zero and non-ASCII values and check every byte. The server
also verifies that the request has no content type by default; the generated
request uses `ByteArrayContent`. Recording-handler and live-wire checks pass on
Windows x86/x64 static/shared builds. The recording-handler check also passes
on Linux x64 shared, and the live-wire check passes all six supported Windows
x86/x64 and Linux x64 static/shared cells, including strict Linux GCC
static/shared builds.
Nullable string `@Body` parameters omit content when null; an empty string
still creates zero-length UTF-8 text content. Recording and live-wire fixtures
check both cases in all six Windows x86/x64 and Linux x64 static/shared cells,
including strict Linux GCC static/shared builds.
Nullable `@Body const byte[]?` parameters also distinguish null (no content)
from an empty array (`ByteArrayContent` with zero bytes and no media type).
The recording fixture checks both states, and the wire fixture verifies the
exact bytes for a populated nullable array in all six Windows x86/x64 and Linux
x64 static/shared cells, including strict Linux GCC static/shared builds.
Nullable DTO `@Body` parameters now follow the same omission rule: null creates
no content, while a non-null DTO uses its generated JSON codec and
`application/json`. The recording-handler and live-wire fixture check both
states; recording and wire runs pass in all six Windows x86/x64 and Linux x64
static/shared cells, plus strict Linux GCC and Clang 14 static/shared builds.
Nullable `HttpContent?` body coverage is in the focused
`cxnet/tests/rest_optional_content` fixture. It verifies unset request content
for null and exact payload preservation for supplied content on the live wire.
the response server waits for a second request before sending body bytes, which
proves the generated `Task<RestResponse<Stream>>` completes from headers first. The fixture
then verifies every response byte through asynchronous reads to EOF. The server
validates request targets and serialized request bodies, multipart boundary and
part framing, stream bytes and order, media types, plus generated static, dynamic, and
authorization headers. A generated collection-valued `@Header` sends repeated
`X-Wire-Tag` fields; the server verifies both values and their order. The error
wrapper checks status, response headers,
bounded body text, and the absence of a value.
The live wire fixture also returns a 5,000-character error body for a DTO
method and verifies `ApiException.ResponseStatusCode`, request URI, retained
response headers, and the 4,096-character error bound.
Malformed UTF-8 error routes are also declared for direct DTO and streamed
results and for `RestResponse<DTO>` and `RestResponse<Stream>` wrappers. The
wrapper cases check status, headers, no materialized value, and empty error
text. They pass in Windows x86/x64 static/shared and Linux x64 GCC/Clang
static/shared builds; Linux builds use strict warnings.
Scalar result routes cover `Task<int>`, `Task<bool>`, `Task<long>`, and
`Task<RestResponse<long>>`, including a maximum `long`, wrapped success status
and headers, an empty 204 wrapper, a wrapped 409 error body with no value, and
rejection of an invalid numeric JSON body. These cases pass in all six Windows
x86/x64 and Linux x64 static/shared cells, and in strict Linux GCC and Clang
static/shared builds.
Additional live-wire routes now cover `Task<double>` with a negative fractional
value and `Task<RestResponse<decimal>>` with a precise fractional value and
response header. The generated fixture and local wire server pass in all six
Windows x86/x64 and Linux x64 static/shared cells; strict GCC builds also pass
on Linux x64 in both link modes.
The scalar wire routes also cover `Task<RestResponse<bool>>`, checking the
materialized `false` value while `HasValue` remains true, success status, and a
retained response header. This distinguishes the default scalar value from an
empty 204 wrapper. The generated fixture and local wire server pass in all six
Windows x86/x64 and Linux x64 static/shared cells.
Direct JSON scalar results now also cover `byte`, `sbyte`, `short`, `ushort`,
`uint`, `ulong`, and `float`; a wrapped `RestResponse<ulong>` checks the maximum
unsigned 64-bit value, status, header, and ownership. This exposed an emitted
`TryParse` fallback typed as `int`; the generator now emits the matching
zero-value type for small integer results. Direct and wrapped scalar live-wire
cases cover `byte`, `sbyte`, `short`, `ushort`, `uint`, `ulong`, and `float`,
including each type's boundary value and wrapper status/header/disposal checks.
They pass all eight Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared
cells. Recording, multipart-setup-failure, and generated-result
continuation-registration CTests pass those same eight cells.
Nullable scalar wire routes cover direct integer and boolean results for JSON
`null` (including surrounding JSON whitespace) and populated values, plus
`RestResponse<decimal?>` for JSON `null`, a
populated decimal, 204, and an HTTP error. They verify null versus present
values, wrapper `HasValue`, status, retained headers, and bounded error content.
These routes pass in all six Windows x86/x64 and Linux x64 static/shared cells,
plus strict Linux GCC and Clang static/shared builds.
The live stream wrapper preserves status and content headers, returns before
the delayed body begins, retains response ownership through asynchronous reads,
and disposes the body with the wrapper. A separate 204 case preserves status
with `HasValue == false`. Both pass in all six Windows x86/x64 and Linux x64
static/shared cells, plus strict Linux GCC and Clang static/shared builds. A
wrapper-disposal case cancels a pending body read and confirms the server sees
the client connection close in the same matrix.
Its 204 routes verify that `RestResponse<RestItem>` retains the no-content
status/header without a value, while a direct DTO result faults on the absent
JSON body. These wire cases pass all six Windows x86/x64 and Linux x64
static/shared cells, plus strict Linux Clang 14.0.6 static/shared builds. The
LinkedList, Queue, and Stack bindings also pass strict Linux GCC and Clang
14.0.6 static/shared builds.
Nullable query/header parameters are also checked on the wire: null omits both,
an empty query string remains `q=`, and an empty header value is transmitted as
an explicitly present empty field. Nullable numeric query/header parameters
are checked both when omitted and when populated, including invariant decimal
formatting.
Nullable sequence elements are omitted across query, header, form, and multipart
bindings while escaped and empty values retain their order. The recording-handler
and live wire cases pass on Windows x86/x64 and Linux x64 with static and shared
linking, including strict Linux GCC and Clang static/shared builds. Nullable
query-map recording checks now verify that null scalar entries are omitted,
empty strings remain present, null elements and null or empty collection values
are omitted, and a null whole map adds no query. The recording fixture passes
Windows x86/x64 and Linux x64 GCC/Clang static/shared builds. Its direct run is
registered as `cxnet_managed_rest_generator_recording` in the HTTP backend
CTest suite. The live wire fixture checks the resulting exact request targets
and passes Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared builds.
`char` wire bindings are checked with a UTF-8 escaped path value, scalar and
repeated query values, and a single-Unicode-scalar JSON response. Repeated
`const List<char>` form and multipart values preserve order and UTF-8 encoding.
These cases pass in all six Windows x86/x64 and Linux x64 static/shared cells.
Nullable `char` JSON results also distinguish null from a populated
supplementary Unicode scalar for direct `Task<char?>` and wrapped
`Task<RestResponse<char?>>` routes. Wrapper cases preserve status and headers;
recording and live-wire checks pass in all eight Windows x86/x64 MSVC and Linux
x64 GCC/Clang static/shared cells.
The recording-handler fixture verifies `RestClient` ownership: disposing a
wrapper leaves a borrowed `HttpClient` usable and closes an owned one.
It also forces generated request setup to fail after attaching `StreamContent`
by supplying a CRLF dynamic header. The call must retain the header-validation
exception and close the source `MemoryStream`; this case passes on Windows
x86/x64 and Linux x64 with static and shared linking.
The focused `cxnet_managed_rest_multipart_setup_failure` CTest injects the same
invalid header after a multipart `HttpContent` part is attached and verifies
the part is disposed during request rollback. It and the full recording fixture
pass in all eight Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared
cells.

Const-qualified standard collections are accepted for repeated query and header
values, URL-encoded form fields, and multipart parts. `const List<T>` is covered
across all four bindings; const-qualified `LinkedList<T>`, `Queue<T>`, and
`Stack<T>` are covered by query, form, and multipart cases. The fixture verifies
the encoded values and order across all six Windows x86/x64 and Linux x64
static/shared cells, plus strict Linux GCC and Clang 14.0.6
`-Wall -Wextra -Werror -Wno-unused-value` static/shared builds.

The default-`HttpClient` wire fixture registers a custom serializer for an
unannotated DTO and verifies request/response media types, the exact request
payload, and materialized fields. A second route registers a custom serializer
for an annotated DTO and verifies its request payload, media type, and response
materialization in place of the generated CXDATA codec. The recording-handler
fixture also checks this override. Both cases pass in all six Windows and Linux
static/shared cells, strict Linux GCC static/shared builds, and strict Linux
Clang 14.0.6 static/shared builds.

The fixture also exercises cancellation through a generated
`Task<Stream>` contract. After reading a body prefix, it cancels a second
pending stream read and verifies the server observes the client closing the
connection. This cancellation case passes on Windows x86/x64 and Linux x64,
with both static and shared linking.
The fixture also disposes a generated stream after reading a body prefix while
another read is pending. It verifies that disposal cancels the read and the
server observes the client connection close in the same six-cell matrix. The
token-cancellation case also disposes the stream immediately after cancellation
while the read is still pending, covering the cancellation/disposal overlap.
The wire fixture also attempts a second read while the first is pending,
requires `InvalidOperationException`, and confirms the original read remains
pending for the subsequent cancellation check. This case passes in all six
Windows x86/x64 and Linux x64 static/shared cells, plus strict Linux GCC and
Clang 14 static/shared builds.
The Linux Release fixture builds with GCC and strict `-Wall -Wextra -Werror`
warnings (suppressing the existing vendored libyaml unused-value warning); it
also builds and passes with Clang 14.0.6 under the same warning flags in both
static and shared link modes.
The REST completion helpers force collection while request tasks are pending,
the wire fixture collects before canceling a pending streamed body read, and
DTO, response-wrapper, and `ApiException` checks collect while their materialized
values remain live. This guards the generated request and result ownership paths
against collection between asynchronous completion and caller inspection.
The generated byte-array response is checked byte-for-byte at 96 KiB, above
the transport's 64 KiB streaming queue capacity; this case also passes in all
six build cells. The wrapped DTO case checks materialized value, status, and
response header on the same six cells.

JSON response collections support `Task<List<DTO>>`, `Task<Array<DTO>>`,
`Task<LinkedList<DTO>>`, `Task<Queue<DTO>>`, `Task<Stack<DTO>>`, their
`const`-qualified forms, and the corresponding `RestResponse` wrappers. The
generated client preserves the contract's standard collection type and const
qualifier; use `const` with the chosen collection when callers should see an
immutable collection. It materializes the declared standard collection type
and requires a JSON array,
deserializes each element through the generated CXDATA codec or the registered
custom serializer, and disposes the parsed document after materialization. The
recording fixture checks populated and empty arrays, rejects an object root,
retains wrapped status and headers, and verifies response ownership. The live
wire fixture checks direct and wrapped DTO arrays through the default HTTP
transport. Mutable and const-qualified `Array<DTO>` responses, including a
wrapped const result, pass recording-handler checks in all six Windows x86/x64
and Linux x64 static/shared cells. Direct and wrapped const `Array<DTO>` results
also pass live-wire checks in all six cells, including strict Linux GCC and
Clang static/shared builds.

JSON object responses support `Task<Dictionary<string, DTO>>` and
`Task<RestResponse<Dictionary<string, DTO>>>`, including `const`-qualified
results. Each object property name becomes a dictionary key, and each value is
deserialized through the generated CXDATA codec or registered custom
serializer. An object root is required; array roots and duplicate property
names fail with `JsonException`, and the response content is still disposed.
The recording fixture checks populated and empty objects, wrong-root and
duplicate-key rejection, wrapped status and headers, and response ownership.
The live-wire fixture checks direct and wrapped `const Dictionary<string,
DTO>` materialization through the default HTTP transport.
Recording and live-wire runs pass on Windows x86/x64 and Linux x64 with GCC and
Clang in static and shared configurations; Linux uses strict warning flags.

JSON object responses also support string and non-null supported scalar values,
such as `Task<Dictionary<string, string>>` and
`Task<RestResponse<const Dictionary<string, int>>>`. String properties must
contain JSON strings; scalar values use the same strict JSON parsing as direct
scalar responses. The generated result preserves `const`, rejects non-object
roots and duplicate keys, and disposes response content on success or failure.
The recording-handler and live-wire fixture checks escaped strings, positive
and negative integers, wrapper status/headers, and response ownership.
These additions pass recording-handler and live-wire runs on Windows x86/x64
MSVC and Linux x64 GCC/Clang in static and shared configurations; Linux builds
use strict warning flags.

Nullable scalar JSON object values also support `const Dictionary<string, int?>`
directly and under `RestResponse<T>`. Recording-handler and live-wire cases
preserve positive and negative integers alongside a JSON null; the wrapped
case checks status, headers, and response-content ownership through disposal.
The recording-handler and live-wire fixtures pass in all eight Windows x86/x64
MSVC and Linux x64 GCC/Clang static/shared cells. Three discarded-`const`
warnings in generated calls involving const-qualified collection parameters
were resolved by using unqualified reference types at the generated C ABI
boundary after CX type checking. The updated fixture builds warning-free in all
eight cells. Linux GCC uses `-Wall -Wextra -Werror`; Linux Clang uses
`-Wall -Wextra`.

The recording-handler fixture also starts serializer registration while its
content-type getter is held in progress, then starts a concurrent serializer
lookup. It verifies both operations complete and the lookup observes the new
registration. This race passes in all six Windows x86/x64 and Linux x64
static/shared cells, plus strict Linux GCC and Clang static/shared builds with
`-Wall -Wextra -Werror -Wno-unused-value`.

Nullable collection-valued JSON maps are covered directly and through
`RestResponse<T>` using standard dictionaries and lists. The cases include a
populated list with nullable DTO elements, an empty list, and a null list value.
The REST generator inserts the completed list into the map directly so emitted
C does not create an invalid nullable collection temporary. Recording and
live-wire checks pass Windows x86/x64 MSVC and Linux x64 GCC/Clang in static and
shared configurations; Linux GCC uses `-Wall -Wextra -Werror` and Clang uses
`-Wall -Wextra`.

Wrapped nullable DTO dictionary values are also checked with
`RestResponse<const Dictionary<string, RestItem?>>`. The recording and live
HTTP responses include a populated DTO and a null value; assertions cover DTO
fields, wrapper status and headers, and content disposal. Both paths pass in all
eight Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared cells.

JSON object collection values also have direct and wrapped
`const Dictionary<string, const Array<RestItem>>`,
`const Dictionary<string, const LinkedList<RestItem>>`,
`const Dictionary<string, const Queue<RestItem>>`, and
`const Dictionary<string, const Stack<RestItem>>`. Populated values preserve
JSON order when enumerated, and empty collections remain empty. Recording
assertions check wrapper metadata and response-content ownership; live-wire
checks use the default HTTP transport. Both fixtures pass in all eight Windows
x86/x64 MSVC and Linux x64 GCC/Clang static/shared cells, including strict Linux
builds.

Nullable string JSON arrays are checked directly and through
`RestResponse<const T>` for `Array<string?>`, `List<string?>`,
`LinkedList<string?>`, `Queue<string?>`, and `Stack<string?>`. Recording and
live-wire cases preserve escaped strings and an intervening JSON null. Wrapped
cases check status, headers, and response-content ownership through disposal.
Both fixture paths pass Windows x86/x64 MSVC and Linux x64 GCC/Clang in static
and shared configurations; Linux builds use strict warning flags.

Nullable Boolean JSON arrays support direct and wrapped results for
`Array<bool?>`, `List<bool?>`, `LinkedList<bool?>`, `Queue<bool?>`, and
`Stack<bool?>`. The fixtures preserve a null between true and false values;
wrapped wire cases verify status and headers, and the recording case verifies
response-content ownership through disposal. Recording and live-wire runs pass
all eight Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared cells, with
strict Linux warning flags.

Nullable decimal JSON responses cover direct and wrapped `const Array`,
`List`, `LinkedList`, `Queue`, and `Stack` results. Each sequence preserves a
null between positive and negative fractional values; wrapped responses also
parse exponent notation and check status, headers, and content ownership
through disposal. Recording and live-wire checks pass in all eight Windows
x86/x64 MSVC and Linux x64 GCC/Clang static/shared cells. The recording and
generated-result continuation-registration CTests also pass in all eight
cells.
Nullable decimal object responses also cover direct and wrapped
`const Dictionary<string, decimal?>` results, preserving fractional values,
nulls, and exponent notation. Recording CTests and live-wire checks pass in all
eight Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared cells.
Nullable Boolean object responses now also cover direct and wrapped
`const Dictionary<string, bool?>` results, preserving `true`, `false`, and
`null`; the wrapped recording case checks status, headers, and response-content
ownership through disposal. The updated fixture compiles and passes on Windows
x86/x64 MSVC and Linux x64 GCC/Clang static/shared builds. Live-wire checks for
the same direct and wrapped responses pass all eight Windows x86/x64 MSVC and
Linux x64 GCC/Clang static/shared cells.

Nullable UUID query and header bindings are also checked with both omitted and
populated values. The recording handler verifies omission and canonical
hyphenated formatting; the live-wire fixture confirms the query target and
header. Both fixture modes pass in all eight Windows x86/x64 MSVC and Linux x64
GCC/Clang static/shared cells.
Nullable UUID form fields are checked alongside nullable integer, decimal, and
Boolean fields: a null UUID is omitted, and a populated UUID uses canonical
hyphenated formatting. The recording-handler and live-wire fixture pass in the
all eight Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared cells.
Linux GCC and Clang builds use `-Wall -Wextra -Werror -Wno-unused-value`.
Nullable UUID multipart parts are also checked for omission and canonical
hyphenated formatting alongside nullable integer, decimal, and string parts.
The exact multipart body passes recording-handler and live-wire checks in all
eight Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared cells. Linux
GCC and Clang builds use `-Wall -Wextra -Werror -Wno-unused-value`.

Nullable UUID collection query binding is also checked with a `const
Array<Uuid?>` containing two UUID values around a null element. The recording
handler and live-wire fixture verify that the null element is omitted, value
order is preserved, and both UUIDs use canonical hyphenated formatting. Both
fixture modes pass all eight Windows x86/x64 MSVC and Linux x64 GCC/Clang
static/shared cells; Linux builds use strict warning flags.

Nullable UUID JSON object values are checked directly and through
`RestResponse<const Dictionary<string, Uuid?>>`. Recording and live-wire
responses preserve two canonical UUID values around a null entry; wrapped
responses also verify status, headers, and response-content ownership through
disposal. Both fixtures pass all eight Windows x86/x64 MSVC and Linux x64
GCC/Clang static/shared cells.

## Nullable scalar response wrappers

The generated REST fixture now covers `RestResponse<int?>` and
`RestResponse<bool?>` for both JSON `null` and populated values. The populated
cases specifically preserve integer zero and boolean false rather than treating
them as absent. Recording and live-wire checks verify status, `HasValue`, value
presence, response headers, and disposal. Both fixture runs pass in all eight
Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared cells; Linux builds
use the strict warning configuration.

Direct nullable decimal results cover JSON `null` with surrounding whitespace
and the populated value `123.5`. The recording handler and live-wire fixture
both verify null/value distinction. The updated generated fixture builds, and
its recording and live-wire runs pass in all eight Windows x86/x64 MSVC and
Linux x64 GCC/Clang static/shared cells; Linux builds use strict warnings.

Direct `float?` and `double?` results, plus `RestResponse<float?>` and
`RestResponse<double?>`, cover JSON `null` and populated values. The cases
include whitespace around a direct `float` null, negative float/double values,
and zero in a wrapped float response. Recording and live-wire checks verify
null/value distinction; wrappers also verify status, headers, and disposal.
Both fixture modes pass all eight Windows x86/x64 MSVC and Linux x64 GCC 13.3
and Clang 18.1.3 static/shared cells, with strict Linux warning flags.

Nullable signed 64-bit scalar results cover JSON `null` and `long.MinValue` in
both direct `Task<long?>` and wrapped `RestResponse<long?>` responses. Recording
and live-wire checks verify the boundary value; wrappers also check status,
headers, and disposal. Both fixture modes pass the same eight Windows and
Linux static/shared cells, with strict Linux warning flags.

## Nullable unsigned 64-bit scalar results

The generated REST fixture covers direct `Task<ulong?>` and wrapped
`Task<RestResponse<ulong?>>` results for JSON `null` and `ulong.MaxValue`. The
direct null case includes surrounding JSON whitespace. Wrapped cases verify
status, `HasValue`, null/value distinction, response headers, and disposal.
Wire runs cover direct and wrapped results in all eight Windows x86/x64 MSVC
and Linux x64 GCC/Clang static/shared cells. Recording coverage checks the
wrapped null and maximum-value cases in those same cells; Linux uses strict
warning builds.

Nullable scalar coverage also includes direct and wrapped `byte?` and `sbyte?`
results for JSON `null`, `byte.MaxValue`, and `sbyte.MinValue`. Recording and
live-wire checks verify null/value distinction; wrappers also check status,
headers, and disposal. Both fixture modes pass in all eight Windows x86/x64
MSVC and Linux x64 GCC 13.3/Clang 18.1.3 static/shared cells. Linux builds use
strict warning flags.

Nullable `short?`, `ushort?`, and `uint?` scalar results now cover JSON `null`
and their minimum or maximum boundary values, directly and inside
`RestResponse<T>`. Recording and live-wire checks verify the parsed value,
wrapper status and headers, and response-content disposal. Both fixture modes
pass all eight Windows x86/x64 MSVC and Linux x64 GCC/Clang static/shared cells;
Linux uses strict warnings, and `ldd -r` finds no unresolved symbols.

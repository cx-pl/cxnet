# REST generator profiling results

## Current profile

The REST fixture was compiled three times on 2026-11-23 using the compiler's
`--profile-generators` option. All three runs used the Release compiler on Windows
x64 (Windows 10.0.26300, .NET SDK 11.0.100-preview.5.26302.115) from the
workspace root:

```powershell
dotnet cxc/src/CxCompiler/bin/Release/net10.0/CxCompiler.dll `
  --profile-generators cxnet/tests/rest_generator/rest-generator.cxproj `
  --compile --configuration Release
```

| Profile phase | Run 1 | Run 2 | Run 3 |
| --- | ---: | ---: | ---: |
| Snapshot creation | 3.6166 s | 3.7974 s | 3.2599 s |
| Snapshot encoding | 0.0079 s | 0.0097 s | 0.0076 s |
| `cxdata-json` worker | 7.6674 s | 7.3343 s | 7.1428 s |
| `cxdata-json` post-worker and total | 0.0416 s / 7.7090 s | 0.0430 s / 7.3773 s | 0.0266 s / 7.1693 s |
| `cxnet-rest` worker | 42.7411 s | 43.0798 s | 43.4299 s |
| `cxnet-rest` post-worker and total | 0.7622 s / 43.5033 s | 0.9973 s / 44.0771 s | 0.7808 s / 44.2107 s |

Both snapshots contained 399 types, 590 annotations, 272 closed-generic
requests, and 50 closed-generic method requests, encoded to 1,441,573 bytes.
The REST generator emitted 22 source outputs totaling 1,917,734 bytes. The
CXDATA generator emitted 3 outputs totaling 4,094 bytes. The REST worker
dominates these measured phases; the compiler's current profile does not split
worker startup, snapshot decoding, generator execution, or response encoding
into separate timings.

## Diagnosis and optimization

The original REST worker took 30m 27.9s on the full fixture. The cause was
repeated linear lookup across closed-generic requests, annotation targets, and
the member-to-declaring-type mapping while generating many REST methods. The
generator now builds indexes for those identities before contract generation:

- Closed generic constructions are indexed by definition identity and
  normalized closed identity.
- Annotation positions are grouped by target identity while preserving source
  order.
- Member identities are mapped to their declaring type identities.

The indexed implementation reduced the worker time to 30.0s in the earlier
comparison, with byte-identical generated output. That was about a 61x reduction
from the original 30m 27.9s run. The current fixture has since grown; its three
fresh worker samples are about 43s. This profile confirms the REST worker is
still the dominant phase, but it does not establish which internal operation
accounts for the remaining time.

## Expanded nullable-decimal collection matrix

On 2026-11-24, the fixture was profiled again after adding direct and wrapped
nullable decimal JSON responses for all five standard collection types. The
Release compiler reported:

| Profile phase | Result |
| --- | ---: |
| Snapshot creation | 5.1698 s |
| Snapshot encoding | 0.0154 s |
| `cxdata-json` worker | 8.8421 s |
| `cxdata-json` post-worker and total | 0.0322 s / 8.8743 s |
| `cxnet-rest` worker | 93.1406 s |
| `cxnet-rest` post-worker and total | 1.9831 s / 95.1237 s |

The snapshot contained 399 types, 663 annotations, 323 closed-generic
requests, and 55 closed-generic method requests, encoded to 1,627,550 bytes.
The REST generator emitted 23 sources totaling 2,574,516 bytes. The worker
time is about 2.1 times the previous 43.43-second sample; the fixture snapshot
also grew from 272 to 323 closed-generic requests and from 22 to 23 outputs, so
this is not a like-for-like regression measurement. The recording and live-wire
fixture runs pass on all eight Windows x86/x64 and Linux GCC/Clang static/shared
cells. The current profile still aggregates all REST worker internals.

For a repeatability check, the 22 generated REST sources in
`cxnet/tests/rest_generator/.obj/generators/cxnet-rest` were SHA-256 hashed
before and after the third profile run. All 22 paths and hashes were unchanged.
This checks determinism between current runs; the earlier byte-for-byte
comparison against the pre-index implementation is recorded in the roadmap.

The profile runs compiled the fixture but did not execute its recording-handler
or live-wire tests. See [the REST fixture instructions](../../tests/rest_generator/README.md)
for the test commands and coverage.

## 2026-11-24 profiled worker-timeout reproduction

The current `rest-generator.cxproj` was rerun with
`--profile-generators --emit-only` after the fixture's previous successful
profile. Snapshot creation took 12.2591 seconds and encoding took 0.0199
seconds; the snapshot again contained 399 types, 663 annotations, 323
closed-generic requests, and 55 closed-generic method requests. The CXDATA
worker completed in 49.3121 seconds, with 0.0894 seconds post-worker (49.4015
seconds total) and three outputs totaling 4,094 bytes. The REST worker then
hit the project's 180-second timeout after 22 response frames, with
`ITextApiRestClient.g.cx` as the last reported source; outputs were unavailable.

This run does not isolate a REST-generator regression: the preceding CXDATA
worker took 8.8421 seconds on the same recorded snapshot size, while this run
took 49.3121 seconds. The result establishes a current worker-timeout
reproduction but is not a comparable timing sample. No REST generator source
was changed in this run.

## 2026-11-24 normal fixture build after timeout

The fixture was rebuilt with the normal, unprofiled command from the workspace
root:

```powershell
dotnet cxc/src/CxCompiler/bin/Release/net10.0/CxCompiler.dll `
  cxnet/tests/rest_generator/rest-generator.cxproj --compile --configuration Release
```

The command exited successfully and regenerated the CXDATA, CXCore, CXNet,
REST model, and `RestGeneratorFixture` outputs. The fixture executable then
passed, all six `rest_generator_lookup_test.py` regression tests passed, and
`rest_wire_fixture.py` completed successfully against the generated executable.
This verifies the current normal generator path on Windows x64 after the
earlier profiled worker-timeout reproduction; it does not replace the broader
eight-cell recording/live-wire matrix documented in the fixture README.

## 2026-11-24 current-source REST fixture matrix

Using the sources generated by the normal build above, the recording fixture,
multipart setup-failure case, and generated-result continuation-registration
failure case were rebuilt and passed in all eight Windows x86/x64 MSVC and
Linux x64 GCC/Clang static/shared cells. The live-wire fixture also passed in
all eight cells. Linux used GCC 14.2.0 and Clang 18.1.3 with
`-Wall -Wextra -Werror -Wno-unused-value`.

## 2026-11-26 current REST generator profile

The current Release compiler ran the REST fixture twice with
`--profile-generators --compile --configuration Release`. Both runs completed
successfully, including compiling `RestGeneratorFixture`:

| Profile phase | Run 1 | Run 2 |
| --- | ---: | ---: |
| Snapshot creation | 7.5478 s | 8.0723 s |
| Snapshot encoding | 0.0197 s | 0.0089 s |
| `cxdata-json` worker | 9.9628 s | 9.8317 s |
| `cxdata-json` post-worker and total | 0.0325 s / 9.9953 s | 0.0355 s / 9.8672 s |
| `cxnet-rest` worker | 156.3889 s | 150.5961 s |
| `cxnet-rest` post-worker and total | 1.4324 s / 157.8213 s | 1.2110 s / 151.8072 s |

Both snapshots contained 399 types, 778 annotations, 437 closed-generic
requests, and 55 closed-generic method requests, encoded to 1,902,238 bytes.
The REST worker emitted 23 source files totaling 3,910,971 bytes; CXDATA
emitted 3 files totaling 4,094 bytes. The REST worker remains the dominant
phase and is close to the fixture's 180-second timeout. These timings should
not be directly compared with prior profiles because this snapshot contains
114 more annotations and 114 more closed-generic requests than the
2026-11-24 profile. The profile still aggregates REST worker internals, so it
does not identify which operation accounts for the remaining worker time.

All 23 generated REST source paths and SHA-256 hashes matched before and after
the first run, and the second run completed with the same snapshot dimensions
and output sizes. The normal compile path therefore succeeds, and the profiled
runs reproduce byte-identical generated output; fine-grained REST worker
instrumentation remains a separate improvement.

## 2026-11-26 fixture-growth timeout and isolation

Adding direct and wrapped primitive map routes for `Array<int>`,
`LinkedList<int>`, `Queue<int>`, and `Stack<int>` to the already large
`rest_generator` fixture caused the REST worker to exceed its 180-second limit
after 22 response frames; `ITextApiRestClient.g.cx` was the last reported
source. The generator implementation was not changed. Those map response
executions now live in the smaller `rest_map_collection_probe`, which covers
all five standard collection value types (including the existing `List<int>`)
without adding them to the main fixture snapshot. The main fixture was rebuilt
successfully after removing those additional routes, and its recording and
live-wire runs pass on Windows. The focused probe passes all eight Windows
x86/x64 MSVC and Linux x64 GCC 11.4/Clang 14 static/shared cells; Linux uses
strict warnings and both shared probe executables pass `ldd -r` without
unresolved symbols. This isolates the coverage from the near-timeout fixture
but does not identify or fix the REST worker's internal bottleneck.

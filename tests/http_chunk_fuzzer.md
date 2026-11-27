The `http_chunk_fuzz_corpus` directory contains 23 seeds for the opt-in HTTP
chunk parser libFuzzer target. They cover valid and empty bodies, extensions
and trailers, truncated payload and trailer endings, malformed trailer fields,
truncated and overflowing size lines, the chunk-line size boundary, repeated CR
delimiters, an LF-only trailer delimiter, a C0 control in a trailer value, and
an obsolete folded trailer line. Additional seeds cover multiple chunks,
leading-zero chunk sizes, tab boundaries in sizes and quoted extensions, empty
extension names and values, and optional whitespace around extensions.
The target also checks that
16,384-byte chunk-size lines and trailers are accepted and their 16,385-byte
forms are rejected, with identical parser state across its callback-fragment
sizes. Its one-time grammar checks reject empty extension names and values,
accept optional extension whitespace, and compare callback fragments. Directed
truncation checks walk every prefix of three valid chunked bodies: an empty
body, a single extended chunk with one trailer, and multiple extended chunks
with multiple trailers. Every incomplete prefix must remain accepted but not
complete; only the full message reaches the parser's done state. Each prefix
is replayed with every fixed fragment size up to its length, covering splits
through size lines, extensions, payloads, terminators, and trailers. These
checks also flip each CR and LF in representative single- and multi-chunk
messages independently, requiring every malformed delimiter mutation to be
rejected across one-byte, two-byte, seven-byte, 31-byte, and whole-input
callbacks. These checks and the full Clang 18 ASan/UBSan CTest campaign pass;
the campaign completed 100,000 iterations in 118.33 seconds with a 20,000-byte
input limit.

The managed HTTP loopback grammar case also sends each delimiter mutation and
every incomplete body prefix through `HttpClient`, verifying that each request
faults. These checks pass in Windows x86/x64 MSVC and Linux x64 GCC/Clang
static/shared configurations; Linux builds use strict warning flags.

Configure the HTTP backend spike with Clang, `CX_STATIC_LINK=ON`,
`BUILD_TESTING=ON`, and `CXNET_BUILD_HTTP_CHUNK_FUZZER=ON`. Then build and run
`cxnet_managed_http_chunk_parser_fuzz` with CTest. The fuzzer compares parser
state and decoded body across one-byte, two-byte, seven-byte, 31-byte, and
whole-input callback fragments, so any callback-boundary-dependent result is
reported as a crash.

**2026-11-26 current-source sanitizer rerun:** rebuilt the fuzzer with Clang
18.1.3, strict warnings, and pinned libcurl 8.23.0. The configured 100,000-run
CTest completed successfully in 49.78 seconds with ASan/UBSan, `max_len=20000`,
and `timeout=2`.

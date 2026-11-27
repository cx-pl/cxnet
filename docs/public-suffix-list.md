# Public Suffix List snapshot

`CookieContainer` uses the official Mozilla Public Suffix List to reject
cookies scoped to public suffixes. The checked-in snapshot is
`public_suffix_list.dat`, version `2026-11-17_06-40-19_UTC`, commit
`88df16b430cd20c98a83e3a7ecac8ee471374842`. The list is distributed under the
Mozilla Public License 2.0; its license notice is retained in the source file.

`platform/common/System/Net/CookiePublicSuffixRules.h` is generated from that
snapshot. Regenerate it after updating the source list with:

```sh
python cxnet/tools/generate_public_suffix_header.py \
  cxnet/docs/public_suffix_list.dat \
  cxnet/platform/common/System/Net/CookiePublicSuffixRules.h
```

Only download updates from
[`publicsuffix.org/list/public_suffix_list.dat`](https://publicsuffix.org/list/public_suffix_list.dat),
as requested by the upstream list. The lookup supports exact, wildcard, and
exception rules, includes the private section, and applies the implicit `*`
rule for single-label names. IP literals are excluded from suffix matching.

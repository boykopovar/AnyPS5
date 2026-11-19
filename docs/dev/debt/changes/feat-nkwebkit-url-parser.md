# libSceNKWebKit

## add Functional

- [WTF::URL parsing](../../../core/libs/prx/libSceNKWebKit/src/WtfUrlParser.cpp) (libSceNKWebKit) - not compared with a console module. `URL::URL(const URL&, const String&, const URLTextEncoding*)` ports the [URLParser.cpp](https://github.com/WebKit/WebKit/blob/e12a212a531a/Source/WTF/wtf/URLParser.cpp) whose `URLParser` signatures the module exports, checked against that revision built for Linux. Hosts that are not ASCII or that start with `xn--` go through `libSceIcu`'s `uidna_openUTS46` and `uidna_nameToASCII`, so they throw while those are placeholders. A non-UTF-8 query encoding is called through slot 0 of its vtable and the returned buffer is freed with `fastFree`. As there, a `w` scheme of four or more letters drops port 0 as its default (`whois://h:0/` gives `whois://h/`), fixed upstream in [38e5d18210](https://github.com/WebKit/WebKit/commit/38e5d18210). A null object, base or string, an encoding without a vtable, and the source's `RELEASE_ASSERT` failures throw.

## remove | `_ZN3WTF3URLC2ERKS0_RKNS_6StringEPKNS_15URLTextEncodingE` | `ANq4kcABc7A` |

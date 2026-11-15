# libSceNKWebKit

## add Functional

- [WTF::URL parsing](../../../core/libs/prx/libSceNKWebKit/src/WtfUrlParser.cpp) (libSceNKWebKit) - not compared with a console module. `URL::URL(const URL&, const String&, const URLTextEncoding*)` runs a port of the [URLParser.cpp](https://github.com/WebKit/WebKit/blob/e12a212a531a/Source/WTF/wtf/URLParser.cpp) whose exported `URLParser` signatures the module matches, checked against that revision built for Linux. Hosts that are not ASCII or that start with `xn--` go through `libSceIcu`'s `uidna_openUTS46` and `uidna_nameToASCII`, so they throw while those are placeholders. A non-UTF-8 query encoding is called through slot 0 of its vtable and the returned buffer is freed with `fastFree`. A null object, base or string, an encoding without a vtable, and the source's `RELEASE_ASSERT` failures throw.

## remove | `_ZN3WTF3URLC2ERKS0_RKNS_6StringEPKNS_15URLTextEncodingE` | `ANq4kcABc7A` |

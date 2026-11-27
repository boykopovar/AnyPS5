# libSceNKWebKit

## add Functional

- [WTF::URL::fakeURLWithRelativePart](../../../core/libs/prx/libSceNKWebKit/src/WtfUrlParser.cpp) (libSceNKWebKit) - not compared with a console module. Follows [URL.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/URL.cpp#L1081-L1084): parses `webkit-fake-url://<uuid>/<relative part>` against a null base, where the UUID is a version 4 value from guest `cryptographicallyRandomValues` formatted in lowercase as [UUID.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/UUID.h) does. The random bytes map to the UUID's low and high words in little-endian order. A view with a length but no characters throws.

## remove `_ZN3WTF3URL23fakeURLWithRelativePartENS_10StringViewE`

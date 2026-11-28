# libSceNKWebKit

## change [User content URL pattern matching]

- [User content URL pattern matching](../../../core/libs/prx/libSceNKWebKit/src/UserContentURLPattern.cpp) (libSceNKWebKit) - the tested URL is read through this module's `WTF::URL` parser, as `API::UserContentURLPattern::matchesURL` does in the pinned source, so that parser's gaps apply: a URL with a non-ASCII host throws until `uidna_openUTS46` is implemented. Port matching, non-standard schemes and console execution remain unverified against the device. Host matching follows the pinned [PS5 11.x WebKit source](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/page/UserContentURLPattern.cpp).

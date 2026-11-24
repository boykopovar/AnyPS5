# libSceNKWebKit

## add Functional

- [Date strings](../../../core/libs/prx/libSceNKWebKit/src/JscDate.cpp) (libSceNKWebKit) - not checked on a console. `Date.parse`, `new Date(string)` and `JSObjectMakeDate` parse as [`DateCache::parseDate`](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/runtime/JSDateMath.cpp): the ES5 parser of WTF, then the legacy one, both ported in this library. A string in local time is converted by the engine's own `Date` constructor from its fields, where JavaScriptCore subtracts `localTimeOffset`; the two may differ within the hour of a daylight saving change. The replacement of U+202F by a space follows the pinned source, which may be newer than the console module. What `Date` prints (`toString`, `toLocaleString`) is still the engine's. Closes when: compared with a console.

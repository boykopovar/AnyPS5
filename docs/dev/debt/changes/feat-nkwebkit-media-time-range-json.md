# libSceNKWebKit

## add Functional

- [WTF::MediaTimeRange::toJSONString](../../../core/libs/prx/libSceNKWebKit/src/WtfMediaTimeRangeJson.cpp) (libSceNKWebKit) - not compared with a console module; the 32-byte `MediaTimeRange` (two 16-byte `MediaTime`: `int64_t`/`double` value, `uint32_t` scale, `uint8_t` flags) and the flag bits follow the pinned [MediaTime.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/MediaTime.h#L140-L145) and are unverified. The JSON text is written directly instead of building `JSON::Object`s: keys in insertion order, numbers in the shortest form of `NumberToString`, non-finite numbers as `null`, and the denominator converted to `int` as `setInteger` does. A null range throws instead of crashing. Closes when: compared with a console module.

## remove | `_ZNK3WTF14MediaTimeRange12toJSONStringEv` | `mz868SDp4PI` |

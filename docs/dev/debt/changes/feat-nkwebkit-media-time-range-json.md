# libSceNKWebKit

## add Unknown function info

- [MediaTimeRange JSON](../../../core/libs/prx/libSceNKWebKit/src/MediaTimeRange.cpp) (libSceNKWebKit) - Console ABI and JSON output remain unmeasured. The two 16-byte MediaTime fields use the existing MediaTime ABI; serialization follows [PS5 WebKit MediaTime.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/MediaTime.cpp) at `80f77e94c63ee7202eec3d4b5b1a021dd89011ba`, producing the nested objects directly with the existing WTF number formatter. This does not implement the general JSON object model or the separate MediaTime JSON exports. Unreadable range fields throw while preserving errno. Closes when: measured on console.

## remove | `_ZNK3WTF14MediaTimeRange12toJSONStringEv` |

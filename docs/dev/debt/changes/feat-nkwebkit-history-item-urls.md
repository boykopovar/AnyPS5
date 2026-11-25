# libSceNKWebKit

## add Functional

- [WebCore::HistoryItem::url and originalURL](../../../core/libs/prx/libSceNKWebKit/src/HistoryItemURL.cpp) (libSceNKWebKit) - not compared with a console module. They follow the pinned [HistoryItem.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/history/HistoryItem.cpp#L186-L194): each parses the stored URL string or original URL string with this library's `WTF::URL(const URL&, const String&, const URLTextEncoding*)` against an empty base and no encoding, and returns the 40-byte `WTF::URL` of `WtfStringAbi.hpp` through the hidden result pointer. Closes when: compared with a console module.

## remove | `_ZNK7WebCore11HistoryItem3urlEv` | `MlqeBoQNTJE` |

## remove | `_ZNK7WebCore11HistoryItem11originalURLEv` | `Eb7c1T8MuSo` |

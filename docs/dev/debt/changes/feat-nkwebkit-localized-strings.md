# libSceNKWebKit

## add Unknown function info

- [WebCore localized strings](../../../core/libs/prx/libSceNKWebKit/src/LocalizedStrings.cpp) (libSceNKWebKit) - not compared with a console module; whether firmware 13.42 translates these strings is unverified. Neither the PS5 11.x tree nor WebKit c55364d53f97 has a PlayStation `LocalizedStrings` file, so the code follows the shared [PS5 11.x LocalizedStrings.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/LocalizedStrings.cpp#L75-L106): `localizedString` returns the UTF-8 key and `multipleFileUploadText` formats `%d files` with `vsnprintf`, so a count above `INT_MAX` prints as negative. c55364d53f97 has no PlayStation branch in `formatLocalizedString` and would return `%d files` unformatted. A null `localizedString` key throws where WebKit calls `strlen` on it.

## remove | `_ZN7WebCore15localizedStringEPKc` | `0cKbHa0gdeg` |

## remove | `_ZN7WebCore18pluginTooSmallTextEv` | `ryA0BvjZKrY` |

## remove | `_ZN7WebCore22multipleFileUploadTextEj` | `3aeIfu8G6SE` |

## remove | `_ZN7WebCore32useBlockedPlugInContextMenuTitleEv` | `KYICSuDQehA` |

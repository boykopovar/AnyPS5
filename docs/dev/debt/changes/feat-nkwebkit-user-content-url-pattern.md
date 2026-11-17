# libSceNKWebKit

## add Functional

- [WebCore::UserContentURLPattern parse and matches](../../../core/libs/prx/libSceNKWebKit/src/UserContentURLPattern.cpp) (libSceNKWebKit) - not compared with a console module. These are the 3.20 `parse(const String&)` and `matches(const URL&)` exports; the later module parses a `StringView` into an error code. They follow pinned [UserContentURLPattern.cpp](https://github.com/WebKit/WebKit/blob/e12a212a531aeddc11259e8515671cf5188e038d/Source/WebCore/page/UserContentURLPattern.cpp#L64-L229) on the 40-byte layout of [UserContentURLPattern.h](https://github.com/WebKit/WebKit/blob/e12a212a531aeddc11259e8515671cf5188e038d/Source/WebCore/page/UserContentURLPattern.h#L61-L67): `parse` sets the scheme, host, path and subdomain flag but not `m_invalid`, which the inline constructor sets, and leaves earlier fields when it fails part way; `matches` reads the URL's protocol, host and path. A null object, string or URL throws. Closes when: compared with a console module.


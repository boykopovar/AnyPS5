# libSceNKWebKit

## add Functional

- [WKSecurityOriginCreateFromString](../../../core/libs/prx/libSceNKWebKit/src/URL.cpp) (libSceNKWebKit) - console execution is unmeasured; follows WebKit `e12a212a531a` `SecurityOrigin::createFromString`. Special schemes (http, https, ftp, file, ws, wss) parse lowercased protocol and host with default ports cleared; `blob:` extracts its inner URL origin; and `file:` retains non-empty hosts with `"file://"` string representation. Invalid URLs, empty hosts on network schemes, and non-special unregistered schemes yield an opaque origin with empty protocol/host, port 0, and `"null"` string representation. Custom registered scheme handlers are not modelled.

## remove | `WKSecurityOriginCreateFromString` |

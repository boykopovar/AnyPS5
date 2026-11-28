# libSceNKWebKit

## add Functional

- [Security origin database identifier operations](../../../core/libs/prx/libSceNKWebKit/src/SecurityOriginDatabaseIdentifier.cpp) (libSceNKWebKit) - Console behavior for database identifier conversions is unverified. `WKSecurityOriginCopyDatabaseIdentifier` and `WKSecurityOriginCreateFromDatabaseIdentifier` convert between `WKSecurityOrigin` and database identifier strings following [WKSecurityOriginRef.cpp](https://github.com/WebKit/WebKit/blob/26b3c24e92fec44326a54753af61a511e737d34d/Source/WebKit/Shared/API/c/WKSecurityOriginRef.cpp) and [SecurityOriginData.cpp](https://github.com/WebKit/WebKit/blob/26b3c24e92fec44326a54753af61a511e737d34d/Source/WebCore/page/SecurityOriginData.cpp); file protocol produces `file__0`, empty ports default to 0, and malformed strings return null.

## remove YE5ube8wC4w

## remove 50a9aq8a5AQ

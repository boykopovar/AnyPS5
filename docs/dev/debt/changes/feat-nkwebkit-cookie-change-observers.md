# libSceNKWebKit

## add Older callable exports

- [WebCore::startObservingCookieChanges and WebCore::stopObservingCookieChanges](../../../core/libs/prx/libSceNKWebKit/src/CookieChangeObservers.cpp) (NIDs `Qs1P8UBKBK4`, `GDHqV2AO5dU`): not compared with a console module. Both return without doing anything, as in the curl backend's [CookieStorageCurl.cpp](https://github.com/WebKit/WebKit/blob/e12a212a531a/Source/WebCore/platform/network/curl/CookieStorageCurl.cpp), where both are `notImplemented()`. The `NetworkStorageSession` and the `WTF::Function` are not read, and the caller keeps ownership of the function, which is never called.

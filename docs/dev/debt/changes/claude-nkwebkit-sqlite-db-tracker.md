# libSceNKWebKit

## add Unknown function info

- [WebCore::SQLiteDatabaseTracker](../../../core/libs/prx/libSceNKWebKit/src/SQLiteDatabaseTracker.cpp) exports have no console verification. They follow the [PS5 11.x WebKit source](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/sql/SQLiteDatabaseTracker.cpp) and call the guest client's `willBeginFirstTransaction` and `didFinishLastTransaction` through vtable slots 2 and 3 of the [pinned client class](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/sql/SQLiteDatabaseTrackerClient.h) (destructor pair first). The callbacks run under the tracker lock as in that source, so a client that calls back into the tracker deadlocks. A decrement at zero with a client set throws instead of wrapping past the source's `ASSERT`. Closes when: compared with a console module.

## remove `_ZN7WebCore21SQLiteDatabaseTracker24hasTransactionInProgressEv`

## remove `_ZN7WebCore21SQLiteDatabaseTracker35decrementTransactionInProgressCountEv`

## remove `_ZN7WebCore21SQLiteDatabaseTracker35incrementTransactionInProgressCountEv`

## remove `_ZN7WebCore21SQLiteDatabaseTracker9setClientEPNS_27SQLiteDatabaseTrackerClientE`

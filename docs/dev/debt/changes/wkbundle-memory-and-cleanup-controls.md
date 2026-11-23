# libSceNKWebKit

## add Functional

- [WKBundleGetJavaScriptObjectsCount, WKBundleReleaseMemory](../../../core/libs/prx/libSceNKWebKit/src/BundleMemory.cpp) (libSceNKWebKit) - the count reports the live object count of the guest JavaScript heap and the release runs a JavaScript garbage collection on the bundle's context; releasing memory held by the rest of the web process is not modelled.

## add Silent stubs

- [WKBundleClearHistoryForTesting, WKBundleClearResourceLoadStatistics, WKBundleResourceLoadStatisticsNotifyObserver](../../../core/libs/prx/libSceNKWebKit/src/BundleMemory.cpp) (libSceNKWebKit) - record a flag or a counter on the bundle that nothing reads; no history is cleared, no resource load statistics are cleared and no observer is notified.

## remove `4XbhuyqB6pA`

## remove `zm7AblpGSLk`

## remove `IzxyWmghAig`

## remove `Zuc39RxgKlE`

## remove `HWRP4l2b7pE`

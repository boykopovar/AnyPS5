# libSceNKWebKit

## add Silent stubs

- [WKPage ad click attribution controls](../../../core/libs/prx/libSceNKWebKit/src/PageAdClickAttribution.cpp) (libSceNKWebKit) - `WKPageMarkAdClickAttributionsAsExpiredForTesting`, `WKPageSetAdClickAttributionConversionURLForTesting`, `WKPageSetAdClickAttributionOverrideTimerForTesting`, `WKPageClearAdClickAttribution` and `WKPageDumpAdClickAttribution` update test configuration and invoke dump callbacks on host `Page` state; underlying web process ad attribution network reporting remains unmodeled.

# libSceNKWebKit

## add Silent stubs

- [WKBundle origin access allow list](../../../core/libs/prx/libSceNKWebKit/src/BundleOriginAccess.cpp) (libSceNKWebKit) - `WKBundleAddOriginAccessAllowListEntry`, `WKBundleRemoveOriginAccessAllowListEntry` and `WKBundleResetOriginAccessAllowLists` only keep a deduplicated list of entries on the host `Bundle` object; nothing reads it, so no cross-origin check, CORS decision or `WebCore::SecurityPolicy` lookup changes until those consult it.

## remove `Ec6dqG56Kog`

## remove `YkgkKjq1zjk`

## remove `8wD6Sq3WBcA`

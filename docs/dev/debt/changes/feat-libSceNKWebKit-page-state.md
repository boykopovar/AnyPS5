# libSceNKWebKit

## add Functional

- [WKPage state accessors](../../../core/libs/prx/libSceNKWebKit/src/PageState.cpp) (libSceNKWebKit) - `WKPageIsClosed`, `WKPageCopyTitle`, `WKPageCopyActiveURL`, `WKPageCopyProvisionalURL`, `WKPageCopyCommittedURL`, `WKPageGetEstimatedProgress`, the application name, custom user agent and custom text encoding name getters and setters, and `WKPageSetBackgroundExtendsBeyondPage` with `WKPageBackgroundExtendsBeyondPage` read and write host `Page` state. No export creates a `WKPage` or loads a document into it yet, so a page reports the defaults (not closed, empty title, null URLs, progress 0, empty strings, background not extended) and only the setters change them, until page loading is connected. A null string reference to a setter clears the value, as WebKit's `toWTFString` returns a null string for it. Signatures and bodies follow the pinned WebKit's `WKPage.h` and `WKPage.cpp`; they are not compared with a console module. Closes when: compared with a console module.

## remove `WKPageIsClosed` | `ErgHdDWCpVc`

## remove `WKPageCopyTitle` | `0X7Pah5xV04`

## remove `WKPageCopyActiveURL` | `xKQvzQ1q63A`

## remove `WKPageCopyProvisionalURL` | `2DE3xMWxTBI`

## remove `WKPageCopyCommittedURL` | `KKdrRaSLnoA`

## remove `WKPageGetEstimatedProgress` | `EEXgOrxJ1go`

## remove `WKPageCopyApplicationNameForUserAgent` | `0PYkLRuvLMw`

## remove `WKPageSetApplicationNameForUserAgent` | `x+PBjiexUWI`

## remove `WKPageCopyCustomUserAgent` | `71-1jbC1u24`

## remove `WKPageSetCustomUserAgent` | `wyOUIvHW0EE`

## remove `WKPageCopyCustomTextEncodingName` | `Vrdrxhj+Ijg`

## remove `WKPageSetBackgroundExtendsBeyondPage` | `iT5rGSq18GI`

## remove `WKPageBackgroundExtendsBeyondPage` | `qKJ2G1pGKmU`

# libSceNKWebKit

## add Functional

- [WKPage editing and media state](../../../core/libs/prx/libSceNKWebKit/src/PageEditingMediaState.cpp) (libSceNKWebKit) - the automation and media capture flags, the content editable, selected range, audio and text encoding queries, and the two session value type strings read and write host `Page` state. No export creates a `WKPage`, runs a web process for it or fills its editor and media state yet, so a page keeps the defaults (media capture on, everything else off) and the setters change only the host flag, where WebKit also notifies the web and network processes. The six setters of this stack that WebKit guards with its suspended-page crash (the automation and media capture setters here, and the application name, user agent, text encoding name and background extension setters of the page state accessors) throw on a suspended page. Follows the pinned `WKPage.cpp` and `WebPageProxy.cpp`; not compared with a console module. Closes when: compared with a console module.

## remove `WKPageGetIsControlledByAutomation` | `+uRH8ci2Zu8`

## remove `WKPageSetControlledByAutomation` | `EgErBK3HZNc`

## remove `WKPageGetMediaCaptureEnabled` | `c4qOBt1+nO8`

## remove `WKPageSetMediaCaptureEnabled` | `QoItjQ1VB1E`

## remove `WKPageIsContentEditable` | `Jub+F7uGS58`

## remove `WKPageHasSelectedRange` | `pY1NNqZbJ+4`

## remove `WKPageIsPlayingAudio` | `Khlt38metko`

## remove `WKPageSupportsTextEncoding` | `3tolGFkslb4`

## remove `WKPageGetSessionHistoryURLValueType` | `lfWRbhGF-vQ`

## remove `WKPageGetSessionBackForwardListItemValueType` | `Xadpbt8wQXo`

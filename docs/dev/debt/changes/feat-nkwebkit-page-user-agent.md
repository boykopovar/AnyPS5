# libSceNKWebKit

## add Functional

- [WKPageCopyUserAgent and WKPageCopyStandardUserAgentWithApplicationName](../../../core/libs/prx/libSceNKWebKit/src/PageUserAgent.cpp) (libSceNKWebKit) - not compared with a console module. `WKPageCopyUserAgent` returns the page's custom user agent when it is not empty, else the standard user agent for the page's application name, which is the value `WebPageProxy::setCustomUserAgent` and `setApplicationNameForUserAgent` keep in `m_userAgent` in the pinned [WebPageProxy.cpp](https://github.com/WebKit/WebKit/blob/e12a212a531a/Source/WebKit/UIProcess/WebPageProxy.cpp). Both build the standard string through `WebCore::standardUserAgent`, as [WebPageProxyPlayStation.cpp](https://github.com/WebKit/WebKit/blob/e12a212a531a/Source/WebKit/UIProcess/playstation/WebPageProxyPlayStation.cpp) does. Closes when: compared with a console module.

## remove | `WKPageCopyStandardUserAgentWithApplicationName` |

## remove | `WKPageCopyUserAgent` |

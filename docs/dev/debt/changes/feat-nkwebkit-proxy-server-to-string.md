# libSceNKWebKit

## add Functional

- [WebCore proxy server PAC string](../../../core/libs/prx/libSceNKWebKit/src/ProxyServer.cpp) (libSceNKWebKit) - not compared with a console module: the device's WebKit revision is unverified. `toString(const Vector<ProxyServer>&)` follows [ProxyServer.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/platform/network/ProxyServer.cpp#L33-L73); the 11.x source has no ProxyServer. An empty list gives "DIRECT"; entries are joined by "; ", and an unknown type writes only " host:port". The result is 16-bit only where that revision's StringBuilder upconverts. The entry is read as `{int type; String host; int port}` (24 bytes). Closes when: compared with a console module.

# libSceNKWebKit

## add Functional

- [WebCore::toString(Vector<ProxyServer>) and proxyServersForURL](../../../core/libs/prx/libSceNKWebKit/src/ProxyServer.cpp) (libSceNKWebKit) - not compared with a console module. Both are in the 3.20 inventory only and follow [ProxyServer.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WebCore/platform/network/ProxyServer.cpp#L33-L72) and [ProxyServerCurl.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WebCore/platform/network/curl/ProxyServerCurl.cpp#L35-L39), unchanged from 2020-06-13 to 2020-11-12, on the 24-byte `ProxyServer` (type 0, host 8, port 16). `proxyServersForURL` returns an empty vector, as the curl port does; whether the console build replaces that file is unknown. A result over `String::MaxLength`, where `StringBuilder` asserts, throws. Closes when: compared with a console module.

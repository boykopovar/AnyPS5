# libSceNKWebKit

## add Functional

- [ProxyServer and ProxyServerCurl guest pointer fault behavior](../../../core/libs/prx/libSceNKWebKit/src/ProxyServer.cpp) (libSceNKWebKit) - Console fault behavior for invalid guest pointers in `WebCore::toString(const WTF::Vector<WebCore::ProxyServer>&)` and `WebCore::proxyServersForURL` is unmeasured; `toString` reads guest `ProxyServerVector`, `ProxyServer` array elements, `WTF::StringImpl` headers, and character buffers through `GuestReadable_nid_no_patch` / `GuestCopyIn_nid_no_patch`, and `proxyServersForURL` validates writable `ProxyServerVector` storage under `GuestArena::HostWrite` and writes through `GuestCopyOut_nid_no_patch`, calling `NotImplementedAt_nid_no_patch` on unmapped, unreadable, or unwritable guest memory. Closes when: console behavior for invalid guest pointers in `ProxyServer` and `ProxyServerCurl` exports is verified on hardware.

# libSceNKWebKit

## add Functional

- [WebCore::createAccessControlPreflightRequest](../../../core/libs/prx/libSceNKWebKit/src/CrossOriginAccessControl.cpp) (libSceNKWebKit) - not compared with a console module. Only in the 3.20 inventory; follows [CrossOriginAccessControl.cpp at WebKit 73a35b43](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/loader/CrossOriginAccessControl.cpp#L82-L121). Both requests use the 3.20 `LegacyResourceRequestBase` layout; the curl `ResourceRequest` adds no member to it. A referrer longer than 4096 units throws `std::logic_error`: the source replaces it with the origin `SecurityOrigin::create` gives its URL, and the module's `SecurityOrigin::create` follows the 11.x rules, not the 2020 ones. Common header numbers are the module's table, as in the preflight cache entry. Closes when: compared with a console module, and the long referrer case when the 2020 origin rules are in the module.

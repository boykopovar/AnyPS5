# libSceNKWebKit

## add Functional

- [ResourceRequestBase::redirectedRequest](../../../core/libs/prx/libSceNKWebKit/src/ResourceRequest.cpp) (libSceNKWebKit) - not compared with a console module. Only in the 3.20 inventory; follows the 2020 [ResourceRequestBase.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/platform/network/ResourceRequestBase.cpp). Both requests use the 3.20 `LegacyResourceRequestBase` layout; the response is read through the module's own accessors (`url`, `httpHeaderField`, the status code). The curl `doUpdateResourceRequest` is empty, so the export marks a request as updated where the other 3.20 exports of this file throw. Common header numbers are the module's table. Closes when: compared with a console module.

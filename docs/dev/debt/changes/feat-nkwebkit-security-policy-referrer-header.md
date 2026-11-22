# libSceNKWebKit

## add Functional

- [SecurityPolicy::generateReferrerHeader](../../../core/libs/prx/libSceNKWebKit/src/SecurityPolicy.cpp) (libSceNKWebKit) - not compared with a console module. The form with three arguments is only in the 3.20 inventory and follows the 2020 [SecurityPolicy.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/page/SecurityPolicy.cpp). Origins come from [LegacySecurityOrigin.cpp](../../../core/libs/prx/libSceNKWebKit/src/LegacySecurityOrigin.cpp), the 2020 `SecurityOrigin::create` without the blob registry's cached origins. `canRequest` is same scheme, host and port: the origin access allow lists are not modelled, their exports throw. A policy outside the enumeration behaves like `NoReferrerWhenDowngrade`, which is what the release build does after its assertion.

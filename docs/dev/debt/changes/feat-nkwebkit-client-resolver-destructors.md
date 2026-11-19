# libSceNKWebKit

## add Functional

- [WebCore::XPathNSResolver and WebCore::ContentSecurityPolicyClient destructors](../../../core/libs/prx/libSceNKWebKit/src/WebCoreClientDestructors.cpp) (libSceNKWebKit) - not compared with a console module. Both destructors are defaulted in the pinned [XPathNSResolver.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/xml/XPathNSResolver.cpp#L32) and [ContentSecurityPolicyClient.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/page/csp/ContentSecurityPolicyClient.h#L44-L54), and neither class has members or virtual bases, so the complete destructor does nothing and each deleting destructor only frees the object. `XPathNSResolver` derives from `RefCounted`, which is `WTF_MAKE_FAST_ALLOCATED` ([XPathNSResolver.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/xml/XPathNSResolver.h#L34-L41), [RefCounted.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/RefCounted.h#L184-L185)), so its deleting destructor frees through `WTF::fastFree`. `ContentSecurityPolicyClient` declares no allocator, so its deleting destructor frees through the guest global `operator delete(void*)`; whether the module calls the sized `operator delete(void*, unsigned long)` instead is not known, and both free the same block. A null object throws. Closes when: compared with a console module.

## remove `_ZN7WebCore15XPathNSResolverD0Ev`

## remove `_ZN7WebCore27ContentSecurityPolicyClientD0Ev`

## remove `_ZN7WebCore27ContentSecurityPolicyClientD1Ev`

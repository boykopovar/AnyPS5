# libSceNKWebKit

## add Unknown function info

- [AuthenticationChallenge getters and platformCompare](../../../core/libs/prx/libSceNKWebKit/src/AuthenticationChallengeBase.cpp) - console ABI unverified beyond the FieldGetters getters. isNull and nullify use the m_isNull byte at 0. platformCompare follows [AuthenticationChallengeCurl.cpp:141–144](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/network/curl/AuthenticationChallengeCurl.cpp#L141-L144): isForMainResource, isInMainFrame and requestURL at 464, 465, 472, the [curl/AuthenticationChallenge.h:84–86](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/network/curl/AuthenticationChallenge.h#L84-L86) member order after the 464-byte base; requestURL compares as WTF::URL operator== (WTF::equal(StringImpl*, StringImpl*)). authenticationClient returns null; nulls throw where the console dereferences. Closes when: compared with a console module.

- [AuthenticationChallenge construction, copy and destruction](../../../core/libs/prx/libSceNKWebKit/src/AuthenticationChallengeBase.cpp) (libSceNKWebKit) - stay throwing, as do assignment and compare. The FieldGetters pin protectionSpace to 0x8-0x50, proposedCredential to 0x50-0x68, failureResponse to 0x70-0x180 and error to 0x180; the 72-byte protectionSpace is the 24-byte ProtectionSpaceBase of WebCoreTypes.hpp plus the 48-byte sum of the pinned [CertificateInfo.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/network/curl/CertificateInfo.h) members, but the response and error interiors (m_httpStatusCode, m_isNull, markables, bitfields, Strings) remain unverified. Construction, copy and destruction ref and free through them; a wrong offset corrupts guest memory. Closes when: the response, error and protection space interiors are measured.

## remove | `_ZN7WebCore27AuthenticationChallengeBase7nullifyEv` | `FGotBpRW4fs` |

## remove | `_ZNK7WebCore27AuthenticationChallengeBase6isNullEv` | `mxh32RfHg5Y` |

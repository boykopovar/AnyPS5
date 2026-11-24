# libSceNKWebKit

## add Functional

- [WebCore::passesAccessControlCheck](../../../core/libs/prx/libSceNKWebKit/src/CrossOriginAccessControl.cpp) (libSceNKWebKit) - not compared with a console module. It follows the pinned [CrossOriginAccessControl.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/loader/CrossOriginAccessControl.cpp#L259-L286), whose message carries the HTTP status code. The `Expected<void, String>` result (error string at 0, flag at 8) and the status code at offset 176 of the response are derived from the pinned `Expected.h`, `ResourceResponseBase.h` and the PlayStation members of the curl `CertificateInfo.h`, not measured. WebKit of the 3.20 era gives the same symbol a message without the status code and another response layout; such a caller is not modelled. Closes when: compared with a console module.

## remove | `_ZN7WebCore24passesAccessControlCheckERKNS_16ResourceResponseENS_23StoredCredentialsPolicyERKNS_14SecurityOriginEPKNS_37CrossOriginAccessControlCheckDisablerE` | `NKP6ONc0Yds` |

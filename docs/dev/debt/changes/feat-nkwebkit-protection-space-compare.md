# libSceNKWebKit

## add Functional

- [WebCore::ProtectionSpaceBase::compare](../../../core/libs/prx/libSceNKWebKit/src/ProtectionSpaceCompare.cpp) (libSceNKWebKit) - not compared with a console module. It follows pinned [ProtectionSpaceBase.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/network/ProtectionSpaceBase.cpp#L82-L97), the Curl [platformCompare](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/network/curl/ProtectionSpaceCurl.h#L57) and the PlayStation [CertificateInfo::operator==](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/network/curl/CertificateInfo.h#L79-L99) on their 72-byte layout (server type at byte 20, scheme at 21, certificate info at 24); the 3.20 `ProtectionSpaceBase` exports keep their own layout. A null protection space throws. Closes when: compared with a console module.

## remove | `_ZN7WebCore19ProtectionSpaceBase7compareERKNS_15ProtectionSpaceES3_` | `6ePLnfc4Qgo` |

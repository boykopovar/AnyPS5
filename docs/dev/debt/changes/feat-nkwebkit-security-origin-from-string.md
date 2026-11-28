# libSceNKWebKit

## add Functional

- [WebCore::SecurityOrigin::createFromString](../../../core/libs/prx/libSceNKWebKit/src/SecurityOriginFromString.cpp) (libSceNKWebKit) - not compared with a console module. It follows the pinned [SecurityOrigin.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/page/SecurityOrigin.cpp#L548-L551): `create(URL { originString })`, parsing the string with this library's `WTF::URL(const URL&, const String&, const URLTextEncoding*)` against an empty base and no encoding and passing the result to this library's `SecurityOrigin::create(const URL&)`, so it shares that export's gaps. Closes when: compared with a console module.

## remove | `_ZN7WebCore14SecurityOrigin16createFromStringERKN3WTF6StringE` | `bim4ILT718Q` |

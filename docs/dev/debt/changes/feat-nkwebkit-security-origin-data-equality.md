# libSceNKWebKit

## add Functional

- [WebCore::SecurityOriginData equality](../../../core/libs/prx/libSceNKWebKit/src/SecurityOriginDataEquality.cpp) (libSceNKWebKit) - not compared with a console module. `operator==` follows [SecurityOriginData.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/page/SecurityOriginData.cpp#L158-L164) and `std::variant` equality over the existing tuple/opaque layout: two valueless values are equal, a port that is not engaged is ignored, and a variant index other than 0, 1 or valueless throws unless both arguments are the same object. A null argument throws.

## remove `_ZN7WebCoreeqERKNS_18SecurityOriginDataES2_`

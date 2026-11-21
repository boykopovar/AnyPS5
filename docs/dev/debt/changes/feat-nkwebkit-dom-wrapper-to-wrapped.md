# libSceNKWebKit

## add Functional

- [JSDocument, JSElement, JSEvent and JSNode toWrapped](../../../core/libs/prx/libSceNKWebKit/src/JSDOMWrapper.cpp) (libSceNKWebKit) - the four exports follow the generated `toWrapped` of [CodeGeneratorJS.pm](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/bindings/scripts/CodeGeneratorJS.pm#L5354-L5360) with the JSType values of [JSDOMWrapper.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/bindings/js/JSDOMWrapper.h#L41-L56). The JSType byte (+0x5), the wrapped pointer (+0x18) and the not-a-cell mask were read from the module with SHA-256 `3af992253665387feeb5df90ff27dff683e3aa270cc4d92a0535b46ab18fa20d`. A null cell throws, where the module would fault. The other `toWrapped` exports are not implemented here, and the other wrapper fields are not compared with a console module.

## remove | `_ZN7WebCore10JSDocument9toWrappedERN3JSC2VMENS1_7JSValueE` | `WkoNcCX2OV4` |

## remove | `_ZN7WebCore9JSElement9toWrappedERN3JSC2VMENS1_7JSValueE` | `RYDvQkQCgJc` |

## remove | `_ZN7WebCore7JSEvent9toWrappedERN3JSC2VMENS1_7JSValueE` | `EqRj7d5zUU4` |

## remove | `_ZN7WebCore6JSNode9toWrappedERN3JSC2VMENS1_7JSValueE` | `E8pE7QZK8y8` |

# libSceNKWebKit

## add Functional

- [WebCore transform operations](../../../core/libs/prx/libSceNKWebKit/src/WebCoreTransform.cpp) (libSceNKWebKit) - Translate and Perspective exports throw: pinned [Length.h](https://github.com/WebKit/WebKit/blob/c55364d53f97/Source/WebCore/platform/Length.h#L133-L135) puts `m_hasQuirk` before `m_type` and has no `Content` type, unlike this library's `Length`, and calculated lengths need `CalculationValueMap`. Identity, Rotate, Scale, Skew, Matrix and Matrix3D use host vtables in pinned [TransformOperation.h](https://github.com/WebKit/WebKit/blob/c55364d53f97/Source/WebCore/platform/graphics/transforms/TransformOperation.h#L37-L117) declaration order (Itanium, no RTTI) and that revision's layout (4-byte `OperationType` at offset 12); neither is console-measured.

## change The 108 remaining

- The 106 remaining WebCore, WebKit C API, JavaScriptCore, Inspector and WTF exports in [Export.cpp](../../../core/libs/prx/libSceNKWebKit/Export.cpp) are present in the PS5 3.20 `libSceNKWebKit` export inventory from [PS5-3.20_Libs](https://github.com/DNNDHH/PS5-3.20_Libs). Each is named after the symbol the [verified NID name database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) gives for its NID, and the NID of that name matches. Prototypes and behavior are unverified; each uses the no-parameter import-resolution convention and throws through `NotImplemented_nid_no_patch`. These declarations make no ABI or behavior claim.

## change 2159 of these [older

- 2069 of these [older callable exports](../../../core/libs/prx/libSceNKWebKit/Export.cpp) are named after the symbols the [verified NID name database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) gives for their NIDs, and the NID of each name matches. Their prototypes and behavior stay unverified, and each still throws through `NotImplemented_nid_no_patch`.

## remove | `_ZN7WebCore24MatrixTransformOperation6createERKNS_20TransformationMatrixE` | `3B0UjWrE4F0` |

## remove | `_ZN7WebCore26IdentityTransformOperation6createEv` | `xPYPMODatn4` |

## remove | `_ZN7WebCore26Matrix3DTransformOperation6createERKNS_20TransformationMatrixE` | `juxjuz-22RA` |

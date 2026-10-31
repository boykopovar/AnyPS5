# libSceFacebook.Yoga technical debt

### Callable exports

- [Callable exports](../../../core/libs/prx/libSceFacebook.Yoga/Export.cpp) - 110 callable NIDs match the [public 3.20 catalogue](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceFacebook.Yoga.c) and the [selected export metadata](https://gist.github.com/kyon-eth/c234fa1ee149b4cc00f997205cb977b6/84fe6a4ffb8f72861a138ef1562f5e165a08e00e) from two version-labelled images (SHA-256 `f76d605ecbc7de2b31117ad9c3c55aa0e461ca608775f166bfc2d83a62ba7bcb`). The labels are not authenticated. Prototypes and behavior are unverified; the declarations throw through `NotImplemented_nid_no_patch` and implement no runtime functionality. Data exports are not included.

| Name | NID |
| --- | --- |
| `YGConfigFree` | `87blI6UBBRg` |
| `YGConfigGetContext` | `71e3PK+sjL8` |
| `YGConfigGetDefault` | `vOMkFwCg4S4` |
| `YGConfigGetInstanceCount` | `kuAFKlN25xY` |
| `YGConfigGetUseWebDefaults` | `0KgyoEODDmA` |
| `YGConfigIsExperimentalFeatureEnabled` | `4FW9T-ft3fk` |
| `YGConfigNew` | `b2rU8HrfcwY` |
| `YGConfigSetContext` | `D2zWfCKPZCk` |
| `YGConfigSetExperimentalFeatureEnabled` | `Dij6Sj04Kwc` |
| `YGConfigSetPointScaleFactor` | `VzSHT3URAAY` |
| `YGConfigSetUseWebDefaults` | `9Nlag1nP2do` |
| `YGFloatIsUndefined` | `WW0jvCgA3f8` |
| `YGInteropSetLogger` | `Mo6V5akY7o8` |
| `YGNodeCalculateLayout` | `69cJDk+7h5E` |
| `YGNodeClone` | `C0A4DMm7c5A` |
| `YGNodeCopyStyle` | `gaQDIwAsA3Y` |
| `YGNodeFree` | `dqB3Ccg43qI` |
| `YGNodeFreeRecursive` | `gEs0lindnCY` |
| `YGNodeGetChild` | `Tdm0NGzRxxo` |
| `YGNodeGetChildCount` | `ruN8WLy404c` |
| `YGNodeGetContext` | `xGzizWELpCk` |
| `YGNodeGetHasNewLayout` | `oWxr+q7HyAU` |
| `YGNodeGetOwner` | `iDZS+QeOv2c` |
| `YGNodeInsertChild` | `p1zyLn2efJY` |
| `YGNodeIsDirty` | `5Mg5Cv4Gw9Q` |
| `YGNodeLayoutGetBorder` | `+xy70VTnR+c` |
| `YGNodeLayoutGetBottom` | `UuqKDcmMtAs` |
| `YGNodeLayoutGetDirection` | `2uglBBTj7Xk` |
| `YGNodeLayoutGetHadOverflow` | `QWjCa7nkT-g` |
| `YGNodeLayoutGetHeight` | `k+6XRp7LzQg` |
| `YGNodeLayoutGetLeft` | `y3959iBFPP0` |
| `YGNodeLayoutGetMargin` | `ouMiXs4gcA8` |
| `YGNodeLayoutGetPadding` | `WszCjTAdGLg` |
| `YGNodeLayoutGetRight` | `Os0sZN9FHAc` |
| `YGNodeLayoutGetTop` | `j3Exaiwjo2g` |
| `YGNodeLayoutGetWidth` | `EIK1tqMar6U` |
| `YGNodeMarkDirty` | `x6YdJcD5edE` |
| `YGNodeNew` | `HOJB+6dHSNM` |
| `YGNodeNewWithConfig` | `0MlK00Ubm2k` |
| `YGNodeRemoveAllChildren` | `-fpF2YnA+X4` |
| `YGNodeRemoveChild` | `sjpZIyT3CZc` |
| `YGNodeReset` | `gAAcmQhQ2TU` |
| `YGNodeSetBaselineFunc` | `vVxgiBpuqaM` |
| `YGNodeSetChildren` | `JMVvoQs4Vuc` |
| `YGNodeSetContext` | `kDWhMOmHaP8` |
| `YGNodeSetHasNewLayout` | `t2uMaDVXwnM` |
| `YGNodeSetMeasureFunc` | `y1uSgrWf0tU` |
| `YGNodeStyleGetAlignContent` | `DAaqGjN-L1E` |
| `YGNodeStyleGetAlignItems` | `4E-foFz4xFA` |
| `YGNodeStyleGetAlignSelf` | `+y-PtxQIHe4` |
| `YGNodeStyleGetAspectRatio` | `8em5CBRlzJg` |
| `YGNodeStyleGetBorder` | `o8PUxPppffo` |
| `YGNodeStyleGetDirection` | `N0v1-dO37MA` |
| `YGNodeStyleGetDisplay` | `WJLWjZr-X+E` |
| `YGNodeStyleGetFlexBasis` | `wNUdyLfKQ9U` |
| `YGNodeStyleGetFlexDirection` | `HkXgZNMy-Sw` |
| `YGNodeStyleGetFlexGrow` | `Msg6ByVQ+u4` |
| `YGNodeStyleGetFlexShrink` | `GHI8khgTP5k` |
| `YGNodeStyleGetFlexWrap` | `lH5InBJyeXY` |
| `YGNodeStyleGetHeight` | `MBq0U407794` |
| `YGNodeStyleGetJustifyContent` | `AREt+4e0IIM` |
| `YGNodeStyleGetMargin` | `NxRzfIUoR1M` |
| `YGNodeStyleGetMaxHeight` | `dT4j12Uj-74` |
| `YGNodeStyleGetMaxWidth` | `ADFCdtovp6c` |
| `YGNodeStyleGetMinHeight` | `b68J2DRfOU0` |
| `YGNodeStyleGetMinWidth` | `O4tvCPW+ezE` |
| `YGNodeStyleGetOverflow` | `eQ-q3E2Jtxs` |
| `YGNodeStyleGetPadding` | `HKsrg+sJBp4` |
| `YGNodeStyleGetPosition` | `5gbr38WlfZE` |
| `YGNodeStyleGetPositionType` | `gKl3XyAKxeU` |
| `YGNodeStyleGetWidth` | `uXJio0aNxk4` |
| `YGNodeStyleSetAlignContent` | `XSqDLKpP88U` |
| `YGNodeStyleSetAlignItems` | `Is3ycYFTIN4` |
| `YGNodeStyleSetAlignSelf` | `9WBY+bRpi74` |
| `YGNodeStyleSetAspectRatio` | `6M4g3C542KU` |
| `YGNodeStyleSetBorder` | `bGKK41RreRI` |
| `YGNodeStyleSetDirection` | `ALZurOIZhys` |
| `YGNodeStyleSetDisplay` | `eauWvmHc5mI` |
| `YGNodeStyleSetFlex` | `Mnf6Y9OxwXQ` |
| `YGNodeStyleSetFlexBasis` | `Bivech5VpYM` |
| `YGNodeStyleSetFlexBasisAuto` | `GdnhqBupzSE` |
| `YGNodeStyleSetFlexBasisPercent` | `d1wSZF1WYp4` |
| `YGNodeStyleSetFlexDirection` | `UtEV+oLbAe0` |
| `YGNodeStyleSetFlexGrow` | `Ot3u9DvTpq4` |
| `YGNodeStyleSetFlexShrink` | `xG+cz2ROfoc` |
| `YGNodeStyleSetFlexWrap` | `kii04cMy8zg` |
| `YGNodeStyleSetHeight` | `QaGs1ek6hCE` |
| `YGNodeStyleSetHeightAuto` | `L4O-FBngxqs` |
| `YGNodeStyleSetHeightPercent` | `jzoBKouomoQ` |
| `YGNodeStyleSetJustifyContent` | `QmKVnjK0Svk` |
| `YGNodeStyleSetMargin` | `oilo1okASh0` |
| `YGNodeStyleSetMarginAuto` | `IMx1-B6ju9g` |
| `YGNodeStyleSetMarginPercent` | `Z3T-jAmhptU` |
| `YGNodeStyleSetMaxHeight` | `xmP9O7ieSMk` |
| `YGNodeStyleSetMaxHeightPercent` | `w6i46WqVNOM` |
| `YGNodeStyleSetMaxWidth` | `IXAh0unHcjI` |
| `YGNodeStyleSetMaxWidthPercent` | `eWX6iliPDKw` |
| `YGNodeStyleSetMinHeight` | `popS2EonCvE` |
| `YGNodeStyleSetMinHeightPercent` | `MZLYhTu5Zss` |
| `YGNodeStyleSetMinWidth` | `mGsrNZGZm0k` |
| `YGNodeStyleSetMinWidthPercent` | `YbcOra6CSBU` |
| `YGNodeStyleSetOverflow` | `qCPLiH0+opI` |
| `YGNodeStyleSetPadding` | `Ua0Ucb+dcNg` |
| `YGNodeStyleSetPaddingPercent` | `P-a-cc3ydug` |
| `YGNodeStyleSetPosition` | `keMnl-oDLwk` |
| `YGNodeStyleSetPositionPercent` | `3Rh+aTCEEAQ` |
| `YGNodeStyleSetPositionType` | `6aj8UF-RB9o` |
| `YGNodeStyleSetWidth` | `IhHOpdG2KGo` |
| `YGNodeStyleSetWidthAuto` | `xXwYFScOgCo` |
| `YGNodeStyleSetWidthPercent` | `37mIA-MjoWg` |

### Additional unidentified exports

- Four [callable exports](../../../core/libs/prx/libSceFacebook.Yoga/Export.cpp) are throwing placeholders. Names, signatures and behavior are unverified. Matching module and library identities occur in two version-labelled images; those labels are not authenticated.

| Name | NID |
| --- | --- |
| `Unknown` | `+3fOvgdndrk` |
| `Unknown` | `C9+the7MAVg` |
| `Unknown` | `rjfUd+WSgRU` |
| `Unknown` | `v4k2ZXffrmY` |

- [Older callable NIDs](../../../core/libs/prx/libSceFacebook.Yoga/Export.cpp): 31 unidentified exports have unverified names, signatures and behavior. These are throwing import-resolution placeholders; no functionality is implemented. No matching NIDs were found in the later module inventory.

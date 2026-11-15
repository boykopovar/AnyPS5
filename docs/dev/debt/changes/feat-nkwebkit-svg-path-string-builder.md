# libSceNKWebKit

## add Functional

- [WebCore::SVGPathStringBuilder](../../../core/libs/prx/libSceNKWebKit/src/SVGPathStringBuilder.cpp) (libSceNKWebKit) - not compared with a console module. Ported from [SVGPathStringBuilder.cpp](https://github.com/WebKit/WebKit/blob/e12a212a531aeddc11259e8515671cf5188e038d/Source/WebCore/svg/SVGPathStringBuilder.cpp) at the 2021 WebKit revision whose ABI the module's WTF exports match. The 48-byte layout (vtable, `StringBuilder`) and the 14-slot vtable were measured on a Linux build of that revision; the five consumer callbacks the module does not export (`lineToHorizontal`, `lineToVertical`, `curveToCubicSmooth`, `curveToQuadraticSmooth`, `arcTo`) and the two trivial ones are local functions in the vtable. Numbers are written with `FormattedNumber::fixedPrecision` (six significant digits, trailing zeros removed), compared with that build's `numberToFixedPrecisionString` and `SVGPathStringBuilder`. A null object or point throws.

## remove | `_ZN7WebCore20SVGPathStringBuilder12curveToCubicERKNS_10FloatPointES3_S3_NS_18PathCoordinateModeE` | `WlBBtkMRe9s` |

## remove | `_ZN7WebCore20SVGPathStringBuilder16curveToQuadraticERKNS_10FloatPointES3_NS_18PathCoordinateModeE` | `ExazMHZlYGM` |

## remove | `_ZN7WebCore20SVGPathStringBuilder6lineToERKNS_10FloatPointENS_18PathCoordinateModeE` | `NaYyo8flntU` |

## remove | `_ZN7WebCore20SVGPathStringBuilder6moveToERKNS_10FloatPointEbNS_18PathCoordinateModeE` | `Fyogm-tTBOc` |

## remove | `_ZN7WebCore20SVGPathStringBuilder6resultEv` | `CiKXl603eqg` |

## remove | `_ZN7WebCore20SVGPathStringBuilder9closePathEv` | `ib2kd7o1mlk` |

## remove | `_ZN7WebCore20SVGPathStringBuilderC2Ev` | `3akdRDQT+nU` |

## remove | `_ZN7WebCore20SVGPathStringBuilderD0Ev` | `oP2ILJnZAkg` |

## remove | `_ZN7WebCore20SVGPathStringBuilderD1Ev` | `U-UjANPLJlM` |

## remove | `_ZN7WebCore20SVGPathStringBuilderD2Ev` | `kNMZlFmxBH8` |

# libSceJsc

## add Functional

- [JSC::Yarr::RegularExpression constructor, match, searchRev and isValid](../../../core/libs/prx/libSceJsc/src/YarrRegularExpression.cpp) (libSceJsc) - not compared with a console module. The wrapper follows [RegularExpression.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/JavaScriptCore/yarr/RegularExpression.cpp) on its 32-byte private object (count at 0, last match length at 16), but patterns are compiled and run by the `libregexp` of the QuickJS submodule, not by Yarr. Lookbehind, inline modifiers and repeated group names, which that Yarr lacks, are refused before compiling. Unverified against Yarr: syntax corner cases, the Unicode version of property escapes and case folding, and the match budget (1 000 000 engine ticks against Yarr's 1 000 000 steps). A pattern beyond a `libregexp` limit (255 groups) throws. The private object comes from the guest `malloc`. `replace` still throws. Closes when: compared with a console module.

## remove | `_ZN3JSC4Yarr17RegularExpressionC1ERKN3WTF6StringENS0_19TextCaseSensitivityENS0_13MultilineModeENS0_11UnicodeModeE` | `wrn8dpfNGYw` |

## remove | `_ZN3JSC4Yarr17RegularExpressionC2ERKN3WTF6StringENS0_19TextCaseSensitivityENS0_13MultilineModeENS0_11UnicodeModeE` | `JpNonrEoHEI` |

## remove | `_ZNK3JSC4Yarr17RegularExpression5matchERKN3WTF6StringEiPi` | `cv8-Cxsrz1Y` |

## remove | `_ZNK3JSC4Yarr17RegularExpression9searchRevERKN3WTF6StringE` | `ofaNlI4jEM0` |

# libSceNKWebKit

## add Functional

- [JSC::Yarr::RegularExpression destructor and copy assignment](../../../core/libs/prx/libSceNKWebKit/src/YarrRegularExpression.cpp) (libSceNKWebKit) - not compared with a console module. They drop and share the private object libSceJsc creates, through its release helper; the count is the 32-bit word at offset 0, as the module's copy constructor uses it. The destructor leaves the pointer in place, as the defaulted `RefPtr` member does. A null object throws. Closes when: compared with a console module.

## remove | `_ZN3JSC4Yarr17RegularExpressionD1Ev` | `dp1-zpR8Tmc` |

## remove | `_ZN3JSC4Yarr17RegularExpressionD2Ev` | `899jyvgvJO8` |

## remove | `_ZN3JSC4Yarr17RegularExpressionaSERKS1_` | `lSppkDFatb8` |

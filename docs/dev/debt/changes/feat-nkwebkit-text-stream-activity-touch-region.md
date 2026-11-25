# libSceNKWebKit

## add Functional

- [WebCore text dumps of activity flags, touch actions and regions](../../../core/libs/prx/libSceNKWebKit/src/ActivityState.cpp) (libSceNKWebKit) - not compared with a console module. `operator<<(TextStream&, OptionSet<ActivityState::Flag>)` follows [ActivityState.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WebCore/page/ActivityState.cpp#L33-L57), the same at `e12a212a531a` and `73a35b43be7a`; the pinned 11.x tree has no nested `Flag` and a tenth name. The `TouchAction` and `Region` overloads are the same in those three revisions and in 11.x. Flag bits above the ninth and touch actions outside the six values print nothing, as a release build does.

## remove | `_ZN7WebCorelsERN3WTF10TextStreamENS_11TouchActionE` | `D5Y1cX5ZyLU` |

## remove | `_ZN7WebCorelsERN3WTF10TextStreamERKNS_6RegionE` | `4W0wxLj52NI` |

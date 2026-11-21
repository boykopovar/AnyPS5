# libSceNKWebKit

## add Functional

- [WTF::GregorianDateTime constructors](../../../core/libs/prx/libSceNKWebKit/src/WtfTime.cpp) - the PS5 module's WebKit revision is unverified. C1/C2 follow pinned public WebKit [GregorianDateTime.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WTF/wtf/GregorianDateTime.cpp#L38-L85) and [DateMath.h](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WTF/wtf/DateMath.h#L124-L138), not console measurements; non-finite milliseconds and values outside ±8.64e15 still throw.

## remove | `_ZN3WTF17GregorianDateTimeC1EdNS_15LocalTimeOffsetE` | `9NbKoZNi0Jo` |

## remove | `_ZN3WTF17GregorianDateTimeC2EdNS_15LocalTimeOffsetE` | `jRKHSzq25WA` |

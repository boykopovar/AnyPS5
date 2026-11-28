# libSceJsc

## add Known export names; ABI prototypes unverified

- [Deprecated::ScriptValue copy, assignment and destructors](../../../core/libs/prx/libSceJsc/src/DeprecatedScriptValue.cpp) (libSceJsc) - not compared with a console module; the console JavaScriptCore version is unverified, and only the 8-byte handle slot at offset 0 that the firmware 13.42 constructors zero is read or written. They follow the `Strong<Unknown>` member of pinned [ScriptValue.h](https://github.com/WebKit/WebKit/blob/c55364d53f97/Source/JavaScriptCore/bindings/ScriptValue.h#L47-L57) and its [Strong.h](https://github.com/WebKit/WebKit/blob/c55364d53f97/Source/JavaScriptCore/heap/Strong.h#L55-L134): copying an empty value writes an empty slot, assigning between empty values changes nothing and returns `this`, and destroying an empty value does nothing. A non-null slot is a handle in a VM's `HandleSet`, which nothing here creates, as the `VM` constructors still throw; those paths throw. Null pointers throw where WebKit faults.

## remove | `_ZN10Deprecated11ScriptValueC1ERKS0_` | `ykyWnXSOang` |

## remove | `_ZN10Deprecated11ScriptValueC2ERKS0_` | `8JDGfjB7ysY` |

## remove | `_ZN10Deprecated11ScriptValueD1Ev` | `ale+MG3sPCU` |

## remove | `_ZN10Deprecated11ScriptValueD2Ev` | `eRd+WZ6MHqY` |

## remove | `_ZN10Deprecated11ScriptValueaSERKS0_` | `35ORvn0jTS8` |

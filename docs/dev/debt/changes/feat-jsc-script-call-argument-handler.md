# libSceJsc

## add Functional

- [Deprecated::ScriptCallArgumentHandler lifecycle and non-string arguments](../../../core/libs/prx/libSceJsc/src/ScriptCallArgumentHandler.cpp) (libSceJsc) - not compared with a console module; the console JavaScriptCore version and the 104-byte layout are unverified. A ninth argument, an out-of-line buffer and a set mark set throw, as `MarkedArgumentBuffer::slowAppend` needs the guest heap; the `String` and `const char*` overloads throw, as libSceJsc creates no `JSString`. Follows pinned [ScriptFunctionCall.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f97/Source/JavaScriptCore/bindings/ScriptFunctionCall.cpp#L59-L99) with the inline slots at byte 16 of [ArgList.h](https://github.com/WebKit/WebKit/blob/c55364d53f97/Source/JavaScriptCore/runtime/ArgList.h#L36-L196) and the 64-bit number and boolean encodings of [JSCJSValue.h](https://github.com/WebKit/WebKit/blob/c55364d53f97/Source/JavaScriptCore/runtime/JSCJSValue.h#L447-L469). Closes when: compared with a console module.

## remove | `_ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEN3JSC7JSValueE` | `b0+W39D6M9A` |

## remove | `_ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEb` | `ntUjLWHNhAM` |

## remove | `_ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEi` | `qt8HISNT7CQ` |

## remove | `_ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEj` | `kntFpAbDN0w` |

## remove | `_ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEl` | `BEYVp9YbGN8` |

## remove | `_ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEm` | `LEC1dK5v-Kg` |

## remove | `_ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEx` | `Oqc2X663Vqc` |

## remove | `_ZN10Deprecated25ScriptCallArgumentHandlerC1EPN3JSC14JSGlobalObjectE` | `zs2x0c+f2hU` |

## remove | `_ZN10Deprecated25ScriptCallArgumentHandlerC2EPN3JSC14JSGlobalObjectE` | `9ByZEM2Rtbk` |

## remove | `_ZN10Deprecated25ScriptCallArgumentHandlerD1Ev` | `Kbexb6kz+D0` |

## remove | `_ZN10Deprecated25ScriptCallArgumentHandlerD2Ev` | `GyFZR4d5pHg` |

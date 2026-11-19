# libSceJsc

## add Functional

- [JSC::ArrayBufferContents construction, move, clear and destruction](../../../core/libs/prx/libSceJsc/src/ArrayBufferContents.cpp) (libSceJsc) - not compared with a console module; the console JavaScriptCore version and allocator are unverified. Dropping the last reference to a `SharedArrayBufferContents` runs and releases its destructor task, then frees it with the guest `free`, as `BitVector` storage does, where WebKit uses `fastFree`. The rest follows pinned [ArrayBuffer.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f97/Source/JavaScriptCore/runtime/ArrayBuffer.cpp#L45-L157) on the 24-byte layout of [ArrayBuffer.h](https://github.com/WebKit/WebKit/blob/c55364d53f97/Source/JavaScriptCore/runtime/ArrayBuffer.h#L104-L108) (packed destructor task at byte 8, packed shared contents at 14, size at 20); destructor tasks run with the stored pointer, as the PlayStation port has no Gigacage. Closes when: compared with a console module.

## remove | `_ZN3JSC19ArrayBufferContents5clearEv` | `W2xTZFf+RzE` |

## remove | `_ZN3JSC19ArrayBufferContentsC1EOS0_` | `oxbJwN+R0zY` |

## remove | `_ZN3JSC19ArrayBufferContentsC1Ev` | `NLBl+ZxCYGU` |

## remove | `_ZN3JSC19ArrayBufferContentsC2EOS0_` | `NN6kXDW5Y5o` |

## remove | `_ZN3JSC19ArrayBufferContentsC2Ev` | `yZJAEU9jvAE` |

## remove | `_ZN3JSC19ArrayBufferContentsD1Ev` | `DMoAIt-s9Ks` |

## remove | `_ZN3JSC19ArrayBufferContentsD2Ev` | `ZmbGnRlFl18` |

## remove | `_ZN3JSC19ArrayBufferContentsaSEOS0_` | `CqjOWM+bC4E` |

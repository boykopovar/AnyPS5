# libSceNKWebKit

## add Functional

- [Inspector backend dispatcher core](../../../core/libs/prx/libSceNKWebKit/src/InspectorBackendDispatcher.cpp) (libSceNKWebKit) - not compared with a console module. `BackendDispatcher`, `CallbackBase` and `SupplementalBackendDispatcher` follow [InspectorBackendDispatcher.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/JavaScriptCore/inspector/InspectorBackendDispatcher.cpp) at `c55364d53f97`, the revision of the module's JSON out-parameter API: `long` request ids are written back through `setInteger(int)`. JSON parsing and writing follow [JSONValues.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WTF/wtf/JSONValues.cpp) there, escaping `<`, `>` and non-ASCII as uppercase `\u`. `BackendDispatcher::create` is not exported. An unknown error code or a null argument throws. Closes when: compared with a console module.

## remove | `_ZN9Inspector17BackendDispatcher12CallbackBase11sendFailureERKN3WTF6StringE` | `uEN6W+91atQ` |

## remove | `_ZN9Inspector17BackendDispatcher12CallbackBase7disableEv` | `GvHRNpJ7Qts` |

## remove | `_ZN9Inspector17BackendDispatcher12CallbackBaseD1Ev` | `Tp+Q2ZxmFn4` |

## remove | `_ZN9Inspector17BackendDispatcher12CallbackBaseD2Ev` | `eaSD4VTr548` |

## remove | `_ZN9Inspector17BackendDispatcher17sendPendingErrorsEv` | `s8kM7N-y-eI` |

## remove | `_ZN9Inspector17BackendDispatcher19reportProtocolErrorENS0_15CommonErrorCodeERKN3WTF6StringE` | `ayoDeN-D0MM` |

## remove | `_ZN9Inspector17BackendDispatcher27registerDispatcherForDomainERKN3WTF6StringEPNS_29SupplementalBackendDispatcherE` | `PvizYcVs3Zc` |

## remove | `_ZN9Inspector17BackendDispatcher8dispatchERKN3WTF6StringE` | `1HngmisEcBY` |

## remove | `_ZN9Inspector17BackendDispatcherD2Ev` | `l33jN8bqpfs` |

## remove | `_ZN9Inspector29SupplementalBackendDispatcherC2ERNS_17BackendDispatcherE` | `r2RAGCixu6c` |

## remove | `_ZN9Inspector29SupplementalBackendDispatcherD0Ev` | `WmpYOowrcIE` |

## remove | `_ZN9Inspector29SupplementalBackendDispatcherD1Ev` | `AjSWJSHcOIk` |

## remove | `_ZN9Inspector29SupplementalBackendDispatcherD2Ev` | `YbwxxrfPW10` |

## remove | `_ZNK3WTF8JSONImpl5Value12toJSONStringEv` | `H9Atnn27DDQ` |

## remove | `_ZNK3WTF8JSONImpl5Value9writeJSONERNS_13StringBuilderE` | `i2H+zidMP3Y` |

## remove | `_ZNK3WTF8JSONImpl9ArrayBase9writeJSONERNS_13StringBuilderE` | `T8fOjmwNgOI` |

## remove | `_ZNK9Inspector17BackendDispatcher12CallbackBase8isActiveEv` | `5nd7FB7MQhE` |

## remove | `_ZNK9Inspector17BackendDispatcher17hasProtocolErrorsEv` | `ODo1XDHSsx0` |

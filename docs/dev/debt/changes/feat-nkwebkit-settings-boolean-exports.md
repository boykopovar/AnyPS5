# libSceNKWebKit

## add Functional

- [WebCore::Settings boolean settings](../../../core/libs/prx/libSceNKWebKit/src/WebCoreSettings.cpp) (libSceNKWebKit) - not compared with a console module. `Settings` is generated with several hundred conditionally compiled members over `SettingsBase`, so no offset is derived: the five setters, their five inspector-override setters and the nine getters keep the value in a host-side table keyed by the settings object address, capped at 256 objects, and never read or write the object. Initial values are the `WebCore` defaults of [Settings.yaml](https://github.com/WebKit/WebKit/blob/6aa3f91fdcd4ec16d4369ca7bdfb76eb9500fa31/Source/WebCore/page/Settings.yaml) at 6aa3f91f. The `onChange` hooks and the `hasFrontends` assertion are skipped because they walk the page frame tree, which is not modelled; a null object throws and the table is never erased. The other 37 `Settings` setters stay stubs because no exported getter reads their value. Closes when: measured on a console module.

## remove | `_ZN7WebCore8Settings16setImagesEnabledEb` | `BJKpbkn0Rto` |

## remove | `_ZN7WebCore8Settings19setShowDebugBordersEb` | `2RVlVZ1oJAw` |

## remove | `_ZN7WebCore8Settings21setShowRepaintCounterEb` | `14QyV2LP+tE` |

## remove | `_ZN7WebCore8Settings29setAuthorAndUserStylesEnabledEb` | `Pays4s+a5y8` |

## remove | `_ZN7WebCore8Settings31setICECandidateFilteringEnabledEb` | `sJrQ1OIhBYw` |

## remove | `_ZNK7WebCore8Settings15isScriptEnabledEv` | `qDQ3PKPFeKc` |

## remove | `_ZNK7WebCore8Settings16areImagesEnabledEv` | `nNudbOEqKVI` |

## remove | `_ZNK7WebCore8Settings16showDebugBordersEv` | `jXkt81d8RC8` |

## remove | `_ZNK7WebCore8Settings18showRepaintCounterEv` | `1xbdPOXWMkY` |

## remove | `_ZNK7WebCore8Settings18webSecurityEnabledEv` | `ZThM23+9DUQ` |

## remove | `_ZNK7WebCore8Settings23needsSiteSpecificQuirksEv` | `ueZqqz5clfg` |

## remove | `_ZNK7WebCore8Settings26authorAndUserStylesEnabledEv` | `yP8syAvWyWI` |

## remove | `_ZNK7WebCore8Settings28iceCandidateFilteringEnabledEv` | `4QzvMaxdGlw` |

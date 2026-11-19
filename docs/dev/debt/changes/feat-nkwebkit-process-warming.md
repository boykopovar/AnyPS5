# libSceNKWebKit

## add Functional

- [WebCore::ProcessWarming prewarm information](../../../core/libs/prx/libSceNKWebKit/src/ProcessWarming.cpp) (libSceNKWebKit) - not compared with a console module; `forCurrentThread`'s per-thread cache is not modelled. The 32-byte `PrewarmInformation` follows [PrewarmInformation.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/page/PrewarmInformation.h#L34-L40); its two `Vector<String>` members follow `FontCache.h`'s `PrewarmInformation`: copies allocate exactly size, assignment shrinks or reuses capacity. Per the PlayStation branch of [FontCache.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/graphics/FontCache.cpp#L479-L490), `collectPrewarmInformation` returns empty and `prewarmWithInformation` does nothing; the export is `const&`, the header `&&`. Null references and invalid vectors throw. Closes when: compared with a console module.

- [WebCore::ProcessWarming::initializeNames and prewarmGlobally](../../../core/libs/prx/libSceNKWebKit/Export.cpp) (libSceNKWebKit) - still throw: they initialize the CommonAtomStrings, QualifiedName, HTML, SVG, XLink, MathML, XMLNS, XML, WebKitFontFamilyNames and TagName name tables plus the user-agent stylesheet, media query schemas and `commonVM`, which this library does not model ([ProcessWarming.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/page/ProcessWarming.cpp#L53-L88)); `FontCache::prewarmGlobally` is a no-op on the PlayStation branch of FontCache.cpp. Closes when: the name tables and stylesheet are modelled.

## remove | `_ZN7WebCore14ProcessWarming25collectPrewarmInformationEv` | `uJhCMIgHtBo` |

## remove | `_ZN7WebCore18PrewarmInformationC1ERKS0_` | `TWMdmWZVXVo` |

## remove | `_ZN7WebCore18PrewarmInformationC1Ev` | `qfwf0ujsr-w` |

## remove | `_ZN7WebCore18PrewarmInformationC2ERKS0_` | `Tg0xDBpwAJI` |

## remove | `_ZN7WebCore18PrewarmInformationC2Ev` | `bYI-b31XduA` |

## remove | `_ZN7WebCore18PrewarmInformationD1Ev` | `S+OTMjuF3sk` |

## remove | `_ZN7WebCore18PrewarmInformationD2Ev` | `I2IwTiwoWkQ` |

## remove | `_ZN7WebCore18PrewarmInformationaSERKS0_` | `NvVqG8w2D7c` |

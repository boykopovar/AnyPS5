# libSceNKWebKit

## change [WebCore::ResourceResponseBase](../../../core/libs/prx/libSceNKWebKit/src/ResourceResponse.cpp)

- [WebCore::ResourceResponseBase](../../../core/libs/prx/libSceNKWebKit/src/ResourceResponse.cpp) - guest layout, header ordinals and curl's no-op lazy initialization are unverified on console. [Historical WebKit](https://github.com/WebKit/WebKit/blob/e12a212a531a/Source/WebCore/platform/network/ResourceResponseBase.h) agrees with the layout only through the header map at offset 80; offsets 112 to 280 follow the pinned [11.x header](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/network/ResourceResponseBase.h) and are checked only against compiler layouts. Cache/date getters re-parse live headers, never filling cached age, date, expires, last-modified or parse flags. `setMimeType`, `setTextEncodingName`, `setHTTPStatusText`, `setHTTPVersion`, four-argument constructors (AtomString) and `syntheticRedirectResponse` throw. `includeCertificateInfo` keeps the older no-argument export and stores an empty certificate.

## remove | `_ZN7WebCore20ResourceResponseBaseC1Ev` | `-Ysw9ZsBANs` |

## remove | `_ZN7WebCore20ResourceResponseBaseC2Ev` | `RzkJnWJ9iOs` |

## remove | `_ZNK7WebCore20ResourceResponseBase14httpStatusCodeEv` | `bFhYDNofRSE` |

## remove | `_ZN7WebCore20ResourceResponseBase17setHTTPStatusCodeEi` | `KCJmvB04zUM` |

## remove | `_ZNK7WebCore20ResourceResponseBase12isSuccessfulEv` | `3a6B43jFjX8` |

## remove | `_ZNK7WebCore20ResourceResponseBase13isRedirectionEv` | `BrSesfxNNvs` |

## remove | `_ZN7WebCore20ResourceResponseBase6setURLERKN3WTF3URLE` | `2V8DFaBEiNs` |

## remove | `_ZN7WebCore20ResourceResponseBase24setExpectedContentLengthEx` | `JeTseBuY-YI` |

## remove | `_ZN7WebCore20ResourceResponseBase7setTypeENS0_4TypeE` | `hBBRr8UkWd8` |

## remove | `_ZNK7WebCore20ResourceResponseBase6sourceEv` | `Nc4OHQvmIOM` |

## remove | `_ZNK7WebCore20ResourceResponseBase8lazyInitENS0_9InitLevelE` | `o+TiqWzvS8s` |

## remove | `_ZN7WebCore20ResourceResponseBase24sanitizeHTTPHeaderFieldsENS0_16SanitizationTypeE` | `C6oLAUS1iPc` |

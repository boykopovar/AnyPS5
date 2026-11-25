# libSceNKWebKit

## add Functional

- [EmptyFrameLoaderClient error factories](../../../core/libs/prx/libSceNKWebKit/src/EmptyLoaderErrors.cpp) (libSceNKWebKit) - the `ResourceError` layout and its return in memory are not compared with a console module. The eight factories return an empty error, of type Cancellation for `cancelledError` and Null for the others, as the pinned [EmptyClients.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/loader/EmptyClients.cpp#L867-L919) does. The 80-byte [ResourceErrorAbi.hpp](../../../core/libs/prx/libSceNKWebKit/src/ResourceErrorAbi.hpp) follows the pinned ResourceErrorBase.h and curl [ResourceError.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/network/curl/ResourceError.h); the empty `failingURL` is the zeroed `URL::invalidate` state.

## remove | `_ZNK7WebCore22EmptyFrameLoaderClient12blockedErrorERKNS_15ResourceRequestE` | `X1bFhjwBxXU` |

## remove | `_ZNK7WebCore22EmptyFrameLoaderClient14cancelledErrorERKNS_15ResourceRequestE` | `BywqS3Q8szU` |

## remove | `_ZNK7WebCore22EmptyFrameLoaderClient18cannotShowURLErrorERKNS_15ResourceRequestE` | `Hn+oQHUrE1M` |

## remove | `_ZNK7WebCore22EmptyFrameLoaderClient21fileDoesNotExistErrorERKNS_16ResourceResponseE` | `fajGVxixYQg` |

## remove | `_ZNK7WebCore22EmptyFrameLoaderClient23cannotShowMIMETypeErrorERKNS_16ResourceResponseE` | `ab+5Dimx0Kw` |

## remove | `_ZNK7WebCore22EmptyFrameLoaderClient25pluginWillHandleLoadErrorERKNS_16ResourceResponseE` | `mGriYqpliqc` |

## remove | `_ZNK7WebCore22EmptyFrameLoaderClient28blockedByContentBlockerErrorERKNS_15ResourceRequestE` | `0d9jr6TzI-o` |

## remove | `_ZNK7WebCore22EmptyFrameLoaderClient31interruptedForPolicyChangeErrorERKNS_15ResourceRequestE` | `tRdCmMXTagg` |

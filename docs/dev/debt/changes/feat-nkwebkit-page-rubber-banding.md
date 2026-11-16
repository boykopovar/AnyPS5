# libSceNKWebKit

## add Functional

- [WKPage rubber banding controls](../../../core/libs/prx/libSceNKWebKit/src/PageRubberBanding.cpp) (libSceNKWebKit) - `WKPageHorizontalRubberBandingIsEnabled`, `WKPageSetEnableHorizontalRubberBanding`, `WKPageVerticalRubberBandingIsEnabled`, `WKPageSetEnableVerticalRubberBanding`, `WKPageRubberBandsAtBottom`, `WKPageSetRubberBandsAtBottom`, `WKPageRubberBandsAtLeft`, `WKPageSetRubberBandsAtLeft`, `WKPageRubberBandsAtRight`, `WKPageSetRubberBandsAtRight`, `WKPageRubberBandsAtTop` and `WKPageSetRubberBandsAtTop` configure and query directional rubber banding settings on the page in host memory. Upstream [WKPage.cpp:745-803](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebKit/UIProcess/API/C/WKPage.cpp#L745-L803) setters invoke `CRASH_IF_SUSPENDED` and forward vertical/horizontal values to WebProcess; neither IPC forwarding nor suspension checks are modeled here.

## remove | `WKPageHorizontalRubberBandingIsEnabled` | `2S-hLekYP7g` |

## remove | `WKPageRubberBandsAtBottom` | `l4ZCWwdqXko` |

## remove | `WKPageRubberBandsAtLeft` | `caZqJUbMXKc` |

## remove | `WKPageRubberBandsAtRight` | `tI-w0kRXieM` |

## remove | `WKPageRubberBandsAtTop` | `ErwSbohShIc` |

## remove | `WKPageSetEnableHorizontalRubberBanding` | `s7QZGcHR6hI` |

## remove | `WKPageSetEnableVerticalRubberBanding` | `7ZTLaTyAWFA` |

## remove | `WKPageSetRubberBandsAtBottom` | `078ew7gQLwA` |

## remove | `WKPageSetRubberBandsAtLeft` | `E6OA1UUy8+4` |

## remove | `WKPageSetRubberBandsAtRight` | `GuIr1eWRunc` |

## remove | `WKPageSetRubberBandsAtTop` | `Nn6eNKOy98M` |

## remove | `WKPageVerticalRubberBandingIsEnabled` | `p-jMGxCpEF4` |

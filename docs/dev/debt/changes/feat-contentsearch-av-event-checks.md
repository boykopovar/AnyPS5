# libSceContentSearch

## remove [libSceContentSearch additional exports]

## remove | `sceContentSearchAddAvContentCreatedEventCondition` | `HzLPPnuWTek` |

## remove | `sceContentSearchDisableAvContentCreatedEvent` | `cb8Dl69jk7A` |

## remove | `sceContentSearchEnableAvContentCreatedEvent` | `vlOqhFPOLDs` |

## remove | `sceContentSearchGetContentIdByAvContentCreatedEvent` | `2Dj56xvkS5M` |

## add Unknown function info

- [sceContentSearchAddAvContentCreatedEventCondition, sceContentSearchEnableAvContentCreatedEvent, sceContentSearchDisableAvContentCreatedEvent and sceContentSearchGetContentIdByAvContentCreatedEvent](../../../core/libs/prx/libSceContentSearch/Export.cpp) (libSceContentSearch) - follow firmware 13.42 ([reading](https://github.com/boykopovar/AnyPS5/issues/21848#issuecomment-6939429732), disassembly only, not run on a console), each answering `0x809D1001` before `sceContentSearchInit`: `AddAvContentCreatedEventCondition(int32_t, int32_t)` accepts only `(1, 1)`, else `0x809D1003`. `Enable(void)` returns `0x809D1014` until that condition is added and past that throws, as the event's registration isn't implemented, so `Disable(void)` always returns `0x809D1014`. `GetContentIdByAvContentCreatedEvent(event, contentId)` writes the 64-bit id at `event+4` when the 32-bit `event+0` is `0x1000001D`, else `0x809D1003`. Unreadable or unwritable pointers give `0x809D1003`, a host-safety fallback

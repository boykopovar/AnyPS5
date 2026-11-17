# libSceAgcDriver

## add Functional

- [Unit shadow publish range](../../../core/libs/prx/libSceAgcDriver/Graphics/src/UnitShadow.cpp) (libSceAgcDriver) - a publish copies, for each 64 KiB unit, only the span from the first to the last byte that write-backs retiled into the unit since its last publish. Bytes the slab holds outside that span (seeded from the import, or left by an earlier retile) are not copied, so a CPU store that the seed missed keeps its bytes. If two retiles leave a gap inside one unit, the seeded bytes in the gap are still copied. Freshness is still judged for the whole unit, so a CPU store anywhere in the unit drops the unit's retiled bytes too.

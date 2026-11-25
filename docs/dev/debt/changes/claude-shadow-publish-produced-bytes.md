# libSceAgcDriver

## add Functional

- [Unit shadow publish range](../../../core/libs/prx/libSceAgcDriver/Graphics/src/UnitShadow.cpp) (libSceAgcDriver) - a publish copies, for each 64 KiB unit, only the byte spans that write-backs retiled into the unit since its last publish. Slab bytes outside those spans (seeded from the import, or left by an earlier retile) are not copied, so after a publish the slab and the import agree only inside the spans. Freshness is still judged for the whole unit, so a CPU store anywhere in the unit drops the unit's retiled bytes too.

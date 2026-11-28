# libSceAgcDriver

## add Functional

- [Out-of-place DB expand of another extent over a live depth surface](../../../core/libs/prx/libSceAgcDriver/Graphics/src/DepthSurface.cpp) (libSceAgcDriver) - when the read base holds a live single-sample depth surface of the same format but another extent, the expand does not use that surface: it treats the read base as it does when no depth surface is there: a read base whose HTILE is cleared gives DB_DEPTH_CLEAR, any other reads the guest bytes as a surface of the expand's own extent. Assumed, not measured on the console: the hardware reads the guest bytes (or the clear value) in the new geometry. Seen in PPSA07411, which expands a 1920x2160 depth surface and then 1717x1931 (about 0.894 of each side, a dynamic resolution step) over the same base. A surface of another format or sample count still throws. Console result unmeasured.

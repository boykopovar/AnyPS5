# libSceAgcDriver

## add Functional

- [Barycentric built-ins beside gl_FragCoord](../../../core/libs/prx/libSceAgcDriver/Execution/src/FragmentBarycentricProbe.cpp) (libSceAgcDriver) - measured only on Mesa ANV 26.2.7 with an Intel Arc B390 (Xe3, Linux); other Intel generations and Mesa versions are not. There, a pixel program that reads `BaryCoordKHR` or `BaryCoordNoPerspKHR` without a live `gl_FragCoord` gets the second weight in the first component and a stale register in the second. At device start a probe draws both built-ins alone; if they are wrong, every barycentric built-in read is kept behind `gl_FragCoord.w > 0`, and if that is still wrong, barycentrics are emulated. Reads away from the pixel centre (`InterpolateAtOffset` of a built-in) carry the same test, but no affected device has been measured with them.

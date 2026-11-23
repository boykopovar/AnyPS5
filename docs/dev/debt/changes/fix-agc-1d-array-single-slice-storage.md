# libSceAgcDriver

## add Functional

- [1D stores to single-slice 1D arrays](../../../core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp) (libSceAgcDriver) - a storage image whose shader shape is 1D and whose T# is a 1D array with equal base and last slices is bound as a 1D view of that slice, the only one the access can address. Over more than one slice the binding still throws: where a DIM 1D store through a 1D-array T# lands (slice `BASE_ARRAY`, as the DIM 1D reads of #23688 measure) is not measured on hardware.

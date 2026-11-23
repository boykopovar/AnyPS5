# libSceAgcDriver

## add Functional

- [Sampled array views past their registered memory](../../../core/libs/prx/libSceAgcDriver/Graphics/src/TextureTiling.cpp) (libSceAgcDriver) - a sampled 1D-array, 2D-array or cube view whose slices run past the contiguous registered guest memory that holds its base throws `an array view extends past its registered guest allocation` before its image is created, instead of building the whole descriptor on the host (issue #9295 asked for 6,145 slices over 90 mapped ones). Adjacent registered ranges count as one run; a base with no registration is left as it was. The view is not clipped to the mapped slices, because what the console samples past them is not known, so the draw or dispatch that binds such a view stops with the error. Storage views are not bounded here. Not measured on hardware.

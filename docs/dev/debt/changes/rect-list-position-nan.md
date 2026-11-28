# shader

## add Functional

- [Rect-list position conversion](../../../core/shader/recompiler/SpirvBackend/src/RectListShaders.cpp) (shader recompiler) - a source position containing NaN culls its rectangle under the usual clip controls. Draws with a NaN conversion or retention override in `PA_CL_NANINF_CNTL` keep the existing finite-position diagnostic. Infinite positions and overflow while reconstructing the fourth corner also keep that diagnostic. Closes when the alternate clip modes and nonfinite fourth corners match measured hardware output.

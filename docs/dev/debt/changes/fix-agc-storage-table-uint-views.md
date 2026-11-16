# libSceAgcDriver

## add Functional

- [Storage image table UINT views](../../../core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp) (libSceAgcDriver) - refusal: a storage image table element that the recompiler flags for a UINT view (every uint storage kind, so written-only float16 and SINT formats) is rejected when the device has no storage view of the matching UINT format, so the shader reads the rejection instead of writing the image. The flag is computed from the element kind when the table is built; a table with a stored element is never reused, so no reuse record carries it. The SINT GPU test cannot prove the view on drivers that accept a mismatched view format; the recompiler unit test is the proof. Closes when: a device that rejects the mismatched view runs the SINT test.

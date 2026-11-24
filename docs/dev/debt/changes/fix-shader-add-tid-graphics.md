# shader

## add Functional

- [Buffer ADD_TID outside compute](../../../core/shader/recompiler/SpirvBackend/src/SpirvRuntimeBuffer.cpp) (shader recompiler) - vertex, local, pixel, mesh and domain programs apply the compute rule measured on RDNA2 in [#21970](https://github.com/boykopovar/AnyPS5/pull/21970) (without IDXEN the index is the lane, with IDXEN ADD_TID is ignored), using the stage's `v_mbcnt` lane; neither gfx1036 nor the PS5's gfx1013 is measured outside compute, as the [hardware oracle](../HW_ORACLE.md#limits) runs only compute. Hull programs throw on an ADD_TID V#. A pixel program with a non-IDXEN buffer access needs fragment-stage basic subgroups, with no fallback; vertex, local and domain programs fall back to one lane on hosts without subgroups there. [VertexBufferAddTid](../../../core/libs/prx/libSceAgcDriver/tests/execution/VertexBufferAddTid.cpp) pins the vertex addresses. Closes when: a graphics-stage ADD_TID access is measured on RDNA2 or PS5 hardware.

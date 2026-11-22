# shader

## change On a host subgroup narrower than 32 lanes

- On a host subgroup narrower than 32 lanes, [`threadBit`](../../../core/shader/recompiler/Translation/src/TranslationContext/ExecMask.cpp) used to index a scalar-written EXEC_LO or VCC_LO by the host subgroup lane, so lane 8 of an 8-lane subgroup read lane 0's bit. Compute programs with one host invocation per guest lane now derive `LaneId` from `gl_LocalInvocationIndex` modulo the guest wave size. The 8-lane [Wave32ScalarVccMask](../../../core/libs/prx/libSceAgcDriver/tests/execution/Wave32ScalarVccMask.cpp) fixture covers a scalar VCC write; this has not been measured on a console. Non-compute stages still use the host subgroup lane, and this change does not make cross-subgroup ballots or shuffles emulate a whole guest wave.

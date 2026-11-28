# shader

## change On a host subgroup narrower than 32 lanes

- On a host subgroup narrower than 32 lanes, [`threadBit`](../../../core/shader/recompiler/Translation/src/TranslationContext/ExecMask.cpp) used to index a scalar-written EXEC_LO or VCC_LO by the host subgroup lane, so lane 8 of an 8-lane subgroup read lane 0's bit. Wave32 compute mask bit tests use `gl_LocalInvocationIndex` modulo 32 for scalar words without subgroup-ballot dependencies. Ballot-derived words and values merged from different control-flow paths retain subgroup-local lane indexing. Cross-subgroup ballots and shuffles still cover only the host subgroup. The [Wave32ScalarVccMask](../../../core/libs/prx/libSceAgcDriver/tests/execution/Wave32ScalarVccMask.cpp) fixture covers a scalar VCC write on 8-lane Vulkan; this has yet to be measured on a console.

# libSceNpScoreRanking technical debt

### Functional

- [libSceNpScoreRanking](../../../core/libs/prx/libSceNpScoreRanking/Export.cpp) (libSceNpScoreRanking) - offline only, not measured on a PS5: the argument checks, their order, the limit of 32 title contexts and 32 requests and the array sizes follow [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/91e6afa69b0a64b0afaa0281b80f8fbb0cd88136/src/core/libraries/np/np_score/np_score.cpp#L64-L1210), and a valid call then returns `SCE_NP_ERROR_SIGNED_OUT` and finishes its request with it, as shadPS4 does when the user [has no connection](https://github.com/shadps4-emu/shadPS4/blob/91e6afa69b0a64b0afaa0281b80f8fbb0cd88136/src/core/libraries/np/np_handler/np_handler_score.cpp#L18-L26). No output is written, where shadPS4 first clears the rank, comment and info arrays. A null `boardInfo` throws: shadPS4 answers it by the title's SDK version

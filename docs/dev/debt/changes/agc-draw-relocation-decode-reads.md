# libSceAgcDriver

## add Functional

- [Draw relocation eligibility](../../../core/libs/prx/libSceAgcDriver/Execution/src/Driver/Draw/DrawCapture.cpp) (AGC driver) - a captured stage is relocatable only when its decode recorded no reads; a vertex stage whose fetch-table reads decode records, or any stage with a recorded read, stays non-relocatable, so a draw whose pointers behind those reads move compiles its stages again instead of patching the stored variants. Closes when: stages with decode-recorded reads relocate too.

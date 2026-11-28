# libSceAgcDriver

## add Functional

- [Draw relocation eligibility](../../../core/libs/prx/libSceAgcDriver/Execution/src/Driver/Draw/DrawCapture.cpp) (AGC driver) - a captured stage is relocatable when its decode recorded no reads or every recorded read attributes to a user word; attributed words join the relocatable base set so a moved pointee is re-read through the word's content-checked runs, and vertex stages carrying fetch resources relocate on the same terms. A stage with a read recorded without a word attribution or attributed past user word 63 stays non-relocatable, so a draw whose pointers move compiles its stages again instead of patching the stored variants. Closes when: unattributed reads relocate too.

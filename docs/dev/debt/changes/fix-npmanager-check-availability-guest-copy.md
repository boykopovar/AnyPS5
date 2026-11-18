# libSceNpManager

## change [sceNpCheckNpAvailability]

- [sceNpCheckNpAvailability](../../../core/libs/prx/libSceNpManager/Export.cpp): native NP initialization and online-ID lookup are not modeled. The offline host returns user-not-found for valid arguments after clearing the optional one-byte availability output; request ID 0 and a null online-ID pointer return invalid-argument before writing the output. The output is written through a checked copy, and an unwritable one returns invalid-argument.

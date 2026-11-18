# libSceAgcDriver

## change [Initial multisample color draw contents]

- [Initial multisample color draw contents](../../../core/libs/prx/libSceAgcDriver/Graphics/src/MultisampleTarget.cpp) do not support incomplete mappings, native depth aliases, unpublished overlapping color owners, arrays, volumes or mip chains. Nonuniform plain R_X contents use formatless multisampled storage writes when enabled. Without them, a per-sample fragment pass writes the original texel bits through an unsigned color attachment of the same texel size. Devices that cannot render to that attachment at the target's sample count or lack sample-rate shading still refuse the upload. Other tilings throw. Capture precedes owner creation and runs outside the targets lock; a changed capture refuses. Initialization retains the upload resources until recorder completion. Console equivalence remains unmeasured.

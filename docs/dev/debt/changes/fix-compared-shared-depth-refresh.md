# libSceAgcDriver

## change [Depth surfaces over compared shared memory]

- [Depth surfaces over compared shared memory](../../../core/libs/prx/libSceAgcDriver/Graphics/src/DepthSurface.cpp) (libSceAgcDriver): outside the write watch, a same-value CPU store is not observable and does not override pending GPU depth. A CPU edit reloads its whole tracking unit; CPU and GPU edits within one unit are not merged. Compared units are observed in bounded chunks against a shared baseline, updated only for processed units and completed GPU write-backs. These reads do not form an atomic image against concurrent CPU writes or remaps, and the texture upload reads guest memory independently of the observation.

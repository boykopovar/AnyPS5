# libSceAgcDriver

## add Functional

- [Pipeline cache saves from a buffer outside Windows](../../../core/libs/prx/libSceAgcDriver/Graphics/src/PipelineCache.cpp) (libSceAgcDriver) - on Linux and macOS a pipeline cache save still has the driver copy the whole cache into a host buffer before the file is written, so host memory rises by the size of the cache for the length of each save, and no save is put off for the host's free memory. Only Windows writes a large save through a mapped file and checks free memory first. On Windows the driver fills the mapped file under the snapshot lock at the disk's speed, so pipeline creations wait for the length of a large save; how long that is on a slow disk is not measured. The mapped save was measured on one title and one NVIDIA GPU.

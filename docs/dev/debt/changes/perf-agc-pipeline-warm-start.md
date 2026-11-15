# libSceAgcDriver

## add Functional

- [First use of a graphics pipeline](../../../core/libs/prx/libSceAgcDriver/Graphics/src/Pipeline.cpp) (libSceAgcDriver): a configuration that neither the pipeline journal's replay nor an earlier draw of the run built is compiled by `CachedPipeline` on the thread that records the draw (the committer under the draw pipeline), and the draws behind it wait, so the first visit of a scene on a GPU, driver and recompiler source version stalls recording for the driver's compiles. The registration prebuild builds fragment-shader libraries for a fixed state only. The `[pipelines]` line (`APS5_PROFILE_DRAW`, `APS5_PIPELINE_STATS`) counts these builds and their time. Closes when: a draw's pipeline is built off the recording thread without reordering draws. Labels: code

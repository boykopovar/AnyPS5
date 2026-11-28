# libSceAgcDriver

## add Silent stubs

- [Lazy pipeline ABI preparation](../../../core/libs/prx/libSceAgcDriver/Execution/src/Driver/Shaders/ShaderRegistry.cpp) extends `APS5_LAZY_SHADER_PREPARE` to explicit pipeline ABI calls after registered-header validation. Draw and dispatch preparation, including unsupported-code failures, remains at first use; first-use latency is expected. Default eager behavior is unchanged. Console pipeline-cache scheduling is not measured.

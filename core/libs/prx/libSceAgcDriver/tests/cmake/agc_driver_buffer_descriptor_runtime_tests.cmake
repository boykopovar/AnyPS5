target_include_directories(agc_driver_buffer_descriptor_runtime_tests PRIVATE $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>)

if(BUILD_TESTING)
    set_property(TEST agc_driver_buffer_descriptor_runtime APPEND PROPERTY ENVIRONMENT_MODIFICATION "ANYPS5_BUFFER_DESCRIPTOR_TABLES=set:1")
endif()

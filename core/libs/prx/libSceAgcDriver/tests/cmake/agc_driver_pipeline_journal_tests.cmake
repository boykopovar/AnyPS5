target_include_directories(agc_driver_pipeline_journal_tests PRIVATE $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>)
target_link_libraries(agc_driver_pipeline_journal_tests PRIVATE ${CMAKE_DL_LIBS})

if(BUILD_TESTING)
    set_tests_properties(agc_driver_pipeline_journal PROPERTIES TIMEOUT 240)
endif()

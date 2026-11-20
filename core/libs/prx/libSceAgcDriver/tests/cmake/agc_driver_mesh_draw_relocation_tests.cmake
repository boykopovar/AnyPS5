if(BUILD_TESTING)
    add_test(NAME agc_driver_mesh_reuse_indirect_pixels COMMAND agc_driver_mesh_draw_relocation_tests mesh-reuse-indirect)
    set_tests_properties(agc_driver_mesh_reuse_indirect_pixels PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_MESH_REUSE=1" SKIP_RETURN_CODE 77 TIMEOUT 60)
    if(WIN32)
        set_tests_properties(agc_driver_mesh_reuse_indirect_pixels PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

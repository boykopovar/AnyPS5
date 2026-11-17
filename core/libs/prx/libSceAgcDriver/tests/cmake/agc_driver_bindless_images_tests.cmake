if(BUILD_TESTING)
    agc_add_gpu_heap_test(agc_driver_bindless_images)
endif()
if(BUILD_TESTING AND TARGET agc_driver_bindless_images_tests)
    agc_add_env_test(agc_driver_bindless_images_heap agc_driver_bindless_images_tests
            ENV ANYPS5_NO_SHADER_CACHE=1 APS5_GPU_HEAP=1 NO_WORKING_DIRECTORY)
endif()

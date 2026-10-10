function(add_jpeg_decoder target runtimeTarget)
    if(NOT TARGET "${runtimeTarget}")
        message(FATAL_ERROR "JPEG decoder requires an existing runtime target: ${runtimeTarget}")
    endif()

    include(ExternalProject)
    if(NOT TARGET jpeg_turbo_external)
        set(jpeg_turbo_prefix "${CMAKE_BINARY_DIR}/libjpeg-turbo")
        if(WIN32)
            if(MSVC)
                set(jpeg_turbo_library "${jpeg_turbo_prefix}/lib/turbojpeg-static.lib")
            else()
                set(jpeg_turbo_library "${jpeg_turbo_prefix}/lib/libturbojpeg.a")
            endif()
        else()
            set(jpeg_turbo_library "${jpeg_turbo_prefix}/lib/libturbojpeg.a")
        endif()
        file(MAKE_DIRECTORY "${jpeg_turbo_prefix}/include")
        ExternalProject_Add(jpeg_turbo_external
            SOURCE_DIR "${PROJECT_SOURCE_DIR}/3rdparty/libjpeg-turbo"
            BINARY_DIR "${CMAKE_BINARY_DIR}/libjpeg-turbo-build"
            INSTALL_DIR "${jpeg_turbo_prefix}"
            CMAKE_ARGS
                -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                -DCMAKE_INSTALL_PREFIX=${jpeg_turbo_prefix}
                -DBUILD_SHARED_LIBS=OFF
                -DENABLE_STATIC=ON
                -DENABLE_SHARED=OFF
                -DWITH_SIMD=OFF
                -DWITH_TURBOJPEG=ON
                -DWITH_TOOLS=OFF
                -DWITH_TESTS=OFF
                -DCMAKE_INSTALL_LIBDIR=lib
                -DCMAKE_C_VISIBILITY_PRESET=hidden
                -DCMAKE_VISIBILITY_INLINES_HIDDEN=ON
                -DCMAKE_POSITION_INDEPENDENT_CODE=ON
                -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
                -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
                -DCMAKE_C_COMPILER_LAUNCHER=${CMAKE_C_COMPILER_LAUNCHER}
                -DCMAKE_CXX_COMPILER_LAUNCHER=${CMAKE_CXX_COMPILER_LAUNCHER}
            BUILD_BYPRODUCTS
                "${jpeg_turbo_library}"
        )
        add_library(jpeg_turbo STATIC IMPORTED GLOBAL)
        set_target_properties(jpeg_turbo PROPERTIES
            IMPORTED_LOCATION "${jpeg_turbo_library}"
            INTERFACE_INCLUDE_DIRECTORIES "${PROJECT_SOURCE_DIR}/3rdparty/libjpeg-turbo/src;${jpeg_turbo_prefix}/include"
        )
        add_dependencies(jpeg_turbo jpeg_turbo_external)
    endif()

    add_library(${target} STATIC ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/src/Jpeg.cpp)
    target_include_directories(${target} PUBLIC ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/include)
    target_include_directories(${target} SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/3rdparty/stb)
    target_link_libraries(${target} PRIVATE ${runtimeTarget} jpeg_turbo)
    set_target_properties(${target} PROPERTIES
        CXX_EXTENSIONS OFF
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        POSITION_INDEPENDENT_CODE ON
    )

    if(BUILD_TESTING)
        set(decoderTestProperties)
        if(WIN32)
            set(decoderTestProperties ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:${runtimeTarget}>")
        endif()
        anyps5_add_test(${target}_tests
            NAME ${target}
            LABEL unit
            SOURCES ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/tests/unit/JpegTests.cpp
            LINK ${target}
            PROPERTIES ${decoderTestProperties}
        )
        configure_windows_unwind(${target}_tests)
    endif()
endfunction()

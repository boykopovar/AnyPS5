cmake_minimum_required(VERSION 3.22)
project(SharedMemoryTests LANGUAGES CXX)
enable_testing()
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "Standalone shared-memory tests require Linux")
endif()
set(LIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../..")
add_executable(guest_named_shm_open_tests
    ../GuestNamedShmOpen.cpp
    Support.cpp
    ${LIBS_DIR}/prx/libkernel/File/src/SharedMemory.cpp
    ${LIBS_DIR}/prx/libc/src/CreationMask.cpp
    ${LIBS_DIR}/nid/src/Sha1.cpp)
target_include_directories(guest_named_shm_open_tests PRIVATE ${LIBS_DIR} ${LIBS_DIR}/nid/include)
target_compile_features(guest_named_shm_open_tests PRIVATE cxx_std_20)
target_link_libraries(guest_named_shm_open_tests PRIVATE rt)
add_test(NAME guest_named_shm_open COMMAND guest_named_shm_open_tests)
set_tests_properties(guest_named_shm_open PROPERTIES TIMEOUT 20)

add_executable(library_oracle_replay EXCLUDE_FROM_ALL replay.cpp)
target_include_directories(library_oracle_replay PRIVATE ${CMAKE_SOURCE_DIR}/core/libs)
target_link_libraries(library_oracle_replay PRIVATE ${CMAKE_DL_LIBS})
configure_windows_unwind(library_oracle_replay)

if(BUILD_TESTING AND Python3_Interpreter_FOUND)
    add_test(NAME library_oracle COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_library_oracle.py)
    set_tests_properties(library_oracle PROPERTIES TIMEOUT 60)
endif()

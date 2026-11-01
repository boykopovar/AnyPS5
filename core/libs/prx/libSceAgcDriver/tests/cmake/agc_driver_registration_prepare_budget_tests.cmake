add_executable(agc_driver_registration_prepare_budget_tests tests/RegistrationPrepareBudget.cpp)
target_include_directories(agc_driver_registration_prepare_budget_tests PRIVATE ${LIBS_INCLUDE_DIR})
set_target_properties(agc_driver_registration_prepare_budget_tests PROPERTIES CXX_EXTENSIONS OFF)

if(MINGW)
    target_compile_options(agc_driver_registration_prepare_budget_tests PRIVATE -fasynchronous-unwind-tables)
    target_link_options(agc_driver_registration_prepare_budget_tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()

if(BUILD_TESTING)
    add_test(NAME agc_driver_registration_prepare_budget COMMAND agc_driver_registration_prepare_budget_tests)
    set_tests_properties(agc_driver_registration_prepare_budget PROPERTIES TIMEOUT 10)
endif()

if(NOT DEFINED SOURCE_FILE)
    message(FATAL_ERROR "SOURCE_FILE was not provided")
endif()

file(READ "${SOURCE_FILE}" source)

string(FIND "${source}" "const auto reserve =" reserveStart)

if(reserveStart EQUAL -1)
    message(FATAL_ERROR "FAIL: reserve allocation not found")
endif()

string(SUBSTRING "${source}" ${reserveStart} -1 block)

string(FIND "${block}" "data.resize(data.size() + size);" resizePosition)

if(resizePosition EQUAL -1)
    message(FATAL_ERROR "FAIL: reserve resize not found")
endif()

string(SUBSTRING "${block}" 0 ${resizePosition} beforeResize)

string(FIND "${beforeResize}" "CheckedRva(static_cast<std::uint64_t>(dataRva) + data.size() + size);" endpointPosition)

if(endpointPosition EQUAL -1)
    message(FATAL_ERROR "FAIL: allocation endpoint is not checked before resize")
endif()

string(FIND "${beforeResize}" "data.max_size() - data.size()" capacityPosition)

if(capacityPosition EQUAL -1)
    message(FATAL_ERROR "FAIL: vector capacity check missing")
endif()

message(STATUS "PASS: allocation endpoint and capacity guards precede resize")
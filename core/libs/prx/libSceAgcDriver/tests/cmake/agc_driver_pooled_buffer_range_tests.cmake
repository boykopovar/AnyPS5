if(NOT ANYPS5_AGC_EXECUTION_TESTS OR "PooledBufferRange" IN_LIST ANYPS5_AGC_EXECUTION_TESTS)
    target_sources(agc_driver_pooled_buffer_range_tests PRIVATE
            Graphics/src/BufferPool.cpp Graphics/src/Resources.cpp tests/AllocationFailure.cpp tests/CommandBatchFailure.cpp)
endif()

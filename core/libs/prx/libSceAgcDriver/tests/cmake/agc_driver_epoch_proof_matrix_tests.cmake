if(BUILD_TESTING)
    set(epochProofWorkloads
        "recapture|agc_driver_draw_recapture_on_change_tests|inline+committer+finisher|ANYPS5_NO_SHADER_CACHE=1"
        "draw_cache|agc_driver_draw_cache_reuse_tests|inline+committer+finisher|ANYPS5_NO_SHADER_CACHE=1,APS5_VERIFY_DRAW_RECIPE=1,APS5_VERIFY_REGISTER_KEYS=1")
    set(epochProofCommitters
        "inline|APS5_PIPELINED_DRAWS=0"
        "committer|"
        "finisher|APS5_DRAW_PIPELINE=1,APS5_PIPELINED_DRAWS=32")
    set(epochProofDispatches
        "dispatch_inline|"
        "dispatch_pipelined|APS5_PIPELINE_DISPATCH=1")
    set(epochProofModes
        "off|APS5_NO_EPOCH_PROOFS=1"
        "on|"
        "verify|APS5_VERIFY_DRAW_PIPELINE=1")
    foreach(workload IN LISTS epochProofWorkloads)
        string(REPLACE "|" ";" workloadFields "${workload}")
        string(REPLACE "," ";" workloadFields "${workloadFields}")
        list(POP_FRONT workloadFields workloadName workloadTarget workloadCommitters)
        string(REPLACE "+" ";" workloadCommitters "${workloadCommitters}")
        foreach(committer IN LISTS epochProofCommitters)
            string(REPLACE "|" ";" committerFields "${committer}")
            string(REPLACE "," ";" committerFields "${committerFields}")
            list(POP_FRONT committerFields committerName)
            if(NOT committerName IN_LIST workloadCommitters)
                continue()
            endif()
            foreach(dispatch IN LISTS epochProofDispatches)
                string(REPLACE "|" ";" dispatchFields "${dispatch}")
                string(REPLACE "," ";" dispatchFields "${dispatchFields}")
                list(POP_FRONT dispatchFields dispatchName)
                foreach(mode IN LISTS epochProofModes)
                    string(REPLACE "|" ";" modeFields "${mode}")
                    string(REPLACE "," ";" modeFields "${modeFields}")
                    list(POP_FRONT modeFields modeName)
                    set(test agc_epoch_proofs_${workloadName}_${committerName}_${dispatchName}_${modeName})
                    add_test(NAME ${test} COMMAND ${workloadTarget})
                    set(environment ${workloadFields} ${committerFields} ${dispatchFields} ${modeFields})
                    list(FILTER environment EXCLUDE REGEX "^$")
                    set_tests_properties(${test} PROPERTIES ENVIRONMENT "${environment}" TIMEOUT 120 SKIP_RETURN_CODE 77
                            FAIL_REGULAR_EXPRESSION "FATAL:" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
                    if(WIN32)
                        set_tests_properties(${test} PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
                    endif()
                endforeach()
            endforeach()
        endforeach()
    endforeach()
endif()

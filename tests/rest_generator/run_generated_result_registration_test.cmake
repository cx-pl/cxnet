if(NOT DEFINED REST_FIXTURE OR NOT DEFINED CXNET_RUNTIME OR
    NOT DEFINED NATIVE_RUNTIME_DIR)
    message(FATAL_ERROR "Generated REST fixture runner arguments are missing.")
endif()

file(MAKE_DIRECTORY "${NATIVE_RUNTIME_DIR}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CXNET_RUNTIME}" "${NATIVE_RUNTIME_DIR}/libcxnet_runtime.so"
    RESULT_VARIABLE copy_result)
if(NOT copy_result EQUAL 0)
    message(FATAL_ERROR "Could not stage libcxnet_runtime.so for the fixture.")
endif()

execute_process(
    COMMAND "${REST_FIXTURE}"
        --generated-result-continuation-registration-failure
    RESULT_VARIABLE fixture_result)
if(NOT fixture_result EQUAL 0)
    message(FATAL_ERROR
        "Generated REST callback-registration fixture failed: ${fixture_result}")
endif()

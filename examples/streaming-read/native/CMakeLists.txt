cmake_minimum_required(VERSION 3.31)
project(streaming_read_e2e C)
include(CTest)

get_filename_component(CX_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../" ABSOLUTE)

set(CXNET_GENERATED_DIR "${CX_REPO_ROOT}/cxnet/src/.obj" CACHE PATH "Generated cxnet project")
set(CX_STREAMING_READ_GENERATED_DIR
    "${CX_REPO_ROOT}/cxnet/examples/streaming-read/.obj"
    CACHE PATH "Generated streaming-read project")
option(CX_STATIC_LINK "Link CX runtime and project references statically" ON)
if(CX_STATIC_LINK)
    set(CX_BUILD_STATIC ON CACHE BOOL "" FORCE)
else()
    set(CX_BUILD_STATIC OFF CACHE BOOL "" FORCE)
endif()
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory("${CX_REPO_ROOT}/cxcore" "${CMAKE_CURRENT_BINARY_DIR}/cxcore")
target_compile_definitions(cxcore PRIVATE CXCORE_TESTING)

set(CX_BUILDING_DEPENDENCY ON)
add_subdirectory("${CXNET_GENERATED_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/cxnet")
add_subdirectory("${CX_STREAMING_READ_GENERATED_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/streamingRead")
unset(CX_BUILDING_DEPENDENCY)

add_executable(streaming_read_e2e
    "${CX_REPO_ROOT}/cxnet/examples/streaming-read/tests/native/streaming_read_e2e.c")
target_compile_features(streaming_read_e2e PRIVATE c_std_11)
if(CX_STATIC_LINK)
    target_compile_definitions(streaming_read_e2e PRIVATE CX_STATIC_LINK)
endif()
if(MSVC)
    target_compile_options(streaming_read_e2e PRIVATE /W4 /WX /utf-8 /experimental:c11atomics)
else()
    target_compile_options(streaming_read_e2e PRIVATE -Wall -Wextra -Werror -Wno-unused-parameter)
endif()
target_include_directories(streaming_read_e2e PRIVATE
    "${CX_REPO_ROOT}/cxcore/src/.obj"
    "${CX_REPO_ROOT}/cxcore/src"
    "${CX_REPO_ROOT}/cxcore/include"
    "${CX_REPO_ROOT}/cxcore/platform/common"
    "${CXNET_GENERATED_DIR}"
    "${CX_REPO_ROOT}/cxnet/include"
    "${CX_STREAMING_READ_GENERATED_DIR}")
target_link_libraries(streaming_read_e2e PRIVATE streamingRead)
if(NOT CX_STATIC_LINK)
    foreach(_runtime_target streamingRead cxnet cxcore)
        add_custom_command(TARGET streaming_read_e2e POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:${_runtime_target}>" "$<TARGET_FILE_DIR:streaming_read_e2e>")
    endforeach()
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set_target_properties(streaming_read_e2e PROPERTIES
            BUILD_WITH_INSTALL_RPATH TRUE
            INSTALL_RPATH "$ORIGIN")
    endif()
endif()
if(WIN32)
    target_link_libraries(streaming_read_e2e PRIVATE ws2_32)
endif()
add_test(NAME streaming_read_before_eof COMMAND streaming_read_e2e)
set_tests_properties(streaming_read_before_eof PROPERTIES TIMEOUT 30)

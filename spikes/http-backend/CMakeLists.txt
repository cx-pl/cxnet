cmake_minimum_required(VERSION 3.31)
project(cxnet_curl_multi_socket_spike C)
include(CTest)

# Keep generated CX fixtures inside this build tree. Their project files
# otherwise default to shared source-tree .bin paths, which lets Windows
# x86/x64 and static/shared builds overwrite one another's executables/DLLs.
set(CX_OUTPUT_DIRECTORY_ROOT "${CMAKE_CURRENT_BINARY_DIR}/cx-generated")

if(NOT CURL_BUILD_DIR)
    message(FATAL_ERROR "Set CURL_BUILD_DIR to the CMake build directory of the pinned libcurl build")
endif()

get_filename_component(CXNET_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
get_filename_component(CXCORE_ROOT "${CXNET_ROOT}/../cxcore" ABSOLUTE)
set(CXNET_SPIKE_BUILD_TESTING ${BUILD_TESTING})
option(CXNET_BUILD_HTTP_BENCHMARKS "Build the opt-in managed HTTP loopback benchmark" OFF)
option(CXNET_BUILD_HTTP_CHUNK_FUZZER
    "Build the Clang libFuzzer target for HTTP chunk framing" OFF)
set(BUILD_TESTING OFF)
if(NOT WIN32)
    find_package(OpenSSL REQUIRED)
    find_package(Threads REQUIRED)
    if(NOT TARGET CURL::OpenSSL_Crypto)
        add_library(CURL::OpenSSL_Crypto INTERFACE IMPORTED)
        target_link_libraries(CURL::OpenSSL_Crypto INTERFACE OpenSSL::Crypto)
    endif()
endif()
find_package(ZLIB CONFIG REQUIRED COMPONENTS static)
if(NOT TARGET ZLIB::ZLIB)
    add_library(ZLIB::ZLIB INTERFACE IMPORTED)
    target_link_libraries(ZLIB::ZLIB INTERFACE ZLIB::ZLIBSTATIC)
endif()
if(NOT TARGET CURL::ZLIB)
    add_library(CURL::ZLIB INTERFACE IMPORTED)
    target_link_libraries(CURL::ZLIB INTERFACE ZLIB::ZLIB)
endif()
if(NOT TARGET CURL::brotli)
    find_library(BROTLIDEC_LIBRARY NAMES brotlidec brotlidec-static
        HINTS "${BROTLI_DIR}/lib" "${BROTLI_DIR}/lib64" REQUIRED)
    find_library(BROTLICOMMON_LIBRARY NAMES brotlicommon brotlicommon-static
        HINTS "${BROTLI_DIR}/lib" "${BROTLI_DIR}/lib64" REQUIRED)
    add_library(CURL::brotli INTERFACE IMPORTED)
    target_link_libraries(CURL::brotli INTERFACE
        "${BROTLIDEC_LIBRARY}" "${BROTLICOMMON_LIBRARY}")
endif()
if(NOT TARGET CURL::zstd)
    find_library(ZSTD_LIBRARY NAMES zstd zstd_static
        HINTS "${ZSTD_DIR}/lib" "${ZSTD_DIR}/lib64" REQUIRED)
    add_library(CURL::zstd INTERFACE IMPORTED)
    target_link_libraries(CURL::zstd INTERFACE "${ZSTD_LIBRARY}")
endif()
include("${CURL_BUILD_DIR}/libcurl-target.cmake")
if(NOT WIN32)
    # The packaged Linux curl archive is built with static Brotli and zstd
    # support. Older local target exports omit these optional dependencies,
    # so carry them explicitly as Linux package consumers do.
    target_link_libraries(CURL::libcurl_static INTERFACE CURL::brotli CURL::zstd)
endif()
if(NOT DEFINED CX_BUILD_STATIC)
    set(CX_BUILD_STATIC ON CACHE BOOL "Build the cxnet runtime statically")
endif()
add_subdirectory("${CXCORE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/cxcore")
if(CXNET_SPIKE_BUILD_TESTING)
    # Keep cxcore's own test executables out of this backend harness, while
    # still exposing test-only scheduler hooks used by cxnet's native tests.
    target_compile_definitions(cxcore PRIVATE CXCORE_TESTING)
endif()
set(BUILD_TESTING ${CXNET_SPIKE_BUILD_TESTING})
add_subdirectory("${CXNET_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/cxnet")
set(BUILD_TESTING ${CXNET_SPIKE_BUILD_TESTING})
if(NOT WIN32 AND NOT CX_BUILD_STATIC AND TARGET cxnet)
    # The generated managed module is placed under cxnet/cxnet-managed/bin,
    # while the native runtime shared library is placed in cxnet/. Give the
    # module CMake's normal build-tree dependency path to that sibling runtime
    # so shared-mode CTests do not need an externally configured LD_LIBRARY_PATH.
    # Keep the generated $ORIGIN install RPATH for packaged consumers.
    set_property(TARGET cxnet PROPERTY BUILD_WITH_INSTALL_RPATH FALSE)
endif()
if((CXNET_SPIKE_BUILD_TESTING OR CXNET_BUILD_HTTP_BENCHMARKS) AND
    WIN32 AND NOT CX_BUILD_STATIC AND TARGET cxnet)
    # The broad C end-to-end fixture exercises internal transport diagnostics.
    # Export those generated C symbols from test DLLs so the fixture can link
    # against the same shared module that public consumers use.
    set_target_properties(cxnet PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

if(CXNET_BUILD_HTTP_BENCHMARKS)
    target_compile_definitions(cxcore PRIVATE CXCORE_ARENA_BENCHMARK)
    add_executable(cxnet_http_benchmarks ../../benchmarks/http_benchmarks.c)
    target_include_directories(cxnet_http_benchmarks PRIVATE
        "${CXNET_ROOT}/src/.obj"
        "${CXCORE_ROOT}/platform/common")
    target_compile_definitions(cxnet_http_benchmarks PRIVATE
        CXCORE_ARENA_BENCHMARK)
    target_link_libraries(cxnet_http_benchmarks PRIVATE cxnet cxcore)
    if(WIN32)
        target_link_libraries(cxnet_http_benchmarks PRIVATE ws2_32)
        if(NOT CX_BUILD_STATIC)
            add_custom_command(TARGET cxnet_http_benchmarks POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:cxnet>
                    $<TARGET_FILE_DIR:cxnet_http_benchmarks>
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:cxnet_runtime>
                    $<TARGET_FILE_DIR:cxnet_http_benchmarks>
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:cxcore>
                    $<TARGET_FILE_DIR:cxnet_http_benchmarks>)
        endif()
    endif()
endif()

add_executable(cxnet_curl_multi_socket_spike multi_socket_spike.c)
target_link_libraries(cxnet_curl_multi_socket_spike PRIVATE CURL::libcurl_static)
if(BUILD_TESTING)
    add_test(NAME cxnet_curl_multi_socket_spike
        COMMAND cxnet_curl_multi_socket_spike)
endif()

add_executable(cxnet_cx_scheduler_multi_spike cx_scheduler_multi_spike.c)
target_include_directories(cxnet_cx_scheduler_multi_spike PRIVATE
    "${CXNET_ROOT}/include")
target_link_libraries(cxnet_cx_scheduler_multi_spike PRIVATE
    cxnet_runtime cxcore CURL::libcurl_static)
if(WIN32)
    target_link_libraries(cxnet_cx_scheduler_multi_spike PRIVATE ws2_32)
endif()
if(BUILD_TESTING)
    add_test(NAME cxnet_cx_scheduler_multi_spike
        COMMAND cxnet_cx_scheduler_multi_spike)
endif()

if(BUILD_TESTING)
    add_executable(cxnet_managed_http_e2e ../../tests/http_client_e2e.c)
    target_include_directories(cxnet_managed_http_e2e PRIVATE
        "${CXNET_ROOT}/src/.obj")
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        target_compile_options(cxnet_managed_http_e2e PRIVATE -Wno-clobbered)
    endif()
    target_compile_definitions(cxnet_managed_http_e2e PRIVATE CXNET_TESTING)
    if(CX_STATIC_LINK)
        target_compile_definitions(cxnet_managed_http_e2e PRIVATE CX_STATIC_LINK)
    endif()
    target_link_libraries(cxnet_managed_http_e2e PRIVATE cxnet cxcore)
    if(WIN32)
        target_link_libraries(cxnet_managed_http_e2e PRIVATE ws2_32)
        if(NOT CX_BUILD_STATIC)
            add_custom_command(TARGET cxnet_managed_http_e2e POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:cxnet>
                    $<TARGET_FILE_DIR:cxnet_managed_http_e2e>
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:cxnet_runtime>
                    $<TARGET_FILE_DIR:cxnet_managed_http_e2e>
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:cxcore>
                    $<TARGET_FILE_DIR:cxnet_managed_http_e2e>)
        endif()
    endif()
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_test(NAME cxnet_managed_http_e2e COMMAND cxnet_managed_http_e2e)
    add_test(NAME cxnet_managed_http_ambiguous_framing_connection_recovery
        COMMAND cxnet_managed_http_e2e
            --ambiguous-framing-connection-recovery)
    set_tests_properties(
        cxnet_managed_http_ambiguous_framing_connection_recovery
        PROPERTIES TIMEOUT 30)
    add_test(NAME cxnet_managed_http_truncated_stream_connection_recovery
        COMMAND cxnet_managed_http_e2e --truncated-stream-connection-recovery)
    set_tests_properties(cxnet_managed_http_truncated_stream_connection_recovery
        PROPERTIES TIMEOUT 30)
    add_test(NAME cxnet_managed_http_direct_curl_handler_send
        COMMAND cxnet_managed_http_e2e --direct-curl-handler-send)
    add_test(NAME cxnet_managed_http_direct_curl_handler_headers_first
        COMMAND cxnet_managed_http_e2e --direct-curl-handler-headers-first)
    add_test(NAME cxnet_managed_http_direct_curl_handler_cancellation
        COMMAND cxnet_managed_http_e2e --direct-curl-handler-cancellation)
    add_test(NAME cxnet_managed_http_direct_curl_handler_dispose_during_send
        COMMAND cxnet_managed_http_e2e
            --direct-curl-handler-dispose-during-send)
    add_test(NAME cxnet_managed_http_direct_curl_handler_dispose_during_body_read
        COMMAND cxnet_managed_http_e2e
            --direct-curl-handler-dispose-during-body-read)
    add_test(NAME cxnet_managed_http_chunk_size_grammar
        COMMAND cxnet_managed_http_e2e --chunk-size-grammar)
    add_test(NAME cxnet_managed_http_partial_response_header_cancel
        COMMAND cxnet_managed_http_e2e --partial-response-header-cancel)
    set_tests_properties(cxnet_managed_http_partial_response_header_cancel
        PROPERTIES TIMEOUT 30)
    add_test(NAME cxnet_managed_http_request_reservation
        COMMAND cxnet_managed_http_e2e --request-reservation)
    add_test(NAME cxnet_managed_http_segmented_chunked
        COMMAND cxnet_managed_http_e2e --segmented-chunked)
    add_test(NAME cxnet_managed_http_upload_cancel_after_headers
        COMMAND cxnet_managed_http_e2e --upload-cancel-after-headers)
    add_test(NAME cxnet_managed_http_request_dispose_during_response_wait
        COMMAND cxnet_managed_http_e2e
            --upload-response-wait-request-dispose)
    if(CXNET_BUILD_HTTP_CHUNK_FUZZER)
        if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang" OR WIN32)
            message(FATAL_ERROR
                "CXNET_BUILD_HTTP_CHUNK_FUZZER requires Clang on a Unix host")
        endif()
        if(NOT CX_STATIC_LINK)
            message(FATAL_ERROR
                "CXNET_BUILD_HTTP_CHUNK_FUZZER requires CX_STATIC_LINK=ON")
        endif()
        add_executable(cxnet_http_chunk_fuzzer
            ../../tests/http_chunk_fuzzer.c
            "${CXNET_ROOT}/src/.obj/cxnet.c"
            "${CXNET_ROOT}/platform/common/SocketScheduler.c"
            "${CXNET_ROOT}/platform/common/System/Net/CookiePublicSuffix.c"
            "${CXNET_ROOT}/platform/common/System/Net/Http/CurlHttpMessageHandler.c"
            "${CXNET_ROOT}/platform/common/System/Net/Sockets/Socket.c"
            "${CXNET_ROOT}/platform/linux/SocketPlatform.c")
        target_compile_definitions(cxnet_http_chunk_fuzzer PRIVATE
            CXNET_TESTING CX_CXNET_BUILD CX_STATIC_LINK CXNET_WITH_CURL)
        target_include_directories(cxnet_http_chunk_fuzzer PRIVATE
            "${CXNET_ROOT}/src/.obj"
            "${CXCORE_ROOT}/src/.obj"
            "${CXNET_ROOT}/include"
            "${CXCORE_ROOT}/include"
            "${CXNET_ROOT}/platform"
            "${CXNET_ROOT}/platform/common"
            "${CXNET_ROOT}/platform/linux"
            "${CXCORE_ROOT}/src"
            "${CXCORE_ROOT}/platform/common")
        target_compile_options(cxnet_http_chunk_fuzzer PRIVATE
            -fsanitize=fuzzer-no-link,address,undefined
            -fno-omit-frame-pointer -Wno-unused-function)
        target_link_options(cxnet_http_chunk_fuzzer PRIVATE
            -fsanitize=fuzzer,address,undefined)
        target_link_libraries(cxnet_http_chunk_fuzzer PRIVATE
            cxnet_runtime cxcore CURL::libcurl_static)
        set(CXNET_HTTP_CHUNK_FUZZ_CORPUS
            "${CMAKE_CURRENT_BINARY_DIR}/http_chunk_fuzz_corpus")
        file(COPY "${CXNET_ROOT}/tests/http_chunk_fuzz_corpus/"
            DESTINATION "${CXNET_HTTP_CHUNK_FUZZ_CORPUS}")
        add_test(NAME cxnet_managed_http_chunk_parser_fuzz
            COMMAND cxnet_http_chunk_fuzzer
                -runs=100000 -max_len=20000 -timeout=2
                "${CXNET_HTTP_CHUNK_FUZZ_CORPUS}")
    endif()
    set(CX_REST_CANCELLATION_GENERATED_DIR
        "${CXNET_ROOT}/tests/rest_cancellation/.obj")
    if(EXISTS "${CX_REST_CANCELLATION_GENERATED_DIR}/CMakeLists.txt")
        add_subdirectory("${CX_REST_CANCELLATION_GENERATED_DIR}"
            cxnet_rest_cancellation_fixture)
        add_test(NAME cxnet_managed_rest_request_cancellation
            COMMAND RestCancellationFixture)
        add_test(NAME cxnet_managed_rest_generated_result_publication_failure
            COMMAND RestCancellationFixture
                --generated-result-publication-failure)
        if(UNIX AND NOT CX_BUILD_STATIC)
            set_tests_properties(cxnet_managed_rest_request_cancellation
                cxnet_managed_rest_generated_result_publication_failure
                PROPERTIES ENVIRONMENT
                    "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:cxcore>:$<TARGET_FILE_DIR:cxnet>:$<TARGET_FILE_DIR:cxnet_runtime>")
        endif()
    endif()
    set(CX_REST_RESPONSE_REENTRANCY_GENERATED_DIR
        "${CXNET_ROOT}/tests/rest_response_reentrancy/.obj")
    if(EXISTS "${CX_REST_RESPONSE_REENTRANCY_GENERATED_DIR}/CMakeLists.txt")
        add_subdirectory("${CX_REST_RESPONSE_REENTRANCY_GENERATED_DIR}"
            cxnet_rest_response_reentrancy_fixture)
        add_test(NAME cxnet_managed_rest_response_reentrancy
            COMMAND RestResponseReentrancy)
        if(UNIX AND NOT CX_STATIC_LINK)
            set_tests_properties(cxnet_managed_rest_response_reentrancy
                PROPERTIES ENVIRONMENT
                    "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:cxcore>:$<TARGET_FILE_DIR:cxnet>:$<TARGET_FILE_DIR:cxnet_runtime>")
        endif()
    endif()
    set(CX_REST_GENERATOR_GENERATED_DIR
        "${CXNET_ROOT}/tests/rest_generator/.obj")
    if(EXISTS "${CX_REST_GENERATOR_GENERATED_DIR}/CMakeLists.txt")
        add_subdirectory("${CX_REST_GENERATOR_GENERATED_DIR}"
            cxnet_rest_generator_fixture)
        add_test(NAME cxnet_managed_rest_generator_recording
            COMMAND RestGeneratorFixture)
        add_test(NAME cxnet_managed_rest_multipart_setup_failure
            COMMAND RestGeneratorFixture --multipart-setup-failure)
        if(UNIX AND NOT CX_STATIC_LINK)
            set_tests_properties(cxnet_managed_rest_generator_recording
                cxnet_managed_rest_multipart_setup_failure
                PROPERTIES ENVIRONMENT
                    "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:cxcore>:$<TARGET_FILE_DIR:cxnet>:$<TARGET_FILE_DIR:cxnet_runtime>:$<TARGET_FILE_DIR:RestGeneratorFixture>")
        endif()
        if(UNIX AND NOT CX_STATIC_LINK)
            set(CX_REST_GENERATOR_NATIVE_RUNTIME_DIR
                "${CMAKE_CURRENT_BINARY_DIR}/cxnet_rest_generator_native_runtime")
            file(MAKE_DIRECTORY "${CX_REST_GENERATOR_NATIVE_RUNTIME_DIR}")
            add_test(
                NAME cxnet_managed_rest_generated_result_continuation_registration_failure
                COMMAND ${CMAKE_COMMAND}
                    "-DREST_FIXTURE=$<TARGET_FILE:RestGeneratorFixture>"
                    "-DCXNET_RUNTIME=$<TARGET_FILE:cxnet_runtime>"
                    "-DNATIVE_RUNTIME_DIR=${CX_REST_GENERATOR_NATIVE_RUNTIME_DIR}"
                    -P "${CXNET_ROOT}/tests/rest_generator/run_generated_result_registration_test.cmake")
            set_tests_properties(
                cxnet_managed_rest_generated_result_continuation_registration_failure
                PROPERTIES ENVIRONMENT
                    "LD_LIBRARY_PATH=${CX_REST_GENERATOR_NATIVE_RUNTIME_DIR}")
        else()
            add_test(
                NAME cxnet_managed_rest_generated_result_continuation_registration_failure
                COMMAND RestGeneratorFixture
                    --generated-result-continuation-registration-failure)
        endif()
    endif()
    set(CX_REST_MAP_COLLECTION_GENERATED_DIR
        "${CXNET_ROOT}/tests/rest_map_collection_probe/.obj")
    if(EXISTS "${CX_REST_MAP_COLLECTION_GENERATED_DIR}/CMakeLists.txt")
        add_subdirectory("${CX_REST_MAP_COLLECTION_GENERATED_DIR}"
            cxnet_rest_map_collection_probe)
        add_test(
            NAME cxnet_managed_rest_map_collection_wire
            COMMAND "${Python3_EXECUTABLE}"
                "${CXNET_ROOT}/tests/rest_map_collection_probe/run_wire.py"
                "$<TARGET_FILE:RestMapCollectionProbe>")
        set_tests_properties(cxnet_managed_rest_map_collection_wire
            PROPERTIES TIMEOUT 40)
        if(UNIX AND NOT CX_STATIC_LINK)
            set_tests_properties(
                cxnet_managed_rest_map_collection_wire
                PROPERTIES ENVIRONMENT
                    "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:RestMapCollectionProbe>")
        endif()
    endif()
    set(CX_HTTP_CONTENT_SETUP_GENERATED_DIR
        "${CXNET_ROOT}/tests/http_content_setup/.obj")
    if(EXISTS "${CX_HTTP_CONTENT_SETUP_GENERATED_DIR}/CMakeLists.txt")
        add_subdirectory("${CX_HTTP_CONTENT_SETUP_GENERATED_DIR}"
            cxnet_http_content_setup_fixture)
        add_test(NAME cxnet_managed_http_content_buffering_started_failure
            COMMAND HttpContentSetup)
        if(UNIX AND NOT CX_STATIC_LINK)
            set_tests_properties(
                cxnet_managed_http_content_buffering_started_failure
                PROPERTIES ENVIRONMENT
                    "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:cxcore>:$<TARGET_FILE_DIR:cxnet>:$<TARGET_FILE_DIR:cxnet_runtime>")
        endif()
    endif()
    add_test(NAME cxnet_managed_http_redirect_cleanup_injection
        COMMAND cxnet_managed_http_e2e --redirect-cleanup-injection)
    add_test(NAME cxnet_managed_http_send_cancellation_registration_rollback
        COMMAND cxnet_managed_http_e2e
            --send-cancellation-registration-rollback)
    add_test(NAME cxnet_managed_http_send_completion_registration_failure
        COMMAND cxnet_managed_http_e2e
            --send-completion-registration-failure)
    add_test(NAME cxnet_managed_rest_result_continuation_registration_failure
        COMMAND cxnet_managed_http_e2e
            --rest-result-continuation-registration-failure)
    add_test(NAME cxnet_managed_http_convenience_continuation_registration_failure
        COMMAND cxnet_managed_http_e2e
            --http-convenience-continuation-registration-failure)
    add_test(NAME cxnet_managed_http_send_handler_continuation_failure
        COMMAND cxnet_managed_http_e2e
            --send-handler-continuation-failure)
    add_test(NAME cxnet_managed_http_content_buffer_continuation_registration_failure
        COMMAND cxnet_managed_http_e2e
            --content-buffer-continuation-registration-failure)
    add_test(NAME cxnet_managed_stream_copy_continuation_registration_failure
        COMMAND cxnet_managed_http_e2e
            --stream-copy-continuation-registration-failure)
    add_test(NAME cxnet_managed_http_curl_upload_continuation_registration_failure
        COMMAND cxnet_managed_http_e2e
            --curl-upload-continuation-registration-failure)
    add_test(NAME cxnet_managed_http_upload_setup_failure_reservation_rollback
        COMMAND cxnet_managed_http_e2e
            --upload-setup-failure-reservation-rollback)
    add_test(NAME cxnet_managed_http_upload_timeout_cancel_race
        COMMAND cxnet_managed_http_e2e --upload-timeout-cancel-race)
    add_test(NAME cxnet_managed_http_upload_timeout_dispose_race
        COMMAND cxnet_managed_http_e2e --upload-timeout-dispose-race)
    add_test(NAME cxnet_managed_http_queued_timeout_cancel_race
        COMMAND cxnet_managed_http_e2e --queued-timeout-cancel-race)
    set_tests_properties(cxnet_managed_http_queued_timeout_cancel_race
        PROPERTIES TIMEOUT 30)
    add_test(NAME cxnet_managed_http_queued_client_dispose
        COMMAND cxnet_managed_http_e2e --queued-client-dispose)
    add_test(NAME cxnet_managed_rest_response_publication_failure
        COMMAND cxnet_managed_http_e2e --rest-response-publication-failure)
    add_test(NAME cxnet_managed_http_request_content_replacement_disposal
        COMMAND cxnet_managed_http_e2e
            --request-content-replacement-disposal)
    add_test(NAME cxnet_managed_http_response_content_replacement_disposal_race
        COMMAND cxnet_managed_http_e2e
            --response-content-replacement-disposal-race)
    add_test(NAME cxnet_managed_http_concurrent_header_mutation_send
        COMMAND cxnet_managed_http_e2e --concurrent-header-mutation-send)
    add_test(NAME cxnet_managed_http_concurrent_client_setting_send
        COMMAND cxnet_managed_http_e2e --concurrent-client-setting-send)
    add_test(NAME cxnet_managed_http_concurrent_request_send_reservation
        COMMAND cxnet_managed_http_e2e
            --concurrent-request-send-reservation)
    add_test(NAME cxnet_managed_http_concurrent_request_mutation_send
        COMMAND cxnet_managed_http_e2e --concurrent-request-mutation-send)
    add_test(NAME cxnet_managed_http_concurrent_content_stream_read
        COMMAND cxnet_managed_http_e2e --concurrent-content-stream-read)
    add_test(NAME cxnet_managed_http_concurrent_content_stream_dispose
        COMMAND cxnet_managed_http_e2e --concurrent-content-stream-dispose)
    add_test(NAME cxnet_managed_http_response_owned_stream_dispose_race
        COMMAND cxnet_managed_http_e2e --response-owned-stream-dispose-race)
    add_test(NAME cxnet_managed_http_response_send_lifetime_dispose_race
        COMMAND cxnet_managed_http_e2e --response-send-lifetime-dispose-race)
    add_test(NAME cxnet_managed_http_response_read_scheduler_post_failure
        COMMAND cxnet_managed_http_e2e
            --response-read-scheduler-post-failure)
    add_test(NAME cxnet_managed_http_response_read_cancellation_registration_failure
        COMMAND cxnet_managed_http_e2e
            --response-read-cancellation-registration-failure)
    add_test(NAME cxnet_managed_http_curl_response_stream_dispose_read_race
        COMMAND cxnet_managed_http_e2e
            --curl-response-stream-dispose-read-race)
    add_test(NAME cxnet_managed_http_curl_upload_stream_dispose_read_race
        COMMAND cxnet_managed_http_e2e
            --curl-upload-stream-dispose-read-race)
    add_test(NAME cxnet_managed_http_multipart_serialization_dispose_race
        COMMAND cxnet_managed_http_e2e
            --multipart-serialization-dispose-race)
    add_test(NAME cxnet_managed_http_multipart_dispose_failure
        COMMAND cxnet_managed_http_e2e --multipart-dispose-failure)
    add_test(NAME cxnet_managed_http_multipart_disposition_validation
        COMMAND cxnet_managed_http_e2e
            --multipart-disposition-validation)
    add_test(NAME cxnet_managed_http_cross_thread_client_dispose_send
        COMMAND cxnet_managed_http_e2e
            --cross-thread-client-dispose-send)
    add_test(NAME cxnet_managed_http_headers_first_client_dispose
        COMMAND cxnet_managed_http_e2e --headers-first-client-dispose)
    if(UNIX AND NOT CX_BUILD_STATIC)
        set_tests_properties(cxnet_managed_http_e2e
            cxnet_managed_http_truncated_stream_connection_recovery
            cxnet_managed_http_chunk_size_grammar
            cxnet_managed_http_partial_response_header_cancel
            cxnet_managed_http_request_reservation
            cxnet_managed_http_segmented_chunked
            cxnet_managed_http_redirect_cleanup_injection
            cxnet_managed_http_send_cancellation_registration_rollback
            cxnet_managed_http_send_completion_registration_failure
            cxnet_managed_rest_result_continuation_registration_failure
            cxnet_managed_http_convenience_continuation_registration_failure
            cxnet_managed_http_send_handler_continuation_failure
            cxnet_managed_http_content_buffer_continuation_registration_failure
            cxnet_managed_http_curl_upload_continuation_registration_failure
            cxnet_managed_http_upload_setup_failure_reservation_rollback
            cxnet_managed_http_upload_timeout_cancel_race
            cxnet_managed_rest_response_publication_failure
            cxnet_managed_http_request_content_replacement_disposal
            cxnet_managed_http_response_content_replacement_disposal_race
            cxnet_managed_http_cross_thread_client_dispose_send
            cxnet_managed_http_concurrent_header_mutation_send
            cxnet_managed_http_concurrent_client_setting_send
            cxnet_managed_http_headers_first_client_dispose
            cxnet_managed_http_concurrent_request_send_reservation
            cxnet_managed_http_concurrent_request_mutation_send
            cxnet_managed_http_concurrent_content_stream_read
            cxnet_managed_http_concurrent_content_stream_dispose
            cxnet_managed_http_response_owned_stream_dispose_race
            cxnet_managed_http_response_send_lifetime_dispose_race
            cxnet_managed_http_response_read_scheduler_post_failure
            cxnet_managed_http_response_read_cancellation_registration_failure
            cxnet_managed_http_curl_response_stream_dispose_read_race
            cxnet_managed_http_curl_upload_stream_dispose_read_race
            cxnet_managed_http_multipart_serialization_dispose_race
            cxnet_managed_http_multipart_dispose_failure
            cxnet_managed_http_multipart_disposition_validation
            PROPERTIES ENVIRONMENT
                "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:cxcore>:$<TARGET_FILE_DIR:cxnet>:$<TARGET_FILE_DIR:cxnet_runtime>")
    endif()
    add_executable(cxnet_managed_http_public_smoke ../../tests/http_public_smoke.c)
    target_include_directories(cxnet_managed_http_public_smoke PRIVATE
        "${CXNET_ROOT}/src/.obj")
    target_link_libraries(cxnet_managed_http_public_smoke PRIVATE cxnet cxcore)
    if(WIN32)
        target_link_libraries(cxnet_managed_http_public_smoke PRIVATE ws2_32)
        if(NOT CX_BUILD_STATIC)
            add_custom_command(TARGET cxnet_managed_http_public_smoke POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:cxnet>
                    $<TARGET_FILE_DIR:cxnet_managed_http_public_smoke>
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:cxnet_runtime>
                    $<TARGET_FILE_DIR:cxnet_managed_http_public_smoke>
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:cxcore>
                    $<TARGET_FILE_DIR:cxnet_managed_http_public_smoke>)
        endif()
    endif()
    add_test(NAME cxnet_managed_http_public_smoke
        COMMAND "${Python3_EXECUTABLE}"
            "${CXNET_ROOT}/tests/redirect_fixture.py"
            --require-stream-cancel
            --require-concurrent-cancel
            --require-concurrent-dispose
            --require-precanceled-upload
            "$<TARGET_FILE:cxnet_managed_http_public_smoke>")
    add_test(NAME cxnet_managed_http_scheduler_shutdown
        COMMAND "${Python3_EXECUTABLE}"
            "${CXNET_ROOT}/tests/redirect_fixture.py"
            --require-scheduler-shutdown
            "$<TARGET_FILE:cxnet_managed_http_public_smoke>")
    if(UNIX AND NOT CX_BUILD_STATIC)
        set_tests_properties(cxnet_managed_http_scheduler_shutdown
            PROPERTIES ENVIRONMENT
                "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:cxcore>:$<TARGET_FILE_DIR:cxnet>:$<TARGET_FILE_DIR:cxnet_runtime>")
    endif()
    add_executable(cxnet_managed_http_redirect_e2e ../../tests/http_redirect_e2e.c)
    target_include_directories(cxnet_managed_http_redirect_e2e PRIVATE
        "${CXNET_ROOT}/src/.obj")
    if(CX_STATIC_LINK)
        target_compile_definitions(cxnet_managed_http_redirect_e2e PRIVATE CX_STATIC_LINK)
    endif()
    target_link_libraries(cxnet_managed_http_redirect_e2e PRIVATE cxnet cxcore)
    if(NOT WIN32)
        target_link_libraries(cxnet_managed_http_redirect_e2e PRIVATE Threads::Threads)
    endif()
    if(WIN32)
        target_link_libraries(cxnet_managed_http_redirect_e2e PRIVATE ws2_32)
        if(NOT CX_BUILD_STATIC)
            add_custom_command(TARGET cxnet_managed_http_redirect_e2e POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:cxnet>
                    $<TARGET_FILE_DIR:cxnet_managed_http_redirect_e2e>
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:cxnet_runtime>
                    $<TARGET_FILE_DIR:cxnet_managed_http_redirect_e2e>
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:cxcore>
                    $<TARGET_FILE_DIR:cxnet_managed_http_redirect_e2e>)
        endif()
    endif()
    add_test(NAME cxnet_managed_http_redirect_e2e
        COMMAND "${Python3_EXECUTABLE}"
            "${CXNET_ROOT}/tests/redirect_fixture.py"
            "$<TARGET_FILE:cxnet_managed_http_redirect_e2e>")
    add_test(NAME cxnet_managed_https_e2e
        COMMAND "${Python3_EXECUTABLE}"
            "${CXNET_ROOT}/tests/https_fixture.py"
            "$<TARGET_FILE:cxnet_managed_http_e2e>")
    add_test(NAME cxnet_managed_https_policy_snapshot
        COMMAND "${Python3_EXECUTABLE}"
            "${CXNET_ROOT}/tests/https_fixture.py"
            "$<TARGET_FILE:cxnet_managed_http_e2e>"
            --https-policy-only)
    if(UNIX AND NOT CX_BUILD_STATIC)
        set_tests_properties(cxnet_managed_http_redirect_e2e
            cxnet_managed_https_e2e
            cxnet_managed_https_policy_snapshot PROPERTIES ENVIRONMENT
                "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:cxcore>:$<TARGET_FILE_DIR:cxnet>:$<TARGET_FILE_DIR:cxnet_runtime>")
    endif()
endif()

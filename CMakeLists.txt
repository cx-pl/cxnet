cmake_minimum_required(VERSION 3.31)
project(cxnet C)
include(CTest)

if(NOT TARGET cxcore)
    message(FATAL_ERROR "cxnet native runtime requires cxcore to be added first")
endif()

if(CX_BUILD_STATIC)
    add_library(cxnet_runtime STATIC
        platform/common/System/Net/CookiePublicSuffix.c
        platform/common/System/Net/Sockets/Socket.c
        platform/common/SocketScheduler.c)
else()
    add_library(cxnet_runtime SHARED
        platform/common/System/Net/CookiePublicSuffix.c
        platform/common/System/Net/Sockets/Socket.c
        platform/common/SocketScheduler.c)
    set_target_properties(cxnet_runtime PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()
target_compile_definitions(cxnet_runtime PRIVATE CX_CXNET_BUILD)
if(BUILD_TESTING)
    target_compile_definitions(cxnet_runtime PRIVATE CXNET_TESTING)
endif()
target_include_directories(cxnet_runtime PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/.obj
    ${CMAKE_CURRENT_SOURCE_DIR}/../cxcore/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../cxcore/src)
target_link_libraries(cxnet_runtime PUBLIC cxcore)
if(NOT WIN32)
    find_package(Threads REQUIRED)
    target_link_libraries(cxnet_runtime PUBLIC Threads::Threads)
endif()

if(WIN32)
    target_sources(cxnet_runtime PRIVATE platform/win/SocketPlatform.c)
    target_link_libraries(cxnet_runtime PRIVATE ws2_32)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(cxnet_runtime PRIVATE platform/linux/SocketPlatform.c)
else()
    message(FATAL_ERROR "cxnet does not support ${CMAKE_SYSTEM_NAME} yet")
endif()

# When the CX compiler has emitted the managed module, also build it so tests
# exercise the real CX exception factories and public socket validations.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/.obj/CMakeLists.txt")
    # The generated CX module must use the same link mode as the native
    # runtime that owns it. Otherwise shared Windows consumers compile its
    # public declarations as dllimport while the module is actually static.
    if(CX_BUILD_STATIC)
        set(CX_STATIC_LINK ON CACHE BOOL "Link CX runtime and project references statically" FORCE)
    else()
        set(CX_STATIC_LINK OFF CACHE BOOL "Link CX runtime and project references statically" FORCE)
    endif()
    # Generated CX project files default to shared source-tree .bin paths.
    # Keep each build tree's generated modules, dependency DLLs, and fixtures
    # isolated so Windows x86/x64 and static/shared builds cannot cross-load
    # artifacts produced for another target architecture or link mode.
    set(CX_OUTPUT_DIRECTORY_ROOT "${CMAKE_CURRENT_BINARY_DIR}/cx-generated")
    set(CX_BUILDING_DEPENDENCY ON)
    add_subdirectory(src/.obj cxnet_generated)
    unset(CX_BUILDING_DEPENDENCY)
    if(BUILD_TESTING)
        target_compile_definitions(cxnet PRIVATE CXNET_TESTING)
    endif()
    target_link_libraries(cxnet PRIVATE cxnet_runtime)
    # Generated project files default their output into the source tree.
    # Override that here so x86/x64 and static/shared build trees cannot reuse
    # or overwrite each other's managed module binaries.
    set_target_properties(cxnet PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/cxnet-managed/bin"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/cxnet-managed/bin"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/cxnet-managed/lib")
endif()

if(BUILD_TESTING)
    add_executable(cxnet_socket_tests tests/socket_tests.c)
    target_compile_definitions(cxnet_socket_tests PRIVATE CXNET_TESTING)
    target_include_directories(cxnet_socket_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(cxnet_socket_tests PRIVATE cxnet_runtime cxcore)
    if(WIN32)
        target_link_libraries(cxnet_socket_tests PRIVATE ws2_32)
    endif()
    if(WIN32 AND NOT CX_BUILD_STATIC)
        add_custom_command(TARGET cxnet_socket_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:cxnet_runtime>
                $<TARGET_FILE_DIR:cxnet_socket_tests>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:cxcore>
                $<TARGET_FILE_DIR:cxnet_socket_tests>)
    endif()
    add_test(NAME cxnet_socket_tests COMMAND cxnet_socket_tests)

    add_executable(cxnet_socket_scheduler_tests tests/socket_scheduler_tests.c)
    target_include_directories(cxnet_socket_scheduler_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(cxnet_socket_scheduler_tests PRIVATE cxnet_runtime)
    if(WIN32)
        target_link_libraries(cxnet_socket_scheduler_tests PRIVATE ws2_32)
    endif()
    if(WIN32 AND NOT CX_BUILD_STATIC)
        add_custom_command(TARGET cxnet_socket_scheduler_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:cxnet_runtime> $<TARGET_FILE_DIR:cxnet_socket_scheduler_tests>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:cxcore> $<TARGET_FILE_DIR:cxnet_socket_scheduler_tests>)
    endif()
    add_test(NAME cxnet_socket_scheduler_tests COMMAND cxnet_socket_scheduler_tests)

    if(TARGET cxnet)
        add_executable(cxnet_dns_async_tests tests/dns_async_tests.c)
        target_compile_definitions(cxnet_dns_async_tests PRIVATE CXNET_TESTING)
        if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
            target_compile_options(cxnet_dns_async_tests PRIVATE -Wno-clobbered)
        endif()
        target_include_directories(cxnet_dns_async_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/.obj
            ${CMAKE_CURRENT_SOURCE_DIR}/platform/common)
        if(CX_STATIC_LINK)
            target_compile_definitions(cxnet_dns_async_tests PRIVATE CX_STATIC_LINK)
        endif()
        target_link_libraries(cxnet_dns_async_tests PRIVATE cxnet)
        if(WIN32)
            target_link_libraries(cxnet_dns_async_tests PRIVATE ws2_32)
        endif()
        if(NOT CX_STATIC_LINK)
            add_custom_command(TARGET cxnet_dns_async_tests POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:cxnet>
                    $<TARGET_FILE_DIR:cxnet_dns_async_tests>
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:cxcore>
                    $<TARGET_FILE_DIR:cxnet_dns_async_tests>)
        endif()
        add_test(NAME cxnet_dns_async_tests COMMAND cxnet_dns_async_tests)
        add_test(NAME cxnet_socket_cross_thread_pending_dispose
            COMMAND cxnet_dns_async_tests --pending-write-cross-thread-dispose)
        add_test(NAME cxnet_socket_saturated_write
            COMMAND cxnet_dns_async_tests --saturated-socket-write)
        add_test(NAME cxnet_socket_concurrent_write_rejection
            COMMAND cxnet_dns_async_tests --socket-concurrent-write-rejection)
        set_tests_properties(cxnet_socket_concurrent_write_rejection
            PROPERTIES TIMEOUT 30)
        add_test(NAME cxnet_socket_pending_write_cancel
            COMMAND cxnet_dns_async_tests --pending-socket-write-cancel)
        add_test(NAME cxnet_socket_pending_write_ready_cancel_race
            COMMAND cxnet_dns_async_tests
                --pending-socket-write-ready-cancel-race)
        add_test(NAME cxnet_network_stream_pending_write_ready_cancel_race
            COMMAND cxnet_dns_async_tests
                --network-stream-pending-write-ready-cancel-race)
        add_test(NAME cxnet_socket_pending_write_shutdown
            COMMAND cxnet_dns_async_tests --pending-write-shutdown)
        add_test(NAME cxnet_network_stream_pending_write_shutdown
            COMMAND cxnet_dns_async_tests
                --network-stream-pending-write-shutdown)
        add_test(NAME cxnet_socket_write_receive_shutdown
            COMMAND cxnet_dns_async_tests --write-receive-shutdown)
        add_test(NAME cxnet_network_stream_write_receive_shutdown
            COMMAND cxnet_dns_async_tests
                --network-stream-write-receive-shutdown)
        add_test(NAME cxnet_socket_pending_read_cancel
            COMMAND cxnet_dns_async_tests --pending-socket-read-cancel)
        add_test(NAME cxnet_network_stream_pending_read_cancel
            COMMAND cxnet_dns_async_tests
                --network-stream-pending-read-cancel)
        set_tests_properties(cxnet_network_stream_pending_read_cancel
            PROPERTIES TIMEOUT 30)
        add_test(NAME cxnet_socket_pending_read_ready_cancel_race
            COMMAND cxnet_dns_async_tests
                --pending-socket-read-ready-cancel-race)
        add_test(NAME cxnet_network_stream_pending_read_ready_cancel_race
            COMMAND cxnet_dns_async_tests
                --network-stream-pending-read-ready-cancel-race)
        set_tests_properties(cxnet_network_stream_pending_read_ready_cancel_race
            PROPERTIES TIMEOUT 30)
        add_test(NAME cxnet_socket_full_duplex_independent_cancel
            COMMAND cxnet_dns_async_tests --full-duplex-independent-cancel)
        add_test(NAME cxnet_network_stream_full_duplex_independent_cancel
            COMMAND cxnet_dns_async_tests
                --network-stream-full-duplex-independent-cancel)
        set_tests_properties(cxnet_network_stream_full_duplex_independent_cancel
            PROPERTIES TIMEOUT 30)
        add_test(NAME cxnet_network_stream_read_dispose_start_race
            COMMAND cxnet_dns_async_tests
                --network-stream-read-dispose-start-race)
        add_test(NAME cxnet_network_stream_nonowning_read_dispose_start_race
            COMMAND cxnet_dns_async_tests
                --network-stream-nonowning-read-dispose-start-race)
        set_tests_properties(cxnet_network_stream_nonowning_read_dispose_start_race
            PROPERTIES TIMEOUT 30)
        add_test(NAME cxnet_socket_write_dispose_start_race
            COMMAND cxnet_dns_async_tests --write-dispose-start-race)
        add_test(NAME cxnet_network_stream_write_dispose_start_race
            COMMAND cxnet_dns_async_tests
                --network-stream-write-dispose-start-race)
        add_test(NAME cxnet_network_stream_nonowning_write_dispose_start_race
            COMMAND cxnet_dns_async_tests
                --network-stream-nonowning-write-dispose-start-race)
        set_tests_properties(cxnet_network_stream_nonowning_write_dispose_start_race
            PROPERTIES TIMEOUT 120)
        add_test(NAME cxnet_socket_write_dispose_handle_reuse
            COMMAND cxnet_dns_async_tests --write-dispose-handle-reuse)
        add_test(NAME cxnet_socket_receive_shutdown
            COMMAND cxnet_dns_async_tests --receive-shutdown)
        add_test(NAME cxnet_socket_pending_receive_shutdown
            COMMAND cxnet_dns_async_tests --pending-receive-shutdown)
        add_test(NAME cxnet_network_stream_pending_receive_shutdown
            COMMAND cxnet_dns_async_tests
                --network-stream-pending-receive-shutdown)
        add_test(NAME cxnet_socket_multiple_accept_dispose
            COMMAND cxnet_dns_async_tests --multiple-accept-dispose)
        add_test(NAME cxnet_socket_concurrent_accepts_succeed
            COMMAND cxnet_dns_async_tests --concurrent-accepts-succeed)
        set_tests_properties(cxnet_socket_concurrent_accepts_succeed
            PROPERTIES TIMEOUT 30)
        add_test(NAME cxnet_socket_cancel_after_native_accept
            COMMAND cxnet_dns_async_tests --accept-cancel-after-native-accept)
        set_tests_properties(cxnet_socket_cancel_after_native_accept
            PROPERTIES TIMEOUT 30)
        add_test(NAME cxnet_socket_host_connect_cross_thread_dispose
            COMMAND cxnet_dns_async_tests --host-connect-cross-thread-dispose)
        add_test(NAME cxnet_endpoint_connect_cross_thread_dispose
            COMMAND cxnet_dns_async_tests
                --endpoint-connect-cross-thread-dispose)
        add_test(NAME cxnet_endpoint_connect_failure_reuse
            COMMAND cxnet_dns_async_tests
                --endpoint-connect-failure-reuse)
        add_test(NAME cxnet_endpoint_connect_reset_buffer_option_race
            COMMAND cxnet_dns_async_tests
                --connect-reset-buffer-option-race)
        add_test(NAME cxnet_endpoint_connect_duplicate_pending
            COMMAND cxnet_dns_async_tests
                --endpoint-connect-duplicate-pending)
        add_test(NAME cxnet_network_stream_peer_reset
            COMMAND cxnet_dns_async_tests --network-stream-peer-reset)
        set_tests_properties(cxnet_network_stream_peer_reset
            PROPERTIES TIMEOUT 30)
        add_test(NAME cxnet_network_stream_pending_write_peer_reset
            COMMAND cxnet_dns_async_tests
                --network-stream-pending-write-peer-reset)
        set_tests_properties(cxnet_network_stream_pending_write_peer_reset
            PROPERTIES TIMEOUT 30)
        add_test(NAME cxnet_socket_pending_write_peer_reset
            COMMAND cxnet_dns_async_tests --socket-pending-write-peer-reset)
        set_tests_properties(cxnet_socket_pending_write_peer_reset
            PROPERTIES TIMEOUT 30)
        add_test(NAME cxnet_network_stream_pending_write_dispose
            COMMAND cxnet_dns_async_tests --network-stream-pending-write-dispose)
        add_test(NAME cxnet_network_stream_nonowning_pending_write_dispose
            COMMAND cxnet_dns_async_tests --network-stream-nonowning-pending-write-dispose)
        add_test(NAME cxnet_network_stream_nonowning_pending_write_socket_dispose
            COMMAND cxnet_dns_async_tests
                --network-stream-nonowning-pending-write-socket-dispose)
        set_tests_properties(
            cxnet_network_stream_nonowning_pending_write_socket_dispose
            PROPERTIES TIMEOUT 30)
        add_test(NAME cxnet_network_stream_nonowning_pending_read_socket_dispose
            COMMAND cxnet_dns_async_tests
                --network-stream-nonowning-pending-read-socket-dispose)
        set_tests_properties(
            cxnet_network_stream_nonowning_pending_read_socket_dispose
            PROPERTIES TIMEOUT 30)
        add_test(NAME cxnet_network_stream_concurrent_read_rejection
            COMMAND cxnet_dns_async_tests
                --network-stream-concurrent-read-rejection)
        set_tests_properties(cxnet_network_stream_concurrent_read_rejection
            PROPERTIES TIMEOUT 30)
        add_test(NAME cxnet_network_stream_concurrent_write_rejection
            COMMAND cxnet_dns_async_tests
                --network-stream-concurrent-write-rejection)
        set_tests_properties(cxnet_network_stream_concurrent_write_rejection
            PROPERTIES TIMEOUT 30)
        add_test(NAME cxnet_accepted_clients_survive_listener_dispose
            COMMAND cxnet_dns_async_tests --accepted-clients-survive-listener-dispose)
        add_executable(cxnet_dns_shutdown_tests tests/dns_shutdown_tests.c)
        target_compile_definitions(cxnet_dns_shutdown_tests PRIVATE CXNET_TESTING)
        target_include_directories(cxnet_dns_shutdown_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/.obj
            ${CMAKE_CURRENT_SOURCE_DIR}/platform/common)
        if(CX_STATIC_LINK)
            target_compile_definitions(cxnet_dns_shutdown_tests PRIVATE CX_STATIC_LINK)
        endif()
        target_link_libraries(cxnet_dns_shutdown_tests PRIVATE cxnet)
        if(WIN32)
            target_link_libraries(cxnet_dns_shutdown_tests PRIVATE ws2_32)
        endif()
        if(NOT CX_STATIC_LINK)
            add_custom_command(TARGET cxnet_dns_shutdown_tests POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:cxnet>
                    $<TARGET_FILE_DIR:cxnet_dns_shutdown_tests>
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:cxcore>
                    $<TARGET_FILE_DIR:cxnet_dns_shutdown_tests>)
        endif()
        add_test(NAME cxnet_dns_shutdown_tests COMMAND cxnet_dns_shutdown_tests)

        add_executable(cxnet_ip_address_tests tests/ip_address_tests.c)
        target_include_directories(cxnet_ip_address_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/.obj)
        if(CX_STATIC_LINK)
            target_compile_definitions(cxnet_ip_address_tests PRIVATE CX_STATIC_LINK)
        endif()
        target_link_libraries(cxnet_ip_address_tests PRIVATE cxnet)
        if(NOT CX_STATIC_LINK)
            add_custom_command(TARGET cxnet_ip_address_tests POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:cxnet>
                    $<TARGET_FILE_DIR:cxnet_ip_address_tests>
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:cxcore>
                    $<TARGET_FILE_DIR:cxnet_ip_address_tests>)
        endif()
        add_test(NAME cxnet_ip_address_tests COMMAND cxnet_ip_address_tests)

    add_executable(cxnet_http_types_tests tests/http_types_tests.c)
    target_include_directories(cxnet_http_types_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/.obj)
    if(CX_STATIC_LINK)
        target_compile_definitions(cxnet_http_types_tests PRIVATE CX_STATIC_LINK)
    endif()
    target_link_libraries(cxnet_http_types_tests PRIVATE cxnet)
    if(NOT CX_STATIC_LINK)
        add_custom_command(TARGET cxnet_http_types_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:cxnet>
                $<TARGET_FILE_DIR:cxnet_http_types_tests>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:cxcore>
                $<TARGET_FILE_DIR:cxnet_http_types_tests>)
    endif()
    add_test(NAME cxnet_http_types_tests COMMAND cxnet_http_types_tests)

        add_executable(cxnet_cx_exception_tests tests/cxnet_cx_exception_tests.c)
        target_include_directories(cxnet_cx_exception_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/.obj)
        if(CX_STATIC_LINK)
            target_compile_definitions(cxnet_cx_exception_tests PRIVATE CX_STATIC_LINK)
        endif()
        target_link_libraries(cxnet_cx_exception_tests PRIVATE cxnet)
        if(NOT CX_STATIC_LINK)
            add_custom_command(TARGET cxnet_cx_exception_tests POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:cxnet>
                    $<TARGET_FILE_DIR:cxnet_cx_exception_tests>
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:cxcore>
                    $<TARGET_FILE_DIR:cxnet_cx_exception_tests>)
        endif()
        add_test(NAME cxnet_cx_exception_tests COMMAND cxnet_cx_exception_tests)
    endif()

    if(UNIX AND NOT CX_BUILD_STATIC)
        get_property(cxnet_registered_tests DIRECTORY PROPERTY TESTS)
        set_tests_properties(${cxnet_registered_tests} PROPERTIES
            ENVIRONMENT
                "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:cxcore>:$<TARGET_FILE_DIR:cxnet>:$<TARGET_FILE_DIR:cxnet_runtime>")
    endif()
endif()

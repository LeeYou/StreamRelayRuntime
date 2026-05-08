cmake_minimum_required(VERSION 3.20)

project(StreamRelayRuntime VERSION 0.1.0 LANGUAGES CXX)

option(STREAMRELAY_BUILD_TESTS "Build StreamRelayRuntime tests" ON)

add_library(streamrelay_runtime STATIC
    src/runtime/app_context.cpp
    src/runtime/module_manager.cpp
    src/messaging/in_memory_message_bus.cpp
    src/protocol/binary_codec.cpp
    src/protocol/websocket_frame.cpp
    src/gateway/write_queue.cpp
    src/gateway/connection_shard.cpp
    src/gateway/connection_manager.cpp
    src/gateway/gateway_runtime.cpp
    src/gateway/gateway_edge.cpp
    src/session/session_service.cpp
    src/device_registry/device_registry.cpp
    src/relay/relay_frame.cpp
    src/relay/relay_channel.cpp
    src/relay/local_relay_data_plane.cpp
    src/relay/gateway_tunnel.cpp
    src/relay/gateway_tunnel_bridge.cpp
    src/control/command_store.cpp
    src/control/control_service.cpp
    src/control/secure_control_service.cpp
    src/router/service_registry.cpp
    src/router/router.cpp
    src/transport/in_memory_transport.cpp
    src/observability/metrics.cpp
    src/observability/prometheus_exporter.cpp
    src/observability/health.cpp
    src/observability/trace.cpp
    src/observability/slo.cpp
    src/security/auth.cpp
    src/ops/runtime_config.cpp
    src/ops/release_manifest.cpp
)

target_include_directories(streamrelay_runtime
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(streamrelay_runtime PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(streamrelay_runtime PRIVATE /W4 /permissive-)
else()
    target_compile_options(streamrelay_runtime PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(streamrelay_allinone
    apps/allinone/main.cpp
)

target_link_libraries(streamrelay_allinone PRIVATE streamrelay_runtime)

if(STREAMRELAY_BUILD_TESTS)
    enable_testing()

    function(add_streamrelay_unit_test target source)
        add_executable(${target} ${source})
        target_link_libraries(${target} PRIVATE streamrelay_runtime)
        add_test(NAME ${target} COMMAND ${target})
    endfunction()

    add_streamrelay_unit_test(streamrelay_runtime_core_tests tests/unit/runtime_core_tests.cpp)
    add_streamrelay_unit_test(streamrelay_protocol_gateway_tests tests/unit/protocol_gateway_tests.cpp)
    add_streamrelay_unit_test(streamrelay_session_device_tests tests/unit/session_device_tests.cpp)
    add_streamrelay_unit_test(streamrelay_relay_router_tests tests/unit/relay_router_tests.cpp)
    add_streamrelay_unit_test(streamrelay_control_tests tests/unit/control_tests.cpp)
    add_streamrelay_unit_test(streamrelay_enterprise_mvp_tests tests/unit/enterprise_mvp_tests.cpp)
    add_streamrelay_unit_test(streamrelay_ops_observability_tests tests/unit/ops_observability_tests.cpp)
    add_streamrelay_unit_test(streamrelay_unit_tests tests/unit/runtime_tests.cpp)
endif()

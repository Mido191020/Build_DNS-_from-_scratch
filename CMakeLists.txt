cmake_minimum_required(VERSION 3.20)
project(dns_proxy_relay C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(ENABLE_WARNINGS "Enable strict compiler warnings" ON)

add_library(dns_core STATIC
    src/dns_wire.c
    src/net_platform.c
)
target_include_directories(dns_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_library(dns_proxy_runtime STATIC
    src/dns_proxy_server.c
)
target_link_libraries(dns_proxy_runtime PUBLIC dns_core)
target_include_directories(dns_proxy_runtime PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_library(dns_query_runtime STATIC
    src/dns_query_client.c
)
target_link_libraries(dns_query_runtime PUBLIC dns_core)
target_include_directories(dns_query_runtime PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(dns_proxy
    apps/dns_proxy_main.c
)
target_link_libraries(dns_proxy PRIVATE dns_proxy_runtime)

add_executable(dns_query
    apps/dns_query_main.c
)
target_link_libraries(dns_query PRIVATE dns_query_runtime)

if(WIN32)
    target_link_libraries(dns_core PUBLIC ws2_32)
endif()

if(ENABLE_WARNINGS)
    if(MSVC)
        target_compile_options(dns_core PRIVATE /W4)
        target_compile_options(dns_proxy_runtime PRIVATE /W4)
        target_compile_options(dns_query_runtime PRIVATE /W4)
        target_compile_options(dns_proxy PRIVATE /W4)
        target_compile_options(dns_query PRIVATE /W4)
    else()
        target_compile_options(dns_core PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
        target_compile_options(dns_proxy_runtime PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
        target_compile_options(dns_query_runtime PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
        target_compile_options(dns_proxy PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
        target_compile_options(dns_query PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
    endif()
endif()

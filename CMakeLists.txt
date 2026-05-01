cmake_minimum_required(VERSION 3.20)
project(build_dns C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENABLE_WARNINGS "Enable strict compiler warnings" ON)
option(ENABLE_PROXY_TARGET "Build dns_proxy target" ON)

set(DNS_CORE_SOURCES
    dns_wire.c
)

# Playbook milestone compatibility: add cache module automatically when it appears.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/dns_cache.c")
    list(APPEND DNS_CORE_SOURCES dns_cache.c)
endif()

add_library(dns_core STATIC ${DNS_CORE_SOURCES})
target_include_directories(dns_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(dns_query
    main.c
)
target_link_libraries(dns_query PRIVATE dns_core)

if(ENABLE_PROXY_TARGET)
    add_executable(dns_proxy
        proxy.c
    )
    target_link_libraries(dns_proxy PRIVATE dns_core)
endif()

if(WIN32)
    target_link_libraries(dns_query PRIVATE ws2_32)
    if(TARGET dns_proxy)
        target_link_libraries(dns_proxy PRIVATE ws2_32)
    endif()
endif()

if(ENABLE_WARNINGS)
    if(MSVC)
        target_compile_options(dns_core PRIVATE /W4)
        target_compile_options(dns_query PRIVATE /W4)
        if(TARGET dns_proxy)
            target_compile_options(dns_proxy PRIVATE /W4)
        endif()
    else()
        target_compile_options(dns_core PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
        target_compile_options(dns_query PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
        if(TARGET dns_proxy)
            target_compile_options(dns_proxy PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
        endif()
    endif()
endif()

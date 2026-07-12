cmake_minimum_required(VERSION 3.25)

project(marc VERSION 0.1.0 LANGUAGES C CXX)

option(MARC_BUILD_SHARED "Build the marc shared library" ON)
option(MARC_BUILD_STATIC "Build the marc static library" ON)
option(MARC_BUILD_TESTS "Build marc tests" ON)

if(NOT MARC_BUILD_SHARED AND NOT MARC_BUILD_STATIC)
    message(FATAL_ERROR "At least one of MARC_BUILD_SHARED or MARC_BUILD_STATIC must be ON")
endif()

set(MARC_SOURCES
    src/core/status.cpp
    src/marc.cpp
)

function(marc_configure_target target)
    target_sources(${target} PRIVATE ${MARC_SOURCES})
    target_compile_features(${target} PUBLIC cxx_std_20)
    target_include_directories(${target}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    set_target_properties(${target} PROPERTIES
        CXX_EXTENSIONS OFF
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN YES
    )
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

if(MARC_BUILD_SHARED)
    add_library(marc_shared SHARED)
    marc_configure_target(marc_shared)
    target_compile_definitions(marc_shared PRIVATE MARC_BUILDING_SHARED)
    target_compile_definitions(marc_shared PUBLIC MARC_USING_SHARED)
    set_target_properties(marc_shared PROPERTIES OUTPUT_NAME marc)
    add_library(marc::shared ALIAS marc_shared)
endif()

if(MARC_BUILD_STATIC)
    add_library(marc_static STATIC)
    marc_configure_target(marc_static)
    set_target_properties(marc_static PROPERTIES OUTPUT_NAME marc_static)
    add_library(marc::static ALIAS marc_static)
endif()

if(MARC_BUILD_TESTS)
    enable_testing()
    add_executable(marc_core_tests tests/core_tests.cpp)
    target_compile_features(marc_core_tests PRIVATE cxx_std_20)
    target_include_directories(marc_core_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(TARGET marc_static)
        target_link_libraries(marc_core_tests PRIVATE marc::static)
    else()
        target_link_libraries(marc_core_tests PRIVATE marc::shared)
    endif()
    add_test(NAME marc_core_tests COMMAND marc_core_tests)

    add_executable(marc_c_api_tests tests/c_api_tests.c)
    target_compile_features(marc_c_api_tests PRIVATE c_std_11)
    if(TARGET marc_shared)
        target_link_libraries(marc_c_api_tests PRIVATE marc::shared)
    else()
        target_link_libraries(marc_c_api_tests PRIVATE marc::static)
    endif()
    add_test(NAME marc_c_api_tests COMMAND marc_c_api_tests)
endif()

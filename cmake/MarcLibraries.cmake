# MarcLibraries: explicit project declarations.
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
    set_target_properties(marc_shared PROPERTIES EXPORT_NAME shared)
    add_library(marc::shared ALIAS marc_shared)
endif()

if(MARC_BUILD_STATIC)
    add_library(marc_static STATIC)
    marc_configure_target(marc_static)
    set_target_properties(marc_static PROPERTIES OUTPUT_NAME marc_static)
    set_target_properties(marc_static PROPERTIES EXPORT_NAME static)
    add_library(marc::static ALIAS marc_static)
endif()

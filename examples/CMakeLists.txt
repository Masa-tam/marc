cmake_minimum_required(VERSION 3.25)

project(marc_c_roundtrip_example LANGUAGES C)

find_package(marc CONFIG REQUIRED)

add_executable(marc_c_roundtrip c_roundtrip.c)
target_compile_features(marc_c_roundtrip PRIVATE c_std_11)
if(TARGET marc::shared)
    target_link_libraries(marc_c_roundtrip PRIVATE marc::shared)
elseif(TARGET marc::static)
    target_link_libraries(marc_c_roundtrip PRIVATE marc::static)
else()
    message(FATAL_ERROR "The marc package exports no supported library target")
endif()

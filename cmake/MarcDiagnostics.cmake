# MarcDiagnostics: explicit project declarations.
if(MARC_BUILD_ENTROPY_DIAGNOSTICS AND TARGET marc_static)
    marc_add_cpp_executable(marc_position_distance_rans_export tools/position_distance_rans_export.cpp)
    target_include_directories(marc_position_distance_rans_export PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(marc_position_distance_rans_export PRIVATE marc::static)
endif()

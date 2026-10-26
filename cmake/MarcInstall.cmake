# MarcInstall: explicit project declarations.
set(MARC_INSTALL_TARGETS)
if(TARGET marc_shared)
    list(APPEND MARC_INSTALL_TARGETS marc_shared)
endif()
if(TARGET marc_static)
    list(APPEND MARC_INSTALL_TARGETS marc_static)
endif()

install(TARGETS ${MARC_INSTALL_TARGETS}
    EXPORT marcTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
if(TARGET marc_cli)
    install(TARGETS marc_cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT marcTargets
    FILE marcTargets.cmake
    NAMESPACE marc::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/marc
)
configure_package_config_file(
    cmake/marcConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/marcConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/marc
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/marcConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMinorVersion
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/marcConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/marcConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/marc
)
install(FILES
    LICENSE THIRD_PARTY_NOTICES.md README.md CHANGELOG.md CONTRIBUTING.md
    AGENTS.md
    DESTINATION ${CMAKE_INSTALL_DATADIR}/marc)
install(FILES assets/images/marc.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/marc/assets/images)
install(DIRECTORY docs/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/marc/docs
    FILES_MATCHING PATTERN "*.md")
install(FILES examples/CMakeLists.txt examples/c_roundtrip.c
    examples/position_distance_roundtrip.c
    DESTINATION ${CMAKE_INSTALL_DATADIR}/marc/examples)

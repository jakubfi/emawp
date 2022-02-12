cmake_minimum_required(VERSION 3.0)
project(emawp
	DESCRIPTION "MERA-400 long and floating point arithmetic"
	LANGUAGES C
)

include(GNUInstallDirs)
include(cmake/appversion.cmake)

include_directories(${CMAKE_SOURCE_DIR}/include)
add_compile_options(-Wall)

# ---- Target: emawp-lib -------------------------------------------------

set(EMAWP_PUBLIC_HEADERS
	include/emawp.h
)

add_library(emawp-lib SHARED
	src/awp.c
	${EMAWP_PUBLIC_HEADERS}
)

set_target_properties(emawp-lib PROPERTIES
	C_STANDARD 99
	OUTPUT_NAME "emawp"
	SOVERSION ${APP_VERSION_MAJOR}.${APP_VERSION_MINOR}
	PUBLIC_HEADER "${EMAWP_PUBLIC_HEADERS}"
)

target_compile_definitions(emawp-lib PRIVATE EMAWP_VERSION="${APP_VERSION}")

install(TARGETS emawp-lib
	EXPORT emawpLibTarget
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
	"${CMAKE_CURRENT_BINARY_DIR}/emawp-config-version.cmake"
	VERSION ${APP_VERSION}
	COMPATIBILITY AnyNewerVersion
)

export(
	EXPORT emawpLibTarget
	FILE "${CMAKE_CURRENT_BINARY_DIR}/emawp-config-targets.cmake"
)

set(ConfigPackageLocation ${CMAKE_INSTALL_LIBDIR}/cmake/emawp)
configure_package_config_file(cmake/emawp-config.cmake.in
	"${CMAKE_CURRENT_BINARY_DIR}/emawp-config.cmake"
	INSTALL_DESTINATION ${ConfigPackageLocation}
	NO_SET_AND_CHECK_MACRO
	NO_CHECK_REQUIRED_COMPONENTS_MACRO
)

install(EXPORT emawpLibTarget
	FILE emawp-config-targets.cmake
	DESTINATION ${ConfigPackageLocation}
)

install(FILES
	"${CMAKE_CURRENT_BINARY_DIR}/emawp-config.cmake"
	"${CMAKE_CURRENT_BINARY_DIR}/emawp-config-version.cmake"
	DESTINATION ${ConfigPackageLocation}
	COMPONENT Devel
)

# ---- Target: emawp -----------------------------------------------------

add_executable(emawp src/emawp-tool.c)
target_link_libraries(emawp m emawp-lib)
install(TARGETS emawp RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# vim: tabstop=4

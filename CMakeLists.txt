cmake_minimum_required(VERSION 2.8.4)
project(emawp C)

include(GNUInstallDirs)
include(appversion.cmake)

set(DEBUG 0)

include_directories(${CMAKE_SOURCE_DIR}/include)

add_definitions(-Wall -std=c99 -DEMAWP_VERSION="${APP_VERSION}")

if(DEBUG)
	add_definitions(-DDEBUG -g -O0)
else(DEBUG)
	add_definitions(-DNDEBUG -O3)
endif(DEBUG)

configure_file(
	emawp-config.cmake.in
	${PROJECT_BINARY_DIR}/emawp-config.cmake
	@ONLY
)

configure_file(
	emawp-config-version.cmake.in
	${PROJECT_BINARY_DIR}/emawp-config-version.cmake
	@ONLY
)

install(FILES
	${PROJECT_BINARY_DIR}/emawp-config.cmake
	${PROJECT_BINARY_DIR}/emawp-config-version.cmake
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/emawp
	COMPONENT dev
)

add_subdirectory(src)

# vim: tabstop=4

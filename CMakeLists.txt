cmake_minimum_required(VERSION 2.8.4)
project(emawp C)

set(DEBUG 0)

set(EMAWP_VERSION_MAJOR 1)
set(EMAWP_VERSION_MINOR 0)
set(EMAWP_VERSION_PATCH 0)
set(EMAWP_VERSION ${EMAWP_VERSION_MAJOR}.${EMAWP_VERSION_MINOR}.${EMAWP_VERSION_PATCH})

include_directories(${CMAKE_SOURCE_DIR}/include)

add_definitions(-Wall -std=c99 -DEMAWP_VERSION="${EMAWP_VERSION}")

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
	DESTINATION lib/emawp
	COMPONENT dev
)

add_subdirectory(src)

# vim: tabstop=4

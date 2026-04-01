cmake_minimum_required(VERSION 3.21)

project(
	FO4Upscaling
	LANGUAGES CXX
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_DEBUG OFF)

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME ON)

# Shared dependency: CommonLibF4
add_subdirectory(extern/CommonLibF4 CommonLibF4 EXCLUDE_FROM_ALL)

# Shared vcpkg packages
find_package(directxtk CONFIG REQUIRED)
find_package(directxtex CONFIG REQUIRED)
find_package(directx-headers CONFIG REQUIRED)
find_package(magic_enum CONFIG REQUIRED)

# Plugin targets
add_subdirectory(plugins/FrameGeneration)
add_subdirectory(plugins/Upscaling)

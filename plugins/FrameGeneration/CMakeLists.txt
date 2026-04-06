set(PLUGIN_NAME "AAAFrameGeneration")

add_library(${PLUGIN_NAME} SHARED)

file(GLOB_RECURSE PLUGIN_SOURCES
	LIST_DIRECTORIES false
	CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

target_sources(${PLUGIN_NAME} PRIVATE ${PLUGIN_SOURCES})

include(Common)
configure_xse_plugin(${PLUGIN_NAME} 1 3 3)

# Streamline (NVIDIA DLSS-G Frame Generation)
add_subdirectory(${CMAKE_SOURCE_DIR}/cmake/Streamline ${CMAKE_CURRENT_BINARY_DIR}/Streamline)

# XeSS (Intel XeSS-FG Frame Generation)
add_subdirectory(${CMAKE_SOURCE_DIR}/cmake/XeSS ${CMAKE_CURRENT_BINARY_DIR}/XeSS)

# NGX SDK (for DLSS-G direct API)
target_include_directories(${PLUGIN_NAME} PRIVATE
	${CMAKE_SOURCE_DIR}/extern/Streamline/external/ngx-sdk/include
)

target_link_libraries(${PLUGIN_NAME} PRIVATE
	debug ${CMAKE_SOURCE_DIR}/include/detours/Debug/detours.lib
	optimized ${CMAKE_SOURCE_DIR}/include/detours/Release/detours.lib
	Microsoft::DirectXTK
	Microsoft::DirectXTex
	Microsoft::DirectX-Headers
	d3d12.lib
	magic_enum::magic_enum
	d3dcompiler.lib
	Streamline
	${CMAKE_SOURCE_DIR}/extern/Streamline/external/ngx-sdk/lib/Windows_x86_64/nvsdk_ngx_d.lib
	XeSS
	${CMAKE_SOURCE_DIR}/extern/XeSS/lib/libxess_fg.lib
	${CMAKE_SOURCE_DIR}/extern/XeSS/lib/libxell.lib
)

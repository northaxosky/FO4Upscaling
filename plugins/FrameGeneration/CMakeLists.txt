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

target_link_libraries(${PLUGIN_NAME} PRIVATE
	debug ${CMAKE_SOURCE_DIR}/include/detours/Debug/detours.lib
	optimized ${CMAKE_SOURCE_DIR}/include/detours/Release/detours.lib
	Microsoft::DirectXTK
	Microsoft::DirectXTex
	Microsoft::DirectX-Headers
	d3d12.lib
	magic_enum::magic_enum
	d3dcompiler.lib
)

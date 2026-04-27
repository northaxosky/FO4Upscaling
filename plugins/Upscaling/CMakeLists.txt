set(PLUGIN_NAME "Upscaling")

add_library(${PLUGIN_NAME} SHARED)

file(GLOB_RECURSE PLUGIN_SOURCES
	LIST_DIRECTORIES false
	CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

target_sources(${PLUGIN_NAME} PRIVATE ${PLUGIN_SOURCES})

include(Common)
configure_xse_plugin(${PLUGIN_NAME} 1 0 0)

# FidelityFX-SDK (FSR3)
set(FFX_API_VK OFF)
set(FFX_API_DX12 OFF)
set(FFX_ALL OFF)
set(FFX_FSR3 ON)
set(FFX_FSR ON)
set(FFX_AUTO_COMPILE_SHADERS 1)
add_subdirectory(${CMAKE_SOURCE_DIR}/extern/FidelityFX-SDK/sdk ${CMAKE_CURRENT_BINARY_DIR}/FidelityFX-SDK)

foreach(FFX_TARGET
		ffx_backend_dx11_x64
		ffx_frameinterpolation_x64
		ffx_fsr3_x64
		ffx_fsr3upscaler_x64
		ffx_opticalflow_x64
		ffx_spd_x64)
	if(TARGET ${FFX_TARGET})
		target_compile_options(${FFX_TARGET} PRIVATE /WX-)
	endif()
endforeach()

# Streamline (NVIDIA DLSS)
add_subdirectory(${CMAKE_SOURCE_DIR}/cmake/Streamline ${CMAKE_CURRENT_BINARY_DIR}/Streamline)

target_link_libraries(${PLUGIN_NAME} PRIVATE
	debug ${CMAKE_SOURCE_DIR}/include/detours/Debug/detours.lib
	optimized ${CMAKE_SOURCE_DIR}/include/detours/Release/detours.lib
	Microsoft::DirectXTK
	Microsoft::DirectXTex
	Microsoft::DirectX-Headers
	d3d12.lib
	magic_enum::magic_enum
	d3dcompiler.lib
	ffx_backend_dx11_x64
	ffx_fsr3_x64
	Streamline
)

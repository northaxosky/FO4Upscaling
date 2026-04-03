#include "Streamline.h"

bool StreamlineFG::InitNGX(ID3D12Device* a_device)
{
	d3d12Device = a_device;

	REX::INFO("[DLSSG] Initializing NGX for DLSS Frame Generation");

	// Pre-load the DLSS-G runtime so NGX can find it
	auto dlssgModule = LoadLibrary(L"Data\\F4SE\\Plugins\\Streamline\\nvngx_dlssg.dll");
	if (dlssgModule) {
		REX::INFO("[DLSSG] Pre-loaded nvngx_dlssg.dll");
	} else {
		REX::WARN("[DLSSG] Failed to pre-load nvngx_dlssg.dll (error {:#x})", GetLastError());
	}

	auto result = NVSDK_NGX_D3D12_Init(0x12345678, L".", a_device);
	REX::INFO("[DLSSG] NGX_D3D12_Init result: {:#x}", (uint32_t)result);

	if (NVSDK_NGX_FAILED(result)) {
		REX::ERROR("[DLSSG] NGX init failed");
		return false;
	}

	result = NVSDK_NGX_D3D12_AllocateParameters(&ngxParams);
	if (NVSDK_NGX_FAILED(result)) {
		REX::ERROR("[DLSSG] Failed to allocate NGX parameters");
		return false;
	}

	// Check if frame generation is available
	int fgAvailable = 0;
	ngxParams->Get(NVSDK_NGX_Parameter_FrameInterpolation_FeatureInitResult, &fgAvailable);
	REX::INFO("[DLSSG] FrameInterpolation feature init result: {}", fgAvailable);

	ngxInitialized = true;
	REX::INFO("[DLSSG] NGX initialized successfully");
	return true;
}

bool StreamlineFG::CreateFrameGenFeature(ID3D12GraphicsCommandList* a_cmdList, uint32_t a_width, uint32_t a_height)
{
	if (!ngxInitialized || !ngxParams) return false;

	REX::INFO("[DLSSG] Creating frame gen feature: {}x{}", a_width, a_height);

	NVSDK_NGX_Parameter_SetUI(ngxParams, NVSDK_NGX_EParameter_Width, a_width);
	NVSDK_NGX_Parameter_SetUI(ngxParams, NVSDK_NGX_EParameter_Height, a_height);
	NVSDK_NGX_Parameter_SetUI(ngxParams, NVSDK_NGX_EParameter_OutWidth, a_width);
	NVSDK_NGX_Parameter_SetUI(ngxParams, NVSDK_NGX_EParameter_OutHeight, a_height);

	auto result = NVSDK_NGX_D3D12_CreateFeature(
		a_cmdList,
		NVSDK_NGX_Feature_FrameGeneration,
		ngxParams,
		&dlssgHandle);

	REX::INFO("[DLSSG] CreateFeature result: {:#x}", (uint32_t)result);

	if (NVSDK_NGX_FAILED(result)) {
		REX::ERROR("[DLSSG] Failed to create frame gen feature");
		return false;
	}

	featureDLSSG = true;
	REX::INFO("[DLSSG] Frame generation feature created");
	return true;
}

bool StreamlineFG::EvaluateFrameGen(ID3D12GraphicsCommandList* a_cmdList,
	ID3D12Resource* a_backbuffer,
	ID3D12Resource* a_depth,
	ID3D12Resource* a_motionVectors,
	ID3D12Resource* a_hudlessColor)
{
	if (!featureDLSSG || !dlssgHandle || !ngxParams) return false;

	NVSDK_NGX_Parameter_SetD3d12Resource(ngxParams, NVSDK_NGX_EParameter_Color, a_backbuffer);
	NVSDK_NGX_Parameter_SetD3d12Resource(ngxParams, NVSDK_NGX_EParameter_Output, a_backbuffer);
	NVSDK_NGX_Parameter_SetD3d12Resource(ngxParams, NVSDK_NGX_EParameter_Depth, a_depth);
	NVSDK_NGX_Parameter_SetD3d12Resource(ngxParams, NVSDK_NGX_EParameter_MotionVectors, a_motionVectors);
	if (a_hudlessColor) {
		NVSDK_NGX_Parameter_SetD3d12Resource(ngxParams, "HUDLessColor", a_hudlessColor);
	}

	auto result = NVSDK_NGX_D3D12_EvaluateFeature(a_cmdList, dlssgHandle, ngxParams, nullptr);

	static bool loggedOnce = false;
	if (!loggedOnce) {
		REX::INFO("[DLSSG] First EvaluateFeature result: {:#x}", (uint32_t)result);
		loggedOnce = true;
	}

	return NVSDK_NGX_SUCCEED(result);
}

void StreamlineFG::Shutdown()
{
	if (dlssgHandle) {
		NVSDK_NGX_D3D12_ReleaseFeature(dlssgHandle);
		dlssgHandle = nullptr;
	}
	if (ngxInitialized) {
		NVSDK_NGX_D3D12_Shutdown1(d3d12Device);
		ngxInitialized = false;
	}
}

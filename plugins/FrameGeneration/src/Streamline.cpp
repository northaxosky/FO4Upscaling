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

	// Set up NGX logging and feature search paths
	NVSDK_NGX_FeatureCommonInfo featureInfo{};

	// Logging
	NVSDK_NGX_LoggingInfo loggingInfo{};
	loggingInfo.LoggingCallback = [](const char* message, NVSDK_NGX_Logging_Level level, NVSDK_NGX_Feature) {
		REX::INFO("[NGX] [{}] {}", (int)level, message);
	};
	loggingInfo.MinimumLoggingLevel = NVSDK_NGX_LOGGING_LEVEL_VERBOSE;
	loggingInfo.DisableOtherLoggingSinks = false;
	featureInfo.LoggingInfo = loggingInfo;

	// Tell NGX where to find nvngx_dlssg.dll and other feature DLLs
	static const wchar_t* searchPaths[] = { L"Data\\F4SE\\Plugins\\Streamline" };
	featureInfo.PathListInfo.Path = searchPaths;
	featureInfo.PathListInfo.Length = 1;

	auto result = NVSDK_NGX_D3D12_Init(0x12345678, L".", a_device, &featureInfo);
	REX::INFO("[DLSSG] NGX_D3D12_Init result: {:#x}", (uint32_t)result);

	if (NVSDK_NGX_FAILED(result)) {
		REX::ERROR("[DLSSG] NGX init failed");
		return false;
	}

	result = NVSDK_NGX_D3D12_GetCapabilityParameters(&ngxParams);
	if (NVSDK_NGX_FAILED(result)) {
		REX::WARN("[DLSSG] GetCapabilityParameters failed ({}), trying AllocateParameters", (uint32_t)result);
		result = NVSDK_NGX_D3D12_AllocateParameters(&ngxParams);
		if (NVSDK_NGX_FAILED(result)) {
			REX::ERROR("[DLSSG] Failed to allocate NGX parameters");
			return false;
		}
	}

	// Query frame gen capabilities
	int fgAvailable = 0;
	ngxParams->Get(NVSDK_NGX_Parameter_FrameInterpolation_FeatureInitResult, &fgAvailable);
	REX::INFO("[DLSSG] FrameInterpolation feature init result: {}", fgAvailable);

	int needsDriver = 0;
	ngxParams->Get(NVSDK_NGX_Parameter_FrameInterpolation_NeedsUpdatedDriver, &needsDriver);
	REX::INFO("[DLSSG] NeedsUpdatedDriver: {}", needsDriver);

	unsigned int minDriverMajor = 0;
	ngxParams->Get(NVSDK_NGX_Parameter_FrameInterpolation_MinDriverVersionMajor, &minDriverMajor);
	REX::INFO("[DLSSG] MinDriverVersionMajor: {}", minDriverMajor);

	ngxInitialized = true;
	REX::INFO("[DLSSG] NGX initialized successfully");
	return true;
}

bool StreamlineFG::CreateFrameGenFeature(ID3D12GraphicsCommandList* a_cmdList, uint32_t a_width, uint32_t a_height)
{
	if (!ngxInitialized || !ngxParams) return false;

	REX::INFO("[DLSSG] Creating frame gen feature: {}x{}", a_width, a_height);

	ngxParams->Set(NVSDK_NGX_Parameter_Width, a_width);
	ngxParams->Set(NVSDK_NGX_Parameter_Height, a_height);
	ngxParams->Set(NVSDK_NGX_Parameter_OutWidth, a_width);
	ngxParams->Set(NVSDK_NGX_Parameter_OutHeight, a_height);
	ngxParams->Set(NVSDK_NGX_Parameter_Resource_Width, a_width);
	ngxParams->Set(NVSDK_NGX_Parameter_Resource_Height, a_height);
	ngxParams->Set(NVSDK_NGX_Parameter_Resource_OutWidth, a_width);
	ngxParams->Set(NVSDK_NGX_Parameter_Resource_OutHeight, a_height);
	ngxParams->Set("FrameInterpolation.EnableInterpolation", 1);
	ngxParams->Set("FrameInterpolation.IsRecording", 0);

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

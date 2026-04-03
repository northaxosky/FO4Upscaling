#include "Streamline.h"

void StreamlineFG::LoadInterposer()
{
	interposer = LoadLibrary(L"Data\\F4SE\\Plugins\\Streamline\\sl.interposer.dll");
	if (!interposer) {
		REX::WARN("[DLSSG] Failed to load interposer: {:#x}", GetLastError());
		return;
	}
	REX::INFO("[DLSSG] Interposer loaded at {:#x}", (uintptr_t)interposer);

	slInit = (PFun_slInit*)GetProcAddress(interposer, "slInit");
	slShutdown = (PFun_slShutdown*)GetProcAddress(interposer, "slShutdown");
	slUpgradeInterface = (PFun_slUpgradeInterface*)GetProcAddress(interposer, "slUpgradeInterface");
	slSetD3DDevice = (PFun_slSetD3DDevice*)GetProcAddress(interposer, "slSetD3DDevice");
	slSetConstants = (PFun_slSetConstants*)GetProcAddress(interposer, "slSetConstants");
	slGetNewFrameToken = (PFun_slGetNewFrameToken*)GetProcAddress(interposer, "slGetNewFrameToken");
	slEvaluateFeature = (PFun_slEvaluateFeature*)GetProcAddress(interposer, "slEvaluateFeature");
}

bool StreamlineFG::InitStreamline()
{
	if (!interposer || !slInit) return false;

	REX::INFO("[DLSSG] Initializing Streamline (eD3D11 mode for interposer)");

	sl::Preferences pref{};
	pref.showConsole = false;
	pref.logLevel = sl::LogLevel::eOff;
	pref.logMessageCallback = nullptr;
	pref.engine = sl::EngineType::eCustom;
	pref.engineVersion = "1.0.0";
	pref.projectId = "f4-frame-generation";
	pref.flags |= sl::PreferenceFlags::eUseManualHooking;
	pref.renderAPI = sl::RenderAPI::eD3D11;

	// Load DLSS-G feature so interposer sets up async frame generation pipeline
	static sl::Feature features[] = { sl::kFeatureDLSS_G };
	pref.featuresToLoad = features;
	pref.numFeaturesToLoad = 1;

	auto result = slInit(pref, sl::kSDKVersion);
	REX::INFO("[DLSSG] slInit result: {}", (int)result);

	if (result != sl::Result::eOk) {
		REX::ERROR("[DLSSG] Streamline init failed");
		return false;
	}

	slInitialized = true;
	REX::INFO("[DLSSG] Streamline initialized for swap chain interception");
	return true;
}

void StreamlineFG::SetD3DDevice(ID3D12Device* a_device)
{
	d3d12Device = a_device;
	if (slSetD3DDevice && slInitialized) {
		slSetD3DDevice(a_device);
		REX::INFO("[DLSSG] D3D12 device set for Streamline");
	}
}

void StreamlineFG::UpgradeSwapChain(IDXGISwapChain4** a_swapChain)
{
	if (slUpgradeInterface && slInitialized) {
		auto result = slUpgradeInterface((void**)a_swapChain);
		REX::INFO("[DLSSG] Swap chain upgrade result: {}", (int)result);
	}
}

bool StreamlineFG::InitNGX(ID3D12Device* a_device)
{
	d3d12Device = a_device;
	REX::INFO("[DLSSG] Initializing NGX");

	// Pre-load nvngx_dlssg.dll
	LoadLibrary(L"Data\\F4SE\\Plugins\\Streamline\\nvngx_dlssg.dll");

	// Set up logging and search paths
	NVSDK_NGX_FeatureCommonInfo featureInfo{};
	NVSDK_NGX_LoggingInfo loggingInfo{};
	loggingInfo.LoggingCallback = [](const char* message, NVSDK_NGX_Logging_Level level, NVSDK_NGX_Feature) {
		REX::INFO("[NGX] [{}] {}", (int)level, message);
	};
	loggingInfo.MinimumLoggingLevel = NVSDK_NGX_LOGGING_LEVEL_VERBOSE;
	featureInfo.LoggingInfo = loggingInfo;

	static const wchar_t* searchPaths[] = { L"Data\\F4SE\\Plugins\\Streamline" };
	featureInfo.PathListInfo.Path = searchPaths;
	featureInfo.PathListInfo.Length = 1;

	auto result = NVSDK_NGX_D3D12_Init(0x12345678, L".", a_device, &featureInfo);
	REX::INFO("[DLSSG] NGX_D3D12_Init result: {:#x}", (uint32_t)result);

	if (NVSDK_NGX_FAILED(result)) {
		REX::ERROR("[DLSSG] NGX init failed");
		return false;
	}

	NVSDK_NGX_D3D12_GetCapabilityParameters(&ngxParams);
	ngxInitialized = true;
	REX::INFO("[DLSSG] NGX initialized");
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
	ngxParams->Set(NVSDK_NGX_Parameter_NumFrames, (unsigned int)1);
	ngxParams->Set(NVSDK_NGX_Parameter_CreationNodeMask, (unsigned int)1);
	ngxParams->Set(NVSDK_NGX_Parameter_VisibilityNodeMask, (unsigned int)1);

	auto result = NVSDK_NGX_D3D12_CreateFeature(
		a_cmdList,
		NVSDK_NGX_Feature_FrameGeneration,
		ngxParams,
		&dlssgHandle);

	REX::INFO("[DLSSG] CreateFeature result: {:#x}", (uint32_t)result);

	if (NVSDK_NGX_FAILED(result)) {
		REX::ERROR("[DLSSG] Feature creation failed");
		return false;
	}

	featureDLSSG = true;
	REX::INFO("[DLSSG] Frame gen feature created!");
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
	if (a_hudlessColor)
		NVSDK_NGX_Parameter_SetD3d12Resource(ngxParams, "HUDLessColor", a_hudlessColor);

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

#include "Streamline.h"

void StreamlineFG::LoadInterposer()
{
	interposer = LoadLibrary(L"Data\\F4SE\\Plugins\\Streamline\\sl.interposer.dll");
	if (!interposer) {
		auto errorCode = GetLastError();
		REX::INFO("[DLSSG] Failed to load interposer: Error Code {0:x}", errorCode);
		return;
	}
	REX::INFO("[DLSSG] Interposer loaded at address: {0:p}", (void*)interposer);

	// Load core SL function pointers
	slInit = (PFun_slInit*)GetProcAddress(interposer, "slInit");
	slShutdown = (PFun_slShutdown*)GetProcAddress(interposer, "slShutdown");
	slIsFeatureSupported = (PFun_slIsFeatureSupported*)GetProcAddress(interposer, "slIsFeatureSupported");
	slIsFeatureLoaded = (PFun_slIsFeatureLoaded*)GetProcAddress(interposer, "slIsFeatureLoaded");
	slSetFeatureLoaded = (PFun_slSetFeatureLoaded*)GetProcAddress(interposer, "slSetFeatureLoaded");
	slEvaluateFeature = (PFun_slEvaluateFeature*)GetProcAddress(interposer, "slEvaluateFeature");
	slSetTag = (PFun_slSetTag2*)GetProcAddress(interposer, "slSetTag");
	slGetFeatureRequirements = (PFun_slGetFeatureRequirements*)GetProcAddress(interposer, "slGetFeatureRequirements");
	slUpgradeInterface = (PFun_slUpgradeInterface*)GetProcAddress(interposer, "slUpgradeInterface");
	slSetConstants = (PFun_slSetConstants*)GetProcAddress(interposer, "slSetConstants");
	slGetFeatureFunction = (PFun_slGetFeatureFunction*)GetProcAddress(interposer, "slGetFeatureFunction");
	slGetNewFrameToken = (PFun_slGetNewFrameToken*)GetProcAddress(interposer, "slGetNewFrameToken");
	slSetD3DDevice = (PFun_slSetD3DDevice*)GetProcAddress(interposer, "slSetD3DDevice");
}

void StreamlineFG::Initialize()
{
	REX::INFO("[DLSSG] Initializing Streamline for DLSS Frame Generation");

	sl::Preferences pref{};

	sl::Feature featuresToLoad[] = { sl::kFeatureDLSS_G, sl::kFeatureReflex, sl::kFeatureDLSS };
	pref.featuresToLoad = featuresToLoad;
	pref.numFeaturesToLoad = _countof(featuresToLoad);

	// Get the directory where sl.interposer.dll is loaded from
	wchar_t interposerPath[MAX_PATH];
	GetModuleFileNameW(interposer, interposerPath, MAX_PATH);
	// Strip filename to get directory
	wchar_t* lastSlash = wcsrchr(interposerPath, L'\\');
	if (!lastSlash) lastSlash = wcsrchr(interposerPath, L'/');
	if (lastSlash) *lastSlash = L'\0';

	{
		char pathBuf[MAX_PATH];
		GetModuleFileNameA(interposer, pathBuf, MAX_PATH);
		char* ls = strrchr(pathBuf, '\\');
		if (!ls) ls = strrchr(pathBuf, '/');
		if (ls) *ls = '\0';
		REX::INFO("[DLSSG] Plugin search path: {}", pathBuf);
	}

	static const wchar_t* pluginPaths[1];
	pluginPaths[0] = interposerPath;
	pref.pathsToPlugins = pluginPaths;
	pref.numPathsToPlugins = 1;

	pref.logLevel = sl::LogLevel::eOff;
	pref.logMessageCallback = nullptr;
	pref.showConsole = false;

	pref.engine = sl::EngineType::eCustom;
	pref.engineVersion = "1.0.0";
	pref.projectId = "f4-frame-generation";
	pref.flags |= sl::PreferenceFlags::eUseManualHooking;

	// DLSS-G requires D3D12 render API — D3D11 causes FeatureNotSupported
	pref.renderAPI = sl::RenderAPI::eD3D12;

	// Pre-load Streamline plugin DLLs so they're in memory when slInit searches
	LoadLibrary(L"Data\\F4SE\\Plugins\\Streamline\\sl.common.dll");
	LoadLibrary(L"Data\\F4SE\\Plugins\\Streamline\\sl.dlss_g.dll");
	LoadLibrary(L"Data\\F4SE\\Plugins\\Streamline\\sl.reflex.dll");
	LoadLibrary(L"Data\\F4SE\\Plugins\\Streamline\\sl.pcl.dll");
	LoadLibrary(L"Data\\F4SE\\Plugins\\Streamline\\sl.dlss.dll");
	LoadLibrary(L"Data\\F4SE\\Plugins\\Streamline\\nvngx_dlssg.dll");
	LoadLibrary(L"Data\\F4SE\\Plugins\\Streamline\\nvngx_dlss.dll");
	REX::INFO("[DLSSG] Pre-loaded Streamline plugin DLLs");

	REX::INFO("[DLSSG] Calling slInit with renderAPI=eD3D12, {} features", pref.numFeaturesToLoad);

	auto initResult = slInit(pref, sl::kSDKVersion);
	REX::INFO("[DLSSG] slInit returned: {}", (int)initResult);

	if (initResult != sl::Result::eOk) {
		REX::CRITICAL("[DLSSG] Failed to initialize Streamline (result={})", (int)initResult);
		return;
	}

	REX::INFO("[DLSSG] Successfully initialized Streamline");
	initialized = true;
}

void StreamlineFG::CheckFeatures(IDXGIAdapter* a_adapter)
{
	REX::INFO("[DLSSG] Checking features");

	sl::AdapterInfo adapterInfo;
	adapterInfo.deviceLUID = nullptr;
	adapterInfo.vkPhysicalDevice = nullptr;

	DXGI_ADAPTER_DESC desc;
	a_adapter->GetDesc(&desc);
	adapterInfo.deviceLUID = (uint8_t*)&desc.AdapterLuid;

	// Check if DLSS-G plugin is loaded
	bool dlssgLoaded = false;
	if (slIsFeatureLoaded) {
		auto loadedResult = slIsFeatureLoaded(sl::kFeatureDLSS_G, dlssgLoaded);
		REX::INFO("[DLSSG] Feature loaded check: result={}, loaded={}", (int)loadedResult, dlssgLoaded);
	}

	// Check DLSS-G support
	auto dlssgResult = slIsFeatureSupported(sl::kFeatureDLSS_G, adapterInfo);
	if (dlssgResult == sl::Result::eOk) {
		featureDLSSG = true;
		REX::INFO("[DLSSG] DLSS Frame Generation is supported");
	} else {
		featureDLSSG = false;
		REX::INFO("[DLSSG] DLSS Frame Generation is NOT supported (result={}: {})", (int)dlssgResult,
			dlssgResult == sl::Result::eErrorMissingOrInvalidAPI ? "MissingOrInvalidAPI" :
			dlssgResult == sl::Result::eErrorNoSupportedAdapterFound ? "NoSupportedAdapter" :
			dlssgResult == sl::Result::eErrorAdapterNotSupported ? "AdapterNotSupported" :
			dlssgResult == sl::Result::eErrorFeatureNotSupported ? "FeatureNotSupported" :
			dlssgResult == sl::Result::eErrorFeatureMissing ? "FeatureMissing" :
			dlssgResult == sl::Result::eErrorFeatureFailedToLoad ? "FeatureFailedToLoad" :
			dlssgResult == sl::Result::eErrorOSDisabledHWS ? "OSDisabledHWS" :
			"Unknown");
	}

	// Check Reflex
	auto reflexResult = slIsFeatureSupported(sl::kFeatureReflex, adapterInfo);
	if (reflexResult == sl::Result::eOk) {
		featureReflex = true;
		REX::INFO("[DLSSG] Reflex is supported");
	} else {
		featureReflex = false;
		REX::INFO("[DLSSG] Reflex is NOT supported (result={})", (int)reflexResult);
	}

	REX::INFO("[DLSSG] DLSS-G {}, Reflex {}", featureDLSSG ? "available" : "unavailable", featureReflex ? "available" : "unavailable");
}

void StreamlineFG::PostDevice()
{
	if (!initialized) return;

	// Load DLSS-G specific functions
	if (featureDLSSG) {
		slGetFeatureFunction(sl::kFeatureDLSS_G, "slDLSSGSetOptions", (void*&)slDLSSGSetOptions);
		slGetFeatureFunction(sl::kFeatureDLSS_G, "slDLSSGGetState", (void*&)slDLSSGGetState);
	}

	// Load Reflex functions
	if (featureReflex) {
		slGetFeatureFunction(sl::kFeatureReflex, "slReflexSetOptions", (void*&)slReflexSetOptions);
		slGetFeatureFunction(sl::kFeatureReflex, "slReflexGetState", (void*&)slReflexGetState);

		// Enable Reflex (required for DLSS-G)
		sl::ReflexOptions reflexOptions{};
		reflexOptions.mode = sl::ReflexMode::eLowLatency;
		if (SL_FAILED(res, slReflexSetOptions(reflexOptions))) {
			REX::WARN("[DLSSG] Failed to enable Reflex");
		} else {
			REX::INFO("[DLSSG] Reflex enabled (low latency mode)");
		}
	}

	REX::INFO("[DLSSG] Post-device setup complete");
}

void StreamlineFG::SetD3DDevice(ID3D12Device* a_device)
{
	if (!initialized) return;
	slSetD3DDevice(a_device);
	REX::INFO("[DLSSG] D3D12 device set");
}

void StreamlineFG::UpgradeSwapChain(IDXGISwapChain4* a_swapChain)
{
	if (!initialized) return;
	slUpgradeInterface((void**)&a_swapChain);
	REX::INFO("[DLSSG] Swap chain upgraded for Streamline interception");
}

void StreamlineFG::SetDLSSGOptions(bool a_enabled, uint32_t a_numFrames)
{
	if (!featureDLSSG || !slDLSSGSetOptions) return;

	sl::DLSSGOptions options{};
	options.mode = a_enabled ? sl::DLSSGMode::eOn : sl::DLSSGMode::eOff;
	options.numFramesToGenerate = a_numFrames;
	options.flags = sl::DLSSGFlags::eEnableFullscreenMenuDetection;

	if (SL_FAILED(result, slDLSSGSetOptions(viewport, options))) {
		REX::ERROR("[DLSSG] Failed to set DLSS-G options");
	}

	static bool loggedOnce = false;
	if (!loggedOnce) {
		REX::INFO("[DLSSG] Options set: mode={}, numFramesToGenerate={}", a_enabled ? "On" : "Off", a_numFrames);
		loggedOnce = true;
	}
}

void StreamlineFG::TagResources(ID3D12GraphicsCommandList* a_cmdList, ID3D12Resource* a_depth, ID3D12Resource* a_motionVectors, ID3D12Resource* a_hudlessColor)
{
	if (!featureDLSSG || !slSetTag) return;

	sl::Resource depth = { sl::ResourceType::eTex2d, a_depth, 0 };
	sl::Resource mvec = { sl::ResourceType::eTex2d, a_motionVectors, 0 };
	sl::Resource hudless = { sl::ResourceType::eTex2d, a_hudlessColor, 0 };

	sl::ResourceTag depthTag = { &depth, sl::kBufferTypeDepth, sl::ResourceLifecycle::eValidUntilPresent, nullptr };
	sl::ResourceTag mvecTag = { &mvec, sl::kBufferTypeMotionVectors, sl::ResourceLifecycle::eValidUntilPresent, nullptr };
	sl::ResourceTag hudlessTag = { &hudless, sl::kBufferTypeHUDLessColor, sl::ResourceLifecycle::eValidUntilPresent, nullptr };

	sl::ResourceTag tags[] = { depthTag, mvecTag, hudlessTag };
	slSetTag(viewport, tags, _countof(tags), (sl::CommandBuffer*)a_cmdList);

	static bool loggedOnce = false;
	if (!loggedOnce) {
		REX::INFO("[DLSSG] Resources tagged: depth={:#x}, mvec={:#x}, hudless={:#x}",
			(uintptr_t)a_depth, (uintptr_t)a_motionVectors, (uintptr_t)a_hudlessColor);
		loggedOnce = true;
	}
}

void StreamlineFG::UpdateConstants()
{
	if (!initialized || !slSetConstants || !slGetNewFrameToken) return;

	sl::Constants constants{};
	constants.cameraNear = 0;
	constants.cameraFar = 1;
	constants.cameraAspectRatio = 0.0f;
	constants.cameraFOV = 0.0f;
	constants.cameraMotionIncluded = sl::Boolean::eTrue;
	constants.cameraPinholeOffset = { 0.f, 0.f };
	constants.depthInverted = sl::Boolean::eFalse;
	constants.jitterOffset = { 0, 0 };
	constants.mvecScale = { 1, 1 };
	constants.reset = sl::Boolean::eFalse;
	constants.motionVectors3D = sl::Boolean::eFalse;
	constants.motionVectorsInvalidValue = FLT_MIN;
	constants.orthographicProjection = sl::Boolean::eFalse;
	constants.motionVectorsDilated = sl::Boolean::eFalse;
	constants.motionVectorsJittered = sl::Boolean::eFalse;

	if (SL_FAILED(res, slGetNewFrameToken(frameToken, nullptr))) {
		REX::ERROR("[DLSSG] Could not get frame token");
	}

	if (SL_FAILED(res, slSetConstants(constants, *frameToken, viewport))) {
		REX::ERROR("[DLSSG] Could not set constants");
	}
}

void StreamlineFG::Present(bool a_useFrameGen)
{
	if (!initialized || !featureDLSSG) return;

	UpdateConstants();
	SetDLSSGOptions(a_useFrameGen);

	// PCL markers are placed by Streamline internally when it intercepts Present
	// The actual frame generation happens asynchronously when swapChain->Present() is called

	static bool loggedOnce = false;
	if (!loggedOnce) {
		REX::INFO("[DLSSG] First Present call: useFrameGen={}", a_useFrameGen);

		if (slDLSSGGetState) {
			sl::DLSSGState state{};
			slDLSSGGetState(viewport, state, nullptr);
			REX::INFO("[DLSSG] State: status={}, minDim={}, maxFrames={}, vramMB={}",
				(int)state.status, state.minWidthOrHeight, state.numFramesToGenerateMax,
				state.estimatedVRAMUsageInBytes / (1024 * 1024));
		}
		loggedOnce = true;
	}
}

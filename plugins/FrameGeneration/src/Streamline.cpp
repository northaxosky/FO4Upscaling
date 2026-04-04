#include "Streamline.h"

void StreamlineFG::LoadInterposer()
{
	interposer = LoadLibrary(L"Data\\F4SE\\Plugins\\Streamline\\sl.interposer.dll");
	if (!interposer) {
		REX::WARN("[DLSSG] Failed to load interposer: {:#x}", GetLastError());
		return;
	}
	REX::INFO("[DLSSG] Interposer loaded");

	slInit = (PFun_slInit*)GetProcAddress(interposer, "slInit");
	slShutdown = (PFun_slShutdown*)GetProcAddress(interposer, "slShutdown");
	slUpgradeInterface = (PFun_slUpgradeInterface*)GetProcAddress(interposer, "slUpgradeInterface");
	slSetD3DDevice = (PFun_slSetD3DDevice*)GetProcAddress(interposer, "slSetD3DDevice");
	slIsFeatureSupported = (PFun_slIsFeatureSupported*)GetProcAddress(interposer, "slIsFeatureSupported");
	slGetFeatureFunction = (PFun_slGetFeatureFunction*)GetProcAddress(interposer, "slGetFeatureFunction");
	slSetTag = (PFun_slSetTag2*)GetProcAddress(interposer, "slSetTag");
	slSetConstants = (PFun_slSetConstants*)GetProcAddress(interposer, "slSetConstants");
	slGetNewFrameToken = (PFun_slGetNewFrameToken*)GetProcAddress(interposer, "slGetNewFrameToken");
	slEvaluateFeature = (PFun_slEvaluateFeature*)GetProcAddress(interposer, "slEvaluateFeature");
}

bool StreamlineFG::InitStreamline()
{
	if (!interposer || !slInit) return false;

	REX::INFO("[DLSSG] Initializing Streamline");

	// Pre-load plugin DLLs for MO2 USVFS compatibility
	LoadLibrary(L"Data\\F4SE\\Plugins\\Streamline\\sl.common.dll");
	LoadLibrary(L"Data\\F4SE\\Plugins\\Streamline\\sl.dlss_g.dll");

	sl::Preferences pref{};
	pref.showConsole = false;
	pref.logLevel = sl::LogLevel::eVerbose;
	pref.logMessageCallback = [](sl::LogType, const char* msg) {
		REX::INFO("[SL-INT] {}", msg);
	};
	pref.engine = sl::EngineType::eCustom;
	pref.engineVersion = "1.0.0";
	pref.projectId = "f4f4f4f4f4f4f4f4f4f4f4f4f4f4f4f4";
	pref.flags = sl::PreferenceFlags::eDisableCLStateTracking | sl::PreferenceFlags::eUseManualHooking;
	pref.renderAPI = sl::RenderAPI::eD3D12;

	// Get the real path where sl.interposer.dll lives
	static wchar_t interposerDir[MAX_PATH];
	GetModuleFileNameW(interposer, interposerDir, MAX_PATH);
	wchar_t* lastSlash = wcsrchr(interposerDir, L'\\');
	if (!lastSlash) lastSlash = wcsrchr(interposerDir, L'/');
	if (lastSlash) *lastSlash = L'\0';

	static const wchar_t* pluginPaths[] = { interposerDir };
	pref.pathsToPlugins = pluginPaths;
	pref.numPathsToPlugins = 1;

	static sl::Feature features[] = { sl::kFeatureDLSS_G, sl::kFeatureReflex, sl::kFeaturePCL };
	pref.featuresToLoad = features;
	pref.numFeaturesToLoad = _countof(features);

	auto result = slInit(pref, sl::kSDKVersion);
	REX::INFO("[DLSSG] slInit result: {}", (int)result);

	if (result != sl::Result::eOk) {
		REX::ERROR("[DLSSG] Streamline init failed");
		return false;
	}

	slInitialized = true;
	return true;
}

void StreamlineFG::SetD3DDevice(ID3D12Device* a_device)
{
	d3d12Device = a_device;
	if (slSetD3DDevice && slInitialized) {
		slSetD3DDevice(a_device);
		REX::INFO("[DLSSG] D3D12 device set");
	}
}

void StreamlineFG::UpgradeSwapChain(IDXGISwapChain4** a_swapChain)
{
	if (slUpgradeInterface && slInitialized) {
		auto result = slUpgradeInterface((void**)a_swapChain);
		REX::INFO("[DLSSG] Swap chain upgrade: {}", (int)result);
	}
}

bool StreamlineFG::CheckAndEnableDLSSG()
{
	if (!slInitialized) return false;

	// Load DLSS-G specific functions
	if (slGetFeatureFunction) {
		auto r1 = slGetFeatureFunction(sl::kFeatureDLSS_G, "slDLSSGSetOptions", (void*&)slDLSSGSetOptions);
		auto r2 = slGetFeatureFunction(sl::kFeatureDLSS_G, "slDLSSGGetState", (void*&)slDLSSGGetState);
		REX::INFO("[DLSSG] slDLSSGSetOptions load: {} (ptr={:#x})", (int)r1, (uintptr_t)slDLSSGSetOptions);
		REX::INFO("[DLSSG] slDLSSGGetState load: {} (ptr={:#x})", (int)r2, (uintptr_t)slDLSSGGetState);
	}

	if (!slDLSSGSetOptions) {
		REX::WARN("[DLSSG] Could not load slDLSSGSetOptions");
		return false;
	}

	// Enable DLSS-G
	sl::DLSSGOptions options{};
	options.mode = sl::DLSSGMode::eOn;
	options.numFramesToGenerate = 1;

	auto result = slDLSSGSetOptions(viewport, options);
	REX::INFO("[DLSSG] SetOptions result: {}", (int)result);

	if (result != sl::Result::eOk) {
		REX::WARN("[DLSSG] Failed to enable DLSS-G: {}", (int)result);
		return false;
	}

	featureDLSSG = true;
	REX::INFO("[DLSSG] DLSS-G enabled!");

	if (slDLSSGGetState) {
		sl::DLSSGState state{};
		slDLSSGGetState(viewport, state, nullptr);
		REX::INFO("[DLSSG] State: status={}, minDim={}, maxFrames={}",
			(int)state.status, state.minWidthOrHeight, state.numFramesToGenerateMax);
	}

	return true;
}

void StreamlineFG::Present(bool a_useFrameGen,
	ID3D12GraphicsCommandList* a_cmdList,
	ID3D12Resource* a_depth,
	ID3D12Resource* a_motionVectors,
	ID3D12Resource* a_hudlessColor,
	float2 a_screenSize, float2 a_renderSize,
	float2 a_jitter,
	float a_cameraNear, float a_cameraFar)
{
	if (!featureDLSSG) return;

	// 1. Get frame token
	if (slGetNewFrameToken) {
		if (SL_FAILED(res, slGetNewFrameToken(frameToken, nullptr))) {
			static bool loggedOnce = false;
			if (!loggedOnce) { REX::ERROR("[DLSSG] Failed to get frame token"); loggedOnce = true; }
			return;
		}
	}

	// 2. Set per-frame constants
	if (slSetConstants && frameToken) {
		sl::Constants constants{};
		constants.cameraNear = a_cameraNear;
		constants.cameraFar = a_cameraFar;
		constants.cameraAspectRatio = a_screenSize.x / a_screenSize.y;
		constants.cameraFOV = 0.0f;
		constants.cameraMotionIncluded = sl::Boolean::eTrue;
		constants.cameraPinholeOffset = { 0.f, 0.f };
		constants.depthInverted = sl::Boolean::eFalse;
		constants.jitterOffset = { -a_jitter.x, -a_jitter.y };
		constants.mvecScale = { a_renderSize.x, a_renderSize.y };
		constants.reset = sl::Boolean::eFalse;
		constants.motionVectors3D = sl::Boolean::eFalse;
		constants.motionVectorsInvalidValue = FLT_MIN;
		constants.orthographicProjection = sl::Boolean::eFalse;
		constants.motionVectorsDilated = sl::Boolean::eFalse;
		constants.motionVectorsJittered = sl::Boolean::eFalse;

		if (SL_FAILED(res, slSetConstants(constants, *frameToken, viewport))) {
			static bool loggedOnce = false;
			if (!loggedOnce) { REX::ERROR("[DLSSG] Failed to set constants"); loggedOnce = true; }
		}
	}

	// 3. Tag D3D12 resources
	if (slSetTag && a_depth && a_motionVectors && a_hudlessColor) {
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
			REX::INFO("[DLSSG] First resource tag: depth={:#x}, mvec={:#x}, hudless={:#x}",
				(uintptr_t)a_depth, (uintptr_t)a_motionVectors, (uintptr_t)a_hudlessColor);
			loggedOnce = true;
		}
	}

	// 4. Toggle DLSS-G mode
	if (slDLSSGSetOptions) {
		sl::DLSSGOptions options{};
		options.mode = a_useFrameGen ? sl::DLSSGMode::eOn : sl::DLSSGMode::eOff;
		options.numFramesToGenerate = 1;
		slDLSSGSetOptions(viewport, options);
	}

	// DLSS-G generates frames asynchronously when swapChain->Present() is called
	// No slEvaluateFeature needed — Streamline intercepts Present internally
}

void StreamlineFG::Shutdown()
{
	if (slInitialized && slShutdown) {
		slShutdown();
		slInitialized = false;
	}
}

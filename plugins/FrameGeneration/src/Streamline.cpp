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
	slSetTagForFrame = (PFun_slSetTagForFrame2*)GetProcAddress(interposer, "slSetTagForFrame");
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
	pref.flags = sl::PreferenceFlags::eUseManualHooking | sl::PreferenceFlags::eUseFrameBasedResourceTagging;
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

void StreamlineFG::UpgradeDevice(ID3D12Device** a_device)
{
	if (slUpgradeInterface && slInitialized) {
		auto result = slUpgradeInterface((void**)a_device);
		REX::INFO("[DLSSG] D3D12 device upgrade: {}", (int)result);
	}
}

void StreamlineFG::UpgradeFactory(IDXGIFactory** a_factory)
{
	if (slUpgradeInterface && slInitialized) {
		auto result = slUpgradeInterface((void**)a_factory);
		REX::INFO("[DLSSG] DXGI factory upgrade: {}", (int)result);
	}
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

		// Load Reflex/PCL functions — required by DLSS-G
		auto r3 = slGetFeatureFunction(sl::kFeatureReflex, "slReflexSetOptions", (void*&)slReflexSetOptions);
		auto r4 = slGetFeatureFunction(sl::kFeatureReflex, "slReflexSleep", (void*&)slReflexSleep);
		auto r5 = slGetFeatureFunction(sl::kFeaturePCL, "slPCLSetMarker", (void*&)slPCLSetMarker);
		auto r6 = slGetFeatureFunction(sl::kFeatureReflex, "slReflexSetMarker", (void*&)slReflexSetMarker);
		REX::INFO("[DLSSG] slReflexSetOptions load: {} (ptr={:#x})", (int)r3, (uintptr_t)slReflexSetOptions);
		REX::INFO("[DLSSG] slReflexSleep load: {} (ptr={:#x})", (int)r4, (uintptr_t)slReflexSleep);
		REX::INFO("[DLSSG] slPCLSetMarker load: {} (ptr={:#x})", (int)r5, (uintptr_t)slPCLSetMarker);
		REX::INFO("[DLSSG] slReflexSetMarker load: {} (ptr={:#x})", (int)r6, (uintptr_t)slReflexSetMarker);
	}

	if (!slDLSSGSetOptions) {
		REX::WARN("[DLSSG] Could not load slDLSSGSetOptions");
		return false;
	}

	// Enable DLSS-G (set once at init, not per-frame per DLSS-G docs)
	sl::DLSSGOptions options{};
	options.mode = sl::DLSSGMode::eOn;
	options.numFramesToGenerate = 1;
	options.colorBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	options.hudLessBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	options.depthBufferFormat = DXGI_FORMAT_R32_FLOAT;

	auto result = slDLSSGSetOptions(viewport, options);
	REX::INFO("[DLSSG] SetOptions result: {}", (int)result);

	if (result != sl::Result::eOk) {
		REX::WARN("[DLSSG] Failed to enable DLSS-G: {}", (int)result);
		return false;
	}

	featureDLSSG = true;
	REX::INFO("[DLSSG] DLSS-G enabled!");

	// Reflex options must be set at least once (even if mode is Off)
	if (slReflexSetOptions) {
		sl::ReflexOptions reflexOptions{};
		reflexOptions.mode = sl::ReflexMode::eLowLatency;
		auto reflexResult = slReflexSetOptions(reflexOptions);
		REX::INFO("[DLSSG] ReflexSetOptions result: {}", (int)reflexResult);
	}

	if (slDLSSGGetState) {
		sl::DLSSGState state{};
		slDLSSGGetState(viewport, state, nullptr);
		REX::INFO("[DLSSG] State: status={}, minDim={}, maxFrames={}",
			(int)state.status, state.minWidthOrHeight, state.numFramesToGenerateMax);
	}

	return true;
}

// Convert game's __m128[4] row-major matrix to Streamline's float4x4
static sl::float4x4 toSLMatrix(const __m128* mat)
{
	sl::float4x4 result;
	for (int i = 0; i < 4; i++) {
		alignas(16) float row[4];
		_mm_store_ps(row, mat[i]);
		result[i] = sl::float4(row[0], row[1], row[2], row[3]);
	}
	return result;
}

static sl::float3 toSLFloat3(const __m128* v)
{
	alignas(16) float vals[4];
	_mm_store_ps(vals, *v);
	return sl::float3(vals[0], vals[1], vals[2]);
}

void StreamlineFG::AcquireFrameToken()
{
	if (!slGetNewFrameToken || !featureDLSSG) return;

	if (SL_FAILED(res, slGetNewFrameToken(frameToken, nullptr))) {
		static bool loggedOnce = false;
		if (!loggedOnce) { REX::ERROR("[DLSSG] Failed to get frame token"); loggedOnce = true; }
	}
}

void StreamlineFG::SetPCLMarker(sl::PCLMarker marker)
{
	if (!frameToken) return;

	if (slReflexSetMarker) {
		auto result = slReflexSetMarker(marker, *frameToken);

		// Log ePresentStart — every frame for first 200, then every 300th
		if (marker == sl::PCLMarker::ePresentStart) {
			static int presentCount = 0;
			presentCount++;
			if (presentCount <= 200 || presentCount % 300 == 0) {
				REX::INFO("[DLSSG] ePresentStart #{}: result={}, token={}", presentCount, (int)result, (uint64_t)*frameToken);
			}
		}
	} else if (slPCLSetMarker) {
		slPCLSetMarker(marker, *frameToken);
	}
}

void StreamlineFG::Present(bool a_useFrameGen,
	ID3D12GraphicsCommandList* a_cmdList,
	ID3D12Resource* a_depth,
	ID3D12Resource* a_motionVectors,
	ID3D12Resource* a_hudlessColor,
	float2 a_screenSize, float2 a_renderSize,
	float2 a_jitter,
	float a_cameraNear, float a_cameraFar,
	const CameraData& a_camera)
{
	if (!featureDLSSG || !frameToken) return;
	(void)a_useFrameGen;

	// 1. Set per-frame constants with camera matrices
	if (slSetConstants && frameToken) {
		sl::Constants constants{};

		// Camera matrices — MUST be unjittered per DLSS-G docs
		// projMat may be jittered, so derive unjittered projection: inv(view) * viewProjUnjittered
		sl::float4x4 viewMatrix = toSLMatrix(a_camera.viewMat);
		sl::float4x4 invView;
		sl::matrixFullInvert(invView, viewMatrix);
		sl::float4x4 vpUnjittered = toSLMatrix(a_camera.viewProjUnjittered);
		sl::matrixMul(constants.cameraViewToClip, invView, vpUnjittered);
		sl::matrixFullInvert(constants.clipToCameraView, constants.cameraViewToClip);

		sl::float4x4 currentVP = toSLMatrix(a_camera.currentViewProjUnjittered);
		sl::float4x4 previousVP = toSLMatrix(a_camera.previousViewProjUnjittered);
		sl::float4x4 invCurrentVP;
		sl::matrixFullInvert(invCurrentVP, currentVP);
		sl::matrixMul(constants.clipToPrevClip, invCurrentVP, previousVP);
		sl::matrixFullInvert(constants.prevClipToClip, constants.clipToPrevClip);

		constants.cameraPos = sl::float3(a_camera.posX, a_camera.posY, a_camera.posZ);
		constants.cameraUp = toSLFloat3(a_camera.viewUp);
		constants.cameraRight = toSLFloat3(a_camera.viewRight);
		constants.cameraFwd = toSLFloat3(a_camera.viewDir);
		constants.cameraNear = a_cameraNear;
		constants.cameraFar = a_cameraFar;
		constants.cameraAspectRatio = a_screenSize.x / a_screenSize.y;
		constants.cameraFOV = 0.0f;
		constants.cameraMotionIncluded = sl::Boolean::eTrue;
		constants.cameraPinholeOffset = { 0.f, 0.f };
		constants.depthInverted = sl::Boolean::eTrue;
		constants.jitterOffset = { -a_jitter.x, -a_jitter.y };
		(void)a_renderSize;
		constants.mvecScale = { 1.0f, 1.0f };
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

		static bool loggedOnce = false;
		if (!loggedOnce) {
			REX::INFO("[DLSSG] First constants with camera matrices: pos=({},{},{}), near={}, far={}",
				a_camera.posX, a_camera.posY, a_camera.posZ, a_cameraNear, a_cameraFar);
			loggedOnce = true;
		}
	}

	// 3. Tag D3D12 resources (frame-based tagging)
	if (frameToken && a_depth && a_motionVectors && a_hudlessColor && slSetTagForFrame) {
		sl::Resource depth = { sl::ResourceType::eTex2d, a_depth, 0 };
		sl::Resource mvec = { sl::ResourceType::eTex2d, a_motionVectors, 0 };
		sl::Resource hudless = { sl::ResourceType::eTex2d, a_hudlessColor, 0 };

		sl::ResourceTag depthTag = { &depth, sl::kBufferTypeDepth, sl::ResourceLifecycle::eValidUntilPresent, nullptr };
		sl::ResourceTag mvecTag = { &mvec, sl::kBufferTypeMotionVectors, sl::ResourceLifecycle::eValidUntilPresent, nullptr };
		sl::ResourceTag hudlessTag = { &hudless, sl::kBufferTypeHUDLessColor, sl::ResourceLifecycle::eValidUntilPresent, nullptr };

		// Tag null UI overlay — tells DLSS-G there's no separate UI layer
		sl::ResourceTag uiTag = { nullptr, sl::kBufferTypeUIColorAndAlpha, sl::ResourceLifecycle::eValidUntilPresent, nullptr };

		sl::ResourceTag tags[] = { depthTag, mvecTag, hudlessTag, uiTag };
		slSetTagForFrame(*frameToken, viewport, tags, _countof(tags), (sl::CommandBuffer*)a_cmdList);

		static bool loggedOnce = false;
		if (!loggedOnce) {
			REX::INFO("[DLSSG] First resource tag: depth={:#x}, mvec={:#x}, hudless={:#x}",
				(uintptr_t)a_depth, (uintptr_t)a_motionVectors, (uintptr_t)a_hudlessColor);
			loggedOnce = true;
		}
	}

	// slDLSSGSetOptions is called once at init (CheckAndEnableDLSSG), not per-frame

	// 5. Periodic DLSS-G state diagnostics — decode ALL status flags
	if (slDLSSGGetState) {
		static int stateCheckCount = 0;
		stateCheckCount++;
		if (stateCheckCount <= 10 || stateCheckCount % 100 == 0) {
			sl::DLSSGState state{};
			slDLSSGGetState(viewport, state, nullptr);

			// Decode status bitmask
			auto s = (uint32_t)state.status;
			if (s == 0) {
				REX::INFO("[DLSSG] State #{}: OK, numFGMax={}", stateCheckCount, state.numFramesToGenerateMax);
			} else {
				std::string flags;
				if (s & 1) flags += "ResolutionTooLow|";
				if (s & 2) flags += "ReflexNotDetected|";
				if (s & 4) flags += "HDRFormatNotSupported|";
				if (s & 8) flags += "ConstantsInvalid|";
				if (s & 16) flags += "GetCurrentBackBufferIndexNotCalled|";
				if (!flags.empty()) flags.pop_back(); // remove trailing |
				REX::WARN("[DLSSG] State #{}: INVALID status=0x{:x} [{}]", stateCheckCount, s, flags);
			}
		}
	}
}

void StreamlineFG::Shutdown()
{
	if (slInitialized && slShutdown) {
		slShutdown();
		slInitialized = false;
	}
}

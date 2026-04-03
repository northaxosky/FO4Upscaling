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
}

bool StreamlineFG::InitStreamline()
{
	if (!interposer || !slInit) return false;

	REX::INFO("[DLSSG] Initializing Streamline");

	sl::Preferences pref{};
	pref.showConsole = false;
	pref.logLevel = sl::LogLevel::eVerbose;
	pref.logMessageCallback = [](sl::LogType, const char* msg) {
		REX::INFO("[SL-INT] {}", msg);
	};
	pref.engine = sl::EngineType::eCustom;
	pref.engineVersion = "1.0.0";
	pref.projectId = "f4-frame-generation";
	pref.flags |= sl::PreferenceFlags::eUseManualHooking;
	pref.renderAPI = sl::RenderAPI::eD3D12;

	// Get the real path where sl.interposer.dll lives (resolves through MO2 USVFS)
	static wchar_t interposerDir[MAX_PATH];
	GetModuleFileNameW(interposer, interposerDir, MAX_PATH);
	wchar_t* lastSlash = wcsrchr(interposerDir, L'\\');
	if (!lastSlash) lastSlash = wcsrchr(interposerDir, L'/');
	if (lastSlash) *lastSlash = L'\0';

	static const wchar_t* pluginPaths[] = { interposerDir };
	pref.pathsToPlugins = pluginPaths;
	pref.numPathsToPlugins = 1;

	// Pre-load plugin DLLs for MO2 USVFS compatibility
	LoadLibrary(L"Data\\F4SE\\Plugins\\Streamline\\sl.common.dll");
	LoadLibrary(L"Data\\F4SE\\Plugins\\Streamline\\sl.dlss_g.dll");

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

	// Skip feature support check — we know we have an RTX 40+ from GPU logging
	// slIsFeatureSupported returns MissingOrInvalidAPI with eD3D11 render API
	// but the feature works through the swap chain interception pathway
	REX::INFO("[DLSSG] Skipping feature support check, attempting direct enable");

	// Load DLSS-G specific functions
	if (slGetFeatureFunction) {
		slGetFeatureFunction(sl::kFeatureDLSS_G, "slDLSSGSetOptions", (void*&)slDLSSGSetOptions);
		slGetFeatureFunction(sl::kFeatureDLSS_G, "slDLSSGGetState", (void*&)slDLSSGGetState);
	}

	if (!slDLSSGSetOptions) {
		REX::WARN("[DLSSG] Could not load slDLSSGSetOptions");
		return false;
	}

	// Enable DLSS-G
	sl::DLSSGOptions options{};
	options.mode = sl::DLSSGMode::eOn;
	options.numFramesToGenerate = 1;
	sl::ViewportHandle viewport{ 0 };

	auto result = slDLSSGSetOptions(viewport, options);
	REX::INFO("[DLSSG] SetOptions result: {}", (int)result);

	if (result != sl::Result::eOk) {
		REX::WARN("[DLSSG] Failed to enable DLSS-G: {}", (int)result);
		return false;
	}

	featureDLSSG = true;
	REX::INFO("[DLSSG] DLSS-G enabled!");

	// Query state
	if (slDLSSGGetState) {
		sl::DLSSGState state{};
		slDLSSGGetState(viewport, state, nullptr);
		REX::INFO("[DLSSG] State: status={}, minDim={}, maxFrames={}", (int)state.status, state.minWidthOrHeight, state.numFramesToGenerateMax);
	}

	return true;
}

void StreamlineFG::Present(bool a_useFrameGen)
{
	// DLSS-G frame generation is handled by Streamline's swap chain interception
	// We just need to enable/disable it via slDLSSGSetOptions
	if (!featureDLSSG || !slDLSSGSetOptions) return;

	sl::DLSSGOptions options{};
	options.mode = a_useFrameGen ? sl::DLSSGMode::eOn : sl::DLSSGMode::eOff;
	options.numFramesToGenerate = 1;
	sl::ViewportHandle viewport{ 0 };
	slDLSSGSetOptions(viewport, options);
}

void StreamlineFG::Shutdown()
{
	if (slInitialized && slShutdown) {
		slShutdown();
		slInitialized = false;
	}
}

#include "XeSSFG.h"
#include "Upscaling.h"

bool XeSSFG::LoadLibraries()
{
	fgModule = LoadLibrary(L"Data\\F4SE\\Plugins\\FrameGeneration\\XeSS\\libxess_fg.dll");
	if (!fgModule) {
		REX::WARN("[XeSS-FG] Failed to load libxess_fg.dll: {:#x}", GetLastError());
		return false;
	}

	xellModule = LoadLibrary(L"Data\\F4SE\\Plugins\\FrameGeneration\\XeSS\\libxell.dll");
	if (!xellModule) {
		REX::WARN("[XeSS-FG] Failed to load libxell.dll: {:#x}", GetLastError());
		FreeLibrary(fgModule);
		fgModule = nullptr;
		return false;
	}

	REX::INFO("[XeSS-FG] Libraries loaded");
	return true;
}

bool XeSSFG::CreateContexts(ID3D12Device* a_device)
{
	if (!fgModule || !xellModule) return false;

	// Create XeLL context first (required by XeSS-FG)
	auto xellResult = xellD3D12CreateContext(a_device, &xellCtx);
	if (xellResult != XELL_RESULT_SUCCESS) {
		REX::WARN("[XeSS-FG] XeLL context creation failed: {}", (int)xellResult);
		return false;
	}
	REX::INFO("[XeSS-FG] XeLL context created");

	// Create XeSS-FG context
	auto xefgResult = xefgSwapChainD3D12CreateContext(a_device, &xefgCtx);
	if (xefgResult != XEFG_SWAPCHAIN_RESULT_SUCCESS) {
		REX::WARN("[XeSS-FG] Context creation failed: {}", (int)xefgResult);
		xellDestroyContext(xellCtx);
		xellCtx = nullptr;
		return false;
	}
	REX::INFO("[XeSS-FG] Context created");

	// Wire XeLL to XeSS-FG for latency reduction
	xefgResult = xefgSwapChainSetLatencyReduction(xefgCtx, xellCtx);
	if (xefgResult != XEFG_SWAPCHAIN_RESULT_SUCCESS) {
		REX::WARN("[XeSS-FG] Failed to set latency reduction: {}", (int)xefgResult);
	}

	// Configure logging
	auto debugLogging = Upscaling::GetSingleton()->settings.debugLogging;
	if (debugLogging) {
		xefgSwapChainSetLoggingCallback(xefgCtx,
			XEFG_SWAPCHAIN_LOGGING_LEVEL_DEBUG,
			[](const char* msg, xefg_swapchain_logging_level_t, void*) {
				REX::INFO("[XeSS-INT] {}", msg);
			}, nullptr);

		xellSetLoggingCallback(xellCtx,
			XELL_LOGGING_LEVEL_DEBUG,
			[](const char* msg, xell_logging_level_t) {
				REX::INFO("[XeLL-INT] {}", msg);
			});
	}

	// Enable XeLL low latency mode
	xell_sleep_params_t sleepParams{};
	sleepParams.bLowLatencyMode = 1;
	sleepParams.minimumIntervalUs = 0;
	xellSetSleepMode(xellCtx, &sleepParams);

	// Query max supported interpolations
	xefg_swapchain_properties_t props{};
	if (xefgSwapChainGetProperties(xefgCtx, &props) == XEFG_SWAPCHAIN_RESULT_SUCCESS) {
		REX::INFO("[XeSS-FG] Max supported interpolations: {}", props.maxSupportedInterpolations);
	}

	return true;
}

bool XeSSFG::InitSwapChain(ID3D12CommandQueue* a_cmdQueue, IDXGIFactory5* a_factory,
	const DXGI_SWAP_CHAIN_DESC1& a_desc, HWND a_hwnd, IDXGISwapChain4** a_outSwapChain)
{
	if (!xefgCtx) return false;

	xefg_swapchain_d3d12_init_params_t initParams{};
	initParams.pApplicationSwapChain = nullptr;
	initParams.initFlags = XEFG_SWAPCHAIN_INIT_FLAG_INVERTED_DEPTH;
	initParams.maxInterpolatedFrames = XEFG_SWAPCHAIN_USE_MAX_SUPPORTED_INTERPOLATED_FRAMES;
	initParams.creationNodeMask = 0;
	initParams.visibleNodeMask = 0;
	initParams.pTempBufferHeap = nullptr;
	initParams.bufferHeapOffset = 0;
	initParams.pTempTextureHeap = nullptr;
	initParams.textureHeapOffset = 0;
	initParams.pPipelineLibrary = nullptr;
	initParams.uiMode = XEFG_SWAPCHAIN_UI_MODE_AUTO;

	auto result = xefgSwapChainD3D12InitFromSwapChainDesc(
		xefgCtx, a_hwnd, &a_desc, nullptr,
		a_cmdQueue, (IDXGIFactory2*)a_factory, &initParams);

	if (result != XEFG_SWAPCHAIN_RESULT_SUCCESS) {
		REX::WARN("[XeSS-FG] Swap chain init failed: {}", (int)result);
		return false;
	}

	result = xefgSwapChainD3D12GetSwapChainPtr(xefgCtx, IID_PPV_ARGS(a_outSwapChain));
	if (result != XEFG_SWAPCHAIN_RESULT_SUCCESS) {
		REX::WARN("[XeSS-FG] Failed to get proxy swap chain: {}", (int)result);
		return false;
	}

	// Enable frame generation
	xefgSwapChainSetEnabled(xefgCtx, 1);

	initialized = true;
	REX::INFO("[XeSS-FG] Initialized and enabled");
	return true;
}

void XeSSFG::BeginFrame(uint32_t a_frameId)
{
	if (!xellCtx) return;

	xellSleep(xellCtx, a_frameId);
	xellAddMarkerData(xellCtx, a_frameId, XELL_SIMULATION_START);
}

void XeSSFG::SetMarker(xell_latency_marker_type_t a_marker, uint32_t a_frameId)
{
	if (!xellCtx) return;
	xellAddMarkerData(xellCtx, a_frameId, a_marker);
}

void XeSSFG::TagResources(uint32_t a_frameId,
	ID3D12GraphicsCommandList* a_cmdList,
	ID3D12Resource* a_depth,
	ID3D12Resource* a_motionVectors,
	float2 a_screenSize, float2 a_jitter, float2 a_mvScale,
	float a_frameTimeDelta,
	const float* a_viewMatrix, const float* a_projMatrix,
	bool a_reset)
{
	if (!xefgCtx) return;

	// Tag depth
	if (a_depth) {
		xefg_swapchain_d3d12_resource_data_t depthData{};
		depthData.type = XEFG_SWAPCHAIN_RES_DEPTH;
		depthData.validity = XEFG_SWAPCHAIN_RV_UNTIL_NEXT_PRESENT;
		depthData.resourceBase = { 0, 0 };
		depthData.resourceSize = { (uint32_t)a_screenSize.x, (uint32_t)a_screenSize.y };
		depthData.pResource = a_depth;
		depthData.incomingState = D3D12_RESOURCE_STATE_COMMON;
		xefgSwapChainD3D12TagFrameResource(xefgCtx, a_cmdList, a_frameId, &depthData);
	}

	// Tag motion vectors
	if (a_motionVectors) {
		xefg_swapchain_d3d12_resource_data_t mvData{};
		mvData.type = XEFG_SWAPCHAIN_RES_MOTION_VECTOR;
		mvData.validity = XEFG_SWAPCHAIN_RV_UNTIL_NEXT_PRESENT;
		mvData.resourceBase = { 0, 0 };
		mvData.resourceSize = { (uint32_t)a_screenSize.x, (uint32_t)a_screenSize.y };
		mvData.pResource = a_motionVectors;
		mvData.incomingState = D3D12_RESOURCE_STATE_COMMON;
		xefgSwapChainD3D12TagFrameResource(xefgCtx, a_cmdList, a_frameId, &mvData);
	}

	// Tag frame constants
	xefg_swapchain_frame_constant_data_t constants{};

	if (a_viewMatrix)
		memcpy(constants.viewMatrix, a_viewMatrix, sizeof(float) * 16);
	if (a_projMatrix)
		memcpy(constants.projectionMatrix, a_projMatrix, sizeof(float) * 16);

	constants.jitterOffsetX = a_jitter.x;
	constants.jitterOffsetY = a_jitter.y;
	constants.motionVectorScaleX = a_mvScale.x;
	constants.motionVectorScaleY = a_mvScale.y;
	constants.resetHistory = a_reset ? 1u : 0u;
	constants.frameRenderTime = a_frameTimeDelta;

	xefgSwapChainTagFrameConstants(xefgCtx, a_frameId, &constants);

	// Set present ID
	xefgSwapChainSetPresentId(xefgCtx, a_frameId);
}

void XeSSFG::Shutdown()
{
	if (xefgCtx) {
		xefgSwapChainDestroy(xefgCtx);
		xefgCtx = nullptr;
	}
	if (xellCtx) {
		xellDestroyContext(xellCtx);
		xellCtx = nullptr;
	}
	initialized = false;
}

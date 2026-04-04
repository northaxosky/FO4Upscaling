#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>

#define NV_WINDOWS
#pragma warning(push)
#pragma warning(disable: 4471)
#include <sl.h>
#include <sl_consts.h>
#include <sl_dlss_g.h>
#include <sl_matrix_helpers.h>
#include <sl_version.h>
#pragma warning(pop)

using PFun_slSetTag2 = sl::Result(const sl::ViewportHandle& viewport, const sl::ResourceTag* tags, uint32_t numTags, sl::CommandBuffer* cmdBuffer);

class StreamlineFG
{
public:
	static StreamlineFG* GetSingleton()
	{
		static StreamlineFG singleton;
		return &singleton;
	}

	void LoadInterposer();
	bool InitStreamline();
	void SetD3DDevice(ID3D12Device* a_device);
	void UpgradeSwapChain(IDXGISwapChain4** a_swapChain);
	bool CheckAndEnableDLSSG();

	// Camera matrices as __m128[4] from BSGraphics::State::ViewData
	struct CameraData
	{
		const __m128* projMat;                     // view → clip (unjittered)
		const __m128* currentViewProjUnjittered;   // current VP (unjittered)
		const __m128* previousViewProjUnjittered;  // previous VP (unjittered)
		const __m128* viewUp;
		const __m128* viewRight;
		const __m128* viewDir;
		float posX, posY, posZ;                    // camera position
	};

	void Present(bool a_useFrameGen,
		ID3D12GraphicsCommandList* a_cmdList,
		ID3D12Resource* a_depth,
		ID3D12Resource* a_motionVectors,
		ID3D12Resource* a_hudlessColor,
		float2 a_screenSize, float2 a_renderSize,
		float2 a_jitter,
		float a_cameraNear, float a_cameraFar,
		const CameraData& a_camera);

	void Shutdown();

	bool slInitialized = false;
	bool featureDLSSG = false;
	ID3D12Device* d3d12Device = nullptr;
	HMODULE interposer = nullptr;
	sl::ViewportHandle viewport{ 0 };
	sl::FrameToken* frameToken{};

	// Core SL function pointers
	PFun_slInit* slInit{};
	PFun_slShutdown* slShutdown{};
	PFun_slUpgradeInterface* slUpgradeInterface{};
	PFun_slSetD3DDevice* slSetD3DDevice{};
	PFun_slIsFeatureSupported* slIsFeatureSupported{};
	PFun_slGetFeatureFunction* slGetFeatureFunction{};
	PFun_slSetTag2* slSetTag{};
	PFun_slSetConstants* slSetConstants{};
	PFun_slGetNewFrameToken* slGetNewFrameToken{};
	PFun_slEvaluateFeature* slEvaluateFeature{};

	// DLSS-G function pointers
	PFun_slDLSSGSetOptions* slDLSSGSetOptions{};
	PFun_slDLSSGGetState* slDLSSGGetState{};
};
